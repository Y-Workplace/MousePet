#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include "../src/SharedBridge.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>

HANDLE PHANDLE = nullptr;

static SharedPetBridge* g_bridge = nullptr;
static int g_shmFd = -1;
static Hyprutils::Signal::CHyprSignalListener g_mouseMoveHook;
static Hyprutils::Signal::CHyprSignalListener g_mouseButtonHook;

static void onMouseMove(const Vector2D& pos, Event::SCallbackInfo&) {
    if (g_bridge) {
        g_bridge->mouseX.store((float)pos.x, std::memory_order_relaxed);
        g_bridge->mouseY.store((float)pos.y, std::memory_order_relaxed);
        g_bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);
    }
}

static void onMouseButton(const IPointer::SButtonEvent& ev, Event::SCallbackInfo&) {
    if (g_bridge) {
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
                g_bridge->version = 1;
                g_bridge->isPluginActive.store(true, std::memory_order_relaxed);
                g_bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);
            }
        }
    }

    g_mouseMoveHook = Event::bus()->m_events.input.mouse.move.listen(onMouseMove);
    g_mouseButtonHook = Event::bus()->m_events.input.mouse.button.listen(onMouseButton);

    return {"mousepet-hyprland", "MousePet Hyprland companion hook for real-time cursor and click burst events", "MousePet", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_mouseMoveHook.reset();
    g_mouseButtonHook.reset();
    if (g_bridge) {
        g_bridge->isPluginActive.store(false, std::memory_order_relaxed);
        munmap(g_bridge, sizeof(SharedPetBridge));
        g_bridge = nullptr;
    }
    if (g_shmFd >= 0) {
        close(g_shmFd);
        g_shmFd = -1;
    }
    shm_unlink(MOUSEPET_SHM_NAME);
    PHANDLE = nullptr;
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}
