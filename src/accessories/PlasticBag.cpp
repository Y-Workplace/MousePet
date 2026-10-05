#include "Common.hpp"

namespace mousepet {
class PlasticBag final : public Accessory {
    float puff_ = 0.f, flutter_ = 0.f, gustAge_ = 100.f;
public:
    void update(Pet& pet) override {
        gustAge_ = std::min(gustAge_ + 1.f, 100.f);
        const float target = std::clamp(pet.mouse.speed * 0.055f + pet.body.burst * 0.8f, 0.f, 1.f);
        puff_ += (target - puff_) * 0.1f;
        flutter_ += 0.12f + pet.mouse.speed * 0.02f;
        pet.body.vy -= puff_ * 0.10f;
    }
    void onBurst(Pet& pet) override { gustAge_ = 0.f; pet.body.vx += 2.f; }
    void render(cairo_t* cr, const Pet& pet) override {
        companionPose(cr, pet, dampedMotion(gustAge_, 0.25f, 0.06f) * 0.25f);
        const float width = 13 + puff_ * 5, ripple = std::sin(flutter_) * (1 + puff_ * 2);
        // Transparent crumpled shopping bag with open handles, never a diamond kite.
        source(cr, rgba(226, 232, 240, 0.35));
        cairo_move_to(cr, -width, -10); cairo_line_to(cr, -width + 1, -23);
        cairo_curve_to(cr, -7, -28, -4, -17, -5, -10);
        cairo_line_to(cr, 5, -10); cairo_curve_to(cr, 4, -17, 7, -28, width - 1, -23);
        cairo_line_to(cr, width, -10);
        cairo_curve_to(cr, width + 3, 1, width + ripple, 13, width - 3, 19);
        cairo_curve_to(cr, 5, 24 + ripple, -7, 19 - ripple, -width + 2, 20);
        cairo_curve_to(cr, -width - 3, 8, -width - 2, 0, -width, -10); cairo_close_path(cr); cairo_fill_preserve(cr);
        source(cr, rgba(241, 245, 249, 0.8)); cairo_set_line_width(cr, 0.9); cairo_stroke(cr);
        source(cr, rgba(148, 163, 184, 0.65));
        cairo_move_to(cr, -width + 3, -7); cairo_line_to(cr, -7 + ripple, 8); cairo_line_to(cr, -width + 4, 17);
        cairo_move_to(cr, width - 3, -7); cairo_line_to(cr, 8, 4); cairo_line_to(cr, width - 4, 17);
        cairo_move_to(cr, -4, -5); cairo_line_to(cr, 1 + ripple, 14); cairo_stroke(cr);
        // Faded green shop emblem.
        source(cr, rgba(16, 185, 129, 0.6)); cairo_set_line_width(cr, 1.5);
        circle(cr, 0, 5, 5); cairo_stroke(cr); cairo_move_to(cr, -2, 5); cairo_line_to(cr, 0, 8);
        cairo_line_to(cr, 3, 2); cairo_stroke(cr);
        source(cr, rgba(255, 255, 255, 0.35)); ellipse(cr, -width + 5, 1, 2, 9, -0.2); cairo_fill(cr);
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 39 * pet.param("size", 1)); }
};
static struct PlasticBagRegistrar { PlasticBagRegistrar() {
    registerAccessory({"plasticbag", "Sacola Voadora", rgb(0xcbd5e1), CoreConfig{75, 0.30f, 0.96f, 0.10f},
        hints({{"orbitX", 30}, {"orbitY", -52}, {"lift", -0.025f}, {"pull", 0.014f}, {"sway", 8}, {"tilt", 0.4f}, {"kick", 0}, {"sparks", 0}}),
        RopeStyle{rgba(226, 232, 240, 0.5), rgba(255, 255, 255, 0.7), 1}, ParticleStyle{rgb(0xcbd5e1), 1, 0},
        {{"size", "Size", 0.7f, 1.5f, 0.05f, 1}}, makeAccessory<PlasticBag>()});
}} s_plasticbag;
}
