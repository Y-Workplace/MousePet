#pragma once

#include <string>

namespace mousepet {

/** Loads any .lua accessory scripts in the specified directory using LuaJIT. */
void loadLuaAccessories(const std::string& dirPath);

}  // namespace mousepet
