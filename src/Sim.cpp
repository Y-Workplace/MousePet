#include "Sim.hpp"

#include <algorithm>
#include <cairo.h>
#include <cstdio>
#include <exception>

namespace mousepet {

const std::array<RangeSpec, 4> CORE_RANGES{{
    {"ropeLength", 20.f, 150.f},
    {"stiffness", 0.1f, 0.9f},
    {"drag", 0.75f, 0.98f},
    {"ropeGravity", 0.f, 0.8f},
}};

Color hsl(double h, double s, double l, double a) {
    h = std::fmod(std::fmod(h, 360.0) + 360.0, 360.0) / 360.0;
    s /= 100.0; l /= 100.0;
    auto f = [&](double n) {
        double k = std::fmod(n + h * 12.0, 12.0);
        double am = s * std::min(l, 1.0 - l);
        return l - am * std::max(-1.0, std::min({k - 3.0, 9.0 - k, 1.0}));
    };
    return {f(0), f(8), f(4), a};
}

// ── Chain ──────────────────────────────────────────────────────────────
Chain::Chain(int n_, float x0, float y0) : n(n_), x(n_), y(n_), px(n_), py(n_) { reset(x0, y0, x0, y0); }

void Chain::reset(float x0, float y0, float x1, float y1) {
    const int last = n - 1;
    for (int i = 0; i <= last; i++) {
        const float t = last ? float(i) / last : 0.f;
        x[i] = px[i] = x0 + (x1 - x0) * t;
        y[i] = py[i] = y0 + (y1 - y0) * t;
    }
}

void Chain::follow(float hx, float hy, float seg, float damping, float gravity, float stiff) {
    x[0] = hx; y[0] = hy;
    for (int i = 1; i < n; i++) {
        const float vx = (x[i] - px[i]) * damping, vy = (y[i] - py[i]) * damping;
        px[i] = x[i]; py[i] = y[i];
        x[i] += vx; y[i] += vy + gravity;
        const float dx = x[i] - x[i - 1], dy = y[i] - y[i - 1];
        float d = std::sqrt(dx * dx + dy * dy);
        if (d == 0) d = 0.001f;
        const float k = (d - seg) / d * stiff;
        x[i] -= dx * k; y[i] -= dy * k;
    }
}

void Chain::hang(float seg, float gravity, float damping, int iters) {
    const int last = n - 1;
    for (int i = 1; i < last; i++) {
        const float vx = (x[i] - px[i]) * damping, vy = (y[i] - py[i]) * damping;
        px[i] = x[i]; py[i] = y[i];
        x[i] += vx; y[i] += vy + gravity;
    }
    float sx = 0, sy = 0;
    for (int it = 0; it < iters; it++) {
        for (int i = 0; i < last; i++) {
            const int j = i + 1;
            float dx = x[j] - x[i], dy = y[j] - y[i];
            float d = std::sqrt(dx * dx + dy * dy);
            if (d == 0) d = 0.001f;
            const float k = (d - seg) / d;
            dx *= k; dy *= k;
            if (i == 0) { x[j] -= dx; y[j] -= dy; }
            else if (j == last) { x[i] += dx * 0.9f; y[i] += dy * 0.9f; sx += dx; sy += dy; }
            else { x[i] += dx * 0.5f; y[i] += dy * 0.5f; x[j] -= dx * 0.5f; y[j] -= dy * 0.5f; }
        }
    }
    pullX = sx; pullY = sy;
}

void Chain::trace(cairo_t* cr) const {
    // Canvas quadraticCurveTo(cx,cy,x,y) expressed as a cubic from the current point.
    cairo_move_to(cr, x[0], y[0]);
    double cx0 = x[0], cy0 = y[0];
    for (int i = 1; i < n - 1; i++) {
        const double qx = x[i], qy = y[i];
        const double ex = (x[i] + x[i + 1]) * 0.5, ey = (y[i] + y[i + 1]) * 0.5;
        cairo_curve_to(cr, cx0 + 2.0 / 3.0 * (qx - cx0), cy0 + 2.0 / 3.0 * (qy - cy0),
                       ex + 2.0 / 3.0 * (qx - ex), ey + 2.0 / 3.0 * (qy - ey), ex, ey);
        cx0 = ex; cy0 = ey;
    }
    cairo_line_to(cr, x[n - 1], y[n - 1]);
}

void Chain::addTo(Bounds& b, float pad) const {
    for (int i = 0; i < n; i++) b.add(x[i], y[i], pad);
}

// ── Particles ─────────────────────────────────────────────────────────
void Particles::spawn(float x, float y, float vx, float vy, float life) {
    if (alive_ >= MAX_PARTICLES) return;
    float* p = &p_[alive_++ * STRIDE];
    p[0] = x; p[1] = y; p[2] = vx; p[3] = vy; p[4] = life; p[5] = life;
}

void Particles::update(float gravity) {
    int n = 0;
    for (int i = 0; i < alive_; i++) {
        const float* s = &p_[i * STRIDE];
        const float life = s[4] - 0.03f;
        if (life <= 0) continue;
        float* d = &p_[n++ * STRIDE];
        const float sx = s[0], sy = s[1], svx = s[2], svy = s[3], smax = s[5];
        d[0] = sx + svx;
        d[1] = sy + svy;
        d[2] = svx * 0.94f;
        d[3] = svy * 0.94f + gravity;
        d[4] = life;
        d[5] = smax;
    }
    alive_ = n;
}

// ── Accessory default bounds ───────────────────────────────────────────
void Accessory::bounds(const Pet& pet, Bounds& b) const { b.add(pet.body.x, pet.body.y, 40); }

// ── Pet ───────────────────────────────────────────────────────────────
Pet::Pet(uint32_t seed) : rng_(seed ? seed : 1u) {}
Pet::~Pet() = default;

float Pet::random() {
    // xorshift32: deterministic per seed, so tests are reproducible.
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (rng_ >> 8) * (1.0f / 16777216.0f);
}

float Pet::param(const std::string& key, float fallback) const {
    auto it = params.find(key);
    return it == params.end() ? fallback : it->second;
}

template <class F> void Pet::guard(const char* hook, F&& fn) {
    if (!inst_) return;
    try { fn(); }
    catch (const std::exception& e) {
        // A broken accessory never takes the loop down: it is disabled instead.
        lastError = std::string(def ? def->id : "?") + "." + hook + ": " + e.what();
        std::fprintf(stderr, "[mousepet] error in %s - accessory disabled\n", lastError.c_str());
        inst_.reset();
    }
}

bool Pet::select(const AccessoryDef& d) {
    inst_.reset();
    def = &d;
    config = d.defaults;
    hints = d.body;
    ropeStyle = d.rope;
    particleStyle = d.particle;
    params.clear();
    for (const auto& p : d.params) params[p.key] = p.value;
    lastError.clear();
    try { inst_ = d.create(*this); }
    catch (const std::exception& e) {
        lastError = d.id + ".create: " + e.what();
        inst_.reset();
    }
    body.vx = body.vy = body.angle = body.burst = 0;
    rope.reset(mouse.x, mouse.y, body.x, body.y);
    particles.clear();
    return true;
}

void Pet::place(float mx, float my) {
    mouse = Mouse{mx, my, 0, 0, 0, mx, my};
    body = Body{mx + hints.orbitX, my + hints.orbitY, 0, 0, 0, 0};
    rope.reset(mx, my, body.x, body.y);
    particles.clear();
    if (inst_) {
        // Let secondary chains (tentacles...) settle at the new location.
        guard("lateUpdate", [&] { inst_->lateUpdate(*this); });
    }
}

void Pet::burst() {
    if (!def) return;
    body.burst = 1;
    body.vy -= hints.kick;
    for (int i = 0; i < hints.sparks; i++) {
        const float a = random() * TAU, s = random() * 2.5f + 1.f;
        particles.spawn(body.x, body.y + 4, std::cos(a) * s, std::sin(a) * s, 1);
    }
    guard("onBurst", [&] { inst_->onBurst(*this); });
}

void Pet::step() {
    if (!def) return;
    Mouse& m = mouse; Body& b = body; const BodyHints& h = hints;
    time += 0.025f;

    m.vx = m.x - m.lastX; m.vy = m.y - m.lastY;
    m.lastX = m.x; m.lastY = m.y;
    m.speed += (std::sqrt(m.vx * m.vx + m.vy * m.vy) - m.speed) * 0.2f;

    // Smooth attraction to the rest point
    b.vx += (m.x + h.orbitX - b.x) * h.pull;
    b.vy += (m.y + h.orbitY + std::sin(time * 2) * h.sway - b.y) * h.pull + h.lift;

    guard("update", [&] { inst_->update(*this); });  // extra accessory forces

    b.burst = b.burst > 0.01f ? b.burst * 0.93f : 0.f;
    b.vx *= config.drag; b.vy *= config.drag;
    b.x += b.vx; b.y += b.vy;

    // Rope & anchor constraint loop every frame
    updateAnchor(m.x, m.y);
    b.vx -= rope.pullX * config.stiffness * 0.5f;
    b.vy -= rope.pullY * config.stiffness * 0.5f;

    // The rope supplies an axis, not a full sprite rotation. Scaling a wrapped
    // angle by a fractional tilt creates discontinuities at +/-pi and can leave
    // upright companions inverted. Fold the axis, limit banking, then ease it.
    const float dx = b.x - rope.x[LAST - 1], dy = b.y - rope.y[LAST - 1];
    const float raw = std::remainder(std::atan2(dy, dx) + h.tiltBase, float(PI));
    const float limit = MAX_BODY_BANK;
    const float target = std::clamp(raw * h.tilt - b.vx * h.tiltVel, -limit, limit);
    b.angle = std::clamp(std::remainder(b.angle, float(TAU)), -limit, limit);
    b.angle += (target - b.angle) * std::clamp(h.tiltEase, 0.f, 1.f);

    guard("lateUpdate", [&] { inst_->lateUpdate(*this); });  // secondary motion
    particles.update(particleStyle.gravity);
}

void Pet::drawParticles(cairo_t* cr) const {
    if (!particles.alive()) return;
    const Color& c = particleStyle.color;
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_ADD);  // canvas 'lighter'
    for (int i = 0; i < particles.alive(); i++) {
        const float* p = particles.at(i);
        const float a = p[4] / p[5];
        const float r = particleStyle.size * a * 1.7f;
        if (r <= 0.05f) continue;
        cairo_pattern_t* g = cairo_pattern_create_radial(p[0], p[1], 0, p[0], p[1], r);
        cairo_pattern_add_color_stop_rgba(g, 0, c.r, c.g, c.b, a);
        cairo_pattern_add_color_stop_rgba(g, 0.45, c.r, c.g, c.b, 0.8 * a);
        cairo_pattern_add_color_stop_rgba(g, 1, c.r, c.g, c.b, 0);
        cairo_set_source(cr, g);
        cairo_rectangle(cr, p[0] - r, p[1] - r, r * 2, r * 2);
        cairo_fill(cr);
        cairo_pattern_destroy(g);
    }
    cairo_restore(cr);
}

void Pet::updateAnchor(float mx, float my) {
    if (!def) return;
    mouse.x = mx;
    mouse.y = my;
    rope.x[0] = mx;
    rope.y[0] = my;

    // Never breaks loose: clamp body distance if dragged fast
    const float dx = body.x - mx, dy = body.y - my;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float max = config.ropeLength * hints.maxStretch;
    if (dist > max && dist > 1e-4f) {
        const float f = max / dist;
        body.x = mx + dx * f;
        body.y = my + dy * f;
        body.vx *= 0.5f;
        body.vy *= 0.5f;
    }
    rope.x[LAST] = body.x;
    rope.y[LAST] = body.y;

    // Fixed-anchor relaxation loop runs every frame (10 iterations)
    rope.hang(config.ropeLength / LAST, config.ropeGravity, 0.88f, 10);
}

void Pet::render(cairo_t* cr) {
    if (!def) return;
    // Always solve anchor constraint on the live cursor every frame
    updateAnchor(mouse.x, mouse.y);
    cairo_save(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    cairo_new_path(cr);
    rope.trace(cr);
    const Color& rc = ropeStyle.color; const Color& rh = ropeStyle.highlight;
    cairo_set_source_rgba(cr, rc.r, rc.g, rc.b, rc.a);
    cairo_set_line_width(cr, ropeStyle.width);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgba(cr, rh.r, rh.g, rh.b, rh.a);
    cairo_set_line_width(cr, ropeStyle.width * 0.43);
    cairo_stroke(cr);
    cairo_restore(cr);

    cairo_save(cr);  // each accessory draws in isolation
    guard("render", [&] { inst_->render(cr, *this); });
    cairo_restore(cr);

    drawParticles(cr);
}

Bounds Pet::bounds() const {
    Bounds b;
    rope.addTo(b, ropeStyle.width + 2);
    b.add(mouse.x, mouse.y, ropeStyle.width + 2);
    if (inst_) inst_->bounds(*this, b);
    else b.add(body.x, body.y, 4);
    for (int i = 0; i < particles.alive(); i++) {
        const float* p = particles.at(i);
        b.add(p[0], p[1], particleStyle.size * 1.7f + 1);
    }
    return b;
}

bool Pet::setCore(std::string_view key, float v) {
    for (const auto& r : CORE_RANGES) {
        if (key != r.key) continue;
        v = std::clamp(v, r.min, r.max);
        if (key == "ropeLength") config.ropeLength = v;
        else if (key == "stiffness") config.stiffness = v;
        else if (key == "drag") config.drag = v;
        else config.ropeGravity = v;
        return true;
    }
    return false;
}

bool Pet::setParam(std::string_view key, float v) {
    if (!def) return false;
    for (const auto& p : def->params) {
        if (p.key != key) continue;
        params[p.key] = std::clamp(v, p.min, p.max);
        return true;
    }
    return false;
}

const char* Pet::state() const {
    if (!lastError.empty()) return "Error";
    return body.burst > 0.2f ? "Burst" : mouse.speed > 4 ? "Following" : "Floating";
}

}  // namespace mousepet
