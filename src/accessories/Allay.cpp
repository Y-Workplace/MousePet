#include "Common.hpp"

namespace mousepet {

class Allay final : public Accessory {
    float wingPhase_ = 0.0f;
    float dance_ = 0.0f;

    // Authentic 28x22 pixel-art sprite matching Minecraft Allay reference
    static constexpr int SPR_W = 28;
    static constexpr int SPR_H = 22;

    static constexpr Color PALETTE[8] = {
        {0, 0, 0, 0},              // 0: transparent
        rgb(0x1b2a5c),             // 1: dark navy border
        rgb(0x005f8b),             // 2: deep blue shade
        rgb(0x028cd4),             // 3: mid blue
        rgb(0x10afe9),             // 4: cyan shadow
        rgb(0x34ccf3),             // 5: main bright cyan
        rgb(0x3bf6ff),             // 6: luminous highlight
        rgb(0xe6e6e8),             // 7: white eye gleam
    };

    static constexpr uint8_t SPRITE[SPR_H][SPR_W] = {
        {0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 2, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 5, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 2, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 5, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 2, 6, 6, 5, 5, 5, 5, 5, 5, 5, 6, 6, 5, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 2, 5, 5, 5, 4, 4, 4, 4, 4, 5, 5, 5, 4, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 2, 5, 5, 7, 7, 4, 4, 4, 7, 7, 5, 5, 4, 3, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 2, 5, 5, 7, 7, 4, 4, 4, 7, 7, 5, 5, 4, 3, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 2, 5, 5, 7, 7, 4, 4, 4, 7, 7, 5, 5, 4, 3, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 2, 5, 5, 7, 7, 5, 5, 5, 7, 7, 5, 5, 4, 3, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 2, 5, 5, 7, 7, 5, 5, 5, 7, 7, 5, 5, 4, 3, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {2, 2, 2, 0, 2, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 3, 3, 1, 0, 0, 0, 0, 0, 2, 2, 2},
        {1, 4, 4, 2, 2, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 3, 3, 1, 0, 0, 2, 2, 2, 4, 4, 1},
        {0, 1, 1, 4, 1, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 3, 3, 1, 2, 2, 4, 4, 4, 1, 1, 0},
        {1, 1, 3, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 5, 4, 2, 1, 3, 3, 1, 1},
        {1, 2, 2, 1, 1, 1, 1, 6, 1, 5, 5, 5, 5, 3, 2, 6, 6, 1, 3, 3, 1, 1, 1, 1, 1, 2, 2, 1},
        {1, 1, 1, 1, 0, 1, 6, 6, 1, 5, 5, 5, 5, 3, 2, 6, 6, 1, 1, 2, 2, 1, 0, 0, 1, 1, 1, 1},
        {0, 0, 0, 0, 1, 6, 6, 6, 1, 1, 4, 4, 4, 3, 3, 2, 6, 6, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 1, 6, 6, 1, 0, 1, 4, 4, 4, 4, 3, 2, 6, 6, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 4, 4, 4, 4, 2, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 3, 3, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 3, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    };

    // 8x8 Minecraft Diamond item held by the Allay
    static constexpr uint8_t DIAMOND[8][8] = {
        {0, 0, 1, 1, 1, 1, 0, 0},
        {0, 1, 6, 6, 5, 5, 1, 0},
        {1, 6, 7, 6, 5, 5, 4, 1},
        {1, 6, 6, 5, 5, 4, 4, 1},
        {1, 5, 5, 5, 4, 4, 3, 1},
        {0, 1, 5, 4, 4, 3, 1, 0},
        {0, 0, 1, 4, 3, 1, 0, 0},
        {0, 0, 0, 1, 1, 0, 0, 0},
    };

    static constexpr Color DIAMOND_PALETTE[8] = {
        {0, 0, 0, 0},
        rgb(0x0f2b38), // dark cyan border
        rgb(0x005577),
        rgb(0x0088aa),
        rgb(0x00bbcc),
        rgb(0x2ce2ee), // vivid diamond cyan
        rgb(0x8cf6fa), // highlight
        rgb(0xffffff), // glint
    };

public:
    void update(Pet& pet) override {
        Body& b = pet.body;
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        wingPhase_ += 0.35f + sp * 0.15f + b.burst * 0.45f;
        dance_ += 0.05f;

        // Minecraft fairy sparkles floating upward
        if (pet.random() < 0.20f || b.burst > 0.1f) {
            float ox = (pet.random() - 0.5f) * 18.0f;
            float oy = (pet.random() - 0.5f) * 16.0f;
            pet.emit(b.x + ox, b.y + oy,
                     (pet.random() - 0.5f) * 0.4f, -0.5f - pet.random() * 0.5f, 0.9f);
        }
    }

    void onBurst(Pet& pet) override {
        // Starburst of musical notes and diamond glints
        for (int i = 0; i < 20; i++) {
            float a = pet.random() * TAU;
            float spd = 1.4f + pet.random() * 2.2f;
            pet.emit(pet.body.x, pet.body.y, std::cos(a) * spd, std::sin(a) * spd, 1.2f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);
        const float flap = std::sin(wingPhase_) * 1.5f;

        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle * 0.35f);
        cairo_scale(cr, sz, sz);

        // 1. Ambient Luminous Cyan Allay Glow
        const double glowSize = 32.0 + b.burst * 14.0;
        glow(cr, 0, -2, 4, glowSize,
             {{0, rgba(59, 246, 255, 0.65)}, {0.5, rgba(52, 204, 243, 0.30)}, {1, rgba(2, 140, 212, 0)}},
             CAIRO_OPERATOR_SCREEN, 1.0, true);

        // 2. Render Pixel Art Sprite
        const float P = 1.55f; // Pixel block size in points
        const float originX = -float(SPR_W) * P * 0.5f;
        const float originY = -float(SPR_H) * P * 0.5f - 2.0f;

        for (int y = 0; y < SPR_H; y++) {
            for (int x = 0; x < SPR_W; x++) {
                const uint8_t idx = SPRITE[y][x];
                if (idx == 0) continue;

                // Separate fluttering wings on sides
                float py = originY + y * P;
                if (x <= 4) {
                    // Left wing flutters
                    py += flap;
                } else if (x >= 21) {
                    // Right wing flutters
                    py -= flap;
                }

                const float px = originX + x * P;
                source(cr, PALETTE[idx]);
                cairo_rectangle(cr, px, py, P, P);
                cairo_fill(cr);
            }
        }

        // 3. Minecraft Item: Diamond held by the Allay
        // Idle: hands hold item near chest (0, 7)
        // Burst: Allay joyfully raises the diamond high above head (0, -16)!
        const float burstLift = b.burst * 22.0f;
        const float itemX = -4.0f * P;
        const float itemY = (6.0f - burstLift) * P;

        // Diamond radiant gleam on burst
        if (b.burst > 0.05f) {
            glow(cr, 0, itemY + 4.0f * P, 0, 20.0f * b.burst,
                 {{0, rgba(140, 246, 250, 0.95)}, {0.5, rgba(44, 226, 238, 0.5)}, {1, rgba(0, 85, 119, 0)}},
                 CAIRO_OPERATOR_SCREEN, b.burst);
        }

        // Render 8x8 Diamond
        for (int dy = 0; dy < 8; dy++) {
            for (int dx = 0; dx < 8; dx++) {
                const uint8_t cidx = DIAMOND[dy][dx];
                if (cidx == 0) continue;
                source(cr, DIAMOND_PALETTE[cidx]);
                cairo_rectangle(cr, itemX + dx * P, itemY + dy * P, P, P);
                cairo_fill(cr);
            }
        }
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 38 * pet.param("size", 1.0f));
    }
};

static struct AllayRegistrar {
    AllayRegistrar() {
        registerAccessory({"allay", "Allay (Minecraft)", rgb(0x38bdf8),
                           CoreConfig{60, 0.45f, 0.91f, 0.18f},
                           hints({{"orbitX", 36}, {"orbitY", -28}, {"sway", 5.5f}, {"lift", -0.04f}, {"kick", 5}, {"sparks", 16}}),
                           RopeStyle{rgb(0x0284c7), rgb(0x7dd3fc), 1.8f},
                           ParticleStyle{rgb(0x38bdf8), 2.2f, -0.04f}, // sparkles float upward
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<Allay>()});
    }
} s_allayRegistrar;

}  // namespace mousepet
