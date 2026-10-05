#include "Common.hpp"

namespace mousepet {
class WindChime final : public Accessory {
    Chain clapper_{5};
    float ringAge_ = 100.f;
public:
    explicit WindChime(Pet& pet) { clapper_.reset(pet.body.x, pet.body.y + 8, pet.body.x, pet.body.y + 34); }
    void update(Pet&) override { ringAge_ = std::min(ringAge_ + 1.f, 100.f); }
    void onBurst(Pet& pet) override {
        ringAge_ = 0.f;
        // Kick only the secondary clapper, keeping the bell suspended.
        for (int i = 1; i < clapper_.n; i++) clapper_.px[i] -= 3.f;
        pet.emit(pet.body.x, pet.body.y + 3, 0.2f, -0.3f, 0.4f);
    }
    void lateUpdate(Pet& pet) override {
        const float a = pet.body.angle;
        clapper_.follow(pet.body.x - std::sin(a) * 6.f, pet.body.y + std::cos(a) * 6.f, 5.5f, 0.9f, 0.35f, 0.8f);
    }
    void render(cairo_t* cr, const Pet& pet) override {
        source(cr, rgb(0xfde68a)); cairo_set_line_width(cr, 1);
        cairo_new_path(cr); clapper_.trace(cr); cairo_stroke(cr);
        const float x = clapper_.x.back(), y = clapper_.y.back();
        source(cr, rgb(0x0d9488)); roundedRect(cr, x - 4, y - 2, 8, 14, 1); cairo_fill(cr);
        source(cr, rgb(0xccfbf1)); cairo_move_to(cr, x - 2, y + 3); cairo_line_to(cr, x + 2, y + 3);
        cairo_move_to(cr, x, y); cairo_line_to(cr, x, y + 8); cairo_stroke(cr);
        companionPose(cr, pet);
        if (ringAge_ < 40) {
            // Expanding sound arcs, rather than an explosion of particles.
            source(cr, rgba(94, 234, 212, (1 - ringAge_ / 40.f) * 0.8)); cairo_set_line_width(cr, 1);
            for (int i = 0; i < 2; i++) {
                const float r = 15 + ringAge_ * 0.45f + i * 6;
                cairo_new_path(cr); cairo_arc(cr, 0, 0, r, -0.5, 0.5);
                cairo_arc(cr, 0, 0, r, PI - 0.5, PI + 0.5); cairo_stroke(cr);
            }
        }
        cairo_pattern_t* brass = cairo_pattern_create_linear(-13, 0, 13, 0);
        stop(brass, 0, rgb(0x92400e)); stop(brass, 0.5, rgb(0xfde68a)); stop(brass, 1, rgb(0xb45309));
        cairo_set_source(cr, brass); cairo_move_to(cr, -14, 8);
        cairo_curve_to(cr, -7, 6, -12, -13, 0, -15);
        cairo_curve_to(cr, 12, -13, 7, 6, 14, 8); cairo_close_path(cr); cairo_fill(cr); cairo_pattern_destroy(brass);
        source(cr, rgb(0x78350f)); ellipse(cr, 0, 8, 14, 3); cairo_fill(cr);
        source(cr, rgb(0xfcd34d)); circle(cr, 0, -18, 3); cairo_stroke(cr);
        circle(cr, 0, 6, 2); cairo_fill(cr);
    }
    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 45 * pet.param("size", 1)); clapper_.addTo(b, 14);
    }
};
static struct WindChimeRegistrar { WindChimeRegistrar() {
    registerAccessory({"windchime", "Sino dos Ventos", rgb(0x5eead4), CoreConfig{70, 0.52f, 0.91f, 0.4f},
        hints({{"orbitX", 20}, {"orbitY", 52}, {"sway", 2}, {"tiltBase", -PI / 2}, {"kick", 0}, {"sparks", 0}, {"tilt", 0.6f}}),
        RopeStyle{rgb(0x92400e), rgb(0xfde68a), 1.4f}, ParticleStyle{rgb(0x99f6e4), 1.2f, 0},
        {{"size", "Size", 0.7f, 1.5f, 0.05f, 1}}, makeAccessory<WindChime>()});
}} s_windchime;
}
