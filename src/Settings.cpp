#include "Settings.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace mousepet {

namespace fs = std::filesystem;

static std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static bool parseFloat(const std::string& s, float& out) {
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (end == s.c_str() || !std::isfinite(v)) return false;
    out = v;
    return true;
}

std::string Settings::defaultPath() {
    if (const char* p = std::getenv("MOUSEPET_CONFIG"); p && *p) return p;
    std::string base;
    if (const char* x = std::getenv("XDG_CONFIG_HOME"); x && *x) base = x;
    else if (const char* h = std::getenv("HOME"); h && *h) base = std::string(h) + "/.config";
    else base = ".";
    return base + "/mousepet/settings.ini";
}

bool Settings::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line, section;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') { section = trim(line.substr(1, line.size() - 2)); continue; }
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0, eq)), val = trim(line.substr(eq + 1));
        float f = 0;
        if (section.empty()) {
            if (key == "accessory") accessory = val;
            else if (key == "scale" && parseFloat(val, f)) scale = std::clamp(f, 0.25f, 4.f);
            else if (key == "motionBlur") motionBlur = (val == "1" || val == "true" || val == "on");
            else if (key == "visible") visible = !(val == "0" || val == "false" || val == "off");
        } else if (parseFloat(val, f)) {
            overrides[section][key] = f;
        }
    }
    return true;
}

bool Settings::save(const std::string& path) const {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        out << "# MousePet settings - written by mousepet, edit while it is stopped\n";
        out << "accessory=" << accessory << "\n";
        out << "scale=" << scale << "\n";
        out << "motionBlur=" << (motionBlur ? 1 : 0) << "\n";
        out << "visible=" << (visible ? 1 : 0) << "\n";
        for (const auto& [id, kv] : overrides) {
            if (kv.empty()) continue;
            out << "\n[" << id << "]\n";
            for (const auto& [k, v] : kv) out << k << "=" << v << "\n";
        }
        out.flush();
        if (!out) return false;
    }
    fs::rename(tmp, path, ec);
    return !ec;
}

void Settings::applyTo(Pet& pet) const {
    const AccessoryDef* def = findAccessory(accessory);
    if (!def) def = &accessories().front();
    pet.select(*def);
    if (auto it = overrides.find(def->id); it != overrides.end()) {
        for (const auto& [k, v] : it->second)
            if (!pet.setCore(k, v)) pet.setParam(k, v);
    }
}

bool Settings::set(Pet& pet, const std::string& key, float value) {
    if (!pet.def) return false;
    if (!pet.setCore(key, value) && !pet.setParam(key, value)) return false;
    // Store the clamped value that was actually applied.
    float applied = value;
    if (key == "ropeLength") applied = pet.config.ropeLength;
    else if (key == "stiffness") applied = pet.config.stiffness;
    else if (key == "drag") applied = pet.config.drag;
    else if (key == "ropeGravity") applied = pet.config.ropeGravity;
    else applied = pet.param(key, value);
    overrides[pet.def->id][key] = applied;
    return true;
}

void Settings::reset(Pet& pet) {
    if (!pet.def) return;
    overrides.erase(pet.def->id);
    const Mouse m = pet.mouse;
    const Body b = pet.body;
    pet.select(*pet.def);
    pet.mouse = m;
    pet.body.x = b.x; pet.body.y = b.y;
}

}  // namespace mousepet
