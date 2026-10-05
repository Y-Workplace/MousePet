#!/usr/bin/env python3
"""Build and install MousePet without root; configure future terminal PATHs."""
import argparse
import os
import pathlib
import shlex
import shutil
import subprocess
import sys
import time


def atomic_copy(source, destination, mode=0o644):
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + f".new.{os.getpid()}")
    shutil.copyfile(source, temporary)
    temporary.chmod(mode)
    temporary.replace(destination)


def configure_path(path, block, begin="# BEGIN MousePet PATH", end="# END MousePet PATH"):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_symlink():
        path = path.resolve()
    original = path.read_text() if path.exists() else ""
    start, finish = original.find(begin), original.find(end)
    if (start < 0) != (finish < 0) or (start >= 0 and finish < start):
        raise RuntimeError(f"Malformed MousePet PATH block in {path}; left unchanged")
    updated = original
    if start >= 0:
        finish += len(end)
        if original[finish:finish + 1] == "\n":
            finish += 1
        updated = original[:start] + original[finish:]
    if updated and not updated.endswith("\n"):
        updated += "\n"
    updated += f"{begin}\n{block}\n{end}\n"
    if updated == original:
        return
    if path.exists():
        shutil.copy2(path, path.with_name(path.name + f".mousepet.bak.{time.time_ns()}"))
    temporary = path.with_name(path.name + f".mousepet.tmp.{os.getpid()}")
    temporary.write_text(updated)
    temporary.chmod(path.stat().st_mode & 0o777 if path.exists() else 0o600)
    temporary.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parent.parent)
    parser.add_argument("--prefix", type=pathlib.Path, default=pathlib.Path(os.environ.get("PREFIX", str(pathlib.Path.home() / ".local"))))
    parser.add_argument("--no-deps", action="store_true", help="skip automatic Arch dependency installation")
    parser.add_argument("--no-path", action="store_true", help="do not change terminal configuration")
    parser.add_argument("--no-service", action="store_true", help="do not install a user systemd service")
    parser.add_argument("--skip-build", action="store_true", help="install previously built binaries")
    parser.add_argument("--core-only", action="store_true", help="omit Hyprland plugin (no automatic clicks or hiding)")
    parser.add_argument("--autostart", action="store_true", help="enable startup in an existing Hyprland config")
    args = parser.parse_args()
    source, prefix = args.source.resolve(), args.prefix.expanduser().resolve()
    if not args.skip_build:
        for command in ("make", "g++", "pkg-config", "wayland-scanner"):
            if not shutil.which(command):
                raise RuntimeError(f"Missing {command}; see README.md build dependencies")
        packages = ["wayland-client", "cairo", "luajit", "wayland-protocols"]
        if not args.core_only:
            packages += ["hyprland", "pixman-1", "libdrm"]
        if subprocess.run(["pkg-config", "--exists", *packages]).returncode:
            raise RuntimeError("Missing development dependencies; see README.md. Hyprland headers must match the running compositor.")
        subprocess.run(["make", "-j", str(min(os.cpu_count() or 2, 6)), "core" if args.core_only else "all"], cwd=source, check=True)
    atomic_copy(source / "build/mousepet", prefix / "bin/mousepet", 0o755)
    if not args.core_only:
        atomic_copy(source / "build/mousepet-hyprland.so", prefix / "lib/mousepet-hyprland.so", 0o755)
    alias = prefix / "bin/cursorpet"
    if alias.is_symlink():
        alias.unlink()
    if not alias.exists():
        alias.symlink_to("mousepet")
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md", "README.md"):
        atomic_copy(source / name, prefix / "share/doc/mousepet" / name)
    for header in (source / "src").rglob("*.hpp"):
        atomic_copy(header, prefix / "include/mousepet" / header.relative_to(source / "src"))
    for example in (source / "accessories").glob("*.lua"):
        atomic_copy(example, prefix / "share/mousepet/accessories" / example.name)
    config = pathlib.Path(os.environ.get("XDG_CONFIG_HOME", str(pathlib.Path.home() / ".config")))
    service = (source / "systemd/mousepet.service").read_text()
    escaped = str(prefix / "bin/mousepet").replace("%", "%%").replace("\\", "\\\\").replace('"', '\\"')
    service = service.replace("%h/.local/bin/mousepet run", f'"{escaped}" run')
    target = config / "systemd/user/mousepet.service"
    if not args.no_service:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(service)
    if not args.no_path:
        block = f"export PATH={shlex.quote(str(prefix / 'bin'))}:\"$PATH\""
        configure_path(pathlib.Path.home() / ".bashrc", block)
        configure_path(pathlib.Path(os.environ.get("ZDOTDIR", str(pathlib.Path.home()))) / ".zshrc", block)
        configure_path(config / "fish/conf.d/mousepet.fish", f"fish_add_path -- {shlex.quote(str(prefix / 'bin'))}")
    if args.autostart:
        subprocess.run([str(prefix / "bin/mousepet"), "autostart", "enable"], check=True)
    print(f"Installed MousePet in {prefix / 'bin'}")
    print("No MousePet environment variables are required inside Hyprland.")
    print(f"Start now: {shlex.quote(str(prefix / 'bin/mousepet'))} run")
    print("Future terminals: mousepet run")
    print('For this terminal: export PATH="$HOME/.local/bin:$PATH" (default prefix)')
    print("Optional startup: mousepet autostart enable")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print(f"Installation failed: {error}", file=sys.stderr)
        sys.exit(1)
