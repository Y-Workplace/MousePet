#include "Overlay.hpp"

#include <cairo.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <numeric>
#include <sstream>
#include <vector>

#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace mousepet {

using Clock = std::chrono::steady_clock;

namespace {

constexpr int SLOTS = 3;
constexpr int TRAIL_FRAMES = 14;           // 0.65^14 < 1/255: older trail pixels are invisible
constexpr auto FRAME_TIMEOUT = std::chrono::milliseconds(100);

int createShmFile(size_t size) {
    const int fd = memfd_create("mousepet-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) return -1;
    if (ftruncate(fd, off_t(size)) != 0) { close(fd); return -1; }
    return fd;
}

struct Buffer {
    wl_shm_pool* pool = nullptr;
    wl_buffer* buf = nullptr;
    cairo_surface_t* cs = nullptr;
    int fd = -1;
    void* data = nullptr;
    size_t cap = 0;
    int w = 0, h = 0, stride = 0;
    bool busy = false;

    static void onRelease(void* data, wl_buffer*) { static_cast<Buffer*>(data)->busy = false; }

    void dropBuffer() {
        if (cs) { cairo_surface_destroy(cs); cs = nullptr; }
        if (buf) { wl_buffer_destroy(buf); buf = nullptr; }
    }
    void destroy() {
        dropBuffer();
        if (pool) { wl_shm_pool_destroy(pool); pool = nullptr; }
        if (data) { munmap(data, cap); data = nullptr; }
        if (fd >= 0) { close(fd); fd = -1; }
        cap = 0; w = h = 0;
    }
    /** Makes this slot hold a w×h ARGB buffer. Must not be busy. */
    bool ensure(wl_shm* shm, int nw, int nh) {
        const int nstride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, nw);
        const size_t need = size_t(nstride) * size_t(nh);
        if (need > cap) {
            dropBuffer();
            const size_t ncap = std::max<size_t>((need + 65535) & ~size_t(65535), 256 * 1024);
            if (fd < 0) {
                fd = createShmFile(ncap);
                if (fd < 0) return false;
            } else if (ftruncate(fd, off_t(ncap)) != 0) {
                return false;
            }
            if (data) munmap(data, cap);
            data = mmap(nullptr, ncap, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (data == MAP_FAILED) { data = nullptr; cap = 0; return false; }
            if (pool) wl_shm_pool_resize(pool, int32_t(ncap));  // pools may only grow
            else pool = wl_shm_create_pool(shm, fd, int32_t(ncap));
            cap = ncap;
        }
        if (!buf || nw != w || nh != h) {
            dropBuffer();
            w = nw; h = nh; stride = nstride;
            buf = wl_shm_pool_create_buffer(pool, 0, w, h, stride, WL_SHM_FORMAT_ARGB8888);
            static const wl_buffer_listener l{onRelease};
            wl_buffer_add_listener(buf, &l, this);
            cs = cairo_image_surface_create_for_data(static_cast<unsigned char*>(data), CAIRO_FORMAT_ARGB32, w, h, stride);
        }
        return cs && cairo_surface_status(cs) == CAIRO_STATUS_SUCCESS;
    }
};

}  // namespace

struct Overlay::Impl;

struct Output {
    Overlay::Impl* d = nullptr;
    uint32_t globalName = 0;
    wl_output* wo = nullptr;
    zxdg_output_v1* xo = nullptr;
    std::string name;
    int lx = 0, ly = 0, lw = 0, lh = 0;  // logical geometry in the compositor layout
    bool geometryKnown = false;
    int intScale = 1;

    wl_surface* surf = nullptr;
    zwlr_layer_surface_v1* layer = nullptr;
    wp_viewport* vp = nullptr;
    wp_fractional_scale_v1* frac = nullptr;
    uint32_t scale120 = 0;
    int cw = 0, ch = 0;
    bool configured = false;

    wl_surface* pet = nullptr;
    wl_subsurface* sub = nullptr;
    wp_viewport* petVp = nullptr;
    Buffer slots[SLOTS];
    int prevSlot = -1, prevX = 0, prevY = 0;
    double prevScale = 0;
    bool petMapped = false;
    int subX = INT_MIN, subY = INT_MIN;
    wl_callback* frameCb = nullptr;
    Clock::time_point frameAt{};
    bool dirty = false;  // a newer simulation state is waiting for this output's frame callback

    double scale() const { return scale120 ? scale120 / 120.0 : double(intScale); }
};

struct Overlay::Impl {
    wl_display* dpy = nullptr;
    wl_registry* reg = nullptr;
    wl_compositor* comp = nullptr;
    wl_subcompositor* subcomp = nullptr;
    wl_shm* shm = nullptr;
    zwlr_layer_shell_v1* layerShell = nullptr;
    zxdg_output_manager_v1* xdgOutputs = nullptr;
    wp_viewporter* viewporter = nullptr;
    wp_fractional_scale_manager_v1* fracMgr = nullptr;
    wl_buffer* clearPixel = nullptr;  // 1×1 transparent buffer stretched over each layer surface
    Buffer clearPixelStore;
    std::vector<std::unique_ptr<Output>> outputs;
    std::deque<Bounds> trail;
    Bounds lastTotal;
    unsigned long commits = 0;
    bool fatal = false;

    ~Impl() {
        for (auto& o : outputs) destroyOutput(*o);
        outputs.clear();
        clearPixelStore.destroy();
        if (fracMgr) wp_fractional_scale_manager_v1_destroy(fracMgr);
        if (viewporter) wp_viewporter_destroy(viewporter);
        if (xdgOutputs) zxdg_output_manager_v1_destroy(xdgOutputs);
        if (layerShell) zwlr_layer_shell_v1_destroy(layerShell);
        if (shm) wl_shm_destroy(shm);
        if (subcomp) wl_subcompositor_destroy(subcomp);
        if (comp) wl_compositor_destroy(comp);
        if (reg) wl_registry_destroy(reg);
        if (dpy) { wl_display_flush(dpy); wl_display_disconnect(dpy); }
    }

    // ── Output lifecycle ──────────────────────────────────────────────
    void destroySurfaces(Output& o) {
        if (o.frameCb) { wl_callback_destroy(o.frameCb); o.frameCb = nullptr; }
        for (auto& s : o.slots) s.destroy();
        if (o.petVp) { wp_viewport_destroy(o.petVp); o.petVp = nullptr; }
        if (o.sub) { wl_subsurface_destroy(o.sub); o.sub = nullptr; }
        if (o.pet) { wl_surface_destroy(o.pet); o.pet = nullptr; }
        if (o.frac) { wp_fractional_scale_v1_destroy(o.frac); o.frac = nullptr; }
        if (o.vp) { wp_viewport_destroy(o.vp); o.vp = nullptr; }
        if (o.layer) { zwlr_layer_surface_v1_destroy(o.layer); o.layer = nullptr; }
        if (o.surf) { wl_surface_destroy(o.surf); o.surf = nullptr; }
        o.configured = false; o.petMapped = false; o.prevSlot = -1; o.subX = o.subY = INT_MIN;
    }
    void destroyOutput(Output& o) {
        destroySurfaces(o);
        if (o.xo) { zxdg_output_v1_destroy(o.xo); o.xo = nullptr; }
        if (o.wo) {
            if (wl_output_get_version(o.wo) >= WL_OUTPUT_RELEASE_SINCE_VERSION) wl_output_release(o.wo);
            else wl_output_destroy(o.wo);
            o.wo = nullptr;
        }
    }

    static wl_region* emptyRegion(wl_compositor* c) { return wl_compositor_create_region(c); }

    void createSurfaces(Output& o) {
        if (o.surf || !layerShell || !comp) return;
        o.surf = wl_compositor_create_surface(comp);
        wl_region* r = emptyRegion(comp);
        wl_surface_set_input_region(o.surf, r);  // click-through: never steals pointer input
        if (viewporter) o.vp = wp_viewporter_get_viewport(viewporter, o.surf);
        if (fracMgr) {
            o.frac = wp_fractional_scale_manager_v1_get_fractional_scale(fracMgr, o.surf);
            static const wp_fractional_scale_v1_listener fl{[](void* data, wp_fractional_scale_v1*, uint32_t s) {
                static_cast<Output*>(data)->scale120 = s;
            }};
            wp_fractional_scale_v1_add_listener(o.frac, &fl, &o);
        }
        o.layer = zwlr_layer_shell_v1_get_layer_surface(layerShell, o.surf, o.wo, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
                                                        "mousepet");
        zwlr_layer_surface_v1_set_anchor(o.layer, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                                      ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_size(o.layer, 0, 0);
        zwlr_layer_surface_v1_set_exclusive_zone(o.layer, -1);  // cover bars, never push windows around
        zwlr_layer_surface_v1_set_keyboard_interactivity(o.layer, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
        static const zwlr_layer_surface_v1_listener ll{
            [](void* data, zwlr_layer_surface_v1* l, uint32_t serial, uint32_t w, uint32_t h) {
                auto* o = static_cast<Output*>(data);
                zwlr_layer_surface_v1_ack_configure(l, serial);
                o->cw = int(w); o->ch = int(h);
                o->d->onConfigure(*o);
            },
            [](void* data, zwlr_layer_surface_v1*) {
                auto* o = static_cast<Output*>(data);
                o->d->destroySurfaces(*o);
            }};
        zwlr_layer_surface_v1_add_listener(o.layer, &ll, &o);

        o.pet = wl_compositor_create_surface(comp);
        wl_surface_set_input_region(o.pet, r);
        wl_region_destroy(r);
        o.sub = wl_subcompositor_get_subsurface(subcomp, o.pet, o.surf);
        wl_subsurface_place_above(o.sub, o.surf);
        if (viewporter) o.petVp = wp_viewporter_get_viewport(viewporter, o.pet);
        wl_surface_commit(o.pet);
        wl_surface_commit(o.surf);
    }

    void onConfigure(Output& o) {
        if (o.cw <= 0 || o.ch <= 0) { o.cw = o.lw; o.ch = o.lh; }
        wl_surface_attach(o.surf, clearPixel, 0, 0);
        wp_viewport_set_destination(o.vp, o.cw, o.ch);
        wl_surface_damage_buffer(o.surf, 0, 0, 1, 1);
        wl_surface_commit(o.surf);
        o.configured = true;
    }


    void maybeCreate(Output& o) {
        if (o.geometryKnown && !o.surf) createSurfaces(o);
    }

    void addOutput(uint32_t name, uint32_t version) {
        auto o = std::make_unique<Output>();
        o->d = this;
        o->globalName = name;
        o->wo = static_cast<wl_output*>(wl_registry_bind(reg, name, &wl_output_interface, std::min<uint32_t>(version, 4)));
        static const wl_output_listener ol{
            [](void*, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char*, const char*, int32_t) {},
            [](void*, wl_output*, uint32_t, int32_t, int32_t, int32_t) {},
            [](void* data, wl_output*) { static_cast<Output*>(data)->d->maybeCreate(*static_cast<Output*>(data)); },
            [](void* data, wl_output*, int32_t f) { static_cast<Output*>(data)->intScale = std::max(1, f); },
            [](void* data, wl_output*, const char* n) { static_cast<Output*>(data)->name = n; },
            [](void*, wl_output*, const char*) {}};
        wl_output_add_listener(o->wo, &ol, o.get());
        if (xdgOutputs) bindXdg(*o);
        outputs.push_back(std::move(o));
    }

    void bindXdg(Output& o) {
        o.xo = zxdg_output_manager_v1_get_xdg_output(xdgOutputs, o.wo);
        static const zxdg_output_v1_listener xl{
            [](void* data, zxdg_output_v1*, int32_t x, int32_t y) {
                auto* o = static_cast<Output*>(data); o->lx = x; o->ly = y; o->geometryKnown = true;
            },
            [](void* data, zxdg_output_v1*, int32_t w, int32_t h) {
                auto* o = static_cast<Output*>(data); o->lw = w; o->lh = h;
            },
            [](void* data, zxdg_output_v1*) {
                auto* o = static_cast<Output*>(data); o->d->maybeCreate(*o);
            },
            [](void* data, zxdg_output_v1*, const char* n) { static_cast<Output*>(data)->name = n; },
            [](void*, zxdg_output_v1*, const char*) {}};
        zxdg_output_v1_add_listener(o.xo, &xl, &o);
    }

    void removeOutput(uint32_t name) {
        for (auto it = outputs.begin(); it != outputs.end(); ++it) {
            if ((*it)->globalName != name) continue;
            destroyOutput(**it);
            outputs.erase(it);
            return;
        }
    }

    // ── Rendering ─────────────────────────────────────────────────────
    void hidePet(Output& o) {
        if (!o.petMapped) return;
        wl_surface_attach(o.pet, nullptr, 0, 0);
        wl_surface_commit(o.pet);
        wl_surface_commit(o.surf);
        o.petMapped = false;
        o.prevSlot = -1;
    }

    void drawOutput(Output& o, Pet& pet, const DrawOptions& opt, const Bounds& global) {
        if (!o.configured || !o.pet) return;
        const double fs = o.scale();
        // Region in output-local logical px, clipped to the output.
        float x0 = global.x0 - o.lx, y0 = global.y0 - o.ly, x1 = global.x1 - o.lx, y1 = global.y1 - o.ly;
        x0 = std::max(x0, 0.f); y0 = std::max(y0, 0.f);
        x1 = std::min(x1, float(o.cw)); y1 = std::min(y1, float(o.ch));
        if (!opt.visible || global.empty() || x1 <= x0 || y1 <= y0) { hidePet(o); o.dirty = false; return; }

        if (o.frameCb) {
            if (Clock::now() - o.frameAt < FRAME_TIMEOUT) return;  // still waiting: draw on the next tick
            wl_callback_destroy(o.frameCb);  // compositor did not present (e.g. output off); don't stall forever
            o.frameCb = nullptr;
        }

        // Align the origin so that origin*scale is an integer pixel (crisp at 0.8, 1.25, 1.5...).
        int step = 1;
        if (o.scale120) {
            step = 120 / std::gcd(int(o.scale120), 120);
            if (step > 12) step = 1;
        }
        const int rx = int(std::floor(x0 / step)) * step, ry = int(std::floor(y0 / step)) * step;
        int rw = int(std::ceil(x1 / step)) * step - rx, rh = int(std::ceil(y1 / step)) * step - ry;
        rw = std::max(rw, step); rh = std::max(rh, step);
        const int bw = std::max(1, int(std::lround(rw * fs))), bh = std::max(1, int(std::lround(rh * fs)));

        int slot = -1;
        for (int i = 0; i < SLOTS; i++)
            if (!o.slots[i].busy && i != o.prevSlot) { slot = i; break; }
        if (slot < 0) return;  // compositor holds every buffer; retried when one is released
        Buffer& b = o.slots[slot];
        if (!b.ensure(shm, bw, bh)) { std::fprintf(stderr, "[mousepet] shm allocation failed\n"); return; }

        cairo_t* cr = cairo_create(b.cs);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_paint(cr);
        if (opt.motionBlur && o.prevSlot >= 0 && o.prevScale == fs && o.slots[o.prevSlot].cs) {
            // Trail: previous frame (still valid in its buffer) shifted into the new region, then faded.
            cairo_set_source_surface(cr, o.slots[o.prevSlot].cs, std::round((o.prevX - rx) * fs), std::round((o.prevY - ry) * fs));
            cairo_paint(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OUT);
            cairo_set_source_rgba(cr, 0, 0, 0, 1);
            cairo_paint_with_alpha(cr, 0.35);
        }
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_scale(cr, fs, fs);
        cairo_translate(cr, -double(rx + o.lx), -double(ry + o.ly));
        cairo_scale(cr, opt.petScale, opt.petScale);
        pet.render(cr);
        cairo_destroy(cr);
        cairo_surface_flush(b.cs);

        wl_surface_attach(o.pet, b.buf, 0, 0);
        wl_surface_damage_buffer(o.pet, 0, 0, bw, bh);
        if (o.petVp) wp_viewport_set_destination(o.petVp, rw, rh);
        o.frameCb = wl_surface_frame(o.pet);
        o.frameAt = Clock::now();
        static const wl_callback_listener cl{[](void* data, wl_callback* cb, uint32_t) {
            auto* o = static_cast<Output*>(data);
            if (o->frameCb == cb) o->frameCb = nullptr;
            wl_callback_destroy(cb);
        }};
        wl_callback_add_listener(o.frameCb, &cl, &o);
        wl_surface_commit(o.pet);
        if (rx != o.subX || ry != o.subY) {
            wl_subsurface_set_position(o.sub, rx, ry);
            o.subX = rx; o.subY = ry;
        }
        wl_surface_commit(o.surf);  // applies the synchronized subsurface state atomically

        b.busy = true;
        o.dirty = false;
        o.petMapped = true;
        o.prevSlot = slot; o.prevX = rx; o.prevY = ry; o.prevScale = fs;
        commits++;
    }
};

static void registryGlobal(void* data, wl_registry* reg, uint32_t name, const char* iface, uint32_t version) {
    auto* d = static_cast<Overlay::Impl*>(data);
    auto bind = [&](const wl_interface* i, uint32_t v) { return wl_registry_bind(reg, name, i, std::min(version, v)); };
    if (!std::strcmp(iface, wl_compositor_interface.name)) d->comp = static_cast<wl_compositor*>(bind(&wl_compositor_interface, 4));
    else if (!std::strcmp(iface, wl_subcompositor_interface.name)) d->subcomp = static_cast<wl_subcompositor*>(bind(&wl_subcompositor_interface, 1));
    else if (!std::strcmp(iface, wl_shm_interface.name)) d->shm = static_cast<wl_shm*>(bind(&wl_shm_interface, 1));
    else if (!std::strcmp(iface, zwlr_layer_shell_v1_interface.name)) d->layerShell = static_cast<zwlr_layer_shell_v1*>(bind(&zwlr_layer_shell_v1_interface, 4));
    else if (!std::strcmp(iface, zxdg_output_manager_v1_interface.name)) {
        d->xdgOutputs = static_cast<zxdg_output_manager_v1*>(bind(&zxdg_output_manager_v1_interface, 3));
        for (auto& o : d->outputs)
            if (!o->xo) d->bindXdg(*o);
    }
    else if (!std::strcmp(iface, wp_viewporter_interface.name)) d->viewporter = static_cast<wp_viewporter*>(bind(&wp_viewporter_interface, 1));
    else if (!std::strcmp(iface, wp_fractional_scale_manager_v1_interface.name)) d->fracMgr = static_cast<wp_fractional_scale_manager_v1*>(bind(&wp_fractional_scale_manager_v1_interface, 1));
    else if (!std::strcmp(iface, wl_output_interface.name)) d->addOutput(name, version);
}

static void registryRemove(void* data, wl_registry*, uint32_t name) { static_cast<Overlay::Impl*>(data)->removeOutput(name); }

Overlay::Overlay() : d_(std::make_unique<Impl>()) {}
Overlay::~Overlay() = default;

bool Overlay::init(std::string& error) {
    d_->dpy = wl_display_connect(nullptr);
    if (!d_->dpy) { error = "cannot connect to the Wayland display (is WAYLAND_DISPLAY set?)"; return false; }
    d_->reg = wl_display_get_registry(d_->dpy);
    static const wl_registry_listener rl{registryGlobal, registryRemove};
    wl_registry_add_listener(d_->reg, &rl, d_.get());
    wl_display_roundtrip(d_->dpy);
    if (!d_->comp || !d_->subcomp || !d_->shm) { error = "compositor lacks wl_compositor/wl_subcompositor/wl_shm"; return false; }
    if (!d_->layerShell) { error = "compositor lacks zwlr_layer_shell_v1 (needed for a desktop overlay)"; return false; }
    if (!d_->xdgOutputs) { error = "compositor lacks zxdg_output_manager_v1 (needed to map the cursor to outputs)"; return false; }
    if (!d_->viewporter) { error = "compositor lacks wp_viewporter"; return false; }
    if (!d_->clearPixelStore.ensure(d_->shm, 1, 1)) { error = "shm allocation failed"; return false; }
    std::memset(d_->clearPixelStore.data, 0, 4);
    d_->clearPixel = d_->clearPixelStore.buf;
    wl_display_roundtrip(d_->dpy);  // output names / geometry / scales
    wl_display_roundtrip(d_->dpy);  // layer surface configures
    return true;
}

int Overlay::fd() const { return wl_display_get_fd(d_->dpy); }

bool Overlay::beforePoll() {
    while (wl_display_prepare_read(d_->dpy) != 0)
        if (wl_display_dispatch_pending(d_->dpy) < 0) return false;
    if (wl_display_flush(d_->dpy) < 0 && errno != EAGAIN) { wl_display_cancel_read(d_->dpy); return false; }
    return true;
}

bool Overlay::afterPoll(short revents) {
    if (revents & POLLIN) {
        if (wl_display_read_events(d_->dpy) < 0) return false;
    } else {
        wl_display_cancel_read(d_->dpy);
    }
    if (revents & (POLLERR | POLLHUP)) return false;
    return wl_display_dispatch_pending(d_->dpy) >= 0;
}

void Overlay::draw(Pet& pet, const DrawOptions& opt) {
    Bounds now = pet.bounds().scaled(opt.petScale);
    Bounds total = now;
    if (opt.motionBlur) {
        d_->trail.push_back(now);
        while (d_->trail.size() > TRAIL_FRAMES) d_->trail.pop_front();
        for (const auto& b : d_->trail) total.unite(b);
    } else {
        d_->trail.clear();
    }
    d_->lastTotal = total;
    for (auto& o : d_->outputs) {
        o->dirty = true;
        d_->drawOutput(*o, pet, opt, total);
    }
}

void Overlay::redrawPending(Pet& pet, const DrawOptions& opt) {
    for (auto& o : d_->outputs)
        if (o->dirty && !o->frameCb) d_->drawOutput(*o, pet, opt, d_->lastTotal);
}

unsigned long Overlay::commits() const { return d_->commits; }

std::string Overlay::describeOutputs() const {
    std::ostringstream s;
    for (const auto& o : d_->outputs) {
        s << (s.tellp() ? ", " : "") << (o->name.empty() ? "?" : o->name) << " " << o->lw << "x" << o->lh << "+" << o->lx
          << "+" << o->ly << " scale=" << o->scale() << (o->configured ? "" : " (unconfigured)");
    }
    return s.str();
}

}  // namespace mousepet
