#pragma once

#include <string>

namespace mousepet {

/** Auto-compiles and loads any .cpp / .so accessory files in the specified directory. */
void loadCppAccessories(const std::string& dirPath);

}  // namespace mousepet
