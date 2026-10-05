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
#include <cmath>
#include <algorithm>

HANDLE PHANDLE = nullptr;

static SharedPetBridge* g_bridge = nullptr;
static int g_shmFd = -1;

static Hyprutils::Signal::CHyprSignalListener* g_cursorHook = nullptr;
static Hyprutils::Signal::CHyprSignalListener* g_tickHook = nullptr;
static Hyprutils::Signal::CHyprSignalListener* g_mouseMoveHook = nullptr;
static Hyprutils::Signal::CHyprSignalListener* g_mouseButtonHook = nullptr;

static void updateCursorShapeAndVisibility() {
    if (!g_bridge || !Pointer::mgr()) return;

    g_bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);

    const auto sz = Pointer::mgr()->cursorSizeLogical();
    const auto hs = Pointer::mgr()->hotspot();
    const auto& img = Pointer::mgr()->currentCursorImage();

    // Determine actual visibility:
    // Cursor is hidden when client sets surface to NULL, or no buffer is attached, or size is 0.
    const bool hasSurface = !img.surface.expired() && img.surface.lock() != nullptr;
    const bool hasBuffer = (img.pBuffer != nullptr || img.bufferTex != nullptr || hasSurface);
    const bool hasSize = (img.size.x > 0.5 && img.size.y > 0.5) || (sz.x > 0.5 && sz.y > 0.5);
    const bool isVis = hasBuffer && hasSize;
    g_bridge->isCursorVisible.store(isVis, std::memory_order_relaxed);

    // Live compositor pointer coordinates
    const auto pos = Pointer::mgr()->position();
    if (pos.x >= 0 && pos.y >= 0) {
        g_bridge->mouseX.store((float)pos.x, std::memory_order_relaxed);
        g_bridge->mouseY.store((float)pos.y, std::memory_order_relaxed);
    }

    if (isVis) {
        // Effective cursor dimensions in logical coordinates
        const double w = (img.size.x > 0.5) ? img.size.x : ((sz.x > 0.5) ? sz.x : 24.0);
        const double h = (img.size.y > 0.5) ? img.size.y : ((sz.y > 0.5) ? sz.y : 24.0);

        // Precise geometric offset from pointer hotspot (where clicks happen) to visual center of the cursor icon:
        // Hotspot (0,0) with 24x24 arrow -> center is at (+12, +12).
        // Hotspot (12,12) with 24x24 I-beam or crosshair -> center is at (0, 0).
        float offX = float(w * 0.5 - hs.x);
        float offY = float(h * 0.5 - hs.y);

        offX = std::clamp(offX, -32.0f, 32.0f);
        offY = std::clamp(offY, -32.0f, 32.0f);

        const bool isCentered = (std::abs(offX) < 2.5f && std::abs(offY) < 2.5f);
        g_bridge->cursorShape.store(isCentered ? 1 : 0, std::memory_order_relaxed);
        g_bridge->centerOffsetX.store(offX, std::memory_order_relaxed);
        g_bridge->centerOffsetY.store(offY, std::memory_order_relaxed);
    }
}

static void onTick() {
    updateCursorShapeAndVisibility();
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
    g_tickHook = new Hyprutils::Signal::CHyprSignalListener(Event::bus()->m_events.tick.listen(onTick));
    g_mouseMoveHook = new Hyprutils::Signal::CHyprSignalListener(Event::bus()->m_events.input.mouse.move.listen(onMouseMove));
    g_mouseButtonHook = new Hyprutils::Signal::CHyprSignalListener(Event::bus()->m_events.input.mouse.button.listen(onMouseButton));

    updateCursorShapeAndVisibility();

    return {"mousepet-hyprland", "MousePet Hyprland companion hook for real-time cursor, shape, visibility and clicks", "MousePet", "1.3"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    delete g_cursorHook; g_cursorHook = nullptr;
    delete g_tickHook; g_tickHook = nullptr;
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
