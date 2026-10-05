#include "Common.hpp"

namespace mousepet {

class Fish final : public Accessory {
    float face_ = 1, tail_ = 0, bubbleIn_ = 40;

public:
    void onBurst(Pet& pet) override {
        pet.body.vx += (face_ >= 0.f ? 1.f : -1.f) * 5.f;
        for (int i = 0; i < 9; i++)
            pet.emit(pet.body.x - face_ * 14.f, pet.body.y + (pet.random() - 0.5f) * 8.f,
                     -face_ * (1.f + pet.random()), -0.4f - pet.random() * 0.5f, 0.8f);
    }
    void update(Pet& pet) override {
        const Body& b = pet.body;
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        if (std::fabs(b.vx) > 0.3f) face_ += ((b.vx > 0 ? 1.f : -1.f) - face_) * 0.15f;
        tail_ += 0.15f + sp * 0.08f + b.burst * 0.55f;
        if (--bubbleIn_ <= 0) {  // occasional bubbles
            bubbleIn_ = 30 + pet.random() * 50;
            pet.emit(b.x + face_ * 14, b.y - 2, 0, -0.3f, 1);
        }
    }
    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1), gl = pet.param("glow", 1);
        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle);
        cairo_scale(cr, face_ * sz, sz);

        // tail (only dynamic part of the body)
        cairo_save(cr);
        cairo_translate(cr, -12, 0);
        cairo_rotate(cr, std::sin(tail_) * 0.5);
        source(cr, rgb(0xd97706));
        cairo_new_path(cr);
        cairo_move_to(cr, 2, 0); cairo_line_to(cr, -10, -7); cairo_line_to(cr, -7, 0); cairo_line_to(cr, -10, 7);
        cairo_close_path(cr);
        cairo_fill(cr);
        cairo_restore(cr);

        // body
        cairo_pattern_t* g = cairo_pattern_create_linear(0, -9, 0, 9);
        stop(g, 0, rgb(0xfbbf24));
        stop(g, 1, rgb(0xb45309));
        cairo_set_source(cr, g);
        ellipse(cr, 0, 0, 14, 8.5);
        cairo_fill(cr);
        cairo_pattern_destroy(g);
        source(cr, rgb(0x92400e));  // dorsal fin
        cairo_move_to(cr, -4, -7); cairo_line_to(cr, -9, -12); cairo_line_to(cr, 2, -8); cairo_close_path(cr);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);  // eye
        circle(cr, 8, -2, 2.6); cairo_fill(cr);
        source(cr, rgb(0x0f172a));
        circle(cr, 8.7, -2, 1.3); cairo_fill(cr);
        source(cr, rgb(0x78350f));  // mouth
        cairo_set_line_width(cr, 1.2);
        cairo_move_to(cr, 14, 1); cairo_line_to(cr, 10, 2.5); cairo_stroke(cr);

        // glowing lure
        const double lx = 14 + std::sin(tail_ * 0.7) * 2, ly = -17;
        cairo_move_to(cr, 6, -8);
        quadTo(cr, 10, -18, lx, ly);
        cairo_stroke(cr);
        glow(cr, lx, ly, 0, 18,
             {{0, rgba(254, 240, 138, 0.95)}, {0.3, rgba(251, 191, 36, 0.45)}, {1, rgba(251, 191, 36, 0)}},
             CAIRO_OPERATOR_SCREEN, gl * (0.8 + std::sin(pet.time * 6) * 0.2 + b.burst * 0.8));
    }
    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 42 * pet.param("size", 1));
    }
};

static struct FishRegistrar {
    FishRegistrar() {
        registerAccessory({"fish", "Lantern fish", rgb(0xf59e0b), CoreConfig{65, 0.4f, 0.9f, 0.25f},
                           hints({{"orbitX", 46}, {"orbitY", -14}, {"sway", 6}, {"tilt", 0}, {"tiltVel", 0.03f}, {"kick", 0}, {"sparks", 0}}),
                           RopeStyle{rgb(0x475569), rgb(0x94a3b8), 2}, ParticleStyle{rgb(0xfbbf24), 1.8f, -0.02f},
                           {{"size", "Size", 0.7f, 1.6f, 0.05f, 1}, {"glow", "Light", 0, 1, 0.05f, 1}},
                           makeAccessory<Fish>()});
    }
} s_fishRegistrar;

}  // namespace mousepet
