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
    CHECK(pet.particles.alive() == pet.hints.sparks, "sparks %d", pet.particles.alive());
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
        const int W = 400, H = 400;
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
        CHECK(outside == 0, "%s painted %d px outside its bounds", def.id.c_str(), outside);
    }
}

int main() {
    testRestPose();
    testViolentShakeNeverDetaches();
    testAngleWrapNoSpin();
    testParticles();
    testBurst();
    testDeterminism();
    testClampingAndSettings();
    testBoundsCoverDrawing();
    if (failures) { std::printf("%d check(s) failed\n", failures); return 1; }
    std::printf("All simulation tests passed (%zu accessories).\n", accessories().size());
    return 0;
}
