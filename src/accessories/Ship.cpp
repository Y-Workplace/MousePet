#include "Common.hpp"

namespace mousepet {

class Ship final : public Accessory {
    float thrusterPulse_ = 0.0f;
    float heading_ = -PI * 0.5f;

public:
    void update(Pet& pet) override {
        Body& b = pet.body;
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        thrusterPulse_ += 0.25f + sp * 0.12f;

        // Smooth flight steering: ship nose aims towards velocity or target mouse
        float targetHeading = heading_;
        if (sp > 0.4f) {
            targetHeading = std::atan2(b.vy, b.vx);
        } else {
            float toMx = pet.mouse.x - b.x, toMy = pet.mouse.y - b.y;
            if (toMx * toMx + toMy * toMy > 30.0f) {
                targetHeading = std::atan2(toMy, toMx);
            }
        }
        float diff = targetHeading - heading_;
        while (diff < -PI) diff += TAU;
        while (diff > PI) diff -= TAU;
        heading_ += diff * 0.16f;

        // Ion engine exhaust trail
        if (sp > 0.6f || pet.random() < 0.25f) {
            float exhaustAng = heading_ + PI;
            float c = std::cos(exhaustAng), s = std::sin(exhaustAng);
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
        // Ship nose (0, -18) points forward in the direction of heading
        cairo_rotate(cr, heading_ + PI * 0.5f);
        cairo_scale(cr, sz, sz);

        // 1. Plasma Thruster Exhaust
        const double flameLen = 12.0 + sp * 3.5 + std::sin(thrusterPulse_) * 3.0 + b.burst * 14.0;
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
        source(cr, rgba(255, 255, 255, 0.95));
        cairo_fill(cr);

        // 2. Main Hull (Sleek sci-fi wedge interceptor)
        cairo_pattern_t* hg = cairo_pattern_create_linear(-16, 0, 16, 0);
        stop(hg, 0, rgb(0x1e293b));
        stop(hg, 0.5, rgb(0x475569));
        stop(hg, 1, rgb(0x1e293b));
        cairo_set_source(cr, hg);

        cairo_new_path(cr);
        cairo_move_to(cr, 0, -18);      // Needle nose
        cairo_line_to(cr, 6, -6);
        cairo_line_to(cr, 18, 8);       // Starboard swept wingtip
        cairo_line_to(cr, 14, 12);
        cairo_line_to(cr, 7, 10);
        cairo_line_to(cr, 5, 12);
        cairo_line_to(cr, -5, 12);      // Thruster bay
        cairo_line_to(cr, -7, 10);
        cairo_line_to(cr, -14, 12);
        cairo_line_to(cr, -18, 8);      // Port swept wingtip
        cairo_line_to(cr, -6, -6);
        cairo_close_path(cr);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(hg);

        source(cr, rgba(56, 189, 248, 0.6));
        cairo_set_line_width(cr, 0.9);
        cairo_stroke(cr);

        // 3. Wingtip Ion Nav Lights
        glow(cr, -17, 8, 0, 5, {{0, rgba(239, 68, 68, 0.9)}, {1, rgba(239, 68, 68, 0)}});
        source(cr, rgb(0xef4444));
        circle(cr, -17, 8, 1.4); cairo_fill(cr);

        glow(cr, 17, 8, 0, 5, {{0, rgba(34, 197, 94, 0.9)}, {1, rgba(34, 197, 94, 0)}});
        source(cr, rgb(0x22c55e));
        circle(cr, 17, 8, 1.4); cairo_fill(cr);

        // 4. Cockpit Canopy (Glowing Neon Cyan)
        glow(cr, 0, -4, 0, 8, {{0, rgba(56, 189, 248, 0.8)}, {1, rgba(56, 189, 248, 0)}});
        cairo_pattern_t* cg = cairo_pattern_create_linear(0, -9, 0, 1);
        stop(cg, 0, rgba(224, 242, 254, 0.95));
        stop(cg, 0.5, rgba(56, 189, 248, 0.9));
        stop(cg, 1, rgba(3, 105, 161, 0.7));
        cairo_set_source(cr, cg);

        cairo_new_path(cr);
        cairo_move_to(cr, 0, -9);
        cairo_curve_to(cr, 3.2, -6, 3.2, -1, 0, 1);
        cairo_curve_to(cr, -3.2, -1, -3.2, -6, 0, -9);
        cairo_close_path(cr);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(cg);

        source(cr, rgba(255, 255, 255, 0.8));
        cairo_set_line_width(cr, 0.8);
        cairo_stroke(cr);
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 36 * pet.param("size", 1.0f));
    }
};

static struct ShipRegistrar {
    ShipRegistrar() {
        registerAccessory({"ship", "Nave Espacial", rgb(0x38bdf8),
                           CoreConfig{55, 0.55f, 0.90f, 0.15f},
                           hints({{"orbitX", 35}, {"orbitY", -30}, {"sway", 3.0f}, {"lift", -0.04f}, {"kick", 5}, {"sparks", 16}}),
                           RopeStyle{rgb(0x334155), rgb(0x38bdf8), 1.5f},
                           ParticleStyle{rgb(0x38bdf8), 2.0f, -0.01f},
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<Ship>()});
    }
} s_shipRegistrar;

}  // namespace mousepet
