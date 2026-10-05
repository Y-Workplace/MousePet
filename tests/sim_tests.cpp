// Headless physics tests (no Wayland, no compositor).
#include <cairo.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "Settings.hpp"
#include "Sim.hpp"

using namespace mousepet;

static int failures = 0;
#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL %s:%d: %s — ", __FILE__, __LINE__, #cond);      \
            std::printf(__VA_ARGS__);                                         \
            std::printf("\n");                                                \
            failures++;                                                       \
        }                                                                     \
    } while (0)

static bool finite(const Pet& p) {
    auto ok = [](float v) { return std::isfinite(v); };
    if (!ok(p.body.x) || !ok(p.body.y) || !ok(p.body.vx) || !ok(p.body.vy) || !ok(p.body.angle)) return false;
    for (int i = 0; i < p.rope.n; i++)
        if (!ok(p.rope.x[i]) || !ok(p.rope.y[i])) return false;
    return true;
}

static float dist(const Pet& p) {
    const float dx = p.body.x - p.mouse.x, dy = p.body.y - p.mouse.y;
    return std::sqrt(dx * dx + dy * dy);
}

static void renderOnce(Pet& pet) {
    cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    cairo_t* cr = cairo_create(s);
    cairo_translate(cr, -pet.body.x + 32, -pet.body.y + 32);
    pet.render(cr);
    CHECK(cairo_status(cr) == CAIRO_STATUS_SUCCESS, "cairo error in %s", pet.def->id.c_str());
    cairo_destroy(cr);
    cairo_surface_destroy(s);
}

static void testRestPose() {
    for (const auto& def : accessories()) {
        Pet pet(7);
        pet.select(def);
        pet.place(500, 400);
        for (int i = 0; i < 600; i++) pet.step();
        CHECK(finite(pet), "%s not finite at rest", def.id.c_str());
        CHECK(pet.lastError.empty(), "%s raised %s", def.id.c_str(), pet.lastError.c_str());
        // Settles near the rest point (cursor + orbit), within rope reach.
        const float rx = 500 + def.body.orbitX, ry = 400 + def.body.orbitY;
        const float e = std::hypot(pet.body.x - rx, pet.body.y - ry);
        CHECK(e < def.defaults.ropeLength, "%s rests %.1f px away from its orbit point", def.id.c_str(), e);
        CHECK(std::fabs(pet.body.angle) < PI, "%s angle %.2f", def.id.c_str(), pet.body.angle);
    }
}

static void testViolentShakeNeverDetaches() {
    for (const auto& def : accessories()) {
        Pet pet(11);
        pet.select(def);
        pet.place(0, 0);
        float worst = 0;
        for (int i = 0; i < 2000; i++) {
            // 300 px jumps every step, plus a teleport now and then.
            const float x = (i % 2 ? 300.f : -300.f) + (i % 97 == 0 ? 4000.f : 0.f);
            const float y = (i % 3 ? 150.f : -150.f);
            pet.setMouse(x, y);
            if (i % 50 == 0) pet.burst();
            pet.step();
            worst = std::fmax(worst, dist(pet));
            if (!finite(pet)) break;
        }
        CHECK(finite(pet), "%s blew up while shaking", def.id.c_str());
        const float max = pet.config.ropeLength * pet.hints.maxStretch;
        CHECK(worst <= max + 0.01f, "%s detached: %.1f > %.1f", def.id.c_str(), worst, max);
        CHECK(pet.particles.alive() <= MAX_PARTICLES, "particle overflow");
        renderOnce(pet);
    }
}

static void testAngleWrapNoSpin() {
    // Circle the cursor around the pet many times: the tilt must never jump by ~2pi in a step.
    Pet pet(3);
    pet.select(*findAccessory("jellyfish"));
    pet.place(0, 0);
    float prev = pet.body.angle, maxJump = 0;
    for (int i = 0; i < 3000; i++) {
        const float t = i * 0.15f;
        pet.setMouse(std::cos(t) * 120, std::sin(t) * 120);
        pet.step();
        maxJump = std::fmax(maxJump, std::fabs(pet.body.angle - prev));
        prev = pet.body.angle;
    }
    CHECK(maxJump < 1.0f, "tilt jumped %.2f rad in one step", maxJump);
}

static void testUprightBanking() {
    for (const auto& def : accessories()) {
        Pet pet(19);
        pet.select(def);
        pet.place(300, 300);
        // Recover from a formerly inverted pose or a accumulated full revolution.
        pet.body.angle = float(TAU * 7 + 2.8);
        for (int i = 0; i < 360; i++) {
            const float a = i * 0.09f;
            pet.setMouse(300 + std::cos(a) * 100, 300 + std::sin(a) * 100);
            if (i % 45 == 0) pet.burst();
            pet.step();
            CHECK(std::abs(pet.body.angle) <= MAX_BODY_BANK + 0.001f,
                  "%s lost upright posture: %.3f", def.id.c_str(), pet.body.angle);
        }
        for (int i = 0; i < 180; i++) pet.step();
        CHECK(finite(pet), "%s invalid after rotation stress", def.id.c_str());
    }
}

static void testNewAccessoryCatalog() {
    for (const char* id : {"airship", "paperdragon", "potion", "hourglass", "windchime",
                           "drone", "satellite", "battery", "yoyo", "plasticbag"}) {
        int count = 0;
        for (const auto& def : accessories()) if (def.id == id) count++;
        CHECK(count == 1, "%s must be registered exactly once", id);
        Pet pet(5);
        pet.select(*findAccessory(id));
        pet.place(200, 200);
        pet.burst();
        for (int i = 0; i < 70; i++) {
            pet.step();
            if (i % 10 == 0) renderOnce(pet);
        }
        CHECK(pet.lastError.empty(), "%s burst failed: %s", id, pet.lastError.c_str());
    }
    Pet yoyo(7);
    yoyo.select(*findAccessory("yoyo")); yoyo.place(200, 200); yoyo.burst();
    CHECK(yoyo.body.vy > 6.f, "yoyo throws down instead of generic upward kick");
    Pet potion(7);
    potion.select(*findAccessory("potion"));
    CHECK(potion.hints.orbitY > 0 && potion.hints.tiltBase < 0, "flask hangs by its top");
    Pet airship(7);
    airship.select(*findAccessory("airship"));
    CHECK(airship.hints.orbitY < 0 && airship.hints.lift < 0, "airship has positive buoyancy");
}

static void testParticles() {
    Particles p;
    for (int i = 0; i < 100; i++) p.spawn(0, 0, 1, 0, 1);
    CHECK(p.alive() == MAX_PARTICLES, "pool cap %d", p.alive());
    for (int i = 0; i < 40; i++) p.update(0);
    CHECK(p.alive() == 0, "particles should expire after ~34 steps, alive %d", p.alive());
    p.spawn(0, 0, 0, 0, 1);
    p.update(0.5f);
    CHECK(std::fabs(p.at(0)[3] - 0.5f) < 1e-6f, "gravity applied: %f", p.at(0)[3]);
}

static void testBurst() {
    Pet pet(5);
    pet.select(*findAccessory("star"));
    pet.place(100, 100);
    const float vy = pet.body.vy;
    pet.burst();
    CHECK(pet.body.burst == 1.f, "burst flag");
    CHECK(pet.body.vy < vy - 4.9f, "kick applied (star kick=5): %.2f", pet.body.vy);
    CHECK(pet.particles.alive() == 15, "star emits three sparks per point: %d", pet.particles.alive());
    CHECK(std::string(pet.state()) == "Burst", "state %s", pet.state());
    for (int i = 0; i < 120; i++) pet.step();
    CHECK(pet.body.burst == 0.f, "burst decays to zero (%.3f)", pet.body.burst);
}

static void testDeterminism() {
    auto runSim = [] {
        Pet pet(42);
        pet.select(*findAccessory("fish"));
        pet.place(10, 10);
        for (int i = 0; i < 500; i++) {
            pet.setMouse(10 + i % 37 * 3.f, 10 + i % 23 * 2.f);
            if (i % 100 == 0) pet.burst();
            pet.step();
        }
        return std::make_pair(pet.body.x, pet.particles.alive());
    };
    CHECK(runSim() == runSim(), "same seed must give the same simulation");
}

static void testClampingAndSettings() {
    Pet pet(9);
    Settings st;
    st.accessory = "balloon";
    st.applyTo(pet);
    CHECK(pet.def && pet.def->id == "balloon", "select from settings");
    CHECK(pet.config.ropeLength == 80.f, "balloon defaults, got %.1f", pet.config.ropeLength);
    CHECK(st.set(pet, "ropeLength", 999), "set core");
    CHECK(pet.config.ropeLength == 150.f, "clamped to 150, got %.1f", pet.config.ropeLength);
    CHECK(st.set(pet, "hue", 120), "set param");
    CHECK(!st.set(pet, "nonsense", 1), "unknown key rejected");
    CHECK(!st.set(pet, "glow", 1), "param of another accessory rejected");

    const std::string path = (std::filesystem::temp_directory_path() / "mousepet-test-settings.ini").string();
    st.scale = 1.5f; st.motionBlur = true;
    CHECK(st.save(path), "save");
    Settings back;
    CHECK(back.load(path), "load");
    std::filesystem::remove(path);
    CHECK(back.accessory == "balloon" && back.scale == 1.5f && back.motionBlur, "round trip of globals");
    Pet pet2(9);
    back.applyTo(pet2);
    CHECK(pet2.config.ropeLength == 150.f && pet2.param("hue") == 120.f, "overrides restored (%.1f, %.1f)",
          pet2.config.ropeLength, pet2.param("hue"));
    back.reset(pet2);
    CHECK(pet2.config.ropeLength == 80.f && pet2.param("hue") == 350.f, "reset to defaults");

    Settings unknown;
    unknown.accessory = "does-not-exist";
    Pet pet3;
    unknown.applyTo(pet3);
    CHECK(pet3.def == &accessories().front(), "unknown accessory falls back to the first one");
}

static void testBoundsCoverDrawing() {
    // Rasterise each accessory and verify every painted pixel lies inside Pet::bounds().
    for (const auto& def : accessories()) {
        Pet pet(21);
        pet.select(def);
        pet.place(200, 200);
        for (int i = 0; i < 90; i++) {
            pet.setMouse(200 + std::sin(i * 0.2f) * 60, 200);
            if (i == 60) pet.burst();
            pet.step();
        }
        pet.setParam("size", 1.5f);
        pet.burst();
        for (int phase = 0; phase < 5; phase++) {
            if (phase) for (int i = 0; i < 12; i++) pet.step();
            const int W = 600, H = 600;
            cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
            cairo_t* cr = cairo_create(s);
            pet.render(cr);
            cairo_destroy(cr);
            cairo_surface_flush(s);
            const Bounds b = pet.bounds();
            const unsigned char* data = cairo_image_surface_get_data(s);
            const int stride = cairo_image_surface_get_stride(s);
            int outside = 0, painted = 0;
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    const uint32_t px = *reinterpret_cast<const uint32_t*>(data + y * stride + x * 4);
                    if ((px >> 24) < 2) continue;
                    painted++;
                    if (x + 1 < b.x0 || x > b.x1 + 1 || y + 1 < b.y0 || y > b.y1 + 1) outside++;
                }
            cairo_surface_destroy(s);
            CHECK(painted > 200, "%s painted only %d px", def.id.c_str(), painted);
            CHECK(outside == 0, "%s phase %d painted %d px outside its bounds", def.id.c_str(), phase, outside);
        }
    }
}

static void testDistinctBursts() {
    Pet fish(7);
    fish.select(*findAccessory("fish"));
    fish.place(200, 200);
    fish.burst();
    CHECK(fish.body.vx > 4.f && fish.body.vy == 0.f, "fish dashes horizontally");
    Pet jelly(7);
    jelly.select(*findAccessory("jellyfish"));
    jelly.place(200, 200);
    jelly.burst();
    CHECK(jelly.body.vy == 0.f, "jellyfish contracts before its swimming impulse");
    Pet restingJelly(7);
    restingJelly.select(*findAccessory("jellyfish"));
    restingJelly.place(200, 200);
    for (int i = 0; i < 5; i++) { jelly.step(); restingJelly.step(); }
    CHECK(jelly.body.y < restingJelly.body.y - 2.f, "jellyfish swims after contraction: %.2f vs %.2f", jelly.body.y, restingJelly.body.y);
    Pet ufo(7);
    ufo.select(*findAccessory("ufo"));
    ufo.place(200, 200);
    ufo.burst();
    CHECK(ufo.body.vy == 0.f && ufo.particles.alive() > 0, "ufo absorbs without generic upward kick");
}

static void testAllayItem() {
    Pet pet(7);
    pet.select(*findAccessory("allay"));
    pet.place(200, 200);
    // Render the accessory alone so rope and particles cannot obscure its palm.
    auto sprite = pet.def->create(pet);
    auto countItemGlints = [&]() {
        cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 400, 400);
        cairo_t* cr = cairo_create(surface);
        sprite->render(cr, pet);
        cairo_destroy(cr);
        cairo_surface_flush(surface);
        const auto* data = cairo_image_surface_get_data(surface);
        const int stride = cairo_image_surface_get_stride(surface);
        int white = 0;
        for (int y = int(pet.body.y) + 2; y < int(pet.body.y) + 13; y++)
            for (int x = int(pet.body.x) - 17; x < int(pet.body.x) - 6; x++) {
                const uint32_t pixel = *reinterpret_cast<const uint32_t*>(data + y * stride + x * 4);
                if (((pixel >> 16) & 255) > 220 && ((pixel >> 8) & 255) > 220 && (pixel & 255) > 220)
                    white++;
            }
        cairo_surface_destroy(surface);
        return white;
    };
    CHECK(countItemGlints() == 0, "Allay holds no item at rest");
    pet.body.burst = 1.f;
    sprite->onBurst(pet);
    CHECK(countItemGlints() > 0, "burst diamond must appear between Allay's hands");
    pet.body.burst = 0.f;
    CHECK(countItemGlints() == 0, "cancelled burst cannot leave Allay holding an item");
    pet.body.burst = 1.f;
    for (int i = 0; i < 60; i++) sprite->update(pet);
    CHECK(countItemGlints() == 0, "Allay item disappears after the burst");
}

int main() {
    testRestPose();
    testViolentShakeNeverDetaches();
    testAngleWrapNoSpin();
    testUprightBanking();
    testNewAccessoryCatalog();
    testParticles();
    testBurst();
    testDeterminism();
    testDistinctBursts();
    testAllayItem();
    testClampingAndSettings();
    testBoundsCoverDrawing();
    if (failures) { std::printf("%d check(s) failed\n", failures); return 1; }
    std::printf("All simulation tests passed (%zu accessories).\n", accessories().size());
    return 0;
}
