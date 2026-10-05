#include "Common.hpp"

namespace mousepet {
class Airship final : public Accessory {
    float propeller_ = 0.f, boostAge_ = 100.f;
public:
    void update(Pet& pet) override {
        propeller_ = std::fmod(propeller_ + 0.18f + std::abs(pet.body.vx) * 0.25f + pet.body.burst * 0.8f, float(TAU));
        boostAge_ = std::min(boostAge_ + 1.f, 100.f);
        pet.body.vy += std::sin(pet.time * 1.8f) * 0.025f;
    }
    void onBurst(Pet& pet) override {
        boostAge_ = 0.f;
        pet.body.vx += 4.f;
        for (int i = 0; i < 9; i++)
            pet.emit(pet.body.x - 28.f, pet.body.y + 10.f, -1.5f - pet.random(), -pet.random() * 0.5f, 0.8f);
    }
    void render(cairo_t* cr, const Pet& pet) override {
        companionPose(cr, pet, dampedMotion(boostAge_, 0.2f, 0.08f) * 0.1f);
        // Stitched copper envelope, brass ribs and a suspended clockwork cabin.
        cairo_pattern_t* skin = cairo_pattern_create_linear(0, -19, 0, 13);
        stop(skin, 0, rgb(0xfde68a)); stop(skin, 0.4, rgb(0xd97706)); stop(skin, 1, rgb(0x78350f));
        cairo_set_source(cr, skin); ellipse(cr, 0, -3, 29, 15); cairo_fill(cr);
        cairo_pattern_destroy(skin);
        source(cr, rgb(0xfbbf24)); cairo_set_line_width(cr, 1.1);
        for (float x : {-16.f, -6.f, 6.f, 16.f}) { ellipse(cr, x, -3, 5, 13); cairo_stroke(cr); }
        source(cr, rgb(0x451a03));
        cairo_move_to(cr, -12, 9); cairo_line_to(cr, -9, 18);
        cairo_move_to(cr, 12, 9); cairo_line_to(cr, 9, 18); cairo_stroke(cr);
        roundedRect(cr, -13, 15, 26, 10, 3); cairo_fill(cr);
        source(cr, rgb(0x67e8f9));
        for (int i = 0; i < 3; i++) { circle(cr, -7 + i * 7, 19, 2.1); cairo_fill(cr); }
        source(cr, rgb(0xfcd34d)); circle(cr, 8, 25, 3); cairo_stroke(cr);
        // Tail fins and a speed-driven propeller.
        source(cr, rgb(0x92400e)); cairo_move_to(cr, -23, -5); cairo_line_to(cr, -35, -14);
        cairo_line_to(cr, -32, 2); cairo_close_path(cr); cairo_fill(cr);
        cairo_save(cr); cairo_translate(cr, -20, 19); cairo_rotate(cr, propeller_);
        source(cr, rgb(0xfde68a)); ellipse(cr, 0, 0, 1.5, 7); cairo_fill(cr);
        source(cr, rgb(0x94a3b8)); circle(cr, 0, 0, 2); cairo_fill(cr); cairo_restore(cr);
        source(cr, rgba(255, 251, 235, 0.6)); ellipse(cr, 9, -11, 10, 2); cairo_fill(cr);
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 46 * pet.param("size", 1)); }
};
static struct AirshipRegistrar { AirshipRegistrar() {
    registerAccessory({"airship", "Dirigível Steampunk", rgb(0xfbbf24), CoreConfig{80, 0.32f, 0.94f, 0.10f},
        hints({{"orbitX", 30}, {"orbitY", -66}, {"lift", -0.06f}, {"sway", 5}, {"tilt", 0.22f}, {"kick", 0}, {"sparks", 0}}),
        RopeStyle{rgb(0x92400e), rgb(0xfde68a), 1.5f}, ParticleStyle{rgb(0xd6d3d1), 1.7f, -0.02f},
        {{"size", "Size", 0.7f, 1.5f, 0.05f, 1}}, makeAccessory<Airship>()});
}} s_airship;
}
