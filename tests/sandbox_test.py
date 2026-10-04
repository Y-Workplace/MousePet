#!/usr/bin/env python3
"""
Nested Hyprland sandbox integration test for MousePet.
Runs MousePet inside an isolated Hyprland compositor so it never touches the user's desktop session.
Tests:
- Startup in headless/nested wayland environment
- Control socket connection and status querying
- Switching accessories, settings adjustment, bursts
- Clean shutdown
"""
import argparse
import json
import os
import pathlib
import shutil
import signal
import subprocess
import tempfile
import time
import sys

def main():
    parser = argparse.ArgumentParser(description="MousePet isolated sandbox test")
    parser.add_argument("--binary", required=True, type=pathlib.Path, help="Path to mousepet binary")
    args = parser.parse_args()

    binary = args.binary.resolve()
    loader = "/lib64/ld-linux-x86-64.so.2" if os.path.exists("/lib64/ld-linux-x86-64.so.2") else None

    # Verify Hyprland exists
    if not shutil.which("Hyprland"):
        print("Hyprland not found in PATH; skipping nested compositor sandbox test.")
        sys.exit(0)

    outer_runtime = os.environ.get("XDG_RUNTIME_DIR", "")
    outer_display = os.environ.get("WAYLAND_DISPLAY", "")
    if not outer_runtime or not outer_display:
        print("Not running in a Wayland session; skipping nested compositor sandbox test.")
        sys.exit(0)

    outer_socket = pathlib.Path(outer_display) if outer_display.startswith("/") else pathlib.Path(outer_runtime) / outer_display
    if not outer_socket.exists():
        print(f"Parent Wayland socket {outer_socket} does not exist; skipping.")
        sys.exit(0)

    # Use short prefix 'mp-' and short runtime folder 'r' so UNIX domain socket path stays < 108 chars!
    base = pathlib.Path(tempfile.mkdtemp(prefix="mp-"))
    runtime = base / "r"
    runtime.mkdir(mode=0o700)
    cache = base / "c"
    cache.mkdir()
    config = base / "test.lua"
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
        XDG_CACHE_HOME=str(cache),
        XDG_CONFIG_HOME=str(base / "cfg"),
        WAYLAND_DISPLAY=str(outer_socket),
        AQ_DRM_DEVICES="/dev/nonexistent",
        HYPRLAND_NO_SD_VARS="1",
        HYPRLAND_NO_SD_NOTIFY="1",
        DBUS_SESSION_BUS_ADDRESS="",
    )

    log = base / "hyprland.log"
    print(f"Launching isolated Hyprland sandbox in {base}...")

    with log.open("w") as out:
        hypr_proc = subprocess.Popen(
            ["Hyprland", "--config", str(config)],
            env=env,
            stdout=out,
            stderr=subprocess.STDOUT,
            start_new_session=True
        )

        pet_proc = None
        try:
            instance = None
            for _ in range(120):
                if hypr_proc.poll() is not None:
                    raise RuntimeError(f"Nested compositor exited prematurely: code {hypr_proc.returncode}")
                matches = list((runtime / "hypr").glob("*/.socket.sock")) if (runtime / "hypr").exists() else []
                if matches:
                    instance = matches[0].parent.name
                    break
                time.sleep(0.1)

            if not instance:
                items = list(runtime.rglob("*"))
                raise RuntimeError(f"Timed out waiting for Hyprland instance socket. runtime contents: {items}")

            print(f"Isolated Hyprland instance ready: {instance}")

            nested_env = env.copy()
            nested_env["HYPRLAND_INSTANCE_SIGNATURE"] = instance
            nested_env["WAYLAND_DISPLAY"] = str(runtime / "wayland-1")

            ctl_sock = runtime / "ctl.sock"
            nested_env["MOUSEPET_SOCKET"] = str(ctl_sock)
            nested_env["MOUSEPET_CONFIG"] = str(base / "settings.ini")

            pet_log = (base / "mousepet.log").open("w")
            cmd = [loader, str(binary), "run", "--demo"] if loader else [str(binary), "run", "--demo"]
            pet_proc = subprocess.Popen(
                cmd,
                env=nested_env,
                stdout=pet_log,
                stderr=subprocess.STDOUT,
                start_new_session=True
            )

            # Wait for control socket to appear
            for _ in range(80):
                if pet_proc.poll() is not None:
                    raise RuntimeError(f"MousePet exited prematurely: code {pet_proc.returncode}\n{(base / 'mousepet.log').read_text()}")
                if ctl_sock.exists():
                    break
                time.sleep(0.1)

            if not ctl_sock.exists():
                raise RuntimeError(f"Control socket {ctl_sock} was not created\nLogs:\n{(base / 'mousepet.log').read_text()}")

            print("MousePet is running inside sandbox! Testing control commands...")

            def ctl(*args):
                c = [loader, str(binary), "ctl", *args] if loader else [str(binary), "ctl", *args]
                res = subprocess.run(c, env=nested_env, capture_output=True, text=True, timeout=5)
                if res.returncode != 0:
                    raise RuntimeError(f"ctl command failed: {' '.join(args)}: {res.stderr} {res.stdout}")
                return res.stdout.strip()

            # Test status command
            status_raw = ctl("status")
            status = json.loads(status_raw)
            assert status["accessory"] in ["jellyfish", "balloon", "star", "ghost", "fish"]
            print(f"Initial status: accessory={status['accessory']}, state={status['state']}")

            # Test burst
            res = ctl("burst")
            assert res == "ok"
            print("Burst command succeeded")

            # Test select
            res = ctl("select", "star")
            assert res == "ok star"
            status = json.loads(ctl("status"))
            assert status["accessory"] == "star"
            print("Switched accessory to star")

            # Test set parameter
            res = ctl("set", "points", "7")
            assert res == "ok"
            status = json.loads(ctl("status"))
            assert status["params"]["points"] == 7
            print("Set points=7 successfully")

            # Test toggle blur & visibility
            ctl("blur", "on")
            ctl("hide")
            status = json.loads(ctl("status"))
            assert status["motionBlur"] is True
            assert status["visible"] is False
            ctl("show")

            # Test quit
            res = ctl("quit")
            assert res == "ok"
            pet_proc.wait(timeout=5)
            print("MousePet exited cleanly via ctl quit")

            print("ALL SANDBOX INTEGRATION TESTS PASSED!")

        finally:
            if pet_proc and pet_proc.poll() is None:
                try:
                    pet_proc.terminate()
                    pet_proc.wait(timeout=2)
                except Exception:
                    pet_proc.kill()
            if hypr_proc and hypr_proc.poll() is None:
                try:
                    hypr_proc.terminate()
                    hypr_proc.wait(timeout=2)
                except Exception:
                    hypr_proc.kill()
            shutil.rmtree(base, ignore_errors=True)

if __name__ == "__main__":
    main()
