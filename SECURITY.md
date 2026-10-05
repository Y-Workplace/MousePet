# Security

The supported development version is the current main branch. Hyprland plugin
compatibility is currently tested with Hyprland 0.56.2; it must match the running
compositor's ABI. Rebuild after upgrades.

MousePet runs as the user, observes pointer events inside Hyprland, and accepts
local control commands through a per-display Unix socket. Custom Lua/C++
accessories are executable code; install only trusted accessories. They are not
a security boundary or an isolation mechanism.

For a vulnerability, use GitHub's private vulnerability reporting if it is
enabled for this repository. Otherwise contact the repository owner through
their public GitHub contact method. Avoid publishing exploit details or private
credentials in a public issue. For ordinary crashes or visual bugs, use Issues.
