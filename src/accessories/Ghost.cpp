#include "Common.hpp"

namespace mousepet {

class Ghost final : public Accessory {
    float frightAge_ = 100.f;
public:
    void onBurst(Pet& pet) override {
        frightAge_ = 0.f;
        pet.body.vx += (pet.mouse.x < pet.body.x ? 1.f : -1.f) * 2.8f;
    }
    void update(Pet&) override { frightAge_ = std::min(frightAge_ + 1.f, 100.f); }
    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body; const Mouse& m = pet.mouse;
        cairo_translate(cr, b.x, b.y);
        glow(cr, 0, 0, 4, 28, {{0, rgba(226, 232, 240, 0.28)}, {1, rgba(226, 232, 240, 0)}});

        const float shiver = std::sin(frightAge_ * 1.3f) * std::exp(-frightAge_ * 0.10f);
        cairo_rotate(cr, b.angle + shiver * 0.12f);
        // Startled stretch gives way to a little shiver instead of uniform growth.
        cairo_scale(cr, 1 - b.burst * 0.16, 1 + b.burst * 0.28);

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
    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 30 * (1 + pet.body.burst * 0.3f));
    }
};

static struct GhostRegistrar {
    GhostRegistrar() {
        registerAccessory({"ghost", "Ghost", rgb(0xe2e8f0), CoreConfig{60, 0.4f, 0.91f, 0.2f},
                           hints({{"orbitX", 42}, {"orbitY", -22}, {"sway", 7}, {"tilt", 0}, {"tiltVel", 0.05f}, {"kick", 2.5f}, {"sparks", 0}}),
                           RopeStyle{rgb(0x64748b), rgb(0xe2e8f0), 2.2f}, ParticleStyle{rgb(0xcbd5e1), 2, 0},
                           {{"opacity", "Opacity", 0.3f, 1, 0.05f, 0.85f}},
                           makeAccessory<Ghost>()});
    }
} s_ghostRegistrar;

}  // namespace mousepet
