#include "Common.hpp"

namespace mousepet {

class HotAirBalloon final : public Accessory {
    float burner_ = 1.0f;

public:
    void update(Pet& pet) override {
        Body& b = pet.body;
        // Warm air buoyancy
        b.vy += std::sin(pet.time * 2.5f) * 0.02f;
        burner_ = 0.8f + std::sin(pet.time * 8.0f) * 0.2f + b.burst * 1.5f;
        if (b.burst > 0.1f || (pet.random() < 0.06f)) {
            // Hot flame sparks rising
            pet.emit(b.x + (pet.random() - 0.5f) * 6.f, b.y + 14.f,
                     (pet.random() - 0.5f) * 0.4f, -0.6f - pet.random() * 0.6f, 0.7f);
        }
    }

    void onBurst(Pet& pet) override {
        for (int i = 0; i < 12; i++) {
            float a = -PI * 0.5f + (pet.random() - 0.5f) * 1.2f;
            float spd = 0.8f + pet.random() * 2.0f;
            pet.emit(pet.body.x, pet.body.y + 12.f, std::cos(a) * spd, std::sin(a) * spd, 1.0f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);
        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle * 0.5f);
        cairo_scale(cr, sz, sz);

        // 1. Suspension rigging lines to basket
        source(cr, rgb(0x78716c));
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, -12, 10); cairo_line_to(cr, -6, 26);
        cairo_move_to(cr, -5, 12);  cairo_line_to(cr, -3, 26);
        cairo_move_to(cr, 5, 12);   cairo_line_to(cr, 3, 26);
        cairo_move_to(cr, 12, 10);  cairo_line_to(cr, 6, 26);
        cairo_stroke(cr);

        // 2. Burner flame
        const double flameH = 6.0 * burner_;
        glow(cr, 0, 14, 0, 12 * burner_,
             {{0, rgba(254, 240, 138, 0.9)}, {0.5, rgba(249, 115, 22, 0.5)}, {1, rgba(239, 68, 68, 0)}},
             CAIRO_OPERATOR_SCREEN, 0.85);

        cairo_new_path(cr);
        cairo_move_to(cr, -3, 16);
        cairo_curve_to(cr, -4, 16 - flameH * 0.5, 0, 16 - flameH, 0, 16 - flameH);
        cairo_curve_to(cr, 0, 16 - flameH, 4, 16 - flameH * 0.5, 3, 16);
        cairo_close_path(cr);
        source(cr, rgb(0xfef08a));
        cairo_fill(cr);

        // 3. Wicker Basket
        cairo_pattern_t* bg = cairo_pattern_create_linear(0, 24, 0, 34);
        stop(bg, 0, rgb(0xb45309));
        stop(bg, 1, rgb(0x78350f));
        cairo_set_source(cr, bg);
        cairo_rectangle(cr, -7, 26, 14, 9);
        cairo_fill(cr);
        cairo_pattern_destroy(bg);

        // Basket rim & weave texture
        source(cr, rgb(0xd97706));
        cairo_set_line_width(cr, 1.2);
        cairo_rectangle(cr, -8, 25, 16, 2.5);
        cairo_fill(cr);
        source(cr, rgb(0x451a03));
        cairo_move_to(cr, -7, 30); cairo_line_to(cr, 7, 30);
        cairo_move_to(cr, -2, 26); cairo_line_to(cr, -2, 35);
        cairo_move_to(cr, 2, 26);  cairo_line_to(cr, 2, 35);
        cairo_stroke(cr);

        // 4. Balloon Envelope (Teardrop shape)
        // Outer glow
        glow(cr, 0, -6, 4, 30, {{0, rgba(254, 215, 170, 0.25)}, {1, rgba(254, 215, 170, 0)}});

        // Base balloon path
        auto makeBalloonPath = [&] {
            cairo_new_path(cr);
            cairo_move_to(cr, -6, 12);
            cairo_curve_to(cr, -24, 2, -26, -24, 0, -26);
            cairo_curve_to(cr, 26, -24, 24, 2, 6, 12);
            cairo_close_path(cr);
        };

        // Clip to balloon and draw vertical colored panels
        cairo_save(cr);
        makeBalloonPath();
        cairo_clip(cr);

        // Background / side stripes: Teal / Cyan
        source(cr, rgb(0x0284c7));
        cairo_paint(cr);

        // Middle stripes: Sunny Gold / Orange
        cairo_new_path(cr);
        cairo_move_to(cr, -12, 12);
        cairo_curve_to(cr, -16, 0, -17, -22, 0, -26);
        cairo_curve_to(cr, 17, -22, 16, 0, 12, 12);
        cairo_close_path(cr);
        source(cr, rgb(0xf59e0b));
        cairo_fill(cr);

        // Center stripe: Coral Crimson
        cairo_new_path(cr);
        cairo_move_to(cr, -5, 12);
        cairo_curve_to(cr, -7, 0, -7, -24, 0, -26);
        cairo_curve_to(cr, 7, -24, 7, 0, 5, 12);
        cairo_close_path(cr);
        source(cr, rgb(0xef4444));
        cairo_fill(cr);

        // Shading vignette (spherical 3D lighting)
        cairo_pattern_t* shad = cairo_pattern_create_radial(-8, -10, 4, 0, -6, 26);
        stop(shad, 0, rgba(255, 255, 255, 0.45));
        stop(shad, 0.6, rgba(255, 255, 255, 0.0));
        stop(shad, 1.0, rgba(15, 23, 42, 0.55));
        cairo_set_source(cr, shad);
        cairo_paint(cr);
        cairo_pattern_destroy(shad);
        cairo_restore(cr);

        // Outline
        makeBalloonPath();
        source(cr, rgba(255, 255, 255, 0.4));
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 44 * pet.param("size", 1.0f));
    }
};

static struct HotAirRegistrar {
    HotAirRegistrar() {
        registerAccessory({"hotair", "Balão de Ar Quente", rgb(0xf97316),
                           CoreConfig{85, 0.38f, 0.92f, 0.20f},
                           hints({{"orbitX", 18}, {"orbitY", -80}, {"sway", 6}, {"lift", -0.06f}, {"kick", 5}, {"sparks", 16}}),
                           RopeStyle{rgb(0xa8a29e), rgb(0xf5f5f4), 1.8f},
                           ParticleStyle{rgb(0xfb923c), 2.2f, -0.03f},
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<HotAirBalloon>()});
    }
} s_hotAirRegistrar;

}  // namespace mousepet
