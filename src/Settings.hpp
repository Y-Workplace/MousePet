// Persistent settings (replaces the HTML localStorage) — ~/.config/mousepet/settings.ini
#pragma once

#include <map>
#include <string>

#include "Sim.hpp"

namespace mousepet {

struct Settings {
    std::string accessory = "jellyfish";
    float scale = 1.f;       // pet size multiplier (logical px)
    bool motionBlur = false;
    bool visible = true;
    // Per-accessory overrides: accessory id -> (key -> value). Keys are core config or accessory params.
    std::map<std::string, std::map<std::string, float>> overrides;

    static std::string defaultPath();
    bool load(const std::string& path);
    bool save(const std::string& path) const;  // atomic (tmp + rename)

    /** Selects the stored accessory on the pet and applies its overrides. */
    void applyTo(Pet& pet) const;
    /** Sets a core/param key on the pet and records it as an override. */
    bool set(Pet& pet, const std::string& key, float value);
    /** Drops the overrides of the current accessory and restores its defaults. */
    void reset(Pet& pet);
};

}  // namespace mousepet
