#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"

echo "Compilando MousePet..."
make -C "$DIR" -j"$(nproc)"

echo "Instalando em $PREFIX/bin/mousepet..."
mkdir -p "$PREFIX/bin"
cp -f "$DIR/build/mousepet" "$PREFIX/bin/mousepet"
chmod +x "$PREFIX/bin/mousepet"

mkdir -p "$HOME/.config/systemd/user"
cp -f "$DIR/systemd/mousepet.service" "$HOME/.config/systemd/user/mousepet.service"
systemctl --user daemon-reload 2>/dev/null || true

echo "Instalação concluída com sucesso!"
echo "Você pode executar agora digitando diretamente: mousepet run"
