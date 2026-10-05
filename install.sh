#!/usr/bin/env bash
set -euo pipefail

# Works from a checkout or via: curl -fsSL .../install.sh | bash
MOUSEPET_INSTALL_SOURCE="${BASH_SOURCE[0]:-}"
if [[ -n "$MOUSEPET_INSTALL_SOURCE" && -f "$MOUSEPET_INSTALL_SOURCE" ]]; then
    MOUSEPET_SOURCE_DIR="$(cd -- "$(dirname -- "$MOUSEPET_INSTALL_SOURCE")" && pwd)"
else
    MOUSEPET_SOURCE_DIR=""
fi
if [[ ! -f "$MOUSEPET_SOURCE_DIR/Makefile" || ! -f "$MOUSEPET_SOURCE_DIR/scripts/install.py" ]]; then
    command -v curl >/dev/null || { echo 'curl is required.' >&2; exit 1; }
    command -v tar >/dev/null || { echo 'tar is required.' >&2; exit 1; }
    MOUSEPET_DOWNLOAD_DIR="$(mktemp -d -t mousepet-install.XXXXXXXX)"
    trap 'rm -rf -- "$MOUSEPET_DOWNLOAD_DIR"' EXIT
    curl -fL --retry 3 https://github.com/Y-Workplace/MousePet/archive/refs/heads/main.tar.gz -o "$MOUSEPET_DOWNLOAD_DIR/source.tar.gz"
    tar -xzf "$MOUSEPET_DOWNLOAD_DIR/source.tar.gz" -C "$MOUSEPET_DOWNLOAD_DIR"
    MOUSEPET_SOURCE_DIR="$MOUSEPET_DOWNLOAD_DIR/MousePet-main"
fi

# Install missing build tools on Arch-based distributions, without upgrading Hyprland.
if [[ " ${*} " != *' --no-deps '* ]] && command -v pacman >/dev/null; then
    MOUSEPET_MISSING_PACKAGES=()
    for package in gcc make pkgconf python wayland wayland-protocols cairo luajit; do
        pacman -Q "$package" >/dev/null 2>&1 || MOUSEPET_MISSING_PACKAGES+=("$package")
    done
    if (( ${#MOUSEPET_MISSING_PACKAGES[@]} )); then
        echo 'Installing missing dependencies (sudo may request your password)...'
        sudo pacman -S --needed --noconfirm "${MOUSEPET_MISSING_PACKAGES[@]}"
    fi
fi
command -v python3 >/dev/null || { echo 'python3 is required; install it with your package manager.' >&2; exit 1; }
python3 "$MOUSEPET_SOURCE_DIR/scripts/install.py" --source "$MOUSEPET_SOURCE_DIR" "$@"
