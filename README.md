# MousePet

A cursor companion and physics-driven desktop pet written in modern C++20 for Wayland and Hyprland, faithful to the web prototype `MousePet.html`.

## Features
- **5 Complete Accessories**:
  - **Jellyfish** (Água-viva): Pulsing swimming physics, trailing tentacles via Verlet chains, glow aura.
  - **Balloon** (Balão): Gentle floating air oscillations, knot and gradient sheen, customizable size and hue.
  - **Star** (Estrela): Angular spin, twinkle trails, burst expansions, customizable points (4-8) and speed.
  - **Ghost** (Fantasma): Floating sway, eyes tracking the cursor, expressions (smile vs "O" on burst).
  - **Lantern Fish** (Peixe-lanterna): Dynamic tail fin, swimming bubble emitter, glowing bioluminescent lure.
- **Verlet Chain Physics**: Rope connects cursor to the pet with tension, drag, gravity and natural tilt.
- **High Performance & Low Overhead**:
  - Uses `wlr-layer-shell-unstable-v1` overlay with a synchronized moving subsurface.
  - Only paints the tight bounding box around the pet — never blits full screen surfaces every frame.
  - Triple-buffered SHM with fractional-scale and viewport support.
  - Click-through input regions (`empty_region`) so the mouse never clicks on the pet accidentally.
- **IPC & Remote Control**:
  - Built-in CLI client (`mousepet ctl ...`) to switch accessories, trigger burst, toggle motion blur, or adjust physics parameters in real time.
  - Integration with Hyprland IPC for high precision cursor tracking across multi-monitor setups.
- **Sandbox Testing**:
  - Fully tested inside an isolated nested Hyprland compositor so tests never disturb or risk crashing your active desktop session.

---

## Building and Testing

### Dependencies
- C++20 compiler (`gcc` or `clang`)
- `wayland-client`, `wayland-protocols`, `wayland-scanner`
- `cairo`
- `python3` (for test runner)

### Build
```bash
make -j$(nproc)
```

### Run Tests
```bash
# Run headless physics unit tests & snapshot tests
make test

# Run isolated sandbox test inside a nested Hyprland compositor
make sandbox-test
```

---

## Usage

### Run Daemon
```bash
# Run in foreground
./build/mousepet run

# Or preview in demo mode (orbits smoothly without requiring cursor movement)
./build/mousepet run --demo
```

### Control via CLI
```bash
# Trigger burst impulse (or press Space in web version)
./build/mousepet ctl burst

# Switch to next / previous accessory
./build/mousepet ctl next
./build/mousepet ctl prev

# Select specific accessory
./build/mousepet ctl select ghost

# Adjust physics / parameters
./build/mousepet ctl set ropeLength 80
./build/mousepet ctl set stiffness 0.6
./build/mousepet ctl set scale 1.2

# Toggle motion blur & visibility
./build/mousepet ctl blur toggle
./build/mousepet ctl toggle

# Query status (JSON)
./build/mousepet ctl status
```

### Hyprland Keybinds (Optional)
Add to your `hyprland.conf`:
```conf
bind = SUPER, Space, exec, mousepet ctl burst
bind = SUPER SHIFT, P, exec, mousepet ctl next
bind = SUPER SHIFT, O, exec, mousepet ctl toggle
```

### Systemd User Service
```bash
make install
systemctl --user enable --now mousepet.service
```
