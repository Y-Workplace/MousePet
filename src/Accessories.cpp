// Built-in accessories — port of the "ACESSÓRIOS" block of MousePet.html.
// To add one: copy a block, change the id and adjust the Accessory subclass.
#include <cairo.h>

#include <cmath>

#include "Sim.hpp"

namespace mousepet {
namespace {

// ── Small cairo helpers mirroring the canvas API ───────────────────────
void source(cairo_t* cr, const Color& c, double alphaMul = 1) { cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a * alphaMul); }

void stop(cairo_pattern_t* p, double at, const Color& c) { cairo_pattern_add_color_stop_rgba(p, at, c.r, c.g, c.b, c.a); }

/** Canvas quadraticCurveTo from the current point. */
void quadTo(cairo_t* cr, double qx, double qy, double x, double y) {
    double x0, y0;
    cairo_get_current_point(cr, &x0, &y0);
    cairo_curve_to(cr, x0 + 2.0 / 3.0 * (qx - x0), y0 + 2.0 / 3.0 * (qy - y0), x + 2.0 / 3.0 * (qx - x),
                   y + 2.0 / 3.0 * (qy - y), x, y);
}

void ellipse(cairo_t* cr, double x, double y, double rx, double ry, double rot = 0) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_rotate(cr, rot);
    cairo_scale(cr, rx, ry);
    cairo_new_sub_path(cr);
    cairo_arc(cr, 0, 0, 1, 0, TAU);
    cairo_restore(cr);
}

void circle(cairo_t* cr, double x, double y, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x, y, r, 0, TAU);
}

/** Radial glow filled in a square, drawn with the given operator and alpha. */
void glow(cairo_t* cr, double x, double y, double r0, double r1, std::initializer_list<std::pair<double, Color>> stops,
          cairo_operator_t op = CAIRO_OPERATOR_SCREEN, double alpha = 1, bool clipCircle = false) {
    cairo_pattern_t* g = cairo_pattern_create_radial(x, y, r0, x, y, r1);
    for (const auto& [at, c] : stops) stop(g, at, c);
    cairo_save(cr);
    cairo_set_operator(cr, op);
    cairo_set_source(cr, g);
    cairo_new_path(cr);
    if (clipCircle) circle(cr, x, y, r1);
    else cairo_rectangle(cr, x - r1, y - r1, r1 * 2, r1 * 2);
    cairo_clip(cr);
    cairo_paint_with_alpha(cr, alpha);
    cairo_restore(cr);
    cairo_pattern_destroy(g);
}

/** Adds a rotated/scaled local box (around the body) to the bounds. */
void addBox(const Pet& pet, Bounds& b, float r) { b.add(pet.body.x, pet.body.y, r); }

// ── Jellyfish ─────────────────────────────────────────────────────────
class Jellyfish final : public Accessory {
    static constexpr int COUNT = 4;
    static constexpr float SEG = 4.5f;
    struct Tentacle { float off; Chain chain; };
    std::vector<Tentacle> tent_;
    float phase_ = 0;

public:
    explicit Jellyfish(Pet& pet) {
        for (int t = 0; t < COUNT; t++)
            tent_.push_back({(t - (COUNT - 1) / 2.f) * 5.5f, Chain(5, pet.body.x, pet.body.y)});
    }
    void update(Pet& pet) override {
        Body& b = pet.body;
        phase_ += (0.06f + b.burst * 0.1f) * pet.param("swim", 1);
        if (std::sin(phase_) > 0.75f) b.vy -= 0.12f;  // swimming thrust
    }
    void lateUpdate(Pet& pet) override {
        const Body& b = pet.body;
        const float c = std::cos(b.angle), s = std::sin(b.angle);
        for (auto& t : tent_) t.chain.follow(b.x + c * t.off - s * 7, b.y + s * t.off + c * 7, SEG);
    }
    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        // tentacles: 2 paths instead of 4
        cairo_set_line_width(cr, 1.6);
        cairo_new_path(cr); tent_[0].chain.trace(cr); tent_[3].chain.trace(cr);
        cairo_set_source_rgba(cr, 34 / 255., 211 / 255., 238 / 255., 0.5); cairo_stroke(cr);
        tent_[1].chain.trace(cr); tent_[2].chain.trace(cr);
        cairo_set_source_rgba(cr, 56 / 255., 189 / 255., 248 / 255., 0.85); cairo_stroke(cr);

        const double s = std::sin(phase_);
        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle);
        cairo_scale(cr, 1 - s * 0.05, 1 + s * 0.09 + b.burst * 0.25);

        // aura
        glow(cr, 0, -2, 2, 24, {{0, rgba(56, 189, 248, 0.4)}, {0.6, rgba(168, 85, 247, 0.15)}, {1, rgba(56, 189, 248, 0)}},
             CAIRO_OPERATOR_SCREEN, pet.param("glow", 1), true);

        // bell
        cairo_new_path(cr);
        cairo_move_to(cr, -16, 5);
        cairo_curve_to(cr, -18, -18, 18, -18, 16, 5);
        quadTo(cr, 11, 8, 7, 5);
        quadTo(cr, 3, 8, 0, 5);
        quadTo(cr, -3, 8, -7, 5);
        quadTo(cr, -11, 8, -16, 5);
        cairo_close_path(cr);
        cairo_pattern_t* g = cairo_pattern_create_linear(0, -18, 0, 8);
        stop(g, 0, rgba(186, 230, 253, 0.90));
        stop(g, 0.45, rgba(56, 189, 248, 0.65));
        stop(g, 0.85, rgba(192, 132, 252, 0.40));
        stop(g, 1, rgba(147, 51, 234, 0.20));
        cairo_set_source(cr, g);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(g);
        source(cr, rgb(0xe0f2fe));
        cairo_set_line_width(cr, 1.3);
        cairo_stroke(cr);

        const double pulse = std::sin(pet.time * 6) * 0.8;  // inner gem
        cairo_set_source_rgba(cr, 56 / 255., 189 / 255., 248 / 255., 0.7);
        circle(cr, 0, -4, 4.5 + pulse); cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        circle(cr, 0, -4, 2.5 + pulse); cairo_fill(cr);
    }
    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 34 * (1 + pet.body.burst * 0.3f));
        for (const auto& t : tent_) t.chain.addTo(b, 2);
    }
};

// ── Balloon ───────────────────────────────────────────────────────────
class Balloon final : public Accessory {
public:
    void update(Pet& pet) override { pet.body.vy += std::sin(pet.time * 3) * 0.02f; }  // floats in the air
    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1), hue = pet.param("hue", 350);
        const double sq = b.burst * 0.12;
        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle);
        cairo_scale(cr, sz * (1 - sq), sz * (1 + sq * 1.5));

        source(cr, hsl(hue, 70, 38));  // knot
        cairo_new_path(cr);
        cairo_move_to(cr, 0, 16); cairo_line_to(cr, -4, 23); cairo_line_to(cr, 4, 23); cairo_close_path(cr);
        cairo_fill(cr);

        cairo_pattern_t* g = cairo_pattern_create_radial(-6, -8, 2, 0, 0, 22);
        stop(g, 0, hsl(hue, 90, 80));
        stop(g, 0.5, hsl(hue, 80, 58));
        stop(g, 1, hsl(hue, 75, 38));
        cairo_set_source(cr, g);
        ellipse(cr, 0, -2, 15, 19);
        cairo_fill(cr);
        cairo_pattern_destroy(g);

        cairo_set_source_rgba(cr, 1, 1, 1, 0.55);  // highlight
        ellipse(cr, -6, -10, 3, 5.5, -0.5);
        cairo_fill(cr);
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 30 * pet.param("size", 1) * 1.2f); }
};

// ── Star ──────────────────────────────────────────────────────────────
class Star final : public Accessory {
    float rot_ = 0;
    unsigned tick_ = 0;

public:
    void update(Pet& pet) override {
        const Body& b = pet.body;
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        rot_ += (0.02f + sp * 0.01f + b.burst * 0.2f) * pet.param("spin", 1);
        if (sp > 1.5f && (++tick_ & 1)) {  // sparkle trail
            pet.emit(b.x, b.y, -b.vx * 0.15f + (pet.random() - 0.5f) * 0.6f,
                     -b.vy * 0.15f + (pet.random() - 0.5f) * 0.6f, 0.7f);
        }
    }
    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        cairo_translate(cr, b.x, b.y);
        glow(cr, 0, 0, 2, 26, {{0, rgba(253, 224, 71, 0.45)}, {1, rgba(253, 224, 71, 0)}});
        const double s = 1 + std::sin(pet.time * 5) * 0.06 + b.burst * 0.3;
        cairo_rotate(cr, rot_);
        cairo_scale(cr, s, s);

        const int n = int(std::lround(pet.param("points", 5)));
        cairo_new_path(cr);
        for (int i = 0; i < n * 2; i++) {
            const double r = (i % 2) ? 6.5 : 14, a = -PI / 2 + i * PI / n;
            if (i) cairo_line_to(cr, std::cos(a) * r, std::sin(a) * r);
            else cairo_move_to(cr, std::cos(a) * r, std::sin(a) * r);
        }
        cairo_close_path(cr);
        cairo_pattern_t* g = cairo_pattern_create_radial(0, 0, 1, 0, 0, 14);
        stop(g, 0, rgb(0xfffbeb));
        stop(g, 1, rgb(0xfacc15));
        cairo_set_source(cr, g);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(g);
        source(cr, rgb(0xfef9c3));
        cairo_set_line_width(cr, 1.2);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_stroke(cr);
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 28 * (1.1f + pet.body.burst * 0.3f)); }
};

// ── Ghost ─────────────────────────────────────────────────────────────
class Ghost final : public Accessory {
public:
    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body; const Mouse& m = pet.mouse;
        cairo_translate(cr, b.x, b.y);
        glow(cr, 0, 0, 4, 28, {{0, rgba(226, 232, 240, 0.28)}, {1, rgba(226, 232, 240, 0)}});

        cairo_rotate(cr, b.angle);
        const double s = 1 + b.burst * 0.3;
        cairo_scale(cr, s, s);

        // body (drawn into a group so the opacity applies to the whole sprite, like the cached canvas)
        cairo_push_group(cr);
        cairo_new_path(cr);
        cairo_move_to(cr, -14, 14);
        cairo_line_to(cr, -14, -4);
        cairo_arc(cr, 0, -4, 14, PI, TAU);
        cairo_line_to(cr, 14, 14);
        quadTo(cr, 9.3, 8, 4.7, 14);
        quadTo(cr, 0, 8, -4.7, 14);
        quadTo(cr, -9.3, 8, -14, 14);
        cairo_close_path(cr);
        cairo_pattern_t* g = cairo_pattern_create_linear(0, -18, 0, 14);
        stop(g, 0, rgb(0xf8fafc));
        stop(g, 1, rgb(0xcbd5e1));
        cairo_set_source(cr, g);
        cairo_fill(cr);
        cairo_pattern_destroy(g);
        cairo_set_source_rgba(cr, 251 / 255., 113 / 255., 133 / 255., 0.28);  // cheeks
        circle(cr, -9, 1, 2.8); circle(cr, 9, 1, 2.8);
        cairo_fill(cr);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, pet.param("opacity", 0.85));

        // eyes follow the cursor
        double ex = m.x - b.x, ey = m.y - b.y;
        double d = std::sqrt(ex * ex + ey * ey);
        if (d == 0) d = 1;
        ex = ex / d * 1.6; ey = ey / d * 1.6;
        source(cr, rgb(0x0f172a));
        circle(cr, -5 + ex, -6 + ey, 2.6);
        circle(cr, 5 + ex, -6 + ey, 2.6);
        cairo_fill(cr);
        if (b.burst > 0.2f) {  // mouth: "O" on burst, smile otherwise
            ellipse(cr, 0, 3, 2.6, 3.4);
            cairo_fill(cr);
        } else {
            cairo_new_path(cr);
            cairo_arc(cr, 0, 1.5, 3, 0.2, PI - 0.2);
            cairo_set_line_width(cr, 1.3);
            cairo_stroke(cr);
        }
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 30 * (1 + pet.body.burst * 0.3f)); }
};

// ── Lantern fish ──────────────────────────────────────────────────────
class Fish final : public Accessory {
    float face_ = 1, tail_ = 0, bubbleIn_ = 40;

public:
    void update(Pet& pet) override {
        const Body& b = pet.body;
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        if (std::fabs(b.vx) > 0.3f) face_ += ((b.vx > 0 ? 1.f : -1.f) - face_) * 0.15f;
        tail_ += 0.15f + sp * 0.08f;
        if (--bubbleIn_ <= 0) {  // occasional bubbles
            bubbleIn_ = 30 + pet.random() * 50;
            pet.emit(b.x + face_ * 14, b.y - 2, 0, -0.3f, 1);
        }
    }
    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1), gl = pet.param("glow", 1);
        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle);
        cairo_scale(cr, face_ * sz, sz);

        // tail (only dynamic part of the body)
        cairo_save(cr);
        cairo_translate(cr, -12, 0);
        cairo_rotate(cr, std::sin(tail_) * 0.5);
        source(cr, rgb(0xd97706));
        cairo_new_path(cr);
        cairo_move_to(cr, 2, 0); cairo_line_to(cr, -10, -7); cairo_line_to(cr, -7, 0); cairo_line_to(cr, -10, 7);
        cairo_close_path(cr);
        cairo_fill(cr);
        cairo_restore(cr);

        // body
        cairo_pattern_t* g = cairo_pattern_create_linear(0, -9, 0, 9);
        stop(g, 0, rgb(0xfbbf24));
        stop(g, 1, rgb(0xb45309));
        cairo_set_source(cr, g);
        ellipse(cr, 0, 0, 14, 8.5);
        cairo_fill(cr);
        cairo_pattern_destroy(g);
        source(cr, rgb(0x92400e));  // dorsal fin
        cairo_move_to(cr, -4, -7); cairo_line_to(cr, -9, -12); cairo_line_to(cr, 2, -8); cairo_close_path(cr);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);  // eye
        circle(cr, 8, -2, 2.6); cairo_fill(cr);
        source(cr, rgb(0x0f172a));
        circle(cr, 8.7, -2, 1.3); cairo_fill(cr);
        source(cr, rgb(0x78350f));  // mouth
        cairo_set_line_width(cr, 1.2);
        cairo_move_to(cr, 14, 1); cairo_line_to(cr, 10, 2.5); cairo_stroke(cr);

        // glowing lure
        const double lx = 14 + std::sin(tail_ * 0.7) * 2, ly = -17;
        cairo_move_to(cr, 6, -8);
        quadTo(cr, 10, -18, lx, ly);
        cairo_stroke(cr);
        glow(cr, lx, ly, 0, 18,
             {{0, rgba(254, 240, 138, 0.95)}, {0.3, rgba(251, 191, 36, 0.45)}, {1, rgba(251, 191, 36, 0)}},
             CAIRO_OPERATOR_SCREEN, gl * (0.8 + std::sin(pet.time * 6) * 0.2));
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 42 * pet.param("size", 1)); }
};

template <class T> std::function<std::unique_ptr<Accessory>(Pet&)> make() {
    return [](Pet& pet) -> std::unique_ptr<Accessory> {
        if constexpr (std::is_constructible_v<T, Pet&>) return std::make_unique<T>(pet);
        else { (void)pet; return std::make_unique<T>(); }
    };
}

BodyHints hints(std::initializer_list<std::pair<const char*, float>> kv) {
    BodyHints h;
    for (const auto& [k, v] : kv) {
        const std::string_view key = k;
        if (key == "orbitX") h.orbitX = v;
        else if (key == "orbitY") h.orbitY = v;
        else if (key == "sway") h.sway = v;
        else if (key == "pull") h.pull = v;
        else if (key == "lift") h.lift = v;
        else if (key == "kick") h.kick = v;
        else if (key == "sparks") h.sparks = int(v);
        else if (key == "tilt") h.tilt = v;
        else if (key == "tiltBase") h.tiltBase = v;
        else if (key == "tiltVel") h.tiltVel = v;
        else if (key == "tiltEase") h.tiltEase = v;
        else if (key == "maxStretch") h.maxStretch = v;
    }
    return h;
}

std::vector<AccessoryDef> build() {
    std::vector<AccessoryDef> v;
    v.push_back({"jellyfish", "Jellyfish", rgb(0x38bdf8), CoreConfig{}, hints({{"orbitX", 38}, {"orbitY", -24}, {"sway", 4}}),
                 RopeStyle{}, ParticleStyle{rgb(0x38bdf8), 2, 0},
                 {{"glow", "Glow", 0, 1, 0.05f, 1}, {"swim", "Pulse", 0.5f, 2, 0.1f, 1}}, make<Jellyfish>()});
    v.push_back({"balloon", "Balloon", rgb(0xfb7185), CoreConfig{80, 0.35f, 0.92f, 0.22f},
                 hints({{"orbitX", 12}, {"orbitY", -70}, {"sway", 6}, {"kick", 3}, {"sparks", 14}}),
                 RopeStyle{rgb(0xcbd5e1), rgb(0xf8fafc), 1.8f}, ParticleStyle{rgb(0xfda4af), 2.2f, 0.04f},
                 {{"hue", "Hue", 0, 360, 5, 350}, {"size", "Size", 0.7f, 1.6f, 0.05f, 1}}, make<Balloon>()});
    v.push_back({"star", "Star", rgb(0xfde047), CoreConfig{60, 0.5f, 0.88f, 0.2f},
                 hints({{"orbitX", 34}, {"orbitY", -30}, {"sway", 5}, {"kick", 5}, {"tilt", 0}}),
                 RopeStyle{rgb(0x92710c), rgb(0xfde047), 2.2f}, ParticleStyle{rgb(0xfde047), 2.4f, 0},
                 {{"points", "Points", 4, 8, 1, 5}, {"spin", "Spin", 0, 3, 0.1f, 1}}, make<Star>()});
    v.push_back({"ghost", "Ghost", rgb(0xe2e8f0), CoreConfig{60, 0.4f, 0.91f, 0.2f},
                 hints({{"orbitX", 42}, {"orbitY", -22}, {"sway", 7}, {"tilt", 0}, {"tiltVel", 0.05f}}),
                 RopeStyle{rgb(0x64748b), rgb(0xe2e8f0), 2.2f}, ParticleStyle{rgb(0xcbd5e1), 2, 0},
                 {{"opacity", "Opacity", 0.3f, 1, 0.05f, 0.85f}}, make<Ghost>()});
    v.push_back({"fish", "Lantern fish", rgb(0xf59e0b), CoreConfig{65, 0.4f, 0.9f, 0.25f},
                 hints({{"orbitX", 46}, {"orbitY", -14}, {"sway", 6}, {"tilt", 0}, {"tiltVel", 0.03f}, {"sparks", 8}}),
                 RopeStyle{rgb(0x475569), rgb(0x94a3b8), 2}, ParticleStyle{rgb(0xfbbf24), 1.8f, -0.02f},  // bubbles rise
                 {{"size", "Size", 0.7f, 1.6f, 0.05f, 1}, {"glow", "Light", 0, 1, 0.05f, 1}}, make<Fish>()});
    return v;
}

}  // namespace

const std::vector<AccessoryDef>& accessories() {
    static const std::vector<AccessoryDef> list = build();
    return list;
}

}  // namespace mousepet
