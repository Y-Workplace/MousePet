#include "Common.hpp"

namespace mousepet {

class ChineseLantern final : public Accessory {
    float sway_ = 0.0f;
    float tasselAngle_ = 0.0f;
    float glowPulse_ = 0.0f;

public:
    void update(Pet& pet) override {
        Body& b = pet.body;
        sway_ += 0.08f;
        glowPulse_ += 0.05f;

        // Dangling tassel and bell inertia
        const float targetTassel = std::clamp(-b.vx * 0.12f, -0.75f, 0.75f);
        tasselAngle_ += (targetTassel - tasselAngle_) * 0.18f;

        // Warm festival ember / petal particles drifting gently
        if (pet.random() < 0.14f || b.burst > 0.08f) {
            float ox = (pet.random() - 0.5f) * 10.0f;
            pet.emit(b.x + ox, b.y + 12.0f,
                     (pet.random() - 0.5f) * 0.3f, 0.4f + pet.random() * 0.5f, 0.8f);
        }
    }

    void onBurst(Pet& pet) override {
        // Starburst of festival gold and crimson firework sparks
        for (int i = 0; i < 18; i++) {
            float a = pet.random() * TAU;
            float spd = 1.4f + pet.random() * 2.2f;
            pet.emit(pet.body.x, pet.body.y, std::cos(a) * spd, std::sin(a) * spd, 1.2f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);
        const double glowStrength = 0.85 + std::sin(glowPulse_) * 0.15 + b.burst * 1.3;

        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle * 0.4f);
        cairo_scale(cr, sz, sz);

        // 1. Warm Radiant Festival Glow
        glow(cr, 0, 0, 4, 38 * glowStrength,
             {{0, rgba(254, 215, 170, 0.95)}, {0.3, rgba(249, 115, 22, 0.55)}, {0.7, rgba(220, 38, 38, 0.25)}, {1, rgba(185, 28, 28, 0)}},
             CAIRO_OPERATOR_SCREEN, glowStrength, true);

        // 2. Hanging Tassel & Golden Bell (Underneath lantern)
        cairo_save(cr);
        cairo_translate(cr, 0, 16.0);
        cairo_rotate(cr, tasselAngle_);

        // Connecting cord
        source(cr, rgb(0x78350f));
        cairo_set_line_width(cr, 1.2);
        cairo_move_to(cr, 0, 0);
        cairo_line_to(cr, 0, 9);
        cairo_stroke(cr);

        // Golden Bell
        cairo_pattern_t* bg = cairo_pattern_create_radial(0, 9, 0, 0, 9, 3.5);
        stop(bg, 0, rgb(0xfef08a));
        stop(bg, 0.6, rgb(0xeab308));
        stop(bg, 1, rgb(0xa16207));
        cairo_set_source(cr, bg);
        circle(cr, 0, 9, 3.2);
        cairo_fill(cr);
        cairo_pattern_destroy(bg);

        // Bell rim and slit
        source(cr, rgb(0x713f12));
        cairo_set_line_width(cr, 0.8);
        circle(cr, 0, 9, 3.2);
        cairo_stroke(cr);

        // Silken Red Tassel Fringe
        cairo_pattern_t* tg = cairo_pattern_create_linear(-4, 12, 4, 28);
        stop(tg, 0, rgb(0xb91c1c));
        stop(tg, 0.5, rgb(0xef4444));
        stop(tg, 1, rgb(0x7f1d1d));
        cairo_set_source(cr, tg);

        cairo_new_path(cr);
        cairo_move_to(cr, -2.5, 12);
        cairo_line_to(cr, -4.5, 28);
        cairo_line_to(cr, 4.5, 28);
        cairo_line_to(cr, 2.5, 12);
        cairo_close_path(cr);
        cairo_fill(cr);
        cairo_pattern_destroy(tg);

        // Gold collar binding on the tassel
        source(cr, rgb(0xfacc15));
        cairo_rectangle(cr, -2.8, 13, 5.6, 2.0);
        cairo_fill(cr);

        // Individual silk tassel threads
        source(cr, rgba(254, 202, 202, 0.4));
        cairo_set_line_width(cr, 0.6);
        cairo_move_to(cr, -2, 15); cairo_line_to(cr, -3, 28);
        cairo_move_to(cr, 0, 15);  cairo_line_to(cr, 0, 28);
        cairo_move_to(cr, 2, 15);  cairo_line_to(cr, 3, 28);
        cairo_stroke(cr);
        cairo_restore(cr);

        // 3. Lantern Paper Body (Cylindrical Oval)
        cairo_pattern_t* lg = cairo_pattern_create_linear(-14, 0, 14, 0);
        stop(lg, 0, rgb(0x991b1b));    // Dark crimson at edges
        stop(lg, 0.25, rgb(0xdc2626));
        stop(lg, 0.5, rgb(0xf97316));  // Vibrant warm orange-red center
        stop(lg, 0.75, rgb(0xdc2626));
        stop(lg, 1, rgb(0x991b1b));

        // Barrel shape
        cairo_new_path(cr);
        cairo_move_to(cr, -10, -14);
        cairo_curve_to(cr, -16, -5, -16, 5, -10, 14);
        cairo_line_to(cr, 10, 14);
        cairo_curve_to(cr, 16, 5, 16, -5, 10, -14);
        cairo_close_path(cr);
        cairo_set_source(cr, lg);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(lg);

        source(cr, rgba(127, 29, 29, 0.85));
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);

        // Inner glowing core
        cairo_pattern_t* ig = cairo_pattern_create_radial(0, 0, 2, 0, 0, 14);
        stop(ig, 0, rgba(254, 240, 138, 0.75));
        stop(ig, 0.6, rgba(249, 115, 22, 0.45));
        stop(ig, 1, rgba(220, 38, 38, 0.0));
        cairo_set_source(cr, ig);
        ellipse(cr, 0, 0, 13, 13);
        cairo_fill(cr);
        cairo_pattern_destroy(ig);

        // Horizontal Bamboo Ribs (Arandelas de bambu)
        source(cr, rgba(120, 53, 15, 0.55));
        cairo_set_line_width(cr, 0.9);
        for (float ry : {-9.0f, -4.5f, 0.0f, 4.5f, 9.0f}) {
            cairo_new_path(cr);
            float w = 14.5f - (ry * ry) * 0.04f;
            cairo_move_to(cr, -w, ry);
            cairo_curve_to(cr, -w * 0.5f, ry + 1.2f, w * 0.5f, ry + 1.2f, w, ry);
            cairo_stroke(cr);
        }

        // 4. Calligraphy Character "燈" (Lamp / Light) in traditional black ink
        source(cr, rgba(24, 24, 27, 0.90));
        cairo_set_line_width(cr, 1.3);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        // Left radical: 火 (Fire)
        cairo_new_path(cr);
        cairo_move_to(cr, -7.5, -3); cairo_line_to(cr, -6.0, 5); // vertical stroke
        cairo_move_to(cr, -9.0, -1); cairo_line_to(cr, -8.0, 0); // left dot
        cairo_move_to(cr, -5.5, -1); cairo_line_to(cr, -4.5, 1); // right dot
        cairo_stroke(cr);

        // Right radical: 登
        cairo_new_path(cr);
        cairo_move_to(cr, -2, -4); cairo_line_to(cr, 7, -4);     // top bar
        cairo_move_to(cr, 2.5, -5); cairo_line_to(cr, 2.5, -2);   // center stem
        cairo_move_to(cr, -1, -2); cairo_line_to(cr, 6, -2);     // second bar
        cairo_move_to(cr, -1, 0);  cairo_line_to(cr, 6, 0);      // lower box top
        cairo_move_to(cr, -1, 0);  cairo_line_to(cr, -1, 5);     // box left
        cairo_move_to(cr, 6, 0);   cairo_line_to(cr, 6, 5);      // box right
        cairo_move_to(cr, -1, 5);  cairo_line_to(cr, 6, 5);      // box bottom
        cairo_move_to(cr, 2.5, 0); cairo_line_to(cr, 2.5, 5);   // box middle
        cairo_stroke(cr);

        // 5. Ornamental Bronze Top & Bottom Fittings
        auto drawFitting = [&](float y, bool top) {
            cairo_pattern_t* fg = cairo_pattern_create_linear(-12, 0, 12, 0);
            stop(fg, 0, rgb(0x713f12));
            stop(fg, 0.5, rgb(0xca8a04));
            stop(fg, 1, rgb(0x713f12));
            cairo_set_source(cr, fg);

            // Collar ring
            cairo_new_path(cr);
            cairo_rectangle(cr, -11, y - 2, 22, 4);
            cairo_fill_preserve(cr);
            source(cr, rgb(0x451a03));
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);

            // Ornamental rim
            source(cr, rgb(0xeab308));
            cairo_rectangle(cr, -8, top ? y - 4 : y + 2, 16, 2.2);
            cairo_fill(cr);

            cairo_pattern_destroy(fg);
        };

        drawFitting(-14, true);
        drawFitting(14, false);

        // 6. Top Hanging Bronze Ring (Where rope attaches)
        source(cr, rgb(0xca8a04));
        cairo_set_line_width(cr, 2.0);
        circle(cr, 0, -21, 3.8);
        cairo_stroke(cr);
        source(cr, rgb(0x713f12));
        cairo_set_line_width(cr, 0.8);
        circle(cr, 0, -21, 3.8);
        cairo_stroke(cr);
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 46 * pet.param("size", 1.0f));
    }
};

static struct ChineseLanternRegistrar {
    ChineseLanternRegistrar() {
        registerAccessory({"chineselantern", "Lanterna Chinesa", rgb(0xdc2626),
                           CoreConfig{60, 0.50f, 0.90f, 0.22f},
                           hints({{"orbitX", 40}, {"orbitY", -22}, {"sway", 4.5f}, {"lift", 0.02f}, {"kick", 5.0f}, {"sparks", 16}}),
                           RopeStyle{rgb(0x991b1b), rgb(0xfacc15), 1.8f},
                           ParticleStyle{rgb(0xfbbf24), 2.2f, 0.02f}, // golden sparks and festival glow
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<ChineseLantern>()});
    }
} s_chineseLanternRegistrar;

}  // namespace mousepet
