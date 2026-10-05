#include "Common.hpp"

namespace mousepet {

class Lantern final : public Accessory {
    float flickerPhase_ = 0.0f;
    float flameTilt_ = 0.0f;

public:
    void update(Pet& pet) override {
        Body& b = pet.body;
        flickerPhase_ += 0.18f;

        // Candle flame responds to drag velocity with inertia
        const float targetFlameTilt = std::clamp(-b.vx * 0.08f, -0.6f, 0.6f);
        flameTilt_ += (targetFlameTilt - flameTilt_) * 0.25f;

        // Occasional gentle floating embers / soot rising
        if (pet.random() < 0.15f || b.burst > 0.08f) {
            float ox = (pet.random() - 0.5f) * 6.0f;
            pet.emit(b.x + ox, b.y - 2.0f,
                     flameTilt_ * 0.5f + (pet.random() - 0.5f) * 0.2f,
                     -0.8f - pet.random() * 0.6f, 0.7f);
        }
    }

    void onBurst(Pet& pet) override {
        // Flare of golden embers shooting upwards from the chimney
        for (int i = 0; i < 18; i++) {
            float a = -PI * 0.5f + (pet.random() - 0.5f) * 1.4f;
            float spd = 1.5f + pet.random() * 2.2f;
            pet.emit(pet.body.x, pet.body.y - 14.0f, std::cos(a) * spd, std::sin(a) * spd, 1.1f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);
        const double flicker = 0.85 + 0.15 * std::sin(flickerPhase_ * 3.1f) * std::cos(flickerPhase_ * 1.7f);
        const double flameIntensity = flicker + b.burst * 1.2;

        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle * 0.35f);
        cairo_scale(cr, sz, sz);

        // 1. Warm Golden Ambient Candlelight Glow
        glow(cr, 0, 0, 4, 38 * flameIntensity,
             {{0, rgba(254, 240, 138, 0.95)}, {0.35, rgba(245, 158, 11, 0.45)}, {0.7, rgba(217, 119, 6, 0.15)}, {1, rgba(180, 83, 9, 0)}},
             CAIRO_OPERATOR_SCREEN, flameIntensity, true);

        // 2. Translucent Glass Chamber (Behind cage)
        cairo_pattern_t* gg = cairo_pattern_create_linear(-12, 0, 12, 0);
        stop(gg, 0, rgba(254, 215, 170, 0.25));
        stop(gg, 0.5, rgba(254, 240, 138, 0.45));
        stop(gg, 1, rgba(254, 215, 170, 0.25));
        cairo_new_path(cr);
        cairo_rectangle(cr, -12, -12, 24, 24);
        cairo_set_source(cr, gg);
        cairo_fill(cr);
        cairo_pattern_destroy(gg);

        // 3. Candle & Flame Inside
        // Candle Wax Body
        cairo_pattern_t* wg = cairo_pattern_create_linear(-4, 0, 4, 0);
        stop(wg, 0, rgb(0xfef3c7));
        stop(wg, 0.5, rgb(0xffedd5));
        stop(wg, 1, rgb(0xfde68a));
        cairo_set_source(cr, wg);
        cairo_new_path(cr);
        cairo_rectangle(cr, -4.5, 2, 9, 10);
        cairo_fill(cr);
        cairo_pattern_destroy(wg);

        // Wax drips
        source(cr, rgb(0xfef3c7));
        circle(cr, -3.5, 4, 1.2); cairo_fill(cr);
        circle(cr, 3.0, 5, 1.4); cairo_fill(cr);

        // Wick
        source(cr, rgb(0x27272a));
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, 0, 2);
        cairo_line_to(cr, flameTilt_ * 2.0f, -0.5);
        cairo_stroke(cr);

        // Lively Teardrop Candle Flame
        cairo_save(cr);
        cairo_translate(cr, flameTilt_ * 2.0f, -0.5);
        cairo_rotate(cr, flameTilt_);
        cairo_scale(cr, 1.f + b.burst * 0.18f, 1.f + b.burst * 0.55f);

        // Outer Flame (Warm orange)
        cairo_new_path(cr);
        cairo_move_to(cr, 0, -10 * flicker);
        cairo_curve_to(cr, 3.5, -5, 4.0, 1, 0, 1.5);
        cairo_curve_to(cr, -4.0, 1, -3.5, -5, 0, -10 * flicker);
        cairo_close_path(cr);
        cairo_pattern_t* fg = cairo_pattern_create_linear(0, -10, 0, 2);
        stop(fg, 0, rgba(239, 68, 68, 0.9));
        stop(fg, 0.4, rgba(245, 158, 11, 0.95));
        stop(fg, 0.8, rgba(254, 240, 138, 1.0));
        stop(fg, 1, rgba(59, 130, 246, 0.7)); // Blue base of flame
        cairo_set_source(cr, fg);
        cairo_fill(cr);
        cairo_pattern_destroy(fg);

        // Inner White Core
        cairo_new_path(cr);
        cairo_move_to(cr, 0, -5 * flicker);
        cairo_curve_to(cr, 1.6, -2, 1.8, 0.5, 0, 0.8);
        cairo_curve_to(cr, -1.8, 0.5, -1.6, -2, 0, -5 * flicker);
        cairo_close_path(cr);
        source(cr, rgb(0xffffff));
        cairo_fill(cr);
        cairo_restore(cr);

        // 4. Medieval Forged Iron Frame & Cage
        // Corner pillars & X-cross guards
        source(cr, rgb(0x27272a)); // Dark forged iron
        cairo_set_line_width(cr, 2.2);

        // Outer rectangular frame
        cairo_rectangle(cr, -12, -12, 24, 24);
        cairo_stroke(cr);

        // Diagonal gothic mullion crossbars on glass
        cairo_set_line_width(cr, 1.2);
        source(cr, rgb(0x3f3f46));
        cairo_move_to(cr, -12, -12); cairo_line_to(cr, 12, 12);
        cairo_move_to(cr, 12, -12);  cairo_line_to(cr, -12, 12);
        cairo_stroke(cr);

        // Iron Corner posts
        source(cr, rgb(0x18181b));
        cairo_rectangle(cr, -13, -13, 3, 26);
        cairo_rectangle(cr, 10, -13, 3, 26);
        cairo_fill(cr);

        // 5. Heavy Base Plate
        cairo_pattern_t* bpg = cairo_pattern_create_linear(-14, 0, 14, 0);
        stop(bpg, 0, rgb(0x18181b));
        stop(bpg, 0.5, rgb(0x3f3f46));
        stop(bpg, 1, rgb(0x18181b));
        cairo_set_source(cr, bpg);
        cairo_rectangle(cr, -14, 12, 28, 4.5);
        cairo_fill(cr);
        cairo_pattern_destroy(bpg);

        // Base feet
        source(cr, rgb(0x09090b));
        cairo_rectangle(cr, -13, 16.5, 4, 2.5);
        cairo_rectangle(cr, 9, 16.5, 4, 2.5);
        cairo_fill(cr);

        // 6. Pyramidal Domed Hood & Chimney Cap
        cairo_new_path(cr);
        cairo_move_to(cr, -14, -12);
        cairo_line_to(cr, -6, -20);
        cairo_line_to(cr, 6, -20);
        cairo_line_to(cr, 14, -12);
        cairo_close_path(cr);
        source(cr, rgb(0x27272a));
        cairo_fill_preserve(cr);
        source(cr, rgb(0x52525b));
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        // Vent slots on roof
        source(cr, rgb(0x09090b));
        cairo_rectangle(cr, -4, -17, 8, 1.8);
        cairo_fill(cr);

        // Smoke chimney finial
        source(cr, rgb(0x18181b));
        cairo_rectangle(cr, -3, -23, 6, 3.5);
        cairo_fill(cr);

        // 7. Forged Iron Hanging Ring (Where rope attaches)
        source(cr, rgb(0x71717a));
        cairo_set_line_width(cr, 2.2);
        circle(cr, 0, -27, 4.5);
        cairo_stroke(cr);
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, (44 + 45 * pet.body.burst) * pet.param("size", 1.0f));
    }
};

static struct LanternRegistrar {
    LanternRegistrar() {
        registerAccessory({"lantern", "Lanterna Medieval", rgb(0xf59e0b),
                           CoreConfig{60, 0.48f, 0.90f, 0.28f},
                           hints({{"orbitX", 38}, {"orbitY", -18}, {"sway", 4.0f}, {"lift", 0.05f}, {"kick", 0}, {"sparks", 0}}),
                           RopeStyle{rgb(0x27272a), rgb(0x71717a), 2.0f},
                           ParticleStyle{rgb(0xf59e0b), 2.2f, -0.04f}, // warm embers float up
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<Lantern>()});
    }
} s_lanternRegistrar;

}  // namespace mousepet
