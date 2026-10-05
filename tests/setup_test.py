#!/usr/bin/env python3
"""Exercise installation and Hyprland autostart without touching the user's files."""
import argparse
import os
import pathlib
import shlex
import shutil
import subprocess
import tempfile
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=pathlib.Path, required=True)
    args = parser.parse_args()
    source = pathlib.Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix="mousepet-setup-") as directory:
        root = pathlib.Path(directory)
        home = root / "home"
        home.mkdir()
        config = home / ".config"
        config.mkdir()
        env = os.environ.copy()
        for key in list(env):
            if key.startswith(("MOUSEPET_", "CURSORPET_")):
                env.pop(key)
        env.update(HOME=str(home), XDG_CONFIG_HOME=str(config), ZDOTDIR=str(home))
        prefix = root / "install space ' $literal"
        executable = prefix / "bin/mousepet"
        # Installation copies the executable off potentially non-executable mounts.
        installer = ["python3", str(source / "scripts/install.py"), "--source", str(source),
                     "--prefix", str(prefix), "--skip-build", "--core-only"]
        assert args.binary.resolve() == source / "build/mousepet", "setup test expects build/mousepet"
        bashrc = home / ".bashrc"
        original_rc = "# Existing terminal settings\nexport SETUP_TEST_PRESERVED=yes\n"
        bashrc.write_text(original_rc)
        hypr = config / "hypr"
        hypr.mkdir()
        lua = hypr / "hyprland.lua"
        original_lua = "-- Existing Hyprland settings\nhl.config({general={gaps_in=5}})\n"
        lua.write_text(original_lua)

        def run(command, expected=0):
            result = subprocess.run(command, env=env, cwd=root, text=True, capture_output=True)
            assert result.returncode == expected, (command, result.stdout, result.stderr)
            return result.stdout

        run(installer)
        assert lua.read_text() == original_lua, "default installation must not enable autostart"
        assert "0.3.0" in run([str(executable), "--version"])
        assert (prefix / "share/doc/mousepet/LICENSE").exists()
        assert (prefix / "include/mousepet/accessories/Common.hpp").exists()
        assert (prefix / "bin/cursorpet").resolve() == executable
        assert "bubble\t" in run([str(executable), "list"])
        rc_after = bashrc.read_text()
        assert rc_after.startswith(original_rc)
        assert str(executable) in run(["bash", "-c", 'source "$HOME/.bashrc"; command -v mousepet'])
        backups = list(home.glob(".bashrc.mousepet.bak.*"))
        run(installer)
        assert bashrc.read_text() == rc_after
        assert list(home.glob(".bashrc.mousepet.bak.*")) == backups

        def autostart(*arguments, expected=0):
            return run([str(executable), "autostart", *arguments], expected)

        assert "disabled:" in autostart("status")
        autostart("enable")
        enabled = lua.read_text()
        assert enabled.startswith(original_lua)
        assert enabled.count("BEGIN MousePet autostart") == 1
        lua_backups = list(hypr.glob("hyprland.lua.mousepet.bak.*"))
        assert len(lua_backups) == 1 and lua_backups[0].read_text() == original_lua
        assert "enabled:" in autostart("status")
        autostart("enable")
        assert lua.read_text() == enabled
        assert list(hypr.glob("hyprland.lua.mousepet.bak.*")) == lua_backups
        # Verify generated Lua parses and passes a correctly quoted shell command.
        lua_command = shutil.which("luajit") or shutil.which("lua")
        if lua_command:
            env["MOUSEPET_SETUP_LUA"] = str(lua)
            captured = run([lua_command, "-e", 'hl={config=function() end,on=function(_,f) f() end,exec_cmd=print}; dofile(os.getenv("MOUSEPET_SETUP_LUA"))'])
            assert shlex.split(captured.strip()) == [str(executable), "run"]
        autostart("disable")
        assert lua.read_text() == original_lua
        autostart("disable")
        assert lua.read_text() == original_lua
        conf = hypr / "hyprland.conf"
        original_conf = "# Existing legacy config\nexec-once = waybar\n"
        conf.write_text(original_conf)
        autostart("enable", "--config", str(conf))
        command = next(line.split(" = ", 1)[1] for line in conf.read_text().splitlines() if "mousepet' run" in line)
        assert shlex.split(command) == [str(executable), "run"]
        autostart("disable", "--config", str(conf))
        assert conf.read_text() == original_conf
        link = hypr / "linked.lua"
        link.symlink_to(lua)
        autostart("enable", "--config", str(link))
        assert link.is_symlink() and "BEGIN MousePet" in lua.read_text()
        autostart("disable", "--config", str(link))
        assert lua.read_text() == original_lua
        malformed = "-- BEGIN MousePet autostart\n"
        lua.write_text(malformed)
        autostart("enable", expected=1)
        assert lua.read_text() == malformed
        lua.write_text(original_lua)
        run(installer + ["--autostart"])
        assert "enabled:" in autostart("status")
        autostart("disable")
        # External C++ accessories must resolve host symbols and quoted paths.
        custom = config / "mousepet/accessories"
        custom.mkdir(parents=True)
        (custom / "custom space.cpp").write_text(
            '#include "accessories/Registry.hpp"\n'
            'extern "C" mousepet::AccessoryDef mousepet_accessory() {\n'
            'mousepet::AccessoryDef d{}; if(auto* base=mousepet::findAccessory("star")) d=*base;\n'
            'd.id="setup_custom"; d.name="Setup custom"; return d; }\n')
        assert "setup_custom\t" in run([str(executable), "list"])
        assert (custom / "custom space.so").exists()
        # Reproduce curl | bash bootstrap with an archive and a local download stub.
        archive = root / "source.tar.gz"
        with tarfile.open(archive, "w:gz") as bundle:
            for name in ("Makefile", "scripts", "src", "accessories", "systemd", "LICENSE",
                         "THIRD_PARTY_NOTICES.md", "README.md", "build/mousepet"):
                bundle.add(source / name, arcname="MousePet-main/" + name)
        mockbin = root / "mockbin"
        mockbin.mkdir()
        curl = mockbin / "curl"
        curl.write_text('#!/usr/bin/env python3\nimport os,shutil,sys\nshutil.copyfile(os.environ["MOUSEPET_SETUP_ARCHIVE"],sys.argv[-1])\n')
        curl.chmod(0o755)
        env["MOUSEPET_SETUP_ARCHIVE"] = str(archive)
        env["PATH"] = str(mockbin) + ":" + env["PATH"]
        downloaded_prefix = root / "downloaded"
        result = subprocess.run(["bash", "-s", "--", "--skip-build", "--core-only", "--no-deps",
                                 "--no-path", "--no-service", "--prefix", str(downloaded_prefix)],
                                input=(source / "install.sh").read_text(), env=env, cwd=root,
                                text=True, capture_output=True)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert "0.3.0" in run([str(downloaded_prefix / "bin/mousepet"), "--version"])
    print("Setup tests passed: installation/download bootstrap, PATH, packaged accessories, Lua/conf autostart, backups, symlinks and custom C++.")


if __name__ == "__main__":
    main()
