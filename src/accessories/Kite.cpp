#include "Common.hpp"

namespace mousepet {

class Kite final : public Accessory {
    static constexpr int TAIL_NODES = 7;
    Chain tail_{TAIL_NODES};
    float flutter_ = 0.0f;

public:
    explicit Kite(Pet& pet) {
        tail_.reset(pet.body.x, pet.body.y, pet.body.x, pet.body.y + 40);
    }

    void update(Pet& pet) override {
        Body& b = pet.body;
        flutter_ += 0.12f;
        b.vy += std::sin(flutter_) * 0.04f;
    }

    void lateUpdate(Pet& pet) override {
        const Body& b = pet.body;
        // Tail starts from the bottom tip of the kite (relative to angle)
        const float c = std::cos(b.angle), s = std::sin(b.angle);
        const float startX = b.x - s * 0 + c * 0 - s * 22;
        const float startY = b.y + c * 0 + s * 0 + c * 22;
        tail_.follow(startX, startY, 6.0f, 0.82f, 0.25f, 0.70f);
    }

    void onBurst(Pet& pet) override {
        // Confetti burst
        for (int i = 0; i < 14; i++) {
            float a = pet.random() * TAU;
            float spd = 1.0f + pet.random() * 2.5f;
            pet.emit(pet.body.x, pet.body.y, std::cos(a) * spd, std::sin(a) * spd, 1.2f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);

        // 1. Draw Tail Line & Bowties (Lacinhos da Pipa)
        cairo_save(cr);
        cairo_set_line_width(cr, 1.3);
        source(cr, rgba(244, 63, 94, 0.75));
        cairo_new_path(cr);
        tail_.trace(cr);
        cairo_stroke(cr);

        // Draw small colorful bowties at tail nodes 2, 4, 6
        static const Color bowColors[] = {rgb(0xfacc15), rgb(0x38bdf8), rgb(0xec4899)};
        for (int i = 1; i < tail_.n; i += 2) {
            float tx = tail_.x[i], ty = tail_.y[i];
            float prevX = tail_.x[i - 1], prevY = tail_.y[i - 1];
            float ang = std::atan2(ty - prevY, tx - prevX);

            cairo_save(cr);
            cairo_translate(cr, tx, ty);
            cairo_rotate(cr, ang + PI * 0.5f);
            source(cr, bowColors[(i / 2) % 3]);

            cairo_new_path(cr);
            cairo_move_to(cr, 0, 0);
            cairo_line_to(cr, -5, -4);
            cairo_line_to(cr, -5, 4);
            cairo_close_path(cr);
            cairo_move_to(cr, 0, 0);
            cairo_line_to(cr, 5, -4);
            cairo_line_to(cr, 5, 4);
            cairo_close_path(cr);
            cairo_fill(cr);

            source(cr, rgb(0xffffff));
            circle(cr, 0, 0, 1.2);
            cairo_fill(cr);
            cairo_restore(cr);
        }
        cairo_restore(cr);

        // 2. Draw Diamond Kite Body
        cairo_save(cr);
        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle);
        cairo_scale(cr, sz, sz);

        // Soft ambient aura
        glow(cr, 0, 0, 2, 28, {{0, rgba(236, 72, 153, 0.35)}, {1, rgba(236, 72, 153, 0)}});

        // Diamond vertices: Top (0, -22), Right (16, -4), Bottom (0, 22), Left (-16, -4)
        // 4 Colored Quadrants
        // Top-Left: Rose Red
        cairo_new_path(cr);
        cairo_move_to(cr, 0, -22); cairo_line_to(cr, -16, -4); cairo_line_to(cr, 0, -4);
        cairo_close_path(cr);
        source(cr, rgb(0xf43f5e)); cairo_fill(cr);

        // Top-Right: Sunset Gold
        cairo_new_path(cr);
        cairo_move_to(cr, 0, -22); cairo_line_to(cr, 16, -4); cairo_line_to(cr, 0, -4);
        cairo_close_path(cr);
        source(cr, rgb(0xfbbf24)); cairo_fill(cr);

        // Bottom-Left: Sky Cyan
        cairo_new_path(cr);
        cairo_move_to(cr, -16, -4); cairo_line_to(cr, 0, 22); cairo_line_to(cr, 0, -4);
        cairo_close_path(cr);
        source(cr, rgb(0x0284c7)); cairo_fill(cr);

        // Bottom-Right: Purple Violet
        cairo_new_path(cr);
        cairo_move_to(cr, 16, -4); cairo_line_to(cr, 0, 22); cairo_line_to(cr, 0, -4);
        cairo_close_path(cr);
        source(cr, rgb(0x8b5cf6)); cairo_fill(cr);

        // Cross Spars (Varetas de bambu)
        source(cr, rgb(0x451a03));
        cairo_set_line_width(cr, 1.4);
        cairo_move_to(cr, 0, -22); cairo_line_to(cr, 0, 22);
        cairo_stroke(cr);
        // Bowed horizontal crossbar
        cairo_new_path(cr);
        cairo_move_to(cr, -16, -4);
        quadTo(cr, 0, -6, 16, -4);
        cairo_stroke(cr);

        // Outer crisp outline
        cairo_new_path(cr);
        cairo_move_to(cr, 0, -22);
        cairo_line_to(cr, 16, -4);
        cairo_line_to(cr, 0, 22);
        cairo_line_to(cr, -16, -4);
        cairo_close_path(cr);
        source(cr, rgba(255, 255, 255, 0.8));
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);

        cairo_restore(cr);
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 32 * pet.param("size", 1.0f));
        tail_.addTo(b, 8);
    }
};

static struct KiteRegistrar {
    KiteRegistrar() {
        registerAccessory({"kite", "Pipa Colorida", rgb(0xf43f5e),
                           CoreConfig{70, 0.50f, 0.90f, 0.22f},
                           hints({{"orbitX", 42}, {"orbitY", -35}, {"sway", 6}, {"tilt", 0.75f}, {"tiltVel", 0.05f}, {"kick", 5}, {"sparks", 14}}),
                           RopeStyle{rgb(0xf43f5e), rgb(0xfecdd3), 1.8f},
                           ParticleStyle{rgb(0xfbbf24), 2.2f, 0.02f},
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<Kite>()});
    }
} s_kiteRegistrar;

}  // namespace mousepet
