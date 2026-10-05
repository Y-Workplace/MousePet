#pragma once
#include <algorithm>

#include <cairo.h>
#include <cmath>
#include <initializer_list>
#include <utility>
#include "../Sim.hpp"
#include "Registry.hpp"

namespace mousepet {

inline void source(cairo_t* cr, const Color& c, double alphaMul = 1) {
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a * alphaMul);
}

inline void stop(cairo_pattern_t* p, double at, const Color& c) {
    cairo_pattern_add_color_stop_rgba(p, at, c.r, c.g, c.b, c.a);
}

/** Canvas quadraticCurveTo from the current point. */
inline void quadTo(cairo_t* cr, double qx, double qy, double x, double y) {
    double x0, y0;
    cairo_get_current_point(cr, &x0, &y0);
    cairo_curve_to(cr, x0 + 2.0 / 3.0 * (qx - x0), y0 + 2.0 / 3.0 * (qy - y0),
                   x + 2.0 / 3.0 * (qx - x), y + 2.0 / 3.0 * (qy - y), x, y);
}

inline void ellipse(cairo_t* cr, double x, double y, double rx, double ry, double rot = 0) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_rotate(cr, rot);
    cairo_scale(cr, rx, ry);
    cairo_new_sub_path(cr);
    cairo_arc(cr, 0, 0, 1, 0, TAU);
    cairo_restore(cr);
}

inline void circle(cairo_t* cr, double x, double y, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x, y, r, 0, TAU);
}

/** Radial glow filled in a square or circle, drawn with the given operator and alpha. */
inline void glow(cairo_t* cr, double x, double y, double r0, double r1,
                 std::initializer_list<std::pair<double, Color>> stops,
                 cairo_operator_t op = CAIRO_OPERATOR_SCREEN, double alpha = 1, bool clipCircle = false) {
    cairo_pattern_t* g = cairo_pattern_create_radial(x, y, r0, x, y, r1);
    for (const auto& [at, c] : stops) stop(g, at, c);
    cairo_save(cr);
    cairo_set_operator(cr, op);
    cairo_set_source(cr, g);
    cairo_new_path(cr);
    if (clipCircle) circle(cr, x, y, r1);
    else cairo_rectangle(cr, x - r1, y - r1, r1 * 2, r1 * 2);
    cairo_clip(cr);
    cairo_paint_with_alpha(cr, alpha);
    cairo_restore(cr);
    cairo_pattern_destroy(g);
}

inline void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, PI / 2, PI);
    cairo_arc(cr, x + r, y + r, r, PI, PI * 1.5);
    cairo_close_path(cr);
}

inline void companionPose(cairo_t* cr, const Pet& pet, float extraAngle = 0.f) {
    cairo_translate(cr, pet.body.x, pet.body.y);
    cairo_rotate(cr, pet.body.angle + extraAngle);
    const float size = pet.param("size", 1.f);
    cairo_scale(cr, size, size);
}

inline float dampedMotion(float age, float frequency, float damping) {
    return std::sin(age * frequency) * std::exp(-age * damping);
}

inline void addBox(const Pet& pet, Bounds& b, float r) {
    b.add(pet.body.x, pet.body.y, r);
}

}  // namespace mousepet
