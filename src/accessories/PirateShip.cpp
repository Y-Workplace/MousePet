#include "Common.hpp"

namespace mousepet {

class PirateShip final : public Accessory {
    float wavePhase_ = 0.0f;
    float flagWave_ = 0.0f;
    float tilt_ = 0.0f;

public:
    void update(Pet& pet) override {
        Body& b = pet.body;
        const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
        wavePhase_ += 0.10f;
        flagWave_ += 0.25f + sp * 0.1f;

        // Sea spray / wake bubbles at the keel
        if (sp > 0.5f || pet.random() < 0.20f) {
            float ox = (pet.random() - 0.5f) * 16.0f;
            pet.emit(b.x + ox, b.y + 12.0f,
                     -b.vx * 0.2f + (pet.random() - 0.5f) * 0.4f,
                     0.4f + pet.random() * 0.6f, 0.6f);
        }

        // Hull pitch and roll based on velocity
        const float targetTilt = std::clamp(b.vx * 0.06f, -0.45f, 0.45f);
        tilt_ += (targetTilt - tilt_) * 0.18f;
    }

    void onBurst(Pet& pet) override {
        // Broadside cannon salvo! Smoke and fiery cannon shot
        for (int i = 0; i < 18; i++) {
            float side = (i % 2 == 0) ? -1.0f : 1.0f;
            float a = (side > 0 ? 0.0f : PI) + (pet.random() - 0.5f) * 0.8f;
            float spd = 2.0f + pet.random() * 2.5f;
            pet.emit(pet.body.x, pet.body.y + 4.0f, std::cos(a) * spd, std::sin(a) * spd, 1.0f);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        const double sz = pet.param("size", 1.0f);
        const double waveBob = std::sin(wavePhase_) * 2.5;

        cairo_translate(cr, b.x, b.y + waveBob);
        cairo_rotate(cr, b.angle * 0.3f + tilt_);
        cairo_scale(cr, sz, sz);

        // 1. Water Wake / Mist at the Keel
        cairo_pattern_t* mg = cairo_pattern_create_radial(0, 14, 2, 0, 14, 22);
        stop(mg, 0, rgba(224, 242, 254, 0.45));
        stop(mg, 0.6, rgba(56, 189, 248, 0.20));
        stop(mg, 1, rgba(3, 105, 161, 0.0));
        cairo_set_source(cr, mg);
        ellipse(cr, 0, 14, 24, 6);
        cairo_fill(cr);
        cairo_pattern_destroy(mg);

        // 2. Wooden Galleon Hull
        cairo_new_path(cr);
        cairo_move_to(cr, -22, -1);      // High stern poop deck
        cairo_line_to(cr, -20, 10);     // Stern curve
        cairo_curve_to(cr, -10, 15, 10, 15, 20, 8); // Keel curve
        cairo_line_to(cr, 25, 1);       // Bow
        cairo_line_to(cr, 18, 1);       // Foredeck
        cairo_line_to(cr, -12, 1);      // Waist deck
        cairo_line_to(cr, -16, -1);     // Quarterdeck step
        cairo_close_path(cr);

        cairo_pattern_t* hg = cairo_pattern_create_linear(0, -2, 0, 15);
        stop(hg, 0, rgb(0x78350f));    // Warm cedar deck
        stop(hg, 0.4, rgb(0x451a03));  // Dark oak planking
        stop(hg, 1, rgb(0x291002));    // Blackened tar keel
        cairo_set_source(cr, hg);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(hg);

        source(cr, rgb(0x1c0b02));
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);

        // Hull plank lines
        source(cr, rgba(28, 11, 2, 0.55));
        cairo_set_line_width(cr, 0.8);
        cairo_move_to(cr, -18, 5);  cairo_curve_to(cr, -6, 7, 8, 7, 21, 4);
        cairo_move_to(cr, -16, 9);  cairo_curve_to(cr, -6, 11, 8, 11, 18, 8);
        cairo_stroke(cr);

        // Gunports along the broadside (Cannon ports)
        source(cr, rgb(0x09090b));
        for (float gx : {-10.0f, -2.0f, 6.0f, 14.0f}) {
            cairo_rectangle(cr, gx, 4.5, 3.2, 2.8);
            cairo_fill(cr);
            // Brass cannon muzzle
            source(cr, rgb(0xd97706));
            cairo_rectangle(cr, gx + 0.8, 5.2, 1.6, 1.4);
            cairo_fill(cr);
            source(cr, rgb(0x09090b));
        }

        // Bowsprit pole extending forward
        source(cr, rgb(0x78350f));
        cairo_set_line_width(cr, 1.8);
        cairo_move_to(cr, 22, 2);
        cairo_line_to(cr, 33, -3);
        cairo_stroke(cr);

        // 3. Masts & Yards (Mainmast and Foremast)
        source(cr, rgb(0x451a03));
        cairo_set_line_width(cr, 2.0);

        // Mainmast (Center, taller)
        cairo_move_to(cr, -2, 2);
        cairo_line_to(cr, -2, -26);
        cairo_stroke(cr);

        // Foremast (Forward, slightly shorter)
        cairo_move_to(cr, 11, 2);
        cairo_line_to(cr, 11, -21);
        cairo_stroke(cr);

        // Yards (Horizontal crossbars)
        cairo_set_line_width(cr, 1.3);
        cairo_move_to(cr, -12, -18); cairo_line_to(cr, 8, -18);  // Main yard
        cairo_move_to(cr, -10, -9);  cairo_line_to(cr, 6, -9);   // Lower yard
        cairo_move_to(cr, 3, -15);   cairo_line_to(cr, 19, -15); // Fore yard
        cairo_stroke(cr);

        // Crow's nest lookout on mainmast
        source(cr, rgb(0x291002));
        cairo_rectangle(cr, -5, -21, 6, 3.5);
        cairo_fill(cr);

        // Rigging shrouds (Cordas do navio)
        source(cr, rgba(202, 138, 4, 0.45));
        cairo_set_line_width(cr, 0.7);
        cairo_move_to(cr, -2, -24); cairo_line_to(cr, -15, 2);
        cairo_move_to(cr, -2, -24); cairo_line_to(cr, 10, 2);
        cairo_move_to(cr, 11, -20); cairo_line_to(cr, 20, 2);
        cairo_move_to(cr, 11, -20); cairo_line_to(cr, 3, 2);
        cairo_stroke(cr);

        // 4. Billowing Black Pirate Sails
        // Main Topsail (Large billowing sail)
        cairo_pattern_t* sg = cairo_pattern_create_linear(-12, -18, 8, -9);
        stop(sg, 0, rgb(0x27272a));
        stop(sg, 0.5, rgb(0x18181b));
        stop(sg, 1, rgb(0x09090b));

        cairo_new_path(cr);
        cairo_move_to(cr, -11, -18);
        cairo_curve_to(cr, -2, -20, 2, -20, 7, -18);
        cairo_curve_to(cr, 9, -13, 8, -9, 5, -9);
        cairo_curve_to(cr, -2, -7, -4, -7, -9, -9);
        cairo_close_path(cr);
        cairo_set_source(cr, sg);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(sg);

        source(cr, rgb(0x3f3f46));
        cairo_set_line_width(cr, 0.9);
        cairo_stroke(cr);

        // Jolly Roger Skull & Crossbones on Main Sail!
        source(cr, rgb(0xf4f4f5)); // Bone white
        // Skull head
        circle(cr, -1.8, -14.2, 2.2);
        cairo_fill(cr);
        // Skull jaw
        cairo_rectangle(cr, -2.6, -12.4, 1.6, 1.2);
        cairo_fill(cr);
        // Eye sockets
        source(cr, rgb(0x09090b));
        circle(cr, -2.4, -14.3, 0.6);
        circle(cr, -1.2, -14.3, 0.6);
        cairo_fill(cr);
        // Crossbones
        source(cr, rgb(0xf4f4f5));
        cairo_set_line_width(cr, 0.8);
        cairo_move_to(cr, -4.5, -16); cairo_line_to(cr, 1.0, -11.5);
        cairo_move_to(cr, 1.0, -16);  cairo_line_to(cr, -4.5, -11.5);
        cairo_stroke(cr);

        // Foresail
        cairo_new_path(cr);
        cairo_move_to(cr, 4, -15);
        cairo_curve_to(cr, 11, -17, 14, -17, 18, -15);
        cairo_curve_to(cr, 19, -11, 18, -8, 16, -8);
        cairo_curve_to(cr, 12, -7, 9, -7, 6, -8);
        cairo_close_path(cr);
        source(cr, rgb(0x18181b));
        cairo_fill_preserve(cr);
        source(cr, rgb(0x3f3f46));
        cairo_set_line_width(cr, 0.8);
        cairo_stroke(cr);

        // 5. Fluttering Black Pirate Flag at the Masthead
        const float flagOffset = std::sin(flagWave_) * 2.0f;
        cairo_new_path(cr);
        cairo_move_to(cr, -2, -26);
        cairo_curve_to(cr, -6, -26 + flagOffset * 0.4f, -9, -27 - flagOffset, -14, -25 + flagOffset);
        cairo_line_to(cr, -11, -22 + flagOffset);
        cairo_curve_to(cr, -8, -23 - flagOffset * 0.5f, -5, -23 + flagOffset * 0.4f, -2, -23);
        cairo_close_path(cr);
        source(cr, rgb(0x09090b));
        cairo_fill(cr);

        // Tiny skull dot on flag
        source(cr, rgb(0xffffff));
        circle(cr, -6, -24.5, 0.7);
        cairo_fill(cr);

        // 6. Stern Brass Lantern
        source(cr, rgb(0xd97706));
        cairo_rectangle(cr, -23.5, 0, 2.5, 3.5);
        cairo_fill(cr);
        source(cr, rgb(0xfef08a));
        circle(cr, -22.2, 1.7, 1.0);
        cairo_fill(cr);
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 46 * pet.param("size", 1.0f));
    }
};

static struct PirateShipRegistrar {
    PirateShipRegistrar() {
        registerAccessory({"pirateship", "Navio Pirata", rgb(0x1c1917),
                           CoreConfig{65, 0.50f, 0.90f, 0.22f},
                           hints({{"orbitX", 42}, {"orbitY", -22}, {"sway", 5.0f}, {"lift", 0.02f}, {"kick", 5.0f}, {"sparks", 16}}),
                           RopeStyle{rgb(0x451a03), rgb(0xd97706), 2.0f},
                           ParticleStyle{rgb(0x38bdf8), 2.2f, 0.05f}, // sea foam spray
                           {{"size", "Size", 0.7f, 1.5f, 0.05f, 1.0f}},
                           makeAccessory<PirateShip>()});
    }
} s_pirateShipRegistrar;

}  // namespace mousepet
