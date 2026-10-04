#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"

echo "Compilando MousePet..."
make -C "$DIR" -j"$(nproc)"

echo "Instalando em $PREFIX/bin/mousepet e $PREFIX/bin/cursorpet..."
mkdir -p "$PREFIX/bin"
cp -f "$DIR/build/mousepet" "$PREFIX/bin/mousepet"
chmod +x "$PREFIX/bin/mousepet"
ln -sf mousepet "$PREFIX/bin/cursorpet"

if [ -f "$DIR/build/mousepet-hyprland.so" ]; then
    mkdir -p "$PREFIX/lib"
    cp -f "$DIR/build/mousepet-hyprland.so" "$PREFIX/lib/mousepet-hyprland.so"
fi

mkdir -p "$HOME/.config/systemd/user"
cp -f "$DIR/systemd/mousepet.service" "$HOME/.config/systemd/user/mousepet.service"
systemctl --user daemon-reload 2>/dev/null || true

echo "Instalação concluída com sucesso!"
echo "Você pode executar agora digitando diretamente: mousepet run (ou cursorpet run)"
