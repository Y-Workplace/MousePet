-- Exemplo de acessório em Lua para o MousePet / CursorPet
-- Para usar, coloque este arquivo ou qualquer outro .lua em ~/.config/mousepet/accessories/
-- ou no diretório ./accessories/

return {
    id = "bubble",
    name = "Bolha Mágica",
    color = 0x60a5fa,

    rope = {
        color = 0x38bdf8,
        highlight = 0xbae6fd,
        width = 2.0
    },

    particle = {
        color = 0x93c5fd,
        size = 2.5,
        gravity = -0.04 -- partículas flutuam para cima como mini bolhas
    },

    body = {
        orbitX = 40,
        orbitY = -28,
        sway = 6,
        pull = 0.028,
        lift = -0.05,
        kick = 5,
        sparks = 16,
        tilt = 0.2
    },

    update = function(pet)
        -- Bobbing suave na água/ar
        pet.body.vy = pet.body.vy + math.sin(pet.time * 4) * 0.02
        if math.random() < 0.08 then
            pet:emit(pet.body.x + (pet:random() - 0.5) * 16,
                     pet.body.y + (pet:random() - 0.5) * 16,
                     (pet:random() - 0.5) * 0.4,
                     -0.4, 0.8)
        end
    end,

    onBurst = function(pet)
        -- Emite um círculo de bolhas na explosão de clique
        for i = 1, 14 do
            local a = pet:random() * 6.28318
            local spd = 0.8 + pet:random() * 2.2
            pet:emit(pet.body.x, pet.body.y, math.cos(a) * spd, math.sin(a) * spd, 1.2)
        end
    end,

    render = function(cr, pet)
        local b = pet.body
        local r = 16 * (1 + b.burst * 0.35)
        local pulse = math.sin(pet.time * 3) * 0.8

        cr:save()
        cr:translate(b.x, b.y)

        -- Corpo translúcido da bolha
        cr:set_source_rgba(56 / 255, 189 / 255, 248 / 255, 0.35)
        cr:circle(0, 0, r + pulse)
        cr:fill_preserve()

        -- Borda iridescente
        cr:set_source_rgba(186 / 255, 230 / 255, 253 / 255, 0.85)
        cr:set_line_width(1.8)
        cr:stroke()

        -- Brilho / reflexo interno da bolha
        cr:set_source_rgba(1, 1, 1, 0.75)
        cr:circle(-r * 0.35, -r * 0.35, r * 0.28)
        cr:fill()

        cr:set_source_rgba(1, 1, 1, 0.5)
        cr:circle(r * 0.4, r * 0.4, r * 0.15)
        cr:fill()

        cr:restore()
    end
}
