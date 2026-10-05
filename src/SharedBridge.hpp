#pragma once

#include <atomic>
#include <cstdint>
#include <chrono>

#ifndef MOUSEPET_SHM_PATH
#define MOUSEPET_SHM_PATH "/mousepet_bridge_shm"
#endif

constexpr const char* MOUSEPET_SHM_NAME = MOUSEPET_SHM_PATH;
constexpr uint32_t MOUSEPET_BRIDGE_MAGIC = 0x50455431;  // 'PET1'

struct alignas(64) SharedPetBridge {
    uint32_t magic{MOUSEPET_BRIDGE_MAGIC};
    uint32_t version{2};

    // Instant cursor coordinates (updated by Hyprland plugin at high frequency)
    std::atomic<float> mouseX{0.0f};
    std::atomic<float> mouseY{0.0f};

    // Cursor visibility state (false if hidden by app / website / video / typing)
    std::atomic<bool> isCursorVisible{true};

    // Active cursor shape offset (offset from hotspot to visual center)
    // For default pointer arrow: ~9.6, 9.6; for I-beam / crosshair: 0.0, 0.0
    std::atomic<float> centerOffsetX{9.6f};
    std::atomic<float> centerOffsetY{9.6f};

    // Click event tracking
    std::atomic<uint32_t> clickSequence{0};
    std::atomic<uint32_t> clickButton{0};
    std::atomic<uint64_t> lastClickTimeMs{0};

    // Heartbeat and liveness
    std::atomic<uint64_t> lastHeartbeatMs{0};
    std::atomic<bool> isPluginActive{false};
};

inline uint64_t getNowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
