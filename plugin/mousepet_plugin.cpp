#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/protocols/cursor-shape-v1.hpp>
#include "../src/SharedBridge.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <string>
#include <memory>

HANDLE PHANDLE = nullptr;

static SharedPetBridge* g_bridge = nullptr;
static int g_shmFd = -1;

struct PluginHooks {
    Hyprutils::Signal::CHyprSignalListener tick;
    Hyprutils::Signal::CHyprSignalListener move;
    Hyprutils::Signal::CHyprSignalListener button;
};
static std::unique_ptr<PluginHooks> g_hooks;

static void updateCursorState() {
    if (!g_bridge) return;
    g_bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);

    if (Pointer::mgr()) {
        const auto pos = Pointer::mgr()->position();
        g_bridge->mouseX.store((float)pos.x, std::memory_order_relaxed);
        g_bridge->mouseY.store((float)pos.y, std::memory_order_relaxed);

        const auto& curImg = Pointer::mgr()->currentCursorImage();
        const bool hasBuffer = (curImg.pBuffer != nullptr);
        const bool hasSurface = (curImg.surface.lock() != nullptr);
        const bool hasCursor = hasBuffer || hasSurface;

        std::string shapeName;
        int shapeEnum = 0;
        if (g_pHyprRenderer) {
            shapeName = g_pHyprRenderer->m_lastCursorData.name;
            shapeEnum = (int)g_pHyprRenderer->m_lastCursorData.shape;
        }

        // Cursor visibility: hidden if hasCursor is false or shape is explicitly "none"/"hidden"
        const bool isHidden = !hasCursor || (shapeName == "none") || (shapeName == "hidden");
        g_bridge->isCursorVisible.store(!isHidden, std::memory_order_relaxed);

        // Cursor shape offset:
        // For text / I-beam cursors, the hotspot is already centered on the vertical bar: offset = (0, 0)
        // For crosshair / move / scroll cursors, the hotspot is also centered: offset = (0, 0)
        // For default arrow/pointer cursors, the hotspot is at the tip: offset = (sz * 0.4, sz * 0.4)
        bool isCenteredCursor = false;
        if (shapeName == "text" || shapeName == "ibeam" || shapeName == "xterm" ||
            shapeName == "crosshair" || shapeName == "vertical-text" ||
            shapeName == "cell" || shapeName == "all-scroll" || shapeName == "move") {
            isCenteredCursor = true;
        } else if (shapeEnum == WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT ||
                   shapeEnum == WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT ||
                   shapeEnum == WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR ||
                   shapeEnum == WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL ||
                   shapeEnum == WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE) {
            isCenteredCursor = true;
        }

        if (isCenteredCursor) {
            g_bridge->centerOffsetX.store(0.0f, std::memory_order_relaxed);
            g_bridge->centerOffsetY.store(0.0f, std::memory_order_relaxed);
        } else {
            // Default pointer / arrow: center is ~40% diagonally across cursor box
            float sz = 24.0f;
            const char* env = std::getenv("XCURSOR_SIZE");
            if (env && *env) {
                float v = std::strtof(env, nullptr);
                if (v >= 8.0f && v <= 128.0f) sz = v;
            }
            g_bridge->centerOffsetX.store(sz * 0.40f, std::memory_order_relaxed);
            g_bridge->centerOffsetY.store(sz * 0.40f, std::memory_order_relaxed);
        }
    }
}

static void onTick() {
    updateCursorState();
}

static void onMouseMove(const Vector2D& pos, Event::SCallbackInfo&) {
    if (g_bridge) {
        g_bridge->mouseX.store((float)pos.x, std::memory_order_relaxed);
        g_bridge->mouseY.store((float)pos.y, std::memory_order_relaxed);
        g_bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);
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

    g_hooks = std::make_unique<PluginHooks>();
    g_hooks->tick = Event::bus()->m_events.tick.listen(onTick);
    g_hooks->move = Event::bus()->m_events.input.mouse.move.listen(onMouseMove);
    g_hooks->button = Event::bus()->m_events.input.mouse.button.listen(onMouseButton);

    updateCursorState();

    return {"mousepet-hyprland", "MousePet Hyprland companion hook for real-time cursor, shape, visibility and clicks", "MousePet", "1.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_hooks.reset();
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
