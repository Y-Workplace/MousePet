#include "Common.hpp"

namespace mousepet {
class Hourglass final : public Accessory {
    float sand_ = 0.f, flip_ = 0.f;
public:
    void update(Pet& pet) override {
        sand_ = std::min(sand_ + pet.param("flow", 1) * 0.003f, 1.f);
        flip_ += (0.f - flip_) * 0.11f;
    }
    void onBurst(Pet&) override { sand_ = 0.f; flip_ = PI; }
    void render(cairo_t* cr, const Pet& pet) override {
        companionPose(cr, pet, flip_);
        // Glass waists and falling golden grains; a click flips and refills it.
        auto chamber = [&]() {
            cairo_new_path(cr); cairo_move_to(cr, -11, -18); cairo_line_to(cr, 11, -18);
            cairo_curve_to(cr, 12, -7, 3, -4, 2, 0);
            cairo_curve_to(cr, 3, 4, 12, 7, 11, 18); cairo_line_to(cr, -11, 18);
            cairo_curve_to(cr, -12, 7, -3, 4, -2, 0);
            cairo_curve_to(cr, -3, -4, -12, -7, -11, -18); cairo_close_path(cr);
        };
        chamber(); source(cr, rgba(165, 243, 252, 0.18)); cairo_fill(cr);
        cairo_save(cr); chamber(); cairo_clip(cr);
        source(cr, rgb(0xfbbf24));
        cairo_move_to(cr, -11, -17 + sand_ * 16); cairo_line_to(cr, 11, -17 + sand_ * 16);
        cairo_line_to(cr, 0, -1); cairo_close_path(cr); cairo_fill(cr);
        const float pile = 2 + sand_ * 13;
        cairo_move_to(cr, -12, 18); cairo_line_to(cr, 0, 18 - pile); cairo_line_to(cr, 12, 18);
        cairo_close_path(cr); cairo_fill(cr);
        if (sand_ < 1.f) {
            source(cr, rgb(0xfef3c7)); cairo_set_line_width(cr, 1);
            cairo_move_to(cr, 0, 0); cairo_line_to(cr, 0, 18 - pile); cairo_stroke(cr);
            circle(cr, 1, std::fmod(pet.time * 15, 15.f), 0.7); cairo_fill(cr);
        }
        cairo_restore(cr); chamber(); source(cr, rgba(207, 250, 254, 0.8)); cairo_set_line_width(cr, 1.1); cairo_stroke(cr);
        source(cr, rgb(0x92400e));
        for (float x : {-14.f, 12.f}) { roundedRect(cr, x, -20, 2, 40, 0.9); cairo_fill(cr); }
        for (float y : {-23.f, 18.f}) {
            source(cr, rgb(0xb45309)); roundedRect(cr, -17, y, 34, 5, 2); cairo_fill(cr);
            source(cr, rgb(0xfcd34d)); cairo_rectangle(cr, -13, y + 1, 26, 1); cairo_fill(cr);
        }
        glow(cr, 0, 0, 0, 28, {{0, rgba(251, 191, 36, 0.16)}, {1, rgba(251, 191, 36, 0)}}, CAIRO_OPERATOR_SCREEN, pet.body.burst);
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 42 * pet.param("size", 1)); }
};
static struct HourglassRegistrar { HourglassRegistrar() {
    registerAccessory({"hourglass", "Ampulheta Flutuante", rgb(0xfcd34d), CoreConfig{65, 0.55f, 0.90f, 0.26f},
        hints({{"orbitX", 26}, {"orbitY", 42}, {"sway", 3}, {"tiltBase", -PI / 2}, {"kick", 0}, {"sparks", 0}, {"tilt", 0.25f}}),
        RopeStyle{rgb(0x92400e), rgb(0xfcd34d), 1.8f}, ParticleStyle{rgb(0xfcd34d), 1, 0.03f},
        {{"size", "Size", 0.7f, 1.5f, 0.05f, 1}, {"flow", "Sand flow", 0.4f, 2, 0.1f, 1}}, makeAccessory<Hourglass>()});
}} s_hourglass;
}
