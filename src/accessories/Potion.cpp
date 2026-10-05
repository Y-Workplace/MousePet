#include "Common.hpp"

namespace mousepet {
class Potion final : public Accessory {
    float slosh_ = 0.f, sloshVelocity_ = 0.f, bubble_ = 0.f;
public:
    void update(Pet& pet) override {
        sloshVelocity_ += -slosh_ * 0.09f - pet.body.vx * 0.005f;
        sloshVelocity_ *= 0.88f; slosh_ = std::clamp(slosh_ + sloshVelocity_, -0.6f, 0.6f);
        bubble_ = std::fmod(bubble_ + 0.035f + pet.body.burst * 0.10f, 1.f);
    }
    void onBurst(Pet& pet) override {
        sloshVelocity_ += 0.18f;
        for (int i = 0; i < 8; i++) pet.emit(pet.body.x, pet.body.y - 24, (pet.random() - 0.5f), -1.f - pet.random(), 0.7f);
    }
    void render(cairo_t* cr, const Pet& pet) override {
        companionPose(cr, pet);
        glow(cr, 0, 8, 0, 28, {{0, rgba(192, 132, 252, 0.35)}, {1, rgba(168, 85, 247, 0)}}, CAIRO_OPERATOR_SCREEN, 0.8 + pet.body.burst);
        // A sealed round-bottom flask; clipping keeps the sloshing liquid inside.
        auto glass = [&]() {
            cairo_new_path(cr); cairo_move_to(cr, -6, -20); cairo_line_to(cr, -6, -9);
            cairo_curve_to(cr, -27, 0, -19, 25, 0, 25);
            cairo_curve_to(cr, 19, 25, 27, 0, 6, -9); cairo_line_to(cr, 6, -20); cairo_close_path(cr);
        };
        glass(); source(cr, rgba(186, 230, 253, 0.18)); cairo_fill(cr);
        cairo_save(cr); glass(); cairo_clip(cr);
        cairo_pattern_t* liquid = cairo_pattern_create_linear(0, 0, 0, 25);
        stop(liquid, 0, rgb(0xc084fc)); stop(liquid, 1, rgb(0x6d28d9)); cairo_set_source(cr, liquid);
        cairo_move_to(cr, -24, 5 - slosh_ * 18);
        cairo_curve_to(cr, -8, 5 - slosh_ * 10, 8, 5 + slosh_ * 10, 24, 5 + slosh_ * 18);
        cairo_line_to(cr, 24, 27); cairo_line_to(cr, -24, 27); cairo_close_path(cr); cairo_fill(cr);
        cairo_pattern_destroy(liquid);
        source(cr, rgba(245, 208, 254, 0.8));
        for (int i = 0; i < 3; i++) {
            const float t = std::fmod(bubble_ + i * 0.31f, 1.f);
            circle(cr, -7 + i * 7, 22 - t * 20, 1 + t); cairo_set_line_width(cr, 0.8); cairo_stroke(cr);
        }
        cairo_restore(cr); glass(); source(cr, rgba(224, 242, 254, 0.8)); cairo_set_line_width(cr, 1.4); cairo_stroke(cr);
        source(cr, rgb(0xa16207)); roundedRect(cr, -7, -25, 14, 7, 2); cairo_fill(cr);
        source(cr, rgb(0x78350f)); cairo_rectangle(cr, -7, -21, 14, 2); cairo_fill(cr);
        source(cr, rgba(255, 255, 255, 0.65)); ellipse(cr, -10, 3, 2, 7, 0.3); cairo_fill(cr);
        source(cr, rgb(0xfde68a)); cairo_set_line_width(cr, 1);
        circle(cr, 0, 13, 4); cairo_stroke(cr); cairo_move_to(cr, -2, 13); cairo_line_to(cr, 2, 13); cairo_stroke(cr);
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 43 * pet.param("size", 1)); }
};
static struct PotionRegistrar { PotionRegistrar() {
    registerAccessory({"potion", "Frasco de Alquimia", rgb(0xc084fc), CoreConfig{70, 0.55f, 0.88f, 0.42f},
        hints({{"orbitX", 24}, {"orbitY", 52}, {"lift", 0.09f}, {"sway", 2}, {"tiltBase", -PI / 2}, {"kick", 0}, {"sparks", 0}, {"tilt", 0.5f}}),
        RopeStyle{rgb(0x7c3aed), rgb(0xe9d5ff), 1.8f}, ParticleStyle{rgb(0xe9d5ff), 1.3f, -0.04f},
        {{"size", "Size", 0.7f, 1.5f, 0.05f, 1}}, makeAccessory<Potion>()});
}} s_potion;
}
