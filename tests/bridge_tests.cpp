#include <cassert>
#include <cstdio>
#include <cmath>
#include "SharedBridge.hpp"
#include "Ipc.hpp"

using namespace mousepet;

int main() {
    SharedPetBridge bridge;
    bridge.magic = MOUSEPET_BRIDGE_MAGIC;
    bridge.version = 2;
    bridge.isPluginActive.store(true);
    bridge.lastHeartbeatMs.store(getNowMs());
    bridge.isCursorVisible.store(true);
    bridge.centerOffsetX.store(9.6f);
    bridge.centerOffsetY.store(9.6f);

    // 1. Verify default pointer offset
    assert(std::fabs(bridge.centerOffsetX.load() - 9.6f) < 1e-4f);
    assert(std::fabs(bridge.centerOffsetY.load() - 9.6f) < 1e-4f);

    // 2. Simulate text/ibeam cursor change
    bridge.centerOffsetX.store(0.0f);
    bridge.centerOffsetY.store(0.0f);
    assert(bridge.centerOffsetX.load() == 0.0f);
    assert(bridge.centerOffsetY.load() == 0.0f);

    // 3. Simulate cursor hiding by website/app
    bridge.isCursorVisible.store(false);
    assert(!bridge.isCursorVisible.load());
    bridge.isCursorVisible.store(true);
    assert(bridge.isCursorVisible.load());

    // 4. Simulate click events
    bridge.clickButton.store(272);
    bridge.clickSequence.store(1);
    assert(bridge.clickSequence.load() == 1);
    assert(bridge.clickButton.load() == 272);

    std::printf("All bridge unit tests passed successfully!\n");
    return 0;
}
