#include "Common.hpp"

namespace mousepet {
class Satellite final : public Accessory {
    float panel_ = 0.f, panelVelocity_ = 0.f, signalAge_ = 100.f;
public:
    void update(Pet& pet) override {
        panelVelocity_ += -panel_ * 0.10f - pet.body.vx * 0.004f;
        panelVelocity_ *= 0.90f; panel_ = std::clamp(panel_ + panelVelocity_, -0.55f, 0.55f);
        signalAge_ = std::min(signalAge_ + 1.f, 100.f);
    }
    void onBurst(Pet&) override { panelVelocity_ += 0.14f; signalAge_ = 0.f; }
    void render(cairo_t* cr, const Pet& pet) override {
        companionPose(cr, pet);
        for (int side : {-1, 1}) {
            cairo_save(cr); cairo_translate(cr, side * 8, 0); cairo_rotate(cr, side * panel_);
            source(cr, rgb(0x94a3b8)); cairo_set_line_width(cr, 2);
            cairo_move_to(cr, 0, 0); cairo_line_to(cr, side * 9, 0); cairo_stroke(cr);
            const float left = side > 0 ? 7 : -30;
            source(cr, rgb(0x1e3a8a)); roundedRect(cr, left, -12, 23, 24, 1.2); cairo_fill(cr);
            source(cr, rgb(0x60a5fa)); cairo_set_line_width(cr, 0.75);
            for (int i = 0; i < 4; i++) {
                cairo_move_to(cr, left + 3 + i * 5, -11); cairo_line_to(cr, left + 3 + i * 5, 11);
            }
            for (int i = 0; i < 3; i++) {
                cairo_move_to(cr, left, -7 + i * 7); cairo_line_to(cr, left + 23, -7 + i * 7);
            }
            cairo_stroke(cr);
            source(cr, rgba(191, 219, 254, 0.55)); cairo_move_to(cr, left, -12);
            cairo_line_to(cr, left + 23, 1); cairo_line_to(cr, left + 23, -12); cairo_close_path(cr); cairo_fill(cr);
            cairo_restore(cr);
        }
        cairo_pattern_t* hull = cairo_pattern_create_linear(-8, 0, 8, 0);
        stop(hull, 0, rgb(0xa16207)); stop(hull, 0.5, rgb(0xfde68a)); stop(hull, 1, rgb(0xca8a04));
        cairo_set_source(cr, hull); roundedRect(cr, -8, -10, 16, 20, 2); cairo_fill(cr); cairo_pattern_destroy(hull);
        source(cr, rgb(0x475569)); cairo_rectangle(cr, -5, -7, 10, 3); cairo_fill(cr);
        source(cr, rgb(0x67e8f9)); circle(cr, 0, 3, 2); cairo_fill(cr);
        source(cr, rgb(0xe2e8f0)); cairo_set_line_width(cr, 1.2);
        cairo_move_to(cr, 0, -10); cairo_line_to(cr, 0, -20); cairo_stroke(cr);
        cairo_new_path(cr); cairo_arc(cr, 0, -20, 7, 0.2, PI - 0.2); cairo_stroke(cr);
        circle(cr, 0, -22, 1.2); cairo_fill(cr);
        if (signalAge_ < 45.f) {
            source(cr, rgba(125, 211, 252, 1.f - signalAge_ / 45.f)); cairo_set_line_width(cr, 1);
            for (int i = 0; i < 2; i++) {
                cairo_new_path(cr); cairo_arc(cr, 0, -19, 10 + signalAge_ * 0.4f + i * 5, PI * 1.2, PI * 1.8); cairo_stroke(cr);
            }
        }
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 57 * pet.param("size", 1)); }
};
static struct SatelliteRegistrar { SatelliteRegistrar() {
    registerAccessory({"satellite", "Satélite Orbital", rgb(0x60a5fa), CoreConfig{78, 0.36f, 0.96f, 0.05f},
        hints({{"orbitX", 48}, {"orbitY", -30}, {"sway", 3}, {"tilt", 0.18f}, {"tiltVel", 0.018f}, {"kick", 0}, {"sparks", 0}}),
        RopeStyle{rgb(0x1e40af), rgb(0x93c5fd), 1.2f}, ParticleStyle{rgb(0x93c5fd), 1, 0},
        {{"size", "Size", 0.7f, 1.5f, 0.05f, 1}}, makeAccessory<Satellite>()});
}} s_satellite;
}
