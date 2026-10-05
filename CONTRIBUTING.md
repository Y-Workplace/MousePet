# Contributing

Bug reports and pull requests are welcome. Describe the observed behavior, the
expected behavior, and a minimal reproduction. Include your Hyprland version,
distribution, companion ID, scale, and whether the plugin is connected.

## Development

Install the dependencies listed in the README, then run:

```sh
make -j"$(nproc)" core
make test
python3 tests/setup_test.py --binary build/mousepet
```

Build the full Hyprland plugin with `make plugin` against matching compositor
headers. Verify plugin changes in a nested compositor first:

```sh
make sandbox-test
python3 tests/plugin_sandbox_test.py
```

Do not load untested compositor hooks into your primary desktop. The nested tests
skip when Hyprland or an outer Wayland session is unavailable.

## Adding companions

Put a native companion in `src/accessories/` and register it with a unique ID.
Provide accurate drawing bounds, stable physics, a distinct burst, and reasonable
parameter limits. The Makefile discovers these source files automatically.
Use the headless render tests to catch drawing outside bounds and invalid Cairo
transforms. Update the README companion table.

Custom Lua/C++ accessories are described in `accessories/README.md`. Use
`MOUSEPET_ACCESSORY_DIR="$PWD/accessories"` to load repository examples during
development. The application does not automatically execute code from arbitrary
working directories.

Keep changes focused. Add meaningful tests for behavior or installer changes;
do not commit generated binaries, credentials, local settings, or build outputs.
Contributions are provided under the repository's MIT license. Retain notices
when using third-party code and identify where it came from.
