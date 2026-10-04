#include "Common.hpp"

namespace mousepet {

class Ship final : public Accessory {
    float thrusterPulse_ = 0.0f;

public:
    void update(Pet& pet) override {
        Body& b = pet.body;
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        thrusterPulse_ += 0.25f + sp * 0.1f;

        // Ion engine exhaust trail
        if (sp > 0.8f || pet.random() < 0.25f) {
            float ang = b.angle + PI;
            float c = std::cos(ang), s = std::sin(ang);
            float ex = b.x + c * 16.0f, ey = b.y + s * 16.0f;
            pet.emit(ex + (pet.random() - 0.5f) * 4.0f,
                     ey + (pet.random() - 0.5f) * 4.0f,
                     c * (1.5f + pet.random() * 2.0f) + (pet.random() - 0.5f) * 0.5f,
                     s * (1.5f + pet.random() * 2.0f) + (pet.random() - 0.5f) * 0.5f,
                     0.6f);
        }
    }

    void onBurst(Pet& pet) override {
        // Warp ring shockwave
        for (int i = 0; i < 18; i++) {
            float a = i * (TAU / 18);
            float spd = 2.5f + pet.random() * 1.5f;
            pet.emit(pet.body.x, pet.body.y, std::cos(a) * spd, std::sin(a) * spd, 1.0f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);

        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle + PI * 0.5f);  // Ship faces forward along motion
        cairo_scale(cr, sz, sz);

        // 1. Plasma Thruster Exhaust
        const double flameLen = 12.0 + sp * 3.5 + std::sin(thrusterPulse_) * 3.0 + b.burst * 14.0;
        // Outer glow
        glow(cr, 0, 14, 0, 18,
             {{0, rgba(56, 189, 248, 0.95)}, {0.5, rgba(59, 130, 246, 0.4)}, {1, rgba(37, 99, 235, 0)}},
             CAIRO_OPERATOR_SCREEN, 0.9);

        // Dual layer flame
        cairo_new_path(cr);
        cairo_move_to(cr, -5, 12);
        cairo_curve_to(cr, -6, 12 + flameLen * 0.6, 0, 12 + flameLen, 0, 12 + flameLen);
        cairo_curve_to(cr, 0, 12 + flameLen, 6, 12 + flameLen * 0.6, 5, 12);
        cairo_close_path(cr);
        cairo_pattern_t* fg = cairo_pattern_create_linear(0, 12, 0, 12 + flameLen);
        stop(fg, 0, rgba(224, 242, 254, 1.0));
        stop(fg, 0.4, rgba(56, 189, 248, 0.85));
        stop(fg, 1.0, rgba(37, 99, 235, 0.0));
        cairo_set_source(cr, fg);
        cairo_fill(cr);
        cairo_pattern_destroy(fg);

        // Core white flame
        cairo_new_path(cr);
        cairo_move_to(cr, -2.5, 12);
        cairo_line_to(cr, 0, 12 + flameLen * 0.55);
        cairo_line_to(cr, 2.5, 12);
        cairo_close_path(cr);
        source(cr, rgb(0xffffff));
        cairo_fill(cr);

        // 2. Shield aura on burst
        if (b.burst > 0.05f) {
            glow(cr, 0, 0, 8, 28,
                 {{0, rgba(56, 189, 248, 0.0)}, {0.8, rgba(56, 189, 248, 0.6)}, {1, rgba(56, 189, 248, 0)}},
                 CAIRO_OPERATOR_SCREEN, b.burst);
        }

        // 3. Wings & Hull (Futuristic Delta Starfighter)
        // Main Wings (Sweep-back)
        cairo_new_path(cr);
        cairo_move_to(cr, 0, -22);       // Nose
        cairo_line_to(cr, 6, -8);
        cairo_line_to(cr, 20, 10);       // Right wingtip
        cairo_line_to(cr, 14, 14);
        cairo_line_to(cr, 5, 11);
        cairo_line_to(cr, 5, 14);        // Right engine block
        cairo_line_to(cr, -5, 14);       // Left engine block
        cairo_line_to(cr, -5, 11);
        cairo_line_to(cr, -14, 14);
        cairo_line_to(cr, -20, 10);      // Left wingtip
        cairo_line_to(cr, -6, -8);
        cairo_close_path(cr);

        cairo_pattern_t* hg = cairo_pattern_create_linear(-20, 0, 20, 0);
        stop(hg, 0, rgb(0x334155));
        stop(hg, 0.5, rgb(0x64748b));
        stop(hg, 1, rgb(0x334155));
        cairo_set_source(cr, hg);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(hg);

        source(cr, rgb(0x94a3b8));
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);

        // Wingtip navigation lights
        source(cr, rgb(0xef4444)); // Left: red beacon
        circle(cr, -19, 10, 1.6); cairo_fill(cr);
        source(cr, rgb(0x22c55e)); // Right: green beacon
        circle(cr, 19, 10, 1.6); cairo_fill(cr);

        // Central Fuselage spine
        cairo_new_path(cr);
        cairo_move_to(cr, 0, -22);
        cairo_line_to(cr, 4, -4);
        cairo_line_to(cr, 3, 11);
        cairo_line_to(cr, -3, 11);
        cairo_line_to(cr, -4, -4);
        cairo_close_path(cr);
        source(cr, rgb(0x1e293b));
        cairo_fill(cr);

        // 4. Cockpit Glass Canopy
        cairo_pattern_t* cg = cairo_pattern_create_linear(0, -14, 0, 2);
        stop(cg, 0, rgba(56, 189, 248, 0.95));
        stop(cg, 0.7, rgba(14, 165, 233, 0.85));
        stop(cg, 1, rgba(2, 132, 199, 0.65));
        cairo_new_path(cr);
        cairo_move_to(cr, 0, -16);
        cairo_curve_to(cr, 3, -12, 3.5, 0, 0, 3);
        cairo_curve_to(cr, -3.5, 0, -3, -12, 0, -16);
        cairo_close_path(cr);
        cairo_set_source(cr, cg);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(cg);

        source(cr, rgba(255, 255, 255, 0.8));
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        // Canopy specular glint
        source(cr, rgba(255, 255, 255, 0.75));
        cairo_new_path(cr);
        cairo_move_to(cr, -1.2, -13);
        cairo_line_to(cr, 0.2, -6);
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 36 * pet.param("size", 1.0f));
    }
};

static struct ShipRegistrar {
    ShipRegistrar() {
        registerAccessory({"ship", "Nave Espacial", rgb(0x38bdf8),
                           CoreConfig{60, 0.55f, 0.93f, 0.15f},
                           hints({{"orbitX", 45}, {"orbitY", -25}, {"sway", 4}, {"tilt", 0.90f}, {"tiltVel", 0.08f}, {"kick", 5}, {"sparks", 16}}),
                           RopeStyle{rgb(0x334155), rgb(0x38bdf8), 2.0f},
                           ParticleStyle{rgb(0x38bdf8), 2.2f, 0.0f},
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<Ship>()});
    }
} s_shipRegistrar;

}  // namespace mousepet
