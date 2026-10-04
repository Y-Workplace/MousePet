#pragma once

#include "../Sim.hpp"
#include <string_view>
#include <vector>
#include <functional>
#include <initializer_list>
#include <utility>

namespace mousepet {

/** Access to registered accessories list. */
std::vector<AccessoryDef>& mutableAccessories();
const std::vector<AccessoryDef>& accessories();
const AccessoryDef* findAccessory(std::string_view id);
bool registerAccessory(AccessoryDef def);

/** Scans ~/.config/mousepet/accessories and ./accessories for Lua scripts and C++ sources/plugins. */
void loadDynamicAccessories();

template <class T>
std::function<std::unique_ptr<Accessory>(Pet&)> makeAccessory() {
    return [](Pet& pet) -> std::unique_ptr<Accessory> {
        if constexpr (std::is_constructible_v<T, Pet&>) return std::make_unique<T>(pet);
        else { (void)pet; return std::make_unique<T>(); }
    };
}

inline BodyHints hints(std::initializer_list<std::pair<const char*, float>> kv) {
    BodyHints h;
    for (const auto& [k, v] : kv) {
        const std::string_view key = k;
        if (key == "orbitX") h.orbitX = v;
        else if (key == "orbitY") h.orbitY = v;
        else if (key == "sway") h.sway = v;
        else if (key == "pull") h.pull = v;
        else if (key == "lift") h.lift = v;
        else if (key == "kick") h.kick = v;
        else if (key == "sparks") h.sparks = int(v);
        else if (key == "tilt") h.tilt = v;
        else if (key == "tiltBase") h.tiltBase = v;
        else if (key == "tiltVel") h.tiltVel = v;
        else if (key == "tiltEase") h.tiltEase = v;
        else if (key == "maxStretch") h.maxStretch = v;
    }
    return h;
}

}  // namespace mousepet
