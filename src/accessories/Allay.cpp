#include "Common.hpp"

namespace mousepet {

class Allay final : public Accessory {
    float wingPhase_ = 0.0f;
    float dance_ = 0.0f;

public:
    void update(Pet& pet) override {
        Body& b = pet.body;
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        // Wings flutter rapidly, faster when moving or on burst
        wingPhase_ += 0.35f + sp * 0.12f + b.burst * 0.4f;
        dance_ += 0.05f;

        // Fairy sparkle particles
        if (pet.random() < 0.15f || b.burst > 0.1f) {
            float ox = (pet.random() - 0.5f) * 16.0f;
            float oy = (pet.random() - 0.5f) * 16.0f;
            pet.emit(b.x + ox, b.y + oy,
                     (pet.random() - 0.5f) * 0.4f, -0.4f - pet.random() * 0.4f, 0.9f);
        }
    }

    void onBurst(Pet& pet) override {
        // Starburst of musical fairy dust
        for (int i = 0; i < 16; i++) {
            float a = pet.random() * TAU;
            float spd = 1.2f + pet.random() * 2.2f;
            pet.emit(pet.body.x, pet.body.y, std::cos(a) * spd, std::sin(a) * spd, 1.2f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);
        const double wingSpan = std::sin(wingPhase_);

        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle * 0.4f);
        cairo_scale(cr, sz, sz);

        // 1. Luminous Cyan Aura (Allay glows in dark)
        glow(cr, 0, -2, 4, 30,
             {{0, rgba(75, 227, 255, 0.55)}, {0.5, rgba(56, 189, 248, 0.25)}, {1, rgba(37, 99, 235, 0)}},
             CAIRO_OPERATOR_SCREEN, 1.0, true);

        // 2. Translucent Fluttering Fairy Wings (Behind body)
        cairo_save(cr);
        source(cr, rgba(200, 245, 255, 0.70));
        cairo_set_line_width(cr, 1.0);

        // Left Wing
        cairo_save(cr);
        cairo_translate(cr, -5, -4);
        cairo_scale(cr, wingSpan, 1.0);
        cairo_new_path(cr);
        cairo_move_to(cr, 0, 0);
        cairo_curve_to(cr, -12, -16, -20, -10, -18, 2);
        cairo_curve_to(cr, -14, 8, -6, 4, 0, 0);
        cairo_fill_preserve(cr);
        source(cr, rgba(255, 255, 255, 0.85));
        cairo_stroke(cr);
        cairo_restore(cr);

        // Right Wing
        cairo_save(cr);
        cairo_translate(cr, 5, -4);
        cairo_scale(cr, -wingSpan, 1.0);
        cairo_new_path(cr);
        cairo_move_to(cr, 0, 0);
        cairo_curve_to(cr, -12, -16, -20, -10, -18, 2);
        cairo_curve_to(cr, -14, 8, -6, 4, 0, 0);
        source(cr, rgba(200, 245, 255, 0.70));
        cairo_fill_preserve(cr);
        source(cr, rgba(255, 255, 255, 0.85));
        cairo_stroke(cr);
        cairo_restore(cr);
        cairo_restore(cr);

        // 3. Allay Floating Gown / Body
        cairo_pattern_t* bg = cairo_pattern_create_linear(0, -6, 0, 14);
        stop(bg, 0, rgb(0x4be3ff));
        stop(bg, 0.7, rgb(0x0ea5e9));
        stop(bg, 1, rgba(2, 132, 199, 0.4));  // Wispy ghost-like tail
        cairo_new_path(cr);
        cairo_move_to(cr, -5, -4);
        cairo_curve_to(cr, -6, 4, -4, 12, 0, 15);
        cairo_curve_to(cr, 4, 12, 6, 4, 5, -4);
        cairo_close_path(cr);
        cairo_set_source(cr, bg);
        cairo_fill(cr);
        cairo_pattern_destroy(bg);

        // 4. Allay Head (Rounded cute Minecraft silhouette)
        cairo_pattern_t* hg = cairo_pattern_create_linear(0, -18, 0, -4);
        stop(hg, 0, rgb(0x7dd3fc));
        stop(hg, 1, rgb(0x38bdf8));
        cairo_new_path(cr);
        cairo_move_to(cr, -7, -6);
        cairo_line_to(cr, -7, -14);
        quadTo(cr, -7, -18, 0, -18);
        quadTo(cr, 7, -18, 7, -14);
        cairo_line_to(cr, 7, -6);
        quadTo(cr, 0, -4, -7, -6);
        cairo_close_path(cr);
        cairo_set_source(cr, hg);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(hg);

        source(cr, rgba(255, 255, 255, 0.5));
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        // Cute Big Dark Eyes
        source(cr, rgb(0x0369a1));
        circle(cr, -3.2, -11, 1.8);
        circle(cr, 3.2, -11, 1.8);
        cairo_fill(cr);

        // Eye highlights
        source(cr, rgb(0xffffff));
        circle(cr, -3.7, -11.6, 0.7);
        circle(cr, 2.7, -11.6, 0.7);
        cairo_fill(cr);

        // Blushing rosy cheeks
        source(cr, rgba(244, 114, 182, 0.45));
        circle(cr, -5.2, -8, 1.4);
        circle(cr, 5.2, -8, 1.4);
        cairo_fill(cr);

        // 5. Holding a Glistening Cookie / Amethyst in tiny hands
        // Tiny cyan hands
        source(cr, rgb(0x38bdf8));
        circle(cr, -4, 2, 1.6);
        circle(cr, 4, 2, 1.6);
        cairo_fill(cr);

        // Golden Cookie between hands
        source(cr, rgb(0xd97706));
        circle(cr, 0, 2.5, 3.2);
        cairo_fill(cr);
        // Chocolate chips
        source(cr, rgb(0x451a03));
        circle(cr, -1, 1.5, 0.7);
        circle(cr, 1.2, 2.8, 0.6);
        circle(cr, 0, 4, 0.6);
        cairo_fill(cr);
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 32 * pet.param("size", 1.0f));
    }
};

static struct AllayRegistrar {
    AllayRegistrar() {
        registerAccessory({"allay", "Allay (Minecraft)", rgb(0x38bdf8),
                           CoreConfig{60, 0.45f, 0.91f, 0.18f},
                           hints({{"orbitX", 36}, {"orbitY", -28}, {"sway", 5.5f}, {"lift", -0.04f}, {"kick", 5}, {"sparks", 16}}),
                           RopeStyle{rgb(0x0284c7), rgb(0x7dd3fc), 1.8f},
                           ParticleStyle{rgb(0x38bdf8), 2.2f, -0.04f}, // sparkles float upward
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<Allay>()});
    }
} s_allayRegistrar;

}  // namespace mousepet
