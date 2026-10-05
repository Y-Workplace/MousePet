#include "Common.hpp"

namespace mousepet {

class Firefly final : public Accessory {
    float wingVibe_ = 0.0f;
    float pulse_ = 0.0f;

public:
    void update(Pet& pet) override {
        Body& b = pet.body;
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        wingVibe_ += 0.45f + sp * 0.15f;
        pulse_ += 0.05f;

        // Magical bioluminescent pollen sparks
        if (pet.random() < 0.20f || b.burst > 0.1f) {
            float ox = (pet.random() - 0.5f) * 8.0f;
            float oy = 8.0f + (pet.random() - 0.5f) * 6.0f;
            pet.emit(b.x + ox, b.y + oy,
                     (pet.random() - 0.5f) * 0.3f, -0.2f - pet.random() * 0.3f, 0.8f);
        }
    }

    void onBurst(Pet& pet) override {
        // Flash dispersal of glowing pollen
        for (int i = 0; i < 16; i++) {
            float a = pet.random() * TAU;
            float spd = 1.0f + pet.random() * 2.2f;
            pet.emit(pet.body.x, pet.body.y + 6.0f, std::cos(a) * spd, std::sin(a) * spd, 1.2f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);
        const double glowIntensity = 0.75 + std::sin(pulse_) * 0.25 + b.burst * 1.2;
        const double wingAngle = std::sin(wingVibe_) * 0.40;

        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle * 0.5f);
        cairo_scale(cr, sz, sz);

        // 1. Lantern Bioluminescent Radiant Glow
        glow(cr, 0, 8, 2, 28 * glowIntensity,
             {{0, rgba(254, 240, 138, 0.95)}, {0.4, rgba(190, 242, 100, 0.45)}, {1, rgba(163, 230, 53, 0)}},
             CAIRO_OPERATOR_SCREEN, glowIntensity, true);

        // 2. Wings attached directly to the thorax (hinged at x = +/-2.5, y = -1.5)
        cairo_save(cr);
        source(cr, rgba(240, 253, 244, 0.70));
        cairo_set_line_width(cr, 0.8);

        // Left forewing
        cairo_save(cr);
        cairo_translate(cr, -2.5, -1.5);
        cairo_rotate(cr, -0.45 - wingAngle);
        cairo_new_path(cr);
        cairo_move_to(cr, 0, 0);
        cairo_curve_to(cr, -6, -8, -14, -6, -11, 2);
        cairo_curve_to(cr, -8, 6, -3, 3, 0, 0);
        cairo_close_path(cr);
        cairo_fill_preserve(cr);
        source(cr, rgba(255, 255, 255, 0.85)); cairo_stroke(cr);
        cairo_restore(cr);

        // Right forewing
        cairo_save(cr);
        cairo_translate(cr, 2.5, -1.5);
        cairo_rotate(cr, 0.45 + wingAngle);
        source(cr, rgba(240, 253, 244, 0.70));
        cairo_new_path(cr);
        cairo_move_to(cr, 0, 0);
        cairo_curve_to(cr, 6, -8, 14, -6, 11, 2);
        cairo_curve_to(cr, 8, 6, 3, 3, 0, 0);
        cairo_close_path(cr);
        cairo_fill_preserve(cr);
        source(cr, rgba(255, 255, 255, 0.85)); cairo_stroke(cr);
        cairo_restore(cr);

        cairo_restore(cr);

        // 3. Glowing Lantern Abdomen (Tail bulb)
        cairo_pattern_t* lg = cairo_pattern_create_radial(0, 7, 1, 0, 8, 9);
        stop(lg, 0, rgb(0xfef08a));
        stop(lg, 0.5, rgb(0xa3e635));
        stop(lg, 1, rgb(0x4d7c0f));
        cairo_set_source(cr, lg);
        ellipse(cr, 0, 8, 7.5, 9.5);
        cairo_fill(cr);
        cairo_pattern_destroy(lg);

        // Lantern highlight
        source(cr, rgba(255, 255, 255, 0.75));
        circle(cr, -2.5, 6.0, 2.0);
        cairo_fill(cr);

        // 4. Thorax (Dark forest green beetle shell)
        source(cr, rgb(0x14532d));
        ellipse(cr, 0, -1, 6.0, 4.5);
        cairo_fill(cr);

        // 5. Head & Cute Antennae
        source(cr, rgb(0x052e16));
        circle(cr, 0, -6.5, 4.0);
        cairo_fill(cr);

        // Big glossy black eyes
        source(cr, rgb(0x022c22));
        circle(cr, -2.8, -7.0, 1.8);
        circle(cr, 2.8, -7.0, 1.8);
        cairo_fill(cr);
        // Eye shine
        source(cr, rgb(0xffffff));
        circle(cr, -3.2, -7.5, 0.7);
        circle(cr, 2.4, -7.5, 0.7);
        cairo_fill(cr);

        // Delicate curled antennae
        source(cr, rgb(0x14532d));
        cairo_set_line_width(cr, 1.0);
        cairo_new_path(cr);
        cairo_move_to(cr, -2, -9); quadTo(cr, -6, -14, -8, -13);
        cairo_move_to(cr, 2, -9);  quadTo(cr, 6, -14, 8, -13);
        cairo_stroke(cr);
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 32 * pet.param("size", 1.0f));
    }
};

static struct FireflyRegistrar {
    FireflyRegistrar() {
        registerAccessory({"firefly", "Vaga-lume da Floresta", rgb(0xa3e635),
                           CoreConfig{50, 0.60f, 0.88f, 0.12f},
                           hints({{"orbitX", 25}, {"orbitY", -25}, {"sway", 4.0f}, {"lift", -0.05f}, {"kick", 5}, {"sparks", 14}}),
                           RopeStyle{rgb(0x4d7c0f), rgb(0xa3e635), 1.2f},
                           ParticleStyle{rgb(0xfef08a), 2.2f, -0.02f},
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<Firefly>()});
    }
} s_fireflyRegistrar;

}  // namespace mousepet
