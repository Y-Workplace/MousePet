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
        // Abduction tractor beam absorption vortex: particles sucked inward and upward into saucer
        for (int i = 0; i < 22; i++) {
            float ox = (pet.random() - 0.5f) * 32.0f;
            float oy = 25.0f + pet.random() * 25.0f;
            float dx = -ox * 0.06f + (pet.random() - 0.5f) * 0.4f;
            float dy = -2.2f - pet.random() * 1.6f; // sucked upward into tractor beam
            pet.emit(pet.body.x + ox, pet.body.y + oy, dx, dy, 1.2f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);

        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle * 0.4f);
        cairo_scale(cr, sz, sz);

        // 1. Tractor Abduction Beam (Projects downward from bottom of saucer)
        const double beamAlpha = 0.25 + std::sin(beamPulse_) * 0.08 + b.burst * 0.55;
        const double beamSpread = 18.0 + b.burst * 10.0;
        const double beamLength = 40.0 + b.burst * 15.0;

        cairo_new_path(cr);
        cairo_move_to(cr, -6, 4);
        cairo_line_to(cr, -beamSpread, beamLength);
        cairo_line_to(cr, beamSpread, beamLength);
        cairo_line_to(cr, 6, 4);
        cairo_close_path(cr);

        cairo_pattern_t* bg = cairo_pattern_create_linear(0, 4, 0, beamLength);
        stop(bg, 0, rgba(74, 222, 128, beamAlpha * 1.5));
        stop(bg, 0.5, rgba(52, 211, 153, beamAlpha * 0.9));
        stop(bg, 1, rgba(16, 185, 129, 0.0));
        cairo_set_source(cr, bg);
        cairo_fill(cr);
        cairo_pattern_destroy(bg);

        // Pulsing energy rings in tractor beam on burst
        if (b.burst > 0.05f) {
            source(cr, rgba(110, 231, 183, b.burst * 0.7));
            cairo_set_line_width(cr, 1.2);
            ellipse(cr, 0, 15.0 + (1.0 - b.burst) * 20.0, 10.0 + b.burst * 4.0, 3.0);
            cairo_stroke(cr);
        }

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
        circle(cr, 0, -7.5, 3.5);
        cairo_fill(cr);
        // Big black alien eyes
        source(cr, rgb(0x022c22));
        ellipse(cr, -1.5, -8.0, 1.1, 1.5, -0.3);
        ellipse(cr, 1.5, -8.0, 1.1, 1.5, 0.3);
        cairo_fill(cr);

        // 3. Metallic Saucer Hull
        cairo_pattern_t* hg = cairo_pattern_create_linear(0, -4, 0, 6);
        stop(hg, 0, rgb(0x94a3b8));
        stop(hg, 0.4, rgb(0xcfd8dc));
        stop(hg, 0.8, rgb(0x64748b));
        stop(hg, 1, rgb(0x334155));
        cairo_set_source(cr, hg);
        ellipse(cr, 0, 0, 20, 6.5);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(hg);

        source(cr, rgba(255, 255, 255, 0.6));
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        // Underside core emitter
        cairo_pattern_t* bh = cairo_pattern_create_radial(0, 2.5, 1, 0, 2.5, 9);
        stop(bh, 0, rgb(0x4ade80));
        stop(bh, 0.5, rgb(0x0f766e));
        stop(bh, 1, rgb(0x0f172a));
        cairo_set_source(cr, bh);
        ellipse(cr, 0, 2.5, 11, 4.2);
        cairo_fill(cr);
        cairo_pattern_destroy(bh);

        // 4. Rotating Perimeter Chasing Lights (6 lights along 3D front rim arc)
        static const Color lightColors[] = {rgb(0x38bdf8), rgb(0xfacc15), rgb(0xf43f5e), rgb(0xa855f7), rgb(0x4ade80), rgb(0xf97316)};
        for (int i = 0; i < 6; i++) {
            float ang = lightRot_ + i * (TAU / 6);
            float cosA = std::cos(ang);
            float sinA = std::sin(ang);
            float lx = cosA * 18.0f;
            float ly = sinA * 5.2f;

            if (sinA < -0.15f) continue;

            float lightScale = 0.85f + sinA * 0.35f;
            glow(cr, lx, ly, 0, 4.5 * lightScale, {{0, rgba(255, 255, 255, 0.85)}, {1, rgba(255, 255, 255, 0)}});
            source(cr, lightColors[i % 6]);
            circle(cr, lx, ly, 1.8f * lightScale);
            cairo_fill(cr);
        }
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 40 * pet.param("size", 1.0f));
    }
};

static struct UfoRegistrar {
    UfoRegistrar() {
        registerAccessory({"ufo", "Disco Voador (OVNI)", rgb(0x4ade80),
                           CoreConfig{60, 0.50f, 0.92f, 0.16f},
                           hints({{"orbitX", 40}, {"orbitY", -30}, {"sway", 4.5f}, {"lift", -0.03f}, {"kick", 5}, {"sparks", 16}}),
                           RopeStyle{rgb(0x475569), rgb(0x4ade80), 2.0f},
                           ParticleStyle{rgb(0x4ade80), 2.2f, -0.02f},
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<Ufo>()});
    }
} s_ufoRegistrar;

}  // namespace mousepet
