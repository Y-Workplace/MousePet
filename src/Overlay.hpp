// Wayland overlay: one click-through wlr-layer-shell surface per output, plus a small
// subsurface that moves with the pet. Only the pet's bounding box is ever rasterised.
#pragma once

#include <memory>
#include <string>

#include "Sim.hpp"

namespace mousepet {

struct DrawOptions {
    float petScale = 1.f;   // simulation units -> logical px
    bool motionBlur = false;
    bool visible = true;
};

class Overlay {
public:
    Overlay();
    ~Overlay();
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;

    /** Connects to $WAYLAND_DISPLAY and creates the layer surfaces. */
    bool init(std::string& error);
    int fd() const;
    /** Flush + prepare_read; call before poll(). Returns false if the connection died. */
    bool beforePoll();
    /** After poll(): read events if readable and dispatch them. Returns false on fatal errors. */
    bool afterPoll(short revents);

    /** Draws the pet on every output it overlaps (skips outputs still waiting for a frame callback). */
    void draw(Pet& pet, const DrawOptions& opt);
    /** Draws outputs whose frame callback arrived after the last draw() skipped them. */
    void redrawPending(Pet& pet, const DrawOptions& opt);
    /** Number of buffers actually committed since start (for stats/tests). */
    unsigned long commits() const;
    std::string describeOutputs() const;

    struct Impl;  // public only so the C listener trampolines can name it

private:
    std::unique_ptr<Impl> d_;
};

}  // namespace mousepet
