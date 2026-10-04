#include "Common.hpp"

namespace mousepet {

class Balloon final : public Accessory {
public:
    void update(Pet& pet) override { pet.body.vy += std::sin(pet.time * 3) * 0.02f; }  // floats in the air
    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1), hue = pet.param("hue", 350);
        const double sq = b.burst * 0.12;
        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle);
        cairo_scale(cr, sz * (1 - sq), sz * (1 + sq * 1.5));

        source(cr, hsl(hue, 70, 38));  // knot
        cairo_new_path(cr);
        cairo_move_to(cr, 0, 16); cairo_line_to(cr, -4, 23); cairo_line_to(cr, 4, 23); cairo_close_path(cr);
        cairo_fill(cr);

        cairo_pattern_t* g = cairo_pattern_create_radial(-6, -8, 2, 0, 0, 22);
        stop(g, 0, hsl(hue, 90, 80));
        stop(g, 0.5, hsl(hue, 80, 58));
        stop(g, 1, hsl(hue, 75, 38));
        cairo_set_source(cr, g);
        ellipse(cr, 0, -2, 15, 19);
        cairo_fill(cr);
        cairo_pattern_destroy(g);

        cairo_set_source_rgba(cr, 1, 1, 1, 0.55);  // highlight
        ellipse(cr, -6, -10, 3, 5.5, -0.5);
        cairo_fill(cr);
    }
    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 30 * pet.param("size", 1) * 1.2f);
    }
};

static struct BalloonRegistrar {
    BalloonRegistrar() {
        registerAccessory({"balloon", "Balloon", rgb(0xfb7185), CoreConfig{80, 0.35f, 0.92f, 0.22f},
                           hints({{"orbitX", 12}, {"orbitY", -70}, {"sway", 6}, {"kick", 3}, {"sparks", 14}}),
                           RopeStyle{rgb(0xcbd5e1), rgb(0xf8fafc), 1.8f}, ParticleStyle{rgb(0xfda4af), 2.2f, 0.04f},
                           {{"hue", "Hue", 0, 360, 5, 350}, {"size", "Size", 0.7f, 1.6f, 0.05f, 1}},
                           makeAccessory<Balloon>()});
    }
} s_balloonRegistrar;

}  // namespace mousepet
