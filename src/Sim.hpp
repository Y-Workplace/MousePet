// MousePet core — port of the "NÚCLEO" block of MousePet.html.
// Knows nothing about specific accessories; they plug in through AccessoryDef.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

typedef struct _cairo cairo_t;

namespace mousepet {

constexpr float PI = 3.14159265358979f;
constexpr float TAU = PI * 2.f;
constexpr int ROPE_NODES = 8;
constexpr int LAST = ROPE_NODES - 1;
constexpr int MAX_PARTICLES = 48;
constexpr double STEP_SECONDS = 1.0 / 60.0;  // fixed physics step (independent of refresh rate)

struct Color {
    double r = 0, g = 0, b = 0, a = 1;
};
constexpr Color rgb(uint32_t hex, double a = 1) {
    return {((hex >> 16) & 255) / 255.0, ((hex >> 8) & 255) / 255.0, (hex & 255) / 255.0, a};
}
constexpr Color rgba(int r, int g, int b, double a) { return {r / 255.0, g / 255.0, b / 255.0, a}; }
Color hsl(double hueDeg, double satPct, double lightPct, double a = 1);

struct Bounds {
    float x0 = std::numeric_limits<float>::infinity(), y0 = x0;
    float x1 = -std::numeric_limits<float>::infinity(), y1 = x1;
    void add(float x, float y, float r = 0) {
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        x0 = std::fmin(x0, x - r); y0 = std::fmin(y0, y - r);
        x1 = std::fmax(x1, x + r); y1 = std::fmax(y1, y + r);
    }
    void unite(const Bounds& o) {
        if (o.empty()) return;
        add(o.x0, o.y0); add(o.x1, o.y1);
    }
    bool empty() const { return !(x1 >= x0 && y1 >= y0); }
    Bounds scaled(float s) const {
        Bounds b; if (!empty()) { b.x0 = x0 * s; b.y0 = y0 * s; b.x1 = x1 * s; b.y1 = y1 * s; }
        return b;
    }
};

/** Chain of Verlet nodes with fixed storage (no per-frame allocation). */
class Chain {
public:
    explicit Chain(int n, float x = 0, float y = 0);
    void reset(float x0, float y0, float x1, float y1);
    /** Node 0 pinned at (hx,hy); the others trail behind (tentacles, tails, ribbons). */
    void follow(float hx, float hy, float seg, float damping = 0.82f, float gravity = 0.35f, float stiff = 0.75f);
    /** Ends already placed by the caller; relaxes the middle like a hanging rope. */
    void hang(float seg, float gravity, float damping = 0.88f, int iters = 3);
    /** Appends a smooth curve through the chain to the current cairo path. */
    void trace(cairo_t* cr) const;
    void addTo(Bounds& b, float pad) const;

    int n;
    std::vector<float> x, y, px, py;
    float pullX = 0, pullY = 0;  // how much the last node was stretched in hang()
};

struct CoreConfig {
    float ropeLength = 55.f, stiffness = 0.45f, drag = 0.90f, ropeGravity = 0.30f;
};

constexpr float MAX_BODY_BANK = 0.85f; // sprite-specific spins remain independent

struct BodyHints {
    float orbitX = 38, orbitY = -24;  // rest point relative to the cursor
    float sway = 4;                   // vertical bobbing
    float pull = 0.025f;              // attraction towards the rest point
    float lift = 0;                   // constant vertical force (negative = up)
    float kick = 4;                   // vertical impulse on burst
    int sparks = 10;                  // sparks on burst
    float tilt = 0.6f;                // how much the body follows the rope direction (0 = ignore)
    float tiltBase = PI / 2;          // +pi/2: top of the body points away from the rope
    float tiltVel = 0.03f;            // extra tilt from horizontal velocity
    float tiltEase = 0.18f;
    float maxStretch = 1.5f;          // max distance = rope length * this
};

struct RopeStyle {
    Color color = rgb(0x6b421a), highlight = rgb(0xb47334);
    float width = 2.8f;
};

struct ParticleStyle {
    Color color = rgb(0x38bdf8);
    float size = 2.f, gravity = 0.f;
};

struct ParamSpec {
    std::string key, label;
    float min, max, step, value;
};

struct RangeSpec {
    const char* key;
    float min, max;
};
/** Core parameter ranges (same limits as the HTML sliders). */
extern const std::array<RangeSpec, 4> CORE_RANGES;

class Pet;

/** Accessory instance. All hooks are optional except render(). */
class Accessory {
public:
    virtual ~Accessory() = default;
    virtual void update(Pet&) {}      // extra forces, before integration
    virtual void lateUpdate(Pet&) {}  // secondary motion (tentacles, tails...)
    virtual void onBurst(Pet&) {}
    virtual void render(cairo_t* cr, const Pet& pet) = 0;
    /** Adds the drawn extent (simulation units) to b. Default: circle around the body. */
    virtual void bounds(const Pet& pet, Bounds& b) const;
};

struct AccessoryDef {
    std::string id, name;
    Color swatch;
    CoreConfig defaults;
    BodyHints body;
    RopeStyle rope;
    ParticleStyle particle;
    std::vector<ParamSpec> params;
    std::function<std::unique_ptr<Accessory>(Pet&)> create;
};

/** Built-in accessories, in display order. */
const std::vector<AccessoryDef>& accessories();
const AccessoryDef* findAccessory(std::string_view id);

struct Mouse {
    float x = 0, y = 0, vx = 0, vy = 0, speed = 0, lastX = 0, lastY = 0;
};
struct Body {
    float x = 0, y = 0, vx = 0, vy = 0, angle = 0, burst = 0;
};

/** Particle pool with in-place compaction. Layout: [x, y, vx, vy, life, maxLife]. */
class Particles {
public:
    static constexpr int STRIDE = 6;
    void spawn(float x, float y, float vx, float vy, float life = 1);
    void update(float gravity);
    void clear() { alive_ = 0; }
    int alive() const { return alive_; }
    const float* at(int i) const { return &p_[i * STRIDE]; }

private:
    std::array<float, MAX_PARTICLES * STRIDE> p_{};
    int alive_ = 0;
};

class Pet {
public:
    explicit Pet(uint32_t seed = 0x9e3779b9u);
    ~Pet();

    bool select(const AccessoryDef& def);
    /** Teleports mouse + body to the rest pose (used at start-up and after big jumps). */
    void place(float mx, float my);
    void setMouse(float x, float y) { mouse.x = x; mouse.y = y; }
    /** Anchors the rope to (mx, my), clamps body distance if overstretched, and relaxes constraints every frame. */
    void updateAnchor(float mx, float my);
    void step();
    void burst();
    /** Draws rope, accessory and particles in simulation coordinates. */
    void render(cairo_t* cr);
    Bounds bounds() const;

    bool setCore(std::string_view key, float value);
    bool setParam(std::string_view key, float value);
    float param(const std::string& key, float fallback = 0) const;
    float random();
    void emit(float x, float y, float vx, float vy, float life = 1) { particles.spawn(x, y, vx, vy, life); }
    const char* state() const;

    float time = 0;
    Mouse mouse;
    Body body;
    CoreConfig config;
    Chain rope{ROPE_NODES};
    Particles particles;
    BodyHints hints;
    RopeStyle ropeStyle;
    ParticleStyle particleStyle;
    std::unordered_map<std::string, float> params;
    const AccessoryDef* def = nullptr;
    std::string lastError;  // set when an accessory hook throws (accessory gets disabled)

private:
    template <class F> void guard(const char* hook, F&& fn);
    void drawParticles(cairo_t* cr) const;

    std::unique_ptr<Accessory> inst_;
    uint32_t rng_;
};

}  // namespace mousepet
