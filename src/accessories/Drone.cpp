#include "Common.hpp"

namespace mousepet {
class Drone final : public Accessory {
    float scan_ = 0.f, recoil_ = 0.f;
public:
    void update(Pet& pet) override {
        scan_ += 0.05f; recoil_ *= 0.88f;
        // Hover stabilization opposes vertical drift with weak local thrust.
        pet.body.vy -= std::clamp(pet.body.vy * 0.02f, -0.12f, 0.12f);
    }
    void onBurst(Pet& pet) override {
        recoil_ = 1.f;
        const float a = std::atan2(pet.mouse.y - pet.body.y, pet.mouse.x - pet.body.x);
        pet.body.vx -= std::cos(a) * 2.f; pet.body.vy -= std::sin(a) * 2.f;
    }
    void render(cairo_t* cr, const Pet& pet) override {
        companionPose(cr, pet);
        const float dx = pet.mouse.x - pet.body.x, dy = pet.mouse.y - pet.body.y;
        const float d = std::max(std::hypot(dx, dy), 1.f);
        const float eyeX = std::clamp(dx / d * 4, -4.f, 4.f), eyeY = std::clamp(dy / d * 3, -3.f, 3.f);
        // Twin stabilizers with blue jets responding to inertial drift.
        source(cr, rgb(0x334155)); roundedRect(cr, -23, -3, 9, 9, 2); roundedRect(cr, 14, -3, 9, 9, 2); cairo_fill(cr);
        for (float x : {-19.f, 19.f}) {
            const float jet = 5 + std::min(std::abs(pet.body.vy), 8.f) * 0.6f;
            glow(cr, x, 9, 0, jet, {{0, rgba(125, 211, 252, 0.8)}, {1, rgba(56, 189, 248, 0)}}, CAIRO_OPERATOR_SCREEN, 0.8);
        }
        cairo_pattern_t* shell = cairo_pattern_create_radial(-7, -9, 1, 0, 0, 18);
        stop(shell, 0, rgb(0xf8fafc)); stop(shell, 0.65, rgb(0xcbd5e1)); stop(shell, 1, rgb(0x475569));
        cairo_set_source(cr, shell); circle(cr, 0, 0, 16); cairo_fill(cr); cairo_pattern_destroy(shell);
        source(cr, rgb(0x0f172a)); ellipse(cr, eyeX, eyeY, 10, 9); cairo_fill(cr);
        glow(cr, eyeX, eyeY, 0, 8 + recoil_ * 4, {{0, rgba(56, 189, 248, 0.9)}, {1, rgba(56, 189, 248, 0)}}, CAIRO_OPERATOR_SCREEN, 1);
        source(cr, rgb(0x38bdf8)); circle(cr, eyeX, eyeY, 4 + recoil_); cairo_fill(cr);
        source(cr, rgb(0xe0f2fe)); circle(cr, eyeX - 1, eyeY - 1.5, 1.4); cairo_fill(cr);
        source(cr, rgb(0x64748b)); cairo_set_line_width(cr, 1.2);
        cairo_new_path(cr); cairo_arc(cr, 0, 0, 14, 0.35, PI - 0.35); cairo_stroke(cr);
        source(cr, rgb(0x22d3ee)); circle(cr, std::cos(scan_) * 13, std::sin(scan_) * 13, 1.1); cairo_fill(cr);
        if (recoil_ > 0.03f) {
            const float radius = 18 + (1 - recoil_) * 24;
            source(cr, rgba(125, 211, 252, recoil_)); cairo_set_line_width(cr, 1.2); circle(cr, 0, 0, radius); cairo_stroke(cr);
        }
    }
    void bounds(const Pet& pet, Bounds& b) const override { addBox(pet, b, 45 * pet.param("size", 1)); }
};
static struct DroneRegistrar { DroneRegistrar() {
    registerAccessory({"drone", "Drone Observador", rgb(0x38bdf8), CoreConfig{70, 0.48f, 0.93f, 0.12f},
        hints({{"orbitX", 42}, {"orbitY", -18}, {"sway", 2}, {"tilt", 0}, {"tiltVel", 0.045f}, {"kick", 0}, {"sparks", 0}}),
        RopeStyle{rgb(0x334155), rgb(0x38bdf8), 2.2f}, ParticleStyle{rgb(0x7dd3fc), 1.2f, 0},
        {{"size", "Size", 0.7f, 1.5f, 0.05f, 1}}, makeAccessory<Drone>()});
}} s_drone;
}
