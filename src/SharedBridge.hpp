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
    uint32_t magic{MOUSEPET_BRIDGE_MAGIC};      // 0..3
    uint32_t version{2};                        // 4..7

    // Instant cursor coordinates (updated by Hyprland plugin at high frequency)
    std::atomic<float> mouseX{0.0f};            // 8..11
    std::atomic<float> mouseY{0.0f};            // 12..15

    // Click event tracking (maintained at offset 16 for strict binary compatibility)
    std::atomic<uint32_t> clickSequence{0};      // 16..19
    std::atomic<uint32_t> clickButton{0};        // 20..23
    std::atomic<uint64_t> lastClickTimeMs{0};    // 24..31

    // Heartbeat and liveness
    std::atomic<uint64_t> lastHeartbeatMs{0};    // 32..39
    std::atomic<bool> isPluginActive{false};     // 40

    // Cursor visibility state (false if hidden by app, video, web page or typing)
    std::atomic<bool> isCursorVisible{true};     // 41

    // Active cursor shape offset (offset from hotspot to visual center)
    // For default pointer arrow: ~10, 10; for I-beam / crosshair: 0, 0
    std::atomic<float> centerOffsetX{9.6f};      // 44..47
    std::atomic<float> centerOffsetY{9.6f};      // 48..51
    std::atomic<int32_t> cursorShape{0};         // 52..55
};

inline uint64_t getNowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
