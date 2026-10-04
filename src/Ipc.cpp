#include "Ipc.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mousepet {

static std::string runtimeDir() {
    const char* r = std::getenv("XDG_RUNTIME_DIR");
    return (r && *r) ? r : "/tmp";
}

static std::string displayTag() {
    std::string d = std::getenv("WAYLAND_DISPLAY") ? std::getenv("WAYLAND_DISPLAY") : "wayland-0";
    if (auto slash = d.find_last_of('/'); slash != std::string::npos) d = d.substr(slash + 1);
    for (char& c : d)
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.')) c = '_';
    return d;
}

std::string controlSocketPath() {
    if (const char* p = std::getenv("MOUSEPET_SOCKET"); p && *p) return p;
    return runtimeDir() + "/mousepet-" + displayTag() + ".sock";
}

std::string lockFilePath() { return controlSocketPath() + ".lock"; }

std::optional<std::string> requestUnix(const std::string& path, const std::string& request, int timeoutMs) {
    sockaddr_un addr{};
    if (path.size() >= sizeof(addr.sun_path)) return std::nullopt;
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return std::nullopt;
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) { close(fd); return std::nullopt; }
    size_t sent = 0;
    while (sent < request.size()) {
        const ssize_t n = write(fd, request.data() + sent, request.size() - sent);
        if (n <= 0) { if (errno == EINTR) continue; close(fd); return std::nullopt; }
        sent += size_t(n);
    }
    shutdown(fd, SHUT_WR);
    std::string out;
    char buf[4096];
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        const ssize_t n = read(fd, buf, sizeof buf);
        if (n > 0) { out.append(buf, size_t(n)); if (out.size() > (1u << 20)) break; continue; }
        if (n == 0) break;
        if (errno == EINTR && std::chrono::steady_clock::now() < deadline) continue;
        close(fd);
        return std::nullopt;
    }
    close(fd);
    return out;
}

bool HyprlandCursor::init() {
    const char* sig = std::getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!sig || !*sig) return false;
    path_ = runtimeDir() + "/hypr/" + sig + "/.socket.sock";
    return access(path_.c_str(), F_OK) == 0;
}

std::optional<std::pair<float, float>> HyprlandCursor::query() const {
    // Plain reply format: "X, Y"
    auto r = requestUnix(path_, "cursorpos", 50);
    if (!r) return std::nullopt;
    float x = 0, y = 0;
    if (std::sscanf(r->c_str(), " %f , %f", &x, &y) != 2) return std::nullopt;
    return std::make_pair(x, y);
}

PetBridgeClient::~PetBridgeClient() {
    disconnect();
}

void PetBridgeClient::disconnect() {
    if (bridge_) {
        munmap(bridge_, sizeof(SharedPetBridge));
        bridge_ = nullptr;
    }
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

bool PetBridgeClient::init() {
    if (bridge_) return true;
    fd_ = shm_open(MOUSEPET_SHM_NAME, O_RDWR, 0);
    if (fd_ < 0) return false;
    struct stat st{};
    if (fstat(fd_, &st) != 0 || st.st_size < (off_t)sizeof(SharedPetBridge)) {
        close(fd_);
        fd_ = -1;
        return false;
    }
    void* ptr = mmap(nullptr, sizeof(SharedPetBridge), PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED) {
        close(fd_);
        fd_ = -1;
        return false;
    }
    bridge_ = static_cast<SharedPetBridge*>(ptr);
    lastClickSeq_ = bridge_->clickSequence.load(std::memory_order_relaxed);
    return isConnected();
}

bool PetBridgeClient::isConnected() {
    if (!bridge_) {
        init();
        if (!bridge_) return false;
    }
    if (bridge_->magic != MOUSEPET_BRIDGE_MAGIC || bridge_->version != 1) {
        disconnect();
        return false;
    }
    const uint64_t now = getNowMs();
    const uint64_t lastHb = bridge_->lastHeartbeatMs.load(std::memory_order_relaxed);
    if (!bridge_->isPluginActive.load(std::memory_order_relaxed) || (now > lastHb && now - lastHb > 2000)) {
        return false;
    }
    return true;
}

std::optional<std::pair<float, float>> PetBridgeClient::queryPos() {
    if (!isConnected()) return std::nullopt;
    return std::make_pair(bridge_->mouseX.load(std::memory_order_relaxed),
                          bridge_->mouseY.load(std::memory_order_relaxed));
}

bool PetBridgeClient::checkClick(uint32_t& outButton) {
    if (!isConnected()) return false;
    const uint32_t cur = bridge_->clickSequence.load(std::memory_order_acquire);
    if (cur > lastClickSeq_) {
        lastClickSeq_ = cur;
        outButton = bridge_->clickButton.load(std::memory_order_relaxed);
        return true;
    }
    return false;
}

}  // namespace mousepet
