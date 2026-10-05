#include "Common.hpp"

namespace mousepet {
class Battery final : public Accessory {
    float pulseAge_ = 100.f, charge_ = 0.6f;
public:
    void update(Pet& pet) override {
        pulseAge_ = std::min(pulseAge_ + 1.f, 100.f);
        charge_ += (0.6f - charge_) * 0.025f;
        if (pulseAge_ < 20.f && int(pulseAge_) % 4 == 0)
            pet.emit(pet.body.x, pet.body.y, (pet.random() - 0.5f) * 2.f, -pet.random(), 0.4f);
    }
    void onBurst(Pet&) override { charge_ = 1.f; pulseAge_ = 0.f; }
    void render(cairo_t* cr, const Pet& pet) override {
        companionPose(cr, pet, dampedMotion(pulseAge_, 0.8f, 0.16f) * 0.06f);
        const Color energy = hsl(pet.param("hue", 165), 85, 65);
        glow(cr, 0, 0, 0, 31, {{0, {energy.r, energy.g, energy.b, 0.4}}, {1, {energy.r, energy.g, energy.b, 0}}}, CAIRO_OPERATOR_SCREEN, charge_);
        source(cr, rgb(0x1e293b)); roundedRect(cr, -12, -22, 24, 44, 5); cairo_fill(cr);
        source(cr, rgb(0x94a3b8)); roundedRect(cr, -9, -24, 18, 6, 2); roundedRect(cr, -9, 18, 18, 6, 2); cairo_fill(cr);
        source(cr, rgb(0x0f172a)); roundedRect(cr, -8, -15, 16, 30, 2); cairo_fill(cr);
        for (int i = 0; i < 5; i++) {
            source(cr, energy, (i < charge_ * 5 ? 0.95 : 0.15));
            roundedRect(cr, -6, 10 - i * 5, 12, 3, 0.7); cairo_fill(cr);
        }
        source(cr, rgb(0xf8fafc)); cairo_move_to(cr, 1, -9); cairo_line_to(cr, -4, 0);
        cairo_line_to(cr, 0, 0); cairo_line_to(cr, -1, 8); cairo_line_to(cr, 5, -2);
        cairo_line_to(cr, 1, -2); cairo_close_path(cr); cairo_fill(cr);
        source(cr, rgb(0x64748b)); cairo_set_line_width(cr, 1);
        for (float x : {-10.f, 10.f}) { cairo_move_to(cr, x, -10); cairo_line_to(cr, x, 10); } cairo_stroke(cr);
        if (pulseAge_ < 25.f) {
            // A square electrical discharge crawls along the casing.
            source(cr, energy, 1.f - pulseAge_ / 25.f); cairo_set_line_width(cr, 1.2);
            cairo_move_to(cr, -16, -16); cairo_line_to(cr, -19, -8); cairo_line_to(cr, -15, -3);
            cairo_line_to(cr, -20, 5); cairo_line_to(cr, -16, 14); cairo_stroke(cr);
            cairo_move_to(cr, 16, -14); cairo_line_to(cr, 20, -5); cairo_line_to(cr, 16, 2);
            cairo_line_to(cr, 19, 12); cairo_stroke(cr);
        }
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 44 * pet.param("size", 1)); }
};
static struct BatteryRegistrar { BatteryRegistrar() {
    registerAccessory({"battery", "Célula de Energia", rgb(0x6ee7b7), CoreConfig{65, 0.58f, 0.89f, 0.3f},
        hints({{"orbitX", 32}, {"orbitY", 38}, {"sway", 2}, {"tiltBase", -PI / 2}, {"tilt", 0.45f}, {"kick", 0}, {"sparks", 0}}),
        RopeStyle{rgb(0x334155), rgb(0x34d399), 2.8f}, ParticleStyle{rgb(0x6ee7b7), 1.1f, 0},
        {{"size", "Size", 0.7f, 1.5f, 0.05f, 1}, {"hue", "Energy hue", 0, 360, 5, 165}}, makeAccessory<Battery>()});
}} s_battery;
}
