#!/usr/bin/env python3
"""
Isolated sandbox test for mousepet-hyprland plugin.
Verifies loading the plugin into a nested Hyprland compositor, tracking motion, and unloading cleanly.
"""
import os
import pathlib
import shutil
import subprocess
import tempfile
import time
import sys
import struct

def main():
    plugin = pathlib.Path("build/mousepet-hyprland.so").resolve()
    if not plugin.exists():
        raise RuntimeError("Build the plugin first: make plugin")

    if not shutil.which("Hyprland"):
        print("Hyprland not available, skipping.")
        return

    outer_runtime = os.environ.get("XDG_RUNTIME_DIR", "")
    outer_display = os.environ.get("WAYLAND_DISPLAY", "")
    if not outer_runtime or not outer_display:
        print("No Wayland display found, skipping.")
        return

    base = pathlib.Path(tempfile.mkdtemp(prefix="mp-plg-"))
    runtime = base / "r"
    runtime.mkdir(mode=0o700)
    config = base / "test.lua"
    outer_socket = pathlib.Path(outer_display) if outer_display.startswith("/") else pathlib.Path(outer_runtime) / outer_display

    config.write_text(
        'hl.env("XCURSOR_THEME","Bibata-Modern-Ice")\n'
        'hl.env("XCURSOR_SIZE","24")\n'
        'hl.monitor({output="", mode="preferred", position="auto", scale=1})\n'
        'hl.config({debug={enable_stdout_logs=true,disable_logs=false}})\n'
    )

    env = os.environ.copy()
    env.pop("HYPRLAND_INSTANCE_SIGNATURE", None)
    env.update(
        XDG_RUNTIME_DIR=str(runtime),
        XDG_CACHE_HOME=str(base / "c"),
        XDG_CONFIG_HOME=str(base / "cfg"),
        WAYLAND_DISPLAY=str(outer_socket),
        AQ_DRM_DEVICES="/dev/nonexistent",
        HYPRLAND_NO_SD_VARS="1",
        HYPRLAND_NO_SD_NOTIFY="1",
        DBUS_SESSION_BUS_ADDRESS="",
    )

    log = base / "hyprland.log"
    print("Starting isolated Hyprland sandbox for plugin verification...")

    with log.open("w") as out:
        hypr = subprocess.Popen(
            ["Hyprland", "--config", str(config)],
            env=env,
            stdout=out,
            stderr=subprocess.STDOUT,
            start_new_session=True
        )

        try:
            instance = None
            for _ in range(100):
                if hypr.poll() is not None:
                    raise RuntimeError("Sandbox Hyprland crashed prematurely")
                matches = list((runtime / "hypr").glob("*/.socket.sock")) if (runtime / "hypr").exists() else []
                if matches:
                    instance = matches[0].parent.name
                    break
                time.sleep(0.1)

            if not instance:
                raise RuntimeError("Timed out waiting for nested Hyprland socket")

            print(f"Isolated compositor running: {instance}")
            nested_env = env.copy()
            nested_env["HYPRLAND_INSTANCE_SIGNATURE"] = instance

            # Load plugin
            load_res = subprocess.run(
                ["hyprctl", "plugin", "load", str(plugin)],
                env=nested_env,
                capture_output=True,
                text=True,
                timeout=5
            )
            print(f"hyprctl plugin load output: {load_res.stdout.strip()}")
            assert load_res.returncode == 0

            # Check plugin list
            list_res = subprocess.run(
                ["hyprctl", "plugin", "list"],
                env=nested_env,
                capture_output=True,
                text=True,
                timeout=5
            )
            print(f"Active plugins in sandbox:\n{list_res.stdout.strip()}")
            assert "mousepet-hyprland" in list_res.stdout

            # No pointer input is needed to keep visibility and liveness current.
            bridge_path = pathlib.Path("/dev/shm") / ("mousepet_bridge_shm_" + instance)
            before = bridge_path.read_bytes()
            assert before[40] == 1
            heartbeat = struct.unpack_from("Q", before, 32)[0]
            # The IPC socket appears before the nested output finishes configuring.
            for _ in range(100):
                time.sleep(0.05)
                after = bridge_path.read_bytes()
                if struct.unpack_from("Q", after, 32)[0] > heartbeat:
                    break
            else:
                raise AssertionError("Idle heartbeat stalled")
            assert int(time.time() * 1000) - struct.unpack_from("Q", after, 32)[0] < 1000

            # Unload plugin cleanly
            unload_res = subprocess.run(
                ["hyprctl", "plugin", "unload", str(plugin)],
                env=nested_env,
                capture_output=True,
                text=True,
                timeout=5
            )
            print(f"hyprctl plugin unload output: {unload_res.stdout.strip()}")
            assert unload_res.returncode == 0

            assert bridge_path.read_bytes()[40] == 0
            bridge_path.unlink()
            print("PLUGIN SANDBOX TEST PASSED: loading, idle heartbeat and clean unloading!")

        except Exception:
            print(log.read_text())
            raise
        finally:
            if hypr and hypr.poll() is None:
                hypr.terminate()
                hypr.wait(timeout=2)
            shutil.rmtree(base, ignore_errors=True)

if __name__ == "__main__":
    main()
