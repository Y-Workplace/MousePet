#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/desktop/view/WLSurface.hpp>
#include <hyprland/src/protocols/core/Compositor.hpp>
#include <new>
#include <stdexcept>
#include <cstring>
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
static wl_event_source* g_pollTimer = nullptr;
static Hyprutils::Signal::CHyprSignalListener* g_mouseMoveHook = nullptr;
static Hyprutils::Signal::CHyprSignalListener* g_mouseButtonHook = nullptr;

static void updateCursorShapeAndVisibility() {
    if (!g_bridge || !Pointer::mgr()) return;

    g_bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);

    const auto sz = Pointer::mgr()->cursorSizeLogical();
    const auto hs = Pointer::mgr()->hotspot();
    const auto& img = Pointer::mgr()->currentCursorImage();

    // Chromium/Electron can release their buffer while the committed cursor texture
    // remains displayable. Follow the same texture state used by PointerManager.
    const auto surface = img.surface.lock();
    const auto resource = surface ? surface->resource() : nullptr;
    const bool hasBuffer = img.pBuffer || img.bufferTex || (resource && resource->m_current.texture);
    const bool isVis = hasBuffer && sz.x > 0.5 && sz.y > 0.5 &&
                       g_pHyprRenderer && g_pHyprRenderer->shouldRenderCursor();
    g_bridge->isCursorVisible.store(isVis, std::memory_order_relaxed);

    // Live compositor pointer coordinates
    const auto pos = Pointer::mgr()->position();
    if (pos.x >= 0 && pos.y >= 0) {
        g_bridge->mouseX.store((float)pos.x, std::memory_order_relaxed);
        g_bridge->mouseY.store((float)pos.y, std::memory_order_relaxed);
    }

    if (isVis) {
        // Effective cursor dimensions in logical coordinates
        const double w = sz.x;
        const double h = sz.y;

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

static int onPollTimer(void*) {
    updateCursorShapeAndVisibility();
    wl_event_source_timer_update(g_pollTimer, 33);
    return 0;
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
    if (std::strcmp(__hyprland_api_get_hash(), __hyprland_api_get_client_hash()) != 0)
        throw std::runtime_error("MousePet plugin ABI mismatch: rebuild against your running Hyprland version");
    PHANDLE = handle;

    g_shmFd = shm_open(mousepetBridgeName().c_str(), O_CREAT | O_RDWR, 0600);
    if (g_shmFd >= 0) {
        if (ftruncate(g_shmFd, sizeof(SharedPetBridge)) == 0) {
            void* p = mmap(nullptr, sizeof(SharedPetBridge), PROT_READ | PROT_WRITE, MAP_SHARED, g_shmFd, 0);
            if (p != MAP_FAILED) {
                g_bridge = new (p) SharedPetBridge{};
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
    // EventBus::tick is not a periodic timer. Poll even when the pointer is idle:
    // clients can hide their cursor without moving it or changing its image.
    g_pollTimer = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, onPollTimer, nullptr);
    if (g_pollTimer) wl_event_source_timer_update(g_pollTimer, 33);
    g_mouseMoveHook = new Hyprutils::Signal::CHyprSignalListener(Event::bus()->m_events.input.mouse.move.listen(onMouseMove));
    g_mouseButtonHook = new Hyprutils::Signal::CHyprSignalListener(Event::bus()->m_events.input.mouse.button.listen(onMouseButton));

    updateCursorShapeAndVisibility();

    return {"mousepet-hyprland", "MousePet Hyprland companion hook for real-time cursor, shape, visibility and clicks", "MousePet", "1.5"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    delete g_cursorHook; g_cursorHook = nullptr;
    if (g_pollTimer) wl_event_source_remove(g_pollTimer);
    g_pollTimer = nullptr;
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
