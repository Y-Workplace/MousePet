#include "CppLoader.hpp"
#include "Registry.hpp"

#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace mousepet {

typedef AccessoryDef (*AccessoryFactoryFn)();

static bool fileExists(const std::string& path) {
    return access(path.c_str(), F_OK) == 0;
}

static time_t fileMtime(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) == 0) return st.st_mtime;
    return 0;
}

static bool loadSharedObject(const std::string& soPath) {
    void* handle = dlopen(soPath.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        std::fprintf(stderr, "[mousepet] dlopen failed for %s: %s\n", soPath.c_str(), dlerror());
        return false;
    }

    // Try finding factory function if the accessory didn't register via static initializer
    auto fn = reinterpret_cast<AccessoryFactoryFn>(dlsym(handle, "mousepet_accessory"));
    if (fn) {
        AccessoryDef def = fn();
        registerAccessory(std::move(def));
        std::fprintf(stderr, "[mousepet] Loaded C++ dynamic accessory from %s\n", soPath.c_str());
    }
    return true;
}

static bool compileAndLoadCpp(const std::string& cppPath) {
    std::string soPath = cppPath;
    if (soPath.size() > 4 && soPath.substr(soPath.size() - 4) == ".cpp") {
        soPath = soPath.substr(0, soPath.size() - 4) + ".so";
    } else {
        soPath += ".so";
    }

    const time_t cppTime = fileMtime(cppPath);
    const time_t soTime = fileMtime(soPath);

    // If .so does not exist or .cpp is newer, compile it on execution!
    if (!fileExists(soPath) || cppTime > soTime) {
        std::fprintf(stderr, "[mousepet] Compiling accessory source %s -> %s...\n", cppPath.c_str(), soPath.c_str());
        std::string cmd = "g++ -O2 -shared -fPIC -std=c++20 "
                          "$(pkg-config --cflags cairo 2>/dev/null) "
                          "\"" + cppPath + "\" -o \"" + soPath + "\" 2>&1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[256];
            std::string output;
            while (std::fgets(buf, sizeof(buf), pipe)) {
                output += buf;
            }
            int code = pclose(pipe);
            if (code != 0) {
                std::fprintf(stderr, "[mousepet] Compilation error for %s (exit %d):\n%s\n", cppPath.c_str(), code, output.c_str());
                return false;
            }
        } else {
            std::fprintf(stderr, "[mousepet] Failed to run compiler command\n");
            return false;
        }
    }

    return loadSharedObject(soPath);
}

void loadCppAccessories(const std::string& dirPath) {
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string fullPath = dirPath + "/" + name;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".cpp") {
            compileAndLoadCpp(fullPath);
        } else if (name.size() > 3 && name.substr(name.size() - 3) == ".so") {
            // Check if there is an accompanying .cpp; if so, compileAndLoadCpp handles it
            std::string cppCandidate = fullPath.substr(0, fullPath.size() - 3) + ".cpp";
            if (!fileExists(cppCandidate)) {
                loadSharedObject(fullPath);
            }
        }
    }
    closedir(dir);
}

}  // namespace mousepet
