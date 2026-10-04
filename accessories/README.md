# Acessórios do MousePet / CursorPet

O MousePet possui arquitetura modular onde cada acessório tem seu próprio arquivo separado, além de suporte a criação dinâmica de pets usando **Lua (amigável e rápido)** ou **C++ (auto-compilado na execução)**.

## Como Adicionar Novos Acessórios

Você pode colocar seus scripts ou fontes em:
1. `~/.config/mousepet/accessories/` (global do seu usuário)
2. `./accessories/` (local no diretório de execução)

### Método 1: Em Lua (Mais Amigável)
Basta criar um arquivo com extensão `.lua` (ex: `meupet.lua`):

```lua
return {
    id = "meupet",
    name = "Meu Pet Amigável",
    color = 0x60a5fa,
    rope = { color = 0x38bdf8, width = 2.0 },
    particle = { color = 0x93c5fd, size = 2.5, gravity = -0.04 },
    body = { orbitX = 40, orbitY = -28, sway = 6 },

    update = function(pet)
        -- forças adicionais ou física
    end,

    onBurst = function(pet)
        -- disparado quando o usuário clica com o mouse
        for i = 1, 10 do
            local a = pet:random() * 6.28
            pet:emit(pet.body.x, pet.body.y, math.cos(a) * 2, math.sin(a) * 2, 1.0)
        end
    end,

    render = function(cr, pet)
        -- desenha usando comandos cairo do Lua
        cr:set_source_rgba(1.0, 0.5, 0.2, 0.9)
        cr:circle(pet.body.x, pet.body.y, 14)
        cr:fill()
    end
}
```

O MousePet carrega o arquivo `.lua` automaticamente ao iniciar ou reiniciar!

### Método 2: Em C++ (Compilação Automática na Execução)
Basta colocar um arquivo `.cpp` (ex: `meupet.cpp`).
Quando o MousePet for executado, ele verifica se existe o `.so` correspondente ou se o `.cpp` foi modificado.
Se necessário, o MousePet compila automaticamente o `.cpp` para `.so` usando `g++` e carrega via `dlopen` sem que você precise compilar manualmente!

```cpp
#include <cairo.h>
#include "accessories/Common.hpp"

extern "C" mousepet::AccessoryDef mousepet_accessory() {
    // Retorna a definição do acessório
}
```
