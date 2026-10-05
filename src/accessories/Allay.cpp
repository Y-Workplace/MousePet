#include "Common.hpp"

namespace mousepet {

class Allay final : public Accessory {
    float wingPhase_ = 0.0f;
    float dance_ = 0.0f;

public:
    void update(Pet& pet) override {
        Body& b = pet.body;
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        wingPhase_ += 0.35f + sp * 0.12f + b.burst * 0.3f;
        dance_ += 0.05f;

        // Fairy sparkle particles
        if (pet.random() < 0.16f || b.burst > 0.1f) {
            float ox = (pet.random() - 0.5f) * 16.0f;
            float oy = (pet.random() - 0.5f) * 16.0f;
            pet.emit(b.x + ox, b.y + oy,
                     (pet.random() - 0.5f) * 0.4f, -0.4f - pet.random() * 0.4f, 0.9f);
        }
    }

    void onBurst(Pet& pet) override {
        // Musical note / fairy sparkle burst
        for (int i = 0; i < 18; i++) {
            float a = pet.random() * TAU;
            float spd = 1.2f + pet.random() * 2.2f;
            pet.emit(pet.body.x, pet.body.y, std::cos(a) * spd, std::sin(a) * spd, 1.2f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);
        const double wingFlap = std::sin(wingPhase_);

        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle * 0.4f);
        cairo_scale(cr, sz, sz);

        // 1. Luminous Cyan Aura (Minecraft Allay glow)
        glow(cr, 0, -2, 4, 26,
             {{0, rgba(75, 227, 255, 0.65)}, {0.5, rgba(56, 189, 248, 0.30)}, {1, rgba(37, 99, 235, 0)}},
             CAIRO_OPERATOR_SCREEN, 1.0, true);

        // 2. Translucent Fluttering Pixel Wings (Behind body)
        cairo_save(cr);
        source(cr, rgba(200, 245, 255, 0.75));

        // Left wing
        cairo_save(cr);
        cairo_translate(cr, -4, -3);
        cairo_scale(cr, wingFlap, 1.0);
        cairo_rectangle(cr, -12, -10, 10, 14);
        cairo_fill(cr);
        source(cr, rgba(255, 255, 255, 0.9));
        cairo_rectangle(cr, -10, -8, 6, 10);
        cairo_fill(cr);
        cairo_restore(cr);

        // Right wing
        cairo_save(cr);
        cairo_translate(cr, 4, -3);
        cairo_scale(cr, wingFlap, 1.0);
        source(cr, rgba(200, 245, 255, 0.75));
        cairo_rectangle(cr, 2, -10, 10, 14);
        cairo_fill(cr);
        source(cr, rgba(255, 255, 255, 0.9));
        cairo_rectangle(cr, 4, -8, 6, 10);
        cairo_fill(cr);
        cairo_restore(cr);

        cairo_restore(cr);

        // 3. Minecraft Allay Body (Pixel/Voxel Robe)
        // Robe base color: #0096c7 / #0284c7
        source(cr, rgb(0x0284c7));
        cairo_rectangle(cr, -4.5, -1, 9, 10);
        cairo_fill(cr);

        // Robe highlight
        source(cr, rgb(0x38bdf8));
        cairo_rectangle(cr, -3.5, 0, 7, 3);
        cairo_fill(cr);

        // Cute Little Floating Arms
        const bool holdingItem = (b.burst > 0.05f);
        if (!holdingItem) {
            source(cr, rgb(0x38bdf8));
            cairo_rectangle(cr, -6.5, 1, 2.5, 6);
            cairo_rectangle(cr, 4.0, 1, 2.5, 6);
            cairo_fill(cr);
        } else {
            // Arms raised forward holding item
            source(cr, rgb(0x38bdf8));
            cairo_rectangle(cr, -6.0, 2, 4, 3);
            cairo_rectangle(cr, 2.0, 2, 4, 3);
            cairo_fill(cr);

            // 4. Minecraft Item: Delicious Pixel Cookie!
            // Cookie base (#c28135)
            source(cr, rgb(0xc28135));
            cairo_rectangle(cr, -5, 3, 10, 9);
            cairo_fill(cr);
            // Cookie shadow edge
            source(cr, rgb(0x8f5619));
            cairo_rectangle(cr, -5, 10, 10, 2);
            cairo_rectangle(cr, 3, 3, 2, 9);
            cairo_fill(cr);
            // Chocolate chips (#381c00)
            source(cr, rgb(0x381c00));
            cairo_rectangle(cr, -3, 5, 2, 2);
            cairo_rectangle(cr, 1, 6, 2, 2);
            cairo_rectangle(cr, -1, 9, 2, 2);
            cairo_fill(cr);
        }

        // 5. Minecraft Allay Head (Pixel Cube)
        // Head cube base: #38bdf8
        source(cr, rgb(0x38bdf8));
        cairo_rectangle(cr, -7.5, -15, 15, 14);
        cairo_fill(cr);

        // Head top highlight
        source(cr, rgb(0x7dd3fc));
        cairo_rectangle(cr, -7.5, -15, 15, 3);
        cairo_fill(cr);

        // Big iconic Minecraft Allay Eyes
        // Dark blue pupil: #0c1a40
        source(cr, rgb(0x0c1a40));
        cairo_rectangle(cr, -5.5, -10, 4, 6);
        cairo_rectangle(cr, 1.5, -10, 4, 6);
        cairo_fill(cr);

        // White eye gleam
        source(cr, rgb(0xffffff));
        cairo_rectangle(cr, -5.5, -10, 4, 2);
        cairo_rectangle(cr, 1.5, -10, 4, 2);
        cairo_fill(cr);
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 32 * pet.param("size", 1.0f));
    }
};

static struct AllayRegistrar {
    AllayRegistrar() {
        registerAccessory({"allay", "Allay (Minecraft)", rgb(0x38bdf8),
                           CoreConfig{50, 0.58f, 0.88f, 0.12f},
                           hints({{"orbitX", 25}, {"orbitY", -28}, {"sway", 4.0f}, {"lift", -0.05f}, {"kick", 5}, {"sparks", 16}}),
                           RopeStyle{rgb(0x0284c7), rgb(0x38bdf8), 1.2f},
                           ParticleStyle{rgb(0x7dd3fc), 2.2f, -0.02f}, // fairy dust floats up
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<Allay>()});
    }
} s_allayRegistrar;

}  // namespace mousepet
