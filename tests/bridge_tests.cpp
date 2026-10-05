#include <cassert>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <fcntl.h>
#include <new>
#include <sys/mman.h>
#include <unistd.h>
#include "SharedBridge.hpp"
#include "Ipc.hpp"

using namespace mousepet;

int main() {
    const std::string session = "bridge-test-" + std::to_string(getpid());
    setenv("HYPRLAND_INSTANCE_SIGNATURE", session.c_str(), 1);
    const std::string name = mousepetBridgeName();
    setenv("HYPRLAND_INSTANCE_SIGNATURE", "another-session", 1);
    assert(mousepetBridgeName() != name);
    setenv("HYPRLAND_INSTANCE_SIGNATURE", session.c_str(), 1);

    const int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    assert(fd >= 0);
    assert(ftruncate(fd, sizeof(SharedPetBridge)) == 0);
    void* memory = mmap(nullptr, sizeof(SharedPetBridge), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(memory != MAP_FAILED);
    auto& bridge = *new (memory) SharedPetBridge{};
    bridge.isPluginActive.store(true);
    bridge.lastHeartbeatMs.store(getNowMs());
    bridge.mouseX.store(-120.f); // monitors can have negative layout coordinates
    bridge.mouseY.store(250.f);
    bridge.clickSequence.store(10); // historical clicks must not trigger a burst

    {
        PetBridgeClient client;
        assert(client.init());
        assert(client.isConnected());
        assert(client.queryPos()->first == -120.f);
        assert(client.queryPos()->second == 250.f);
        assert(std::fabs(client.queryOffset().first - 9.6f) < 1e-4f);
        bridge.centerOffsetX.store(0.f);
        bridge.centerOffsetY.store(0.f);
        assert(client.queryOffset() == std::make_pair(0.f, 0.f));

        assert(client.isCursorVisible());
        bridge.isCursorVisible.store(false);
        assert(!client.isCursorVisible());
        bridge.isCursorVisible.store(true);
        assert(client.isCursorVisible());

        uint32_t button = 0;
        assert(!client.checkClick(button));
        bridge.clickButton.store(272);
        bridge.clickSequence.fetch_add(1, std::memory_order_release);
        assert(client.checkClick(button));
        assert(button == 272);
        assert(!client.checkClick(button));
        bridge.clickButton.store(273);
        bridge.clickSequence.fetch_add(1, std::memory_order_release);
        assert(client.checkClick(button));
        assert(button == 273);

        bridge.lastHeartbeatMs.store(getNowMs() - MOUSEPET_HEARTBEAT_TIMEOUT_MS - 100);
        bridge.clickSequence.fetch_add(1, std::memory_order_release);
        assert(!client.isConnected());
        assert(!client.checkClick(button));
        assert(!client.queryPos());
        bridge.lastHeartbeatMs.store(getNowMs());
        assert(client.isConnected());

        bridge.isPluginActive.store(false);
        assert(!client.isConnected());
        assert(!client.checkClick(button));
        bridge.isPluginActive.store(true);
        bridge.version = 99;
        assert(!client.isConnected());
        bridge.version = 2;
        assert(client.init());
        assert(!client.checkClick(button));
    }

    munmap(memory, sizeof(SharedPetBridge));
    close(fd);
    shm_unlink(name.c_str());
    std::printf("Bridge client tests passed: visibility, clicks, session isolation and liveness.\n");
}
