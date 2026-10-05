#include <cairo.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <unistd.h>
#include <climits>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "Ipc.hpp"
#include "Autostart.hpp"
#include "Process.hpp"
#include <filesystem>
#include "Overlay.hpp"
#include "Settings.hpp"
#include "Sim.hpp"
#include "accessories/Registry.hpp"

namespace mousepet {
namespace {

constexpr const char* VERSION = "0.3.0";

void usage() {
    std::fprintf(
        stderr,
        "MousePet / CursorPet %s — physical desktop cursor companion\n"
        "\n"
        "Usage:\n"
        "  mousepet [run] [options]                   run the background companion\n"
        "  mousepet pet next | prev | <id>            switch pet accessory\n"
        "  mousepet --pet --next | --pet <id>         switch pet (flag syntax)\n"
        "  mousepet burst                             emit click burst\n"
        "  mousepet plugin load | unload | status     manage Hyprland hook plugin\n"
        "  mousepet autostart enable|disable|status   configure Hyprland startup\n"
        "  mousepet ctl COMMAND...                    control running instance\n"
        "  mousepet list                              list accessories and parameters\n"
        "  mousepet snapshot OUT.png [options]        headless render (tests/previews)\n"
        "\n"
        "Control commands:\n"
        "  burst | next | prev | select ID | set KEY VALUE | reset\n"
        "  blur on|off|toggle | show | hide | toggle | status | list | quit\n"
        "  KEY: ropeLength stiffness drag ropeGravity scale, or any accessory parameter\n"
        "\n"
        "Run options:\n"
        "  --demo            orbit a virtual cursor for sandbox testing\n"
        "  --accessory ID    start with this accessory\n"
        "\n"
        "Environment variables:\n"
        "  MOUSEPET_ACCESSORY / CURSORPET_ACCESSORY   default accessory on start\n"
        "  MOUSEPET_SCALE / CURSORPET_SCALE           visual scale factor (0.25..4.0)\n"
        "  MOUSEPET_CONFIG                            path to settings.ini\n"
        "  MOUSEPET_SOCKET                            control socket path\n",
        VERSION);
}

std::string fmt(float v) {
    char b[32];
    std::snprintf(b, sizeof b, "%g", double(v));
    return b;
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (static_cast<unsigned char>(c) < 0x20) o += ' ';
        else o += c;
    }
    return o;
}

std::string listText() {
    loadDynamicAccessories();
    std::ostringstream s;
    for (const auto& d : accessories()) {
        s << d.id << "\t" << d.name << "\t";
        bool first = true;
        for (const auto& p : d.params) {
            s << (first ? "" : " ") << p.key << "=" << fmt(p.value) << "[" << fmt(p.min) << ".." << fmt(p.max) << "]";
            first = false;
        }
        s << "\n";
    }
    return s.str();
}

/** Virtual cursor paths used by --demo and the snapshot renderer. */
std::pair<float, float> demoCursor(const std::string& motion, int frame, float cx, float cy) {
    const float t = frame / 60.f;
    if (motion == "circle") return {cx + std::cos(t * 1.7f) * 160.f, cy + std::sin(t * 1.7f) * 90.f};
    if (motion == "shake") return {cx + ((frame / 3) % 2 ? 120.f : -120.f), cy + ((frame / 5) % 2 ? 40.f : -40.f)};
    return {cx, cy};
}

// ── Headless snapshot ─────────────────────────────────────────────────
int snapshot(int argc, char** argv) {
    loadDynamicAccessories();
    if (argc < 1) { usage(); return 2; }
    const std::string out = argv[0];
    std::string acc = "jellyfish", motion = "still";
    int steps = 120, burstAt = -1;
    float scale = 1.f;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--accessory") acc = next();
        else if (a == "--steps") steps = std::atoi(next().c_str());
        else if (a == "--burst-at") burstAt = std::atoi(next().c_str());
        else if (a == "--motion") motion = next();
        else if (a == "--scale") scale = std::strtof(next().c_str(), nullptr);
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    }
    const AccessoryDef* def = findAccessory(acc);
    if (!def) { std::fprintf(stderr, "unknown accessory '%s'\n", acc.c_str()); return 2; }
    const int W = 360, H = 300;
    const float cx = W / 2.f / scale, cy = H * 0.62f / scale;
    Pet pet(1234);
    pet.select(*def);
    pet.place(cx, cy);
    for (int f = 0; f < steps; f++) {
        auto [mx, my] = demoCursor(motion, f, cx, cy);
        pet.setMouse(mx / 1.f, my / 1.f);
        if (f == burstAt) pet.burst();
        pet.step();
    }
    cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t* cr = cairo_create(s);
    cairo_set_source_rgb(cr, 6 / 255., 9 / 255., 20 / 255.);  // HTML background, for previews
    cairo_paint(cr);
    cairo_scale(cr, scale, scale);
    pet.render(cr);
    cairo_destroy(cr);
    const Bounds b = pet.bounds();
    const bool ok = cairo_surface_write_to_png(s, out.c_str()) == CAIRO_STATUS_SUCCESS;
    cairo_surface_destroy(s);
    std::printf("{\"accessory\":\"%s\",\"bounds\":[%.1f,%.1f,%.1f,%.1f],\"body\":[%.2f,%.2f],\"angle\":%.3f,"
                "\"particles\":%d,\"state\":\"%s\",\"error\":\"%s\"}\n",
                def->id.c_str(), b.x0 * scale, b.y0 * scale, b.x1 * scale, b.y1 * scale, pet.body.x * scale,
                pet.body.y * scale, pet.body.angle, pet.particles.alive(), pet.state(), jsonEscape(pet.lastError).c_str());
    return ok ? 0 : 1;
}

// ── Control client ────────────────────────────────────────────────────
int ctl(int argc, char** argv) {
    if (argc < 1) { usage(); return 2; }
    std::string req;
    for (int i = 0; i < argc; i++) req += (i ? " " : "") + std::string(argv[i]);
    auto r = requestUnix(controlSocketPath(), req + "\n", 2000);
    if (!r) { std::fprintf(stderr, "mousepet is not running (no %s)\n", controlSocketPath().c_str()); return 1; }
    std::fputs(r->c_str(), stdout);
    if (!r->empty() && r->back() != '\n') std::fputc('\n', stdout);
    return r->rfind("error", 0) == 0 ? 1 : 0;
}

std::string pluginPath() {
    const auto executable = std::filesystem::path(executablePath());
    std::vector<std::filesystem::path> candidates = {
        executable.parent_path().parent_path() / "lib/mousepet-hyprland.so",
        executable.parent_path() / "mousepet-hyprland.so"
    };
    if (const char* override = std::getenv("MOUSEPET_PLUGIN"); override && *override)
        candidates.insert(candidates.begin(), override);
    for (const auto& candidate : candidates)
        if (std::filesystem::is_regular_file(candidate)) return std::filesystem::canonical(candidate).string();
    return {};
}

int pluginCmd(int argc, char** argv) {
    const std::string sub = argc ? argv[0] : "status";
    if (sub == "status") return runProcess({"hyprctl", "plugin", "list"});
    if (sub != "load" && sub != "unload") {
        std::fprintf(stderr, "Usage: mousepet plugin load|unload|status\n"); return 1;
    }
    const auto path = pluginPath();
    if (path.empty()) { std::fprintf(stderr, "Plugin not found. Reinstall MousePet or run make plugin.\n"); return 1; }
    return runProcess({"hyprctl", "plugin", sub, path});
}

// ── Daemon ────────────────────────────────────────────────────────────
struct Daemon {
    Pet pet{uint32_t(std::chrono::steady_clock::now().time_since_epoch().count())};
    Settings settings;
    std::string settingsPath = Settings::defaultPath();
    PetBridgeClient bridge;
    HyprlandCursor cursor;
    Overlay overlay;
    bool demo = false, running = true;
    int demoFrame = 0;
    float demoCx = 600, demoCy = 400;
    float fps = 0;
    unsigned framesInWindow = 0;
    std::chrono::steady_clock::time_point fpsAt = std::chrono::steady_clock::now();
    unsigned long lastCommits = 0;

    DrawOptions options() const {
        const bool effectiveVisible = settings.visible && (demo || bridge.isCursorVisible());
        return {settings.scale, settings.motionBlur, effectiveVisible};
    }

    void save() {
        if (!settings.save(settingsPath)) std::fprintf(stderr, "[mousepet] could not save %s\n", settingsPath.c_str());
    }

    bool readCursor(float& x, float& y) {
        if (demo) {
            auto p = demoCursor("circle", demoFrame++, demoCx, demoCy);
            x = p.first; y = p.second;
            return true;
        }
        bool ok = false;
        // Priority 1: Hyprland socket IPC query ("cursorpos") - always live and accurate across all monitors
        auto p = cursor.query();
        if (p) {
            x = p->first; y = p->second;
            ok = true;
        } else {
            // Priority 2: SHM bridge position if socket is temporarily busy
            auto bp = bridge.queryPos();
            if (bp) {
                x = bp->first; y = bp->second;
                ok = true;
            }
        }
        if (ok) {
            const auto [ox, oy] = bridge.queryOffset();
            x += ox;
            y += oy;
            return true;
        }
        return false;
    }

    void select(const AccessoryDef& def) {
        const Mouse m = pet.mouse;
        const Body b = pet.body;
        settings.accessory = def.id;
        settings.applyTo(pet);
        pet.mouse = m;
        pet.body.x = b.x; pet.body.y = b.y;
        pet.rope.reset(m.x, m.y, b.x, b.y);
    }

    std::string status() const {
        std::ostringstream s;
        s << "{\"version\":\"" << VERSION << "\",\"accessory\":\"" << (pet.def ? pet.def->id : "") << "\",\"state\":\""
          << pet.state() << "\",\"fps\":" << fmt(fps) << ",\"visible\":" << (settings.visible ? "true" : "false")
          << ",\"cursorVisible\":" << (bridge.isCursorVisible() ? "true" : "false")
          << ",\"pluginConnected\":" << (bridge.isConnected() ? "true" : "false")
          << ",\"motionBlur\":" << (settings.motionBlur ? "true" : "false") << ",\"scale\":" << fmt(settings.scale)
          << ",\"commits\":" << overlay.commits() << ",\"cursor\":[" << fmt(pet.mouse.x * settings.scale) << ","
          << fmt(pet.mouse.y * settings.scale) << "],\"config\":{\"ropeLength\":" << fmt(pet.config.ropeLength)
          << ",\"stiffness\":" << fmt(pet.config.stiffness) << ",\"drag\":" << fmt(pet.config.drag)
          << ",\"ropeGravity\":" << fmt(pet.config.ropeGravity) << "},\"params\":{";
        bool first = true;
        if (pet.def)
            for (const auto& p : pet.def->params) {
                s << (first ? "" : ",") << "\"" << p.key << "\":" << fmt(pet.param(p.key));
                first = false;
            }
        s << "},\"outputs\":\"" << jsonEscape(overlay.describeOutputs()) << "\",\"error\":\"" << jsonEscape(pet.lastError)
          << "\"}\n";
        return s.str();
    }

    std::string command(const std::string& line) {
        std::istringstream in(line);
        std::string cmd, a, b;
        in >> cmd >> a >> b;
        const auto& list = accessories();
        auto indexOf = [&]() -> size_t {
            for (size_t i = 0; i < list.size(); i++)
                if (pet.def && list[i].id == pet.def->id) return i;
            return 0;
        };
        auto toggle = [&](bool& flag, const std::string& v) {
            if (v == "on" || v == "1" || v == "true") flag = true;
            else if (v == "off" || v == "0" || v == "false") flag = false;
            else flag = !flag;
        };
        if (cmd == "burst") { pet.burst(); return "ok\n"; }
        if (cmd == "next" || cmd == "prev") {
            const size_t i = indexOf(), n = list.size();
            select(list[(i + (cmd == "next" ? 1 : n - 1)) % n]);
            save();
            return "ok " + pet.def->id + "\n";
        }
        if (cmd == "select") {
            const AccessoryDef* d = findAccessory(a);
            if (!d) return "error: unknown accessory '" + a + "'\n";
            select(*d);
            save();
            return "ok " + d->id + "\n";
        }
        if (cmd == "set") {
            char* end = nullptr;
            const float v = std::strtof(b.c_str(), &end);
            if (b.empty() || end == b.c_str() || !std::isfinite(v)) return "error: usage: set KEY VALUE\n";
            if (a == "scale") {
                const float old = settings.scale;
                settings.scale = std::clamp(v, 0.25f, 4.f);
                const float k = old / settings.scale;
                pet.place(pet.mouse.x * k, pet.mouse.y * k);
            } else if (!settings.set(pet, a, v)) {
                return "error: unknown key '" + a + "'\n";
            }
            save();
            return "ok\n";
        }
        if (cmd == "reset") { settings.reset(pet); save(); return "ok\n"; }
        if (cmd == "blur") { toggle(settings.motionBlur, a); save(); return std::string("ok ") + (settings.motionBlur ? "on" : "off") + "\n"; }
        if (cmd == "show") { settings.visible = true; save(); return "ok\n"; }
        if (cmd == "hide") { settings.visible = false; save(); return "ok\n"; }
        if (cmd == "toggle") { toggle(settings.visible, ""); save(); return std::string("ok ") + (settings.visible ? "shown" : "hidden") + "\n"; }
        if (cmd == "status") return status();
        if (cmd == "list") return listText();
        if (cmd == "quit") { running = false; return "ok\n"; }
        return "error: unknown command '" + cmd + "' (try: burst next prev select set reset blur show hide toggle status list quit)\n";
    }

    void tick(uint64_t expirations) {
        float x, y;
        if (readCursor(x, y)) {
            x /= settings.scale; y /= settings.scale;
            const float dx = x - pet.mouse.x, dy = y - pet.mouse.y;
            if (dx * dx + dy * dy > 1500.f * 1500.f / (settings.scale * settings.scale)) pet.place(x, y);
            else pet.setMouse(x, y);
        }

        // Periodic reconnect check if bridge started later
        if (!bridge.isConnected() && (framesInWindow % 60 == 0)) {
            bridge.init();
        }

        // Click detection: trigger burst on pointer down
        uint32_t btn = 0;
        if (bridge.checkClick(btn)) {
            pet.burst();
        }

        const int steps = int(std::min<uint64_t>(expirations, 4));
        for (int i = 0; i < steps; i++) pet.step();
        overlay.draw(pet, options());

        framesInWindow++;
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - fpsAt).count();
        if (dt >= 1.0) {
            fps = float((overlay.commits() - lastCommits) / dt);
            lastCommits = overlay.commits();
            fpsAt = now;
            framesInWindow = 0;
        }
    }
};

int makeListenSocket(const std::string& path) {
    sockaddr_un addr{};
    if (path.size() >= sizeof addr.sun_path) return -1;
    unlink(path.c_str());
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) return -1;
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
    const mode_t old = umask(0077);
    const int rc = bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
    umask(old);
    if (rc != 0 || listen(fd, 8) != 0) { close(fd); return -1; }
    return fd;
}

void serveClient(int lfd, Daemon& dm) {
    for (;;) {
        const int c = accept4(lfd, nullptr, nullptr, SOCK_CLOEXEC);
        if (c < 0) return;
        timeval tv{0, 200000};
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
        std::string line;
        char buf[512];
        while (line.find('\n') == std::string::npos && line.size() < 4096) {
            const ssize_t n = read(c, buf, sizeof buf);
            if (n <= 0) break;
            line.append(buf, size_t(n));
        }
        if (auto nl = line.find('\n'); nl != std::string::npos) line.resize(nl);
        const std::string reply = dm.command(line);
        ssize_t ignored = write(c, reply.data(), reply.size());
        (void)ignored;
        close(c);
    }
}

int run(int argc, char** argv) {
    loadDynamicAccessories();
    Daemon dm;
    std::string startAccessory;

    // Check environment variables
    const char* envAcc = std::getenv("MOUSEPET_ACCESSORY");
    if (!envAcc || !*envAcc) envAcc = std::getenv("CURSORPET_ACCESSORY");
    if (envAcc && *envAcc) startAccessory = envAcc;

    const char* envScale = std::getenv("MOUSEPET_SCALE");
    if (!envScale || !*envScale) envScale = std::getenv("CURSORPET_SCALE");
    if (envScale && *envScale) {
        char* end = nullptr;
        float sc = std::strtof(envScale, &end);
        if (end != envScale && sc >= 0.25f && sc <= 4.f) dm.settings.scale = sc;
    }

    for (int i = 0; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--demo") dm.demo = true;
        else if (a == "--accessory" && i + 1 < argc) startAccessory = argv[++i];
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); usage(); return 2; }
    }

    // Single instance per Wayland display.
    const std::string lockPath = lockFilePath();
    const int lockFd = open(lockPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (lockFd < 0 || flock(lockFd, LOCK_EX | LOCK_NB) != 0) {
        std::fprintf(stderr, "mousepet is already running on this display (%s)\n", lockPath.c_str());
        return 1;
    }

    dm.settings.load(dm.settingsPath);
    if (!startAccessory.empty()) {
        if (!findAccessory(startAccessory)) { std::fprintf(stderr, "unknown accessory '%s'\n", startAccessory.c_str()); return 2; }
        dm.settings.accessory = startAccessory;
    }
    dm.settings.applyTo(dm.pet);

    // Detect Hyprland session and auto-load companion hook plugin for clicks/offsets
    if (!dm.demo) {
        dm.cursor.init();
        dm.bridge.init();
        if (!dm.bridge.isConnected()) {
            const auto path = pluginPath();
            if (!path.empty() && runProcess({"hyprctl", "plugin", "load", path}, true) == 0 && dm.bridge.init())
                std::fprintf(stderr, "[mousepet] Automatically connected Hyprland click and cursor hook\n");
        }
        if (!dm.bridge.isConnected()) {
            std::fprintf(stderr, "[mousepet] Hyprland hook unavailable: automatic click bursts and cursor hiding require the plugin.\n");
        }
    }

    if (!dm.demo && !dm.bridge.isConnected() && !dm.cursor.init()) {
        std::fprintf(stderr, "mousepet needs Hyprland's IPC socket or mousepet-hyprland plugin for cursor position.\n"
                             "(HYPRLAND_INSTANCE_SIGNATURE not set or socket missing). Use --demo to preview.\n");
        return 1;
    }

    std::string err;
    if (!dm.overlay.init(err)) { std::fprintf(stderr, "mousepet: %s\n", err.c_str()); return 1; }
    std::fprintf(stderr, "[mousepet] outputs: %s\n", dm.overlay.describeOutputs().c_str());

    float x = 0, y = 0;
    if (dm.demo) {
        dm.demoCx = 500; dm.demoCy = 350;
    }
    if (dm.readCursor(x, y)) dm.pet.place(x / dm.settings.scale, y / dm.settings.scale);

    const int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    const long stepNs = long(STEP_SECONDS * 1e9);
    itimerspec its{{0, stepNs}, {0, stepNs}};
    timerfd_settime(tfd, 0, &its, nullptr);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT); sigaddset(&mask, SIGTERM); sigaddset(&mask, SIGHUP); sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, nullptr);
    signal(SIGPIPE, SIG_IGN);
    const int sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);

    const std::string sockPath = controlSocketPath();
    const int lfd = makeListenSocket(sockPath);
    if (lfd < 0) std::fprintf(stderr, "[mousepet] warning: control socket unavailable at %s\n", sockPath.c_str());
    std::fprintf(stderr, "[mousepet] running (%s), control: %s\n", dm.pet.def->id.c_str(), sockPath.c_str());

    int rc = 0;
    while (dm.running) {
        if (!dm.overlay.beforePoll()) { std::fprintf(stderr, "[mousepet] Wayland connection lost\n"); rc = 1; break; }
        pollfd fds[4] = {{dm.overlay.fd(), POLLIN, 0}, {tfd, POLLIN, 0}, {sfd, POLLIN, 0}, {lfd, POLLIN, 0}};
        const int n = poll(fds, lfd >= 0 ? 4 : 3, -1);
        if (n < 0 && errno != EINTR) { dm.overlay.afterPoll(0); rc = 1; break; }
        if (!dm.overlay.afterPoll(n > 0 ? fds[0].revents : 0)) { std::fprintf(stderr, "[mousepet] Wayland connection closed\n"); rc = 1; break; }
        if (n <= 0) continue;
        if (fds[2].revents & POLLIN) {
            signalfd_siginfo si;
            while (read(sfd, &si, sizeof si) == sizeof si) {
                if (si.ssi_signo == SIGUSR1) dm.pet.burst();
                else dm.running = false;
            }
        }
        if (lfd >= 0 && (fds[3].revents & POLLIN)) serveClient(lfd, dm);
        if (fds[1].revents & POLLIN) {
            uint64_t exp = 0;
            if (read(tfd, &exp, sizeof exp) == sizeof exp && exp) dm.tick(exp);
        } else {
            dm.overlay.redrawPending(dm.pet, dm.options());
        }
    }
    if (lfd >= 0) { close(lfd); unlink(sockPath.c_str()); }
    close(tfd);
    close(sfd);
    close(lockFd);
    std::fprintf(stderr, "[mousepet] bye\n");
    return rc;
}

}  // namespace
}  // namespace mousepet

int main(int argc, char** argv) {
    using namespace mousepet;
    const std::string cmd = argc > 1 ? argv[1] : "run";
    if (cmd == "-h" || cmd == "--help" || cmd == "help") { usage(); return 0; }
    if (cmd == "--version") { std::puts(VERSION); return 0; }
    if (cmd == "list") { std::fputs(listText().c_str(), stdout); return 0; }
    if (cmd == "ctl") return ctl(argc - 2, argv + 2);
    if (cmd == "autostart") return autostartCommand(argc - 2, argv + 2);
    if (cmd == "plugin") return pluginCmd(argc - 2, argv + 2);
    if (cmd == "burst") {
        char* args[] = {(char*)"burst"};
        return ctl(1, args);
    }
    if (cmd == "next" || cmd == "prev") {
        char* args[] = {(char*)cmd.c_str()};
        return ctl(1, args);
    }

    // Elegant syntax: mousepet pet next | prev | <id>
    if (cmd == "pet") {
        if (argc <= 2) {
            char* args[] = {(char*)"status"};
            return ctl(1, args);
        }
        std::string sub = argv[2];
        if (sub == "next" || sub == "prev") {
            char* args[] = {(char*)sub.c_str()};
            return ctl(1, args);
        }
        char* args[] = {(char*)"select", (char*)sub.c_str()};
        return ctl(2, args);
    }

    // Elegant syntax: mousepet --pet --next | --next | <id>
    if (cmd == "--pet") {
        if (argc <= 2) {
            char* args[] = {(char*)"next"};
            return ctl(1, args);
        }
        std::string sub = argv[2];
        if (sub == "--next" || sub == "next") {
            char* args[] = {(char*)"next"};
            return ctl(1, args);
        }
        if (sub == "--prev" || sub == "prev") {
            char* args[] = {(char*)"prev"};
            return ctl(1, args);
        }
        if (sub.rfind("--", 0) == 0) sub = sub.substr(2);
        char* args[] = {(char*)"select", (char*)sub.c_str()};
        return ctl(2, args);
    }

    if (cmd == "select") {
        if (argc <= 2) { std::fprintf(stderr, "usage: mousepet select <id>\n"); return 2; }
        char* args[] = {(char*)"select", argv[2]};
        return ctl(2, args);
    }
    if (cmd == "status") {
        char* args[] = {(char*)"status"};
        return ctl(1, args);
    }
    if (cmd == "toggle" || cmd == "show" || cmd == "hide" || cmd == "reset") {
        char* args[] = {(char*)cmd.c_str()};
        return ctl(1, args);
    }
    // Direct pet name syntax: mousepet allay | kite | ufo | ship | etc.
    loadDynamicAccessories();
    if (findAccessory(cmd)) {
        char* args[] = {(char*)"select", (char*)cmd.c_str()};
        return ctl(2, args);
    }

    if (cmd == "snapshot") return snapshot(argc - 2, argv + 2);
    if (cmd == "run") return run(argc - 2, argv + 2);
    if (!cmd.empty() && cmd[0] == '-') return run(argc - 1, argv + 1);

    std::fprintf(stderr, "unknown command '%s'\n", cmd.c_str());
    usage();
    return 2;
}
