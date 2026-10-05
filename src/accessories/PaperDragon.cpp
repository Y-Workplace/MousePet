#include "Common.hpp"

namespace mousepet {
class PaperDragon final : public Accessory {
    Chain tail_{8};
    float wing_ = 0.f, swoopAge_ = 100.f;
public:
    explicit PaperDragon(Pet& pet) { tail_.reset(pet.body.x, pet.body.y, pet.body.x - 35, pet.body.y + 16); }
    void update(Pet& pet) override {
        wing_ += 0.14f + pet.mouse.speed * 0.018f + pet.body.burst * 0.25f;
        swoopAge_ = std::min(swoopAge_ + 1.f, 100.f);
        if (swoopAge_ < 30.f) {
            pet.body.vx += std::cos(swoopAge_ * 0.2f) * pet.body.burst * 0.32f;
            pet.body.vy -= std::sin(swoopAge_ * 0.2f) * pet.body.burst * 0.22f;
        }
    }
    void onBurst(Pet& pet) override { swoopAge_ = 0.f; pet.body.vx += 3.f; }
    void lateUpdate(Pet& pet) override {
        const float a = pet.body.angle, c = std::cos(a), s = std::sin(a);
        tail_.follow(pet.body.x - c * 11.f, pet.body.y - s * 11.f, 5.f, 0.86f, 0.10f, 0.8f);
    }
    void render(cairo_t* cr, const Pet& pet) override {
        source(cr, rgb(0xf97316)); cairo_set_line_width(cr, 3);
        cairo_new_path(cr); tail_.trace(cr); cairo_stroke(cr);
        for (int i = 2; i < tail_.n; i += 2) {
            source(cr, rgb(0xfde68a)); circle(cr, tail_.x[i], tail_.y[i] - 2, 2); cairo_fill(cr);
        }
        companionPose(cr, pet);
        const float flap = std::sin(wing_) * 7;
        // Origami wings: angular silhouettes, fold seams and alternating paper faces.
        source(cr, rgb(0xef4444)); cairo_move_to(cr, -7, 0); cairo_line_to(cr, -19, -18 - flap);
        cairo_line_to(cr, 4, -7); cairo_close_path(cr); cairo_fill(cr);
        source(cr, rgb(0xfca5a5)); cairo_move_to(cr, -7, 0); cairo_line_to(cr, -19, -18 - flap);
        cairo_line_to(cr, -8, -9); cairo_close_path(cr); cairo_fill(cr);
        source(cr, rgb(0xfb923c)); cairo_move_to(cr, -12, 3); cairo_line_to(cr, 0, -7);
        cairo_line_to(cr, 14, -2); cairo_line_to(cr, 4, 9); cairo_close_path(cr); cairo_fill(cr);
        source(cr, rgb(0xfde68a)); cairo_move_to(cr, 1, 0); cairo_line_to(cr, 12, -23 + flap);
        cairo_line_to(cr, 17, -2); cairo_close_path(cr); cairo_fill(cr);
        source(cr, rgb(0xf97316)); cairo_move_to(cr, 11, -2); cairo_line_to(cr, 18, -12);
        cairo_line_to(cr, 27, -5); cairo_line_to(cr, 21, 3); cairo_close_path(cr); cairo_fill(cr);
        source(cr, rgb(0x7f1d1d)); cairo_set_line_width(cr, 1);
        cairo_move_to(cr, -12, 3); cairo_line_to(cr, 4, 1); cairo_line_to(cr, 14, -2); cairo_stroke(cr);
        circle(cr, 21, -5, 1.4); cairo_fill(cr);
        source(cr, rgb(0xfffbeb)); cairo_move_to(cr, 17, -10); cairo_line_to(cr, 15, -16);
        cairo_line_to(cr, 21, -11); cairo_close_path(cr); cairo_fill(cr);
        if (pet.body.burst > 0.06f) {
            // A short paper-fire ribbon from the snout, distinct from ambient sparks.
            source(cr, rgba(251, 191, 36, pet.body.burst));
            cairo_move_to(cr, 26, -4); cairo_line_to(cr, 35 + pet.body.burst * 8, -8);
            cairo_line_to(cr, 32, -3); cairo_line_to(cr, 39, 1);
            cairo_line_to(cr, 26, -1); cairo_close_path(cr); cairo_fill(cr);
        }
    }
    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 49 * pet.param("size", 1)); tail_.addTo(b, 5);
    }
};
static struct PaperDragonRegistrar { PaperDragonRegistrar() {
    registerAccessory({"paperdragon", "Dragão de Papel", rgb(0xf97316), CoreConfig{75, 0.42f, 0.94f, 0.14f},
        hints({{"orbitX", 48}, {"orbitY", -22}, {"pull", 0.015f}, {"sway", 7}, {"kick", 0}, {"sparks", 0}, {"tilt", 0.3f}}),
        RopeStyle{rgb(0xdc2626), rgb(0xfef08a), 1.6f}, ParticleStyle{rgb(0xfbbf24), 1.4f, 0},
        {{"size", "Size", 0.7f, 1.5f, 0.05f, 1}}, makeAccessory<PaperDragon>()});
}} s_paperdragon;
}
