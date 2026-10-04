#include "Common.hpp"

namespace mousepet {

class Star final : public Accessory {
    float rot_ = 0;
    unsigned tick_ = 0;

public:
    void update(Pet& pet) override {
        const Body& b = pet.body;
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        rot_ += (0.02f + sp * 0.01f + b.burst * 0.2f) * pet.param("spin", 1);
        if (sp > 1.5f && (++tick_ & 1)) {  // sparkle trail
            pet.emit(b.x, b.y, -b.vx * 0.15f + (pet.random() - 0.5f) * 0.6f,
                     -b.vy * 0.15f + (pet.random() - 0.5f) * 0.6f, 0.7f);
        }
    }
    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        cairo_translate(cr, b.x, b.y);
        glow(cr, 0, 0, 2, 26, {{0, rgba(253, 224, 71, 0.45)}, {1, rgba(253, 224, 71, 0)}});
        const double s = 1 + std::sin(pet.time * 5) * 0.06 + b.burst * 0.3;
        cairo_rotate(cr, rot_);
        cairo_scale(cr, s, s);

        const int n = int(std::lround(pet.param("points", 5)));
        cairo_new_path(cr);
        for (int i = 0; i < n * 2; i++) {
            const double r = (i % 2) ? 6.5 : 14, a = -PI / 2 + i * PI / n;
            if (i) cairo_line_to(cr, std::cos(a) * r, std::sin(a) * r);
            else cairo_move_to(cr, std::cos(a) * r, std::sin(a) * r);
        }
        cairo_close_path(cr);
        cairo_pattern_t* g = cairo_pattern_create_radial(0, 0, 1, 0, 0, 14);
        stop(g, 0, rgb(0xfffbeb));
        stop(g, 1, rgb(0xfacc15));
        cairo_set_source(cr, g);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(g);
        source(cr, rgb(0xfef9c3));
        cairo_set_line_width(cr, 1.2);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_stroke(cr);
    }
    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 28 * (1.1f + pet.body.burst * 0.3f));
    }
};

static struct StarRegistrar {
    StarRegistrar() {
        registerAccessory({"star", "Star", rgb(0xfde047), CoreConfig{60, 0.5f, 0.88f, 0.2f},
                           hints({{"orbitX", 34}, {"orbitY", -30}, {"sway", 5}, {"kick", 5}, {"tilt", 0}}),
                           RopeStyle{rgb(0x92710c), rgb(0xfde047), 2.2f}, ParticleStyle{rgb(0xfde047), 2.4f, 0},
                           {{"points", "Points", 4, 8, 1, 5}, {"spin", "Spin", 0, 3, 0.1f, 1}},
                           makeAccessory<Star>()});
    }
} s_starRegistrar;

}  // namespace mousepet
