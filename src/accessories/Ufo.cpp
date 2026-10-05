#include "Common.hpp"

namespace mousepet {

class Ufo final : public Accessory {
    float lightRot_ = 0.0f;
    float beamPulse_ = 0.0f;

public:
    void update(Pet& pet) override {
        Body& b = pet.body;
        lightRot_ += 0.08f;
        beamPulse_ += 0.06f;

        // Downward abduction ray particles
        if (pet.random() < 0.12f || b.burst > 0.1f) {
            float ox = (pet.random() - 0.5f) * 14.0f;
            pet.emit(b.x + ox, b.y + 6.0f,
                     (pet.random() - 0.5f) * 0.3f, 0.8f + pet.random() * 0.8f, 0.7f);
        }
    }

    void onBurst(Pet& pet) override {
        // High-energy abduction scan pulse
        for (int i = 0; i < 18; i++) {
            float a = pet.random() * TAU;
            float spd = 1.2f + pet.random() * 2.5f;
            pet.emit(pet.body.x, pet.body.y, std::cos(a) * spd, std::sin(a) * spd, 1.0f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);

        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle * 0.5f);
        cairo_scale(cr, sz, sz);

        // 1. Tractor Abduction Beam (Projects downward from bottom of saucer)
        const double beamAlpha = 0.25 + std::sin(beamPulse_) * 0.08 + b.burst * 0.45;
        cairo_new_path(cr);
        cairo_move_to(cr, -6, 4);
        cairo_line_to(cr, -18, 38);
        cairo_line_to(cr, 18, 38);
        cairo_line_to(cr, 6, 4);
        cairo_close_path(cr);

        cairo_pattern_t* bg = cairo_pattern_create_linear(0, 4, 0, 38);
        stop(bg, 0, rgba(74, 222, 128, beamAlpha * 1.5));
        stop(bg, 0.5, rgba(52, 211, 153, beamAlpha * 0.8));
        stop(bg, 1, rgba(16, 185, 129, 0.0));
        cairo_set_source(cr, bg);
        cairo_fill(cr);
        cairo_pattern_destroy(bg);

        // 2. Cockpit Glass Dome
        glow(cr, 0, -8, 2, 16, {{0, rgba(167, 243, 208, 0.4)}, {1, rgba(167, 243, 208, 0)}});

        cairo_pattern_t* dg = cairo_pattern_create_linear(0, -16, 0, -2);
        stop(dg, 0, rgba(110, 231, 183, 0.95));
        stop(dg, 0.6, rgba(52, 211, 153, 0.75));
        stop(dg, 1, rgba(16, 185, 129, 0.4));
        cairo_new_path(cr);
        cairo_arc(cr, 0, -4, 9, PI, TAU);
        cairo_close_path(cr);
        cairo_set_source(cr, dg);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(dg);

        source(cr, rgba(255, 255, 255, 0.7));
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        // Cute Little Alien inside
        source(cr, rgb(0x22c55e));
        circle(cr, 0, -8, 3.2); cairo_fill(cr);
        // Alien big glossy eyes
        source(cr, rgb(0x022c22));
        ellipse(cr, -1.3, -8.2, 1.1, 1.6, -0.2);
        ellipse(cr, 1.3, -8.2, 1.1, 1.6, 0.2);
        cairo_fill(cr);

        // 3. Metallic Saucer Disc (Middle flange)
        cairo_pattern_t* sg = cairo_pattern_create_linear(-22, 0, 22, 0);
        stop(sg, 0, rgb(0x475569));
        stop(sg, 0.25, rgb(0x94a3b8));
        stop(sg, 0.5, rgb(0xf1f5f9));
        stop(sg, 0.75, rgb(0x94a3b8));
        stop(sg, 1, rgb(0x475569));
        cairo_set_source(cr, sg);
        ellipse(cr, 0, -1, 20, 6.5);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(sg);

        source(cr, rgb(0xcbd5e1));
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);

        // Bottom saucer hull
        cairo_pattern_t* bh = cairo_pattern_create_linear(0, 0, 0, 7);
        stop(bh, 0, rgb(0x334155));
        stop(bh, 1, rgb(0x1e293b));
        cairo_set_source(cr, bh);
        ellipse(cr, 0, 2.5, 11, 4.2);
        cairo_fill(cr);
        cairo_pattern_destroy(bh);

        // 4. Rotating Perimeter Chasing Lights (5 lights)
        for (int i = 0; i < 5; i++) {
            float ang = lightRot_ + i * (TAU / 5);
            float lx = std::cos(ang) * 16.5f;
            float ly = -1.0f + std::sin(ang) * 4.5f;
            if (std::sin(ang) < -0.2f) continue; // Behind saucer horizon

            static const Color lightColors[] = {rgb(0x38bdf8), rgb(0xfacc15), rgb(0xf43f5e), rgb(0xa855f7), rgb(0x4ade80)};
            source(cr, lightColors[i % 5]);
            circle(cr, lx, ly, 1.8);
            cairo_fill(cr);
        }
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 38 * pet.param("size", 1.0f));
    }
};

static struct UfoRegistrar {
    UfoRegistrar() {
        registerAccessory({"ufo", "Disco Voador (OVNI)", rgb(0x4ade80),
                           CoreConfig{60, 0.50f, 0.92f, 0.16f},
                           hints({{"orbitX", 40}, {"orbitY", -30}, {"sway", 4.5f}, {"lift", -0.03f}, {"kick", 5}, {"sparks", 16}}),
                           RopeStyle{rgb(0x475569), rgb(0x4ade80), 2.0f},
                           ParticleStyle{rgb(0x4ade80), 2.2f, 0.04f}, // particles drift down
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<Ufo>()});
    }
} s_ufoRegistrar;

}  // namespace mousepet
