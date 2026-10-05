#include "Common.hpp"

namespace mousepet {

class Jellyfish final : public Accessory {
    static constexpr int COUNT = 4;
    static constexpr float SEG = 4.5f;
    struct Tentacle { float off; Chain chain; };
    std::vector<Tentacle> tent_;
    float phase_ = 0;

    float strokeAge_ = 100.f;

public:
    explicit Jellyfish(Pet& pet) {
        for (int t = 0; t < COUNT; t++)
            tent_.push_back({(t - (COUNT - 1) / 2.f) * 5.5f, Chain(5, pet.body.x, pet.body.y)});
    }
    void onBurst(Pet&) override { strokeAge_ = 0.f; }
    void update(Pet& pet) override {
        strokeAge_ = std::min(strokeAge_ + 1.f, 100.f);
        // Brief bell contraction, then a swimming jet. Tentacles follow with inertia.
        if (strokeAge_ == 5.f) {
            pet.body.vy -= 5.5f;
            for (int i = 0; i < 8; i++)
                pet.emit(pet.body.x + (pet.random() - 0.5f) * 14.f, pet.body.y + 10.f,
                         (pet.random() - 0.5f) * 0.8f, 0.8f + pet.random(), 0.8f);
        }
        Body& b = pet.body;
        phase_ += (0.06f + b.burst * 0.1f) * pet.param("swim", 1);
        if (std::sin(phase_) > 0.75f) b.vy -= 0.12f;  // swimming thrust
    }
    void lateUpdate(Pet& pet) override {
        const Body& b = pet.body;
        const float c = std::cos(b.angle), s = std::sin(b.angle);
        for (auto& t : tent_) t.chain.follow(b.x + c * t.off - s * 7, b.y + s * t.off + c * 7, SEG);
    }
    void render(cairo_t* cr, const Pet& pet) override {
        const Body& b = pet.body;
        cairo_set_line_width(cr, 1.6);
        cairo_new_path(cr); tent_[0].chain.trace(cr); tent_[3].chain.trace(cr);
        cairo_set_source_rgba(cr, 34 / 255., 211 / 255., 238 / 255., 0.5); cairo_stroke(cr);
        tent_[1].chain.trace(cr); tent_[2].chain.trace(cr);
        cairo_set_source_rgba(cr, 56 / 255., 189 / 255., 248 / 255., 0.85); cairo_stroke(cr);

        const double s = std::sin(phase_);
        cairo_translate(cr, b.x, b.y);
        cairo_rotate(cr, b.angle);
        const float stroke = std::sin(strokeAge_ * 0.24f) * std::exp(-strokeAge_ * 0.07f);
        cairo_scale(cr, 1 - s * 0.05 + stroke * 0.20, 1 + s * 0.09 - stroke * 0.22);

        // aura
        glow(cr, 0, -2, 2, 24, {{0, rgba(56, 189, 248, 0.4)}, {0.6, rgba(168, 85, 247, 0.15)}, {1, rgba(56, 189, 248, 0)}},
             CAIRO_OPERATOR_SCREEN, pet.param("glow", 1), true);

        // bell
        cairo_new_path(cr);
        cairo_move_to(cr, -16, 5);
        cairo_curve_to(cr, -18, -18, 18, -18, 16, 5);
        quadTo(cr, 11, 8, 7, 5);
        quadTo(cr, 3, 8, 0, 5);
        quadTo(cr, -3, 8, -7, 5);
        quadTo(cr, -11, 8, -16, 5);
        cairo_close_path(cr);
        cairo_pattern_t* g = cairo_pattern_create_linear(0, -18, 0, 8);
        stop(g, 0, rgba(186, 230, 253, 0.90));
        stop(g, 0.45, rgba(56, 189, 248, 0.65));
        stop(g, 0.85, rgba(192, 132, 252, 0.40));
        stop(g, 1, rgba(147, 51, 234, 0.20));
        cairo_set_source(cr, g);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(g);
        source(cr, rgb(0xe0f2fe));
        cairo_set_line_width(cr, 1.3);
        cairo_stroke(cr);

        const double pulse = std::sin(pet.time * 6) * 0.8;  // inner gem
        cairo_set_source_rgba(cr, 56 / 255., 189 / 255., 248 / 255., 0.7);
        circle(cr, 0, -4, 4.5 + pulse); cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        circle(cr, 0, -4, 2.5 + pulse); cairo_fill(cr);
    }
    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 34 * (1 + pet.body.burst * 0.3f));
        for (const auto& t : tent_) t.chain.addTo(b, 2);
    }
};

static struct JellyfishRegistrar {
    JellyfishRegistrar() {
        registerAccessory({"jellyfish", "Jellyfish", rgb(0x38bdf8), CoreConfig{},
                           hints({{"orbitX", 38}, {"orbitY", -24}, {"sway", 4}, {"kick", 0}, {"sparks", 0}}),
                           RopeStyle{}, ParticleStyle{rgb(0x38bdf8), 2, 0},
                           {{"glow", "Glow", 0, 1, 0.05f, 1}, {"swim", "Pulse", 0.5f, 2, 0.1f, 1}},
                           makeAccessory<Jellyfish>()});
    }
} s_jellyfishRegistrar;

}  // namespace mousepet
