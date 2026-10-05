#!/usr/bin/env python3
"""Package prebuilt MousePet binaries and installation files for GitHub Releases."""
import hashlib
import pathlib
import shutil
import subprocess
import tarfile
import tempfile


def main():
    source = pathlib.Path(__file__).resolve().parent.parent
    binary = source / "build/mousepet"
    plugin = source / "build/mousepet-hyprland.so"
    if not binary.is_file() or not plugin.is_file():
        raise SystemExit("Build first: make all")
    with tempfile.TemporaryDirectory(prefix="mousepet-release-") as temporary:
        root = pathlib.Path(temporary)
        probe = root / "mousepet"
        shutil.copyfile(binary, probe)
        probe.chmod(0o755)
        version = subprocess.check_output([str(probe), "--version"], text=True).strip()
        architecture = subprocess.check_output(["file", "-b", str(binary)], text=True)
        if "x86-64" not in architecture:
            raise SystemExit("This release package currently supports x86-64 binaries only")
        name = f"mousepet-{version}-linux-x86_64"
        package = root / name
        package.mkdir()
        for filename in ("Makefile", "README.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "install.sh"):
            shutil.copy2(source / filename, package / filename)
        for directory in ("scripts", "accessories", "systemd", "docs"):
            shutil.copytree(source / directory, package / directory,
                            ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        for header in (source / "src").rglob("*.hpp"):
            target = package / header.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(header, target)
        (package / "build").mkdir()
        for artifact in (binary, plugin):
            target = package / "build" / artifact.name
            shutil.copyfile(artifact, target)
            target.chmod(0o755)
            subprocess.run(["strip", "--strip-unneeded", str(target)], check=True)
        (package / "INSTALL.txt").write_text(
            "MousePet " + version + " - Linux x86-64 prebuilt package\n\n"
            "Requires glibc 2.43+, GLIBCXX_3.4.35, Wayland, Cairo and LuaJIT.\n"
            "The companion plugin requires Hyprland 0.56.2 and the matching build hash.\n"
            "For other systems, use the source installer from the repository.\n\n"
            "Install from this directory:\n"
            "  bash install.sh --skip-build --no-deps\n"
            "  export PATH=\"$HOME/.local/bin:$PATH\"\n"
            "  mousepet run\n\n"
            "Optional Hyprland startup: mousepet autostart enable\n"
            "See README.md and THIRD_PARTY_NOTICES.md for details.\n")
        output = source / "build/releases"
        output.mkdir(parents=True, exist_ok=True)
        archive = output / (name + ".tar.gz")
        with tarfile.open(archive, "w:gz") as bundle:
            bundle.add(package, arcname=name)
        checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
        (output / "SHA256SUMS").write_text(f"{checksum}  {archive.name}\n")
        print(archive)
        print(output / "SHA256SUMS")


if __name__ == "__main__":
    main()
