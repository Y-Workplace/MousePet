#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include "../src/SharedBridge.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>

HANDLE PHANDLE = nullptr;

static SharedPetBridge* g_bridge = nullptr;
static int g_shmFd = -1;

static Hyprutils::Signal::CHyprSignalListener* g_cursorHook = nullptr;
static Hyprutils::Signal::CHyprSignalListener* g_mouseMoveHook = nullptr;
static Hyprutils::Signal::CHyprSignalListener* g_mouseButtonHook = nullptr;

static void updateCursorShapeAndVisibility() {
    if (!g_bridge || !Pointer::mgr()) return;

    g_bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);

    const auto sz = Pointer::mgr()->cursorSizeLogical();
    const auto hs = Pointer::mgr()->hotspot();

    // Check if cursor is visible: size must be strictly > 0.
    // When hidden by app/browser/video, size is 0x0 or surface is null.
    const bool isVis = (sz.x > 0.5 && sz.y > 0.5);
    g_bridge->isCursorVisible.store(isVis, std::memory_order_relaxed);

    if (isVis) {
        // If hotspot is located centrally (e.g. I-beam / text / crosshair / move):
        // In XCursor / Wayland themes, I-beam hotspot x is near width / 2.
        // For arrow pointer, hotspot is at (0, 0) or tip (x <= 0.25 * width).
        if (hs.x > sz.x * 0.25f || hs.y > sz.y * 0.25f) {
            g_bridge->cursorShape.store(1, std::memory_order_relaxed);
            g_bridge->centerOffsetX.store(0.0f, std::memory_order_relaxed);
            g_bridge->centerOffsetY.store(0.0f, std::memory_order_relaxed);
        } else {
            g_bridge->cursorShape.store(0, std::memory_order_relaxed);
            // Default arrow cursor: visual center is ~40% diagonal across cursor box
            float baseSz = (sz.x >= 8.0f && sz.x <= 128.0f) ? (float)sz.x : 24.0f;
            g_bridge->centerOffsetX.store(baseSz * 0.40f, std::memory_order_relaxed);
            g_bridge->centerOffsetY.store(baseSz * 0.40f, std::memory_order_relaxed);
        }
    }
}

static void onCursorChanged() {
    updateCursorShapeAndVisibility();
}

static void onMouseMove(const Vector2D& pos, Event::SCallbackInfo&) {
    if (g_bridge) {
        g_bridge->mouseX.store((float)pos.x, std::memory_order_relaxed);
        g_bridge->mouseY.store((float)pos.y, std::memory_order_relaxed);
        g_bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);
        updateCursorShapeAndVisibility();
    }
}

static void onMouseButton(const IPointer::SButtonEvent& ev, Event::SCallbackInfo&) {
    if (g_bridge) {
        g_bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);
        if (ev.state == WL_POINTER_BUTTON_STATE_PRESSED) {
            g_bridge->clickButton.store(ev.button, std::memory_order_relaxed);
            g_bridge->clickSequence.fetch_add(1, std::memory_order_release);
            g_bridge->lastClickTimeMs.store(getNowMs(), std::memory_order_relaxed);
        }
        updateCursorShapeAndVisibility();
    }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    g_shmFd = shm_open(MOUSEPET_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (g_shmFd >= 0) {
        if (ftruncate(g_shmFd, sizeof(SharedPetBridge)) == 0) {
            void* p = mmap(nullptr, sizeof(SharedPetBridge), PROT_READ | PROT_WRITE, MAP_SHARED, g_shmFd, 0);
            if (p != MAP_FAILED) {
                g_bridge = static_cast<SharedPetBridge*>(p);
                g_bridge->magic = MOUSEPET_BRIDGE_MAGIC;
                g_bridge->version = 2;
                g_bridge->isPluginActive.store(true, std::memory_order_relaxed);
                g_bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);
            }
        }
    }

    if (Pointer::mgr()) {
        g_cursorHook = new Hyprutils::Signal::CHyprSignalListener(Pointer::mgr()->m_events.cursorChanged.listen(onCursorChanged));
    }
    g_mouseMoveHook = new Hyprutils::Signal::CHyprSignalListener(Event::bus()->m_events.input.mouse.move.listen(onMouseMove));
    g_mouseButtonHook = new Hyprutils::Signal::CHyprSignalListener(Event::bus()->m_events.input.mouse.button.listen(onMouseButton));

    updateCursorShapeAndVisibility();

    return {"mousepet-hyprland", "MousePet Hyprland companion hook for real-time cursor, shape, visibility and clicks", "MousePet", "1.2"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    delete g_cursorHook; g_cursorHook = nullptr;
    delete g_mouseMoveHook; g_mouseMoveHook = nullptr;
    delete g_mouseButtonHook; g_mouseButtonHook = nullptr;
    if (g_bridge) {
        g_bridge->isPluginActive.store(false, std::memory_order_relaxed);
        munmap(g_bridge, sizeof(SharedPetBridge));
        g_bridge = nullptr;
    }
    if (g_shmFd >= 0) {
        close(g_shmFd);
        g_shmFd = -1;
    }
    PHANDLE = nullptr;
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}
