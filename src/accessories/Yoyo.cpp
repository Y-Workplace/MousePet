#include "Common.hpp"

namespace mousepet {
class Yoyo final : public Accessory {
    float spin_ = 0.f, dropAge_ = 100.f;
public:
    void update(Pet& pet) override {
        dropAge_ = std::min(dropAge_ + 1.f, 100.f);
        spin_ = std::fmod(spin_ + pet.body.vy * 0.07f + pet.body.burst * 0.5f, float(TAU));
        // A downward throw followed by a return tug on the elastic string.
        if (dropAge_ == 14.f) pet.body.vy -= 7.f;
    }
    void onBurst(Pet& pet) override { dropAge_ = 0.f; pet.body.vy += 7.f; }
    void render(cairo_t* cr, const Pet& pet) override {
        companionPose(cr, pet);
        source(cr, rgb(0xbe185d)); ellipse(cr, 2, 1, 17, 18); cairo_fill(cr);
        source(cr, rgb(0xf472b6)); ellipse(cr, -2, -1, 16, 18); cairo_fill(cr);
        source(cr, rgb(0x9d174d)); ellipse(cr, -2, -1, 12, 14); cairo_fill(cr);
        cairo_save(cr); cairo_translate(cr, -2, -1); cairo_rotate(cr, spin_);
        for (int i = 0; i < 6; i++) {
            const float a = i * TAU / 6;
            source(cr, i % 2 ? rgb(0xfde047) : rgb(0xfbcfe8));
            cairo_move_to(cr, 0, 0); cairo_line_to(cr, std::cos(a) * 11, std::sin(a) * 12);
            cairo_line_to(cr, std::cos(a + 0.45f) * 11, std::sin(a + 0.45f) * 12);
            cairo_close_path(cr); cairo_fill(cr);
        }
        cairo_restore(cr);
        source(cr, rgb(0xe2e8f0)); circle(cr, -2, -1, 3); cairo_fill(cr);
        source(cr, rgb(0x64748b)); circle(cr, -2, -1, 1.1); cairo_fill(cr);
        source(cr, rgba(255, 255, 255, 0.65)); cairo_set_line_width(cr, 1.5);
        cairo_new_path(cr); cairo_arc(cr, -2, -1, 14, PI * 1.1, PI * 1.6); cairo_stroke(cr);
        if (dropAge_ < 28.f) {
            source(cr, rgba(251, 207, 232, (1 - dropAge_ / 28.f) * 0.6));
            cairo_set_line_width(cr, 1);
            for (float x : {-22.f, 22.f}) { cairo_move_to(cr, x, -10); cairo_line_to(cr, x, 8); } cairo_stroke(cr);
        }
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 31 * pet.param("size", 1)); }
};
static struct YoyoRegistrar { YoyoRegistrar() {
    registerAccessory({"yoyo", "Ioiô Desgovernado", rgb(0xf472b6), CoreConfig{88, 0.28f, 0.94f, 0.5f},
        hints({{"orbitX", 16}, {"orbitY", 72}, {"pull", 0.018f}, {"sway", 1}, {"maxStretch", 1.8f}, {"tilt", 0.1f}, {"tiltBase", -PI / 2}, {"kick", 0}, {"sparks", 0}}),
        RopeStyle{rgb(0xf8fafc), rgb(0xf9a8d4), 1.3f}, ParticleStyle{rgb(0xfbcfe8), 1, 0},
        {{"size", "Size", 0.7f, 1.5f, 0.05f, 1}}, makeAccessory<Yoyo>()});
}} s_yoyo;
}
