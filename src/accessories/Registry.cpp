#include "Registry.hpp"
#include "LuaLoader.hpp"
#include "CppLoader.hpp"
#include "../Process.hpp"
#include <filesystem>

#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

namespace mousepet {

std::vector<AccessoryDef>& mutableAccessories() {
    static std::vector<AccessoryDef> list;
    return list;
}

const std::vector<AccessoryDef>& accessories() {
    return mutableAccessories();
}

const AccessoryDef* findAccessory(std::string_view id) {
    for (const auto& a : accessories())
        if (a.id == id) return &a;
    return nullptr;
}

bool registerAccessory(AccessoryDef def) {
    auto& list = mutableAccessories();
    for (auto& existing : list) {
        if (existing.id == def.id) {
            existing = std::move(def);
            return true;
        }
    }
    list.push_back(std::move(def));
    return true;
}

void loadDynamicAccessories() {
    static bool loaded = false;
    if (loaded) return;
    loaded = true;

    std::vector<std::string> searchDirs;

    // 1. User config directory: ~/.config/mousepet/accessories
    const char* home = std::getenv("HOME");
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    std::string userDir;
    if (xdgConfig && *xdgConfig) {
        userDir = std::string(xdgConfig) + "/mousepet/accessories";
    } else if (home && *home) {
        userDir = std::string(home) + "/.config/mousepet/accessories";
    }
    // Shipped examples are loaded first; user accessories can override them.
    const auto binary = std::filesystem::path(executablePath());
    searchDirs.push_back((binary.parent_path().parent_path() / "share/mousepet/accessories").string());
    if (!userDir.empty()) searchDirs.push_back(userDir);
    // Project folders are opt-in: never execute accessories from arbitrary CWDs.
    if (const char* extra = std::getenv("MOUSEPET_ACCESSORY_DIR"); extra && *extra) searchDirs.push_back(extra);

    for (const auto& dir : searchDirs) {
        if (access(dir.c_str(), F_OK) == 0) {
            loadLuaAccessories(dir);
            loadCppAccessories(dir);
        }
    }
}

}  // namespace mousepet
