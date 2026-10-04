// Unix-socket helpers: Hyprland IPC (cursor position) and the MousePet control socket.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include "SharedBridge.hpp"

namespace mousepet {

/** Zero-latency shared memory client connected to the Hyprland companion hook plugin. */
class PetBridgeClient {
public:
    PetBridgeClient() = default;
    ~PetBridgeClient();
    bool init();
    bool isConnected();
    std::optional<std::pair<float, float>> queryPos();
    bool checkClick(uint32_t& outButton);

private:
    SharedPetBridge* bridge_ = nullptr;
    int fd_ = -1;
    uint32_t lastClickSeq_ = 0;
    void disconnect();
};

/** Reads the global cursor position (layout/logical coordinates) through Hyprland's IPC socket. */
class HyprlandCursor {
public:
    /** Returns false when not running under Hyprland (no instance signature / socket). */
    bool init();
    std::optional<std::pair<float, float>> query() const;
    const std::string& socketPath() const { return path_; }

private:
    std::string path_;
};

/** Sends one request line to a unix stream socket and returns the full reply. */
std::optional<std::string> requestUnix(const std::string& path, const std::string& request, int timeoutMs);

/** Per Wayland display, so a nested/test compositor never talks to the user's instance. */
std::string controlSocketPath();
std::string lockFilePath();

}  // namespace mousepet
