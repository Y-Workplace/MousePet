#include "Autostart.hpp"
#include "Process.hpp"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace mousepet {
namespace fs = std::filesystem;
int autostartCommand(int argc, char** argv) {
    try {
        const std::string action = argc ? argv[0] : "status";
        if (action != "enable" && action != "disable" && action != "status")
            throw std::runtime_error("usage: mousepet autostart enable|disable|status [--config PATH]");
        fs::path path;
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) != "--config" || i + 1 >= argc || !path.empty())
                throw std::runtime_error("usage: mousepet autostart enable|disable|status [--config PATH]");
            path = argv[++i];
        }
        if (path.empty()) {
            const char* xdg = std::getenv("XDG_CONFIG_HOME");
            const char* home = std::getenv("HOME");
            if ((!xdg || !*xdg) && (!home || !*home)) throw std::runtime_error("HOME or XDG_CONFIG_HOME is required");
            const fs::path base = xdg && *xdg ? fs::path(xdg) : fs::path(home) / ".config";
            const fs::path lua = base / "hypr/hyprland.lua", conf = base / "hypr/hyprland.conf";
            if (fs::exists(lua)) path = lua;
            else if (fs::exists(conf)) path = conf;
            else throw std::runtime_error("Hyprland config not found; pass --config PATH to your existing config");
        }
        if (!fs::is_regular_file(path)) throw std::runtime_error("config must be an existing regular file");
        path = fs::canonical(path); // Preserve config symlinks by updating their target.
        const bool lua = path.extension() == ".lua";
        if (!lua && path.extension() != ".conf") throw std::runtime_error("config must end in .lua or .conf");
        const std::string comment = lua ? "-- " : "# ";
        const std::string begin = comment + "BEGIN MousePet autostart\n", end = comment + "END MousePet autostart\n";
        std::ifstream in(path, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), {});
        if (!in || in.bad()) throw std::runtime_error("could not read config");
        const size_t first = content.find(begin), last = content.find(end);
        if ((first == std::string::npos) != (last == std::string::npos) ||
            (first != std::string::npos && (last < first || content.find(begin, first + begin.size()) != std::string::npos ||
                                          content.find(end, last + end.size()) != std::string::npos)))
            throw std::runtime_error("malformed MousePet block; config left unchanged");
        if (action == "status") { std::printf("%s: %s\n", first != std::string::npos ? "enabled" : "disabled", path.c_str()); return 0; }
        if (action == "disable" && first == std::string::npos) { std::puts("Autostart is already disabled."); return 0; }
        std::string updated = content;
        if (first != std::string::npos) updated.erase(first, last + end.size() - first);
        if (action == "enable") {
            const std::string executable = executablePath();
            if (executable.empty() || executable.find_first_of("\n\r") != std::string::npos)
                throw std::runtime_error("invalid executable path");
            const std::string command = shellQuote(executable) + " run";
            std::ostringstream block;
            block << begin;
            if (lua) block << "hl.on(\"hyprland.start\", function()\n    hl.exec_cmd(" << std::quoted(command) << ")\nend)\n";
            else block << "exec-once = " << command << "\n";
            block << end;
            if (!updated.empty() && updated.back() != '\n') updated += '\n';
            updated += block.str();
        }
        if (updated == content) { std::puts("Autostart is already configured."); return 0; }
        const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
        const fs::path backup = path.string() + ".mousepet.bak." + std::to_string(stamp);
        fs::copy_file(path, backup);
        const fs::path temporary = path.string() + ".mousepet.tmp." + std::to_string(getpid());
        { std::ofstream out(temporary, std::ios::binary); out << updated; out.close();
          if (!out) { fs::remove(temporary); throw std::runtime_error("could not write updated config"); } }
        fs::permissions(temporary, fs::status(path).permissions());
        fs::rename(temporary, path);
        std::printf("Autostart %s in %s\nBackup: %s\nChanges apply on your next Hyprland login.\n", action == "enable" ? "enabled" : "disabled", path.c_str(), backup.c_str());
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "mousepet: %s\n", e.what()); return 1; }
}
}
