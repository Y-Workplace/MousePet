# Custom MousePet accessories

MousePet loads shipped examples from its installation directory and user
accessories from `$XDG_CONFIG_HOME/mousepet/accessories` (normally
`~/.config/mousepet/accessories`). User IDs can override shipped examples.
Only load code you trust. These accessories run inside the MousePet process.

An optional `MOUSEPET_ACCESSORY_DIR` adds another directory. Repository examples
are opt-in: `MOUSEPET_ACCESSORY_DIR="$PWD/accessories" mousepet run`.
Restart MousePet after adding or changing an accessory.

## Lua

Copy `bubble.lua` to your user accessory directory as a starting point. It is
also installed as a shipped example. Each script returns a definition with
an ID, parameters, and optional update/onBurst hooks, plus a render hook.

```lua
return {
    id = "mycompanion",
    name = "My Companion",
    color = 0x60a5fa,
    body = { orbitX = 40, orbitY = -28, sway = 6 },
    render = function(cr, pet)
        cr:set_source_rgba(0.3, 0.6, 1.0, 0.9)
        cr:circle(pet.body.x, pet.body.y, 14)
        cr:fill()
    end,
}
```

See `src/accessories/LuaLoader.cpp` and the complete `bubble.lua` example for the
available API. Keep rendering inside the declared/default bounds.

## C++

Native repository companions belong in `src/accessories/` and build with Make.
For user companions, MousePet can compile `.cpp` files into adjacent `.so` files
and load a `mousepet_accessory` factory returning an `AccessoryDef`. It needs g++,
pkg-config and Cairo development files. The installer provides MousePet headers
under `~/.local/include/mousepet`. Include `accessories/Common.hpp` and follow a
native companion's factory structure.

The C++ interface is not a stable binary ABI. Rebuild custom `.so` files after
MousePet or compiler upgrades. Avoid loading a plugin built for a different
MousePet version. A custom C++ accessory is separate from the Hyprland hook
plugin, which must match Hyprland's own ABI.
