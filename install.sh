#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"

echo "==> Compilando MousePet & Plugin Hyprland..."
make -C "$DIR" -j"$(nproc)"

echo "==> Instalando executáveis em $PREFIX/bin/..."
mkdir -p "$PREFIX/bin"
cp -f "$DIR/build/mousepet" "$PREFIX/bin/mousepet"
chmod +x "$PREFIX/bin/mousepet"
ln -sf mousepet "$PREFIX/bin/cursorpet"

echo "==> Instalando plugin em $PREFIX/lib/..."
mkdir -p "$PREFIX/lib"
cp -f "$DIR/build/mousepet-hyprland.so" "$PREFIX/lib/mousepet-hyprland.so"

echo "==> Instalando diretório de acessórios..."
mkdir -p "$HOME/.config/mousepet/accessories"
if [ ! -f "$HOME/.config/mousepet/accessories/bubble.lua" ]; then
    cp -f "$DIR/accessories/bubble.lua" "$HOME/.config/mousepet/accessories/bubble.lua"
fi

echo "==> Configurando serviço systemd..."
mkdir -p "$HOME/.config/systemd/user"
cp -f "$DIR/systemd/mousepet.service" "$HOME/.config/systemd/user/mousepet.service"
systemctl --user daemon-reload 2>/dev/null || true

echo ""
echo "=========================================================="
echo "Instalação concluída com sucesso!"
echo "Comandos disponíveis globalmente:"
echo "  mousepet run                  (inicia o pet)"
echo "  cursorpet pet next            (troca para o próximo pet)"
echo "  cursorpet pet prev            (troca para o pet anterior)"
echo "  cursorpet pet <nome>          (ex: cursorpet pet star)"
echo "  cursorpet --pet --next        (sintaxe alternativa)"
echo "  cursorpet burst               (dispara o efeito de clique)"
echo "  cursorpet list                (lista todos os acessórios)"
echo ""
echo "Plugin Hyprland para cliques em tempo real (opcional):"
echo "  cursorpet plugin load         (carrega o hook no Hyprland)"
echo "  cursorpet plugin unload       (descarrega o hook)"
echo "=========================================================="
