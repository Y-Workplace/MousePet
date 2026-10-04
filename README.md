# MousePet / CursorPet

A cursor companion and physics-driven desktop pet written in modern C++26 for Wayland and Hyprland, faithful to the web prototype `MousePet.html`.

## Features
- **Modular & Extensible Accessory System**:
  - Each accessory lives in its own separate file under `src/accessories/`.
  - **Lua Scripting (via LuaJIT)**: Drop `.lua` files into `~/.config/mousepet/accessories/` or `./accessories/` to instantly create and customize pets without recompiling!
  - **C++ Auto-Compilation**: Drop `.cpp` files and MousePet automatically compiles them into shared libraries on execution!
  - Built-in accessories:
    - **Jellyfish** (Água-viva): Pulsing swimming physics, trailing tentacles via Verlet chains, glow aura.
    - **Balloon** (Balão): Gentle floating air oscillations, knot and gradient sheen, customizable size and hue.
    - **Star** (Estrela): Angular spin, twinkle trails, burst expansions, customizable points (4-8) and speed.
    - **Ghost** (Fantasma): Floating sway, eyes tracking the cursor, expressions (smile vs "O" on burst).
    - **Lantern Fish** (Peixe-lanterna): Dynamic tail fin, swimming bubble emitter, glowing bioluminescent lure.
    - **Magic Bubble** (Bolha em Lua): Sample scriptable pet with iridescent shading and water particles.
- **Anchored Rope Constraint Loop**:
  - High-frequency Verlet relaxation loop runs every frame prior to rendering.
  - When dragging or flicking the cursor fast, the rope remains strictly anchored to the cursor tip with zero floating or lag.
- **Click Burst Effects**:
  - Companion Hyprland hook plugin (`mousepet-hyprland.so`) captures pointer down clicks in real time and streams coordinates via zero-latency POSIX shared memory.
  - Control socket command (`cursorpet burst`) and Hyprland binds can also trigger bursts.
- **Ergonomic CLI & Environment Variables**:
  - Control commands from anywhere: `cursorpet pet next`, `cursorpet pet prev`, `cursorpet pet <name>`, `cursorpet --pet --next`.
  - Startup defaults via `MOUSEPET_ACCESSORY=star` or `CURSORPET_ACCESSORY=fish`.
  - Scale factor via `MOUSEPET_SCALE=1.2`.
- **High Performance & Low Overhead**:
  - Uses `wlr-layer-shell-unstable-v1` overlay with a synchronized moving subsurface.
  - Only paints the tight bounding box around the pet — never blits full screen surfaces every frame.
  - Triple-buffered SHM with fractional-scale and viewport support.
  - Transparent click-through input regions (`empty_region`).
- **Sandbox Testing**:
  - Tested inside an isolated nested Hyprland compositor so tests never disturb or risk crashing your active desktop session.

---

## Installation

```bash
bash install.sh
```

This compiles and installs:
- Executables: `~/.local/bin/mousepet` and symlink `~/.local/bin/cursorpet`
- Hyprland plugin: `~/.local/lib/mousepet-hyprland.so`
- Accessories folder: `~/.config/mousepet/accessories/`
- Systemd user service: `~/.config/systemd/user/mousepet.service`

---

## Usage

### Run Daemon
```bash
mousepet run
# or
cursorpet run

# Orbit preview without requiring cursor tracking
cursorpet run --demo
```

### Switching Pets (CLI)
```bash
cursorpet pet next
cursorpet pet prev
cursorpet pet jellyfish
cursorpet pet star

# Flag syntax also supported:
cursorpet --pet --next
cursorpet --pet balloon
```

### Trigger Burst Effect
```bash
cursorpet burst
```

### Hyprland Plugin (Real-Time Clicks & 0-Latency Tracking)
```bash
cursorpet plugin load      # Loads mousepet-hyprland into Hyprland
cursorpet plugin status    # Checks plugin status
cursorpet plugin unload    # Unloads the plugin
```

### Environment Variables
```bash
export CURSORPET_ACCESSORY="star"
export CURSORPET_SCALE="1.2"
mousepet run
```

---

## Adding Custom Accessories

Drop your custom pet script into `~/.config/mousepet/accessories/` or `./accessories/`:

### Option A: Lua Script (`~/.config/mousepet/accessories/meupet.lua`)
```lua
return {
    id = "meupet",
    name = "Meu Pet",
    color = 0x60a5fa,
    rope = { color = 0x38bdf8, width = 2.0 },
    particle = { color = 0x93c5fd, size = 2.5, gravity = -0.04 },
    body = { orbitX = 40, orbitY = -28, sway = 6 },

    update = function(pet)
        pet.body.vy = pet.body.vy + math.sin(pet.time * 4) * 0.02
    end,

    onBurst = function(pet)
        for i = 1, 12 do
            local a = pet:random() * 6.28
            pet:emit(pet.body.x, pet.body.y, math.cos(a) * 2, math.sin(a) * 2, 1.0)
        end
    end,

    render = function(cr, pet)
        cr:set_source_rgba(0.2, 0.7, 1.0, 0.8)
        cr:circle(pet.body.x, pet.body.y, 16)
        cr:fill()
    end
}
```

### Option B: C++ Source (`~/.config/mousepet/accessories/meupet.cpp`)
MousePet detects modified `.cpp` files on execution and automatically compiles them using `g++` into dynamic `.so` plugins!

---

## Running Tests

```bash
# Unit tests + Snapshot PNG tests for all accessories
make test

# Nested compositor sandbox test (zero risk to current session)
make sandbox-test

# Plugin load/unload sandbox test
python3 tests/plugin_sandbox_test.py
```
