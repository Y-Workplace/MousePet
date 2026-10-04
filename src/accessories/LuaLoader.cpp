#include "LuaLoader.hpp"
#include "Registry.hpp"
#include "Common.hpp"

#include <lua.hpp>
#include <dirent.h>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace mousepet {

namespace {

struct LuaAccessoryContext {
    lua_State* L = nullptr;
    int tableRef = LUA_NOREF;
    int updateRef = LUA_NOREF;
    int lateUpdateRef = LUA_NOREF;
    int burstRef = LUA_NOREF;
    int renderRef = LUA_NOREF;
};

// ── Cairo userdata for Lua ───────────────────────────────────────────
static const char* CAIRO_MT = "MousePet.Cairo";

static cairo_t* checkCairo(lua_State* L, int idx) {
    void* ud = luaL_checkudata(L, idx, CAIRO_MT);
    luaL_argcheck(L, ud != nullptr, idx, "cairo_t* expected");
    return *static_cast<cairo_t**>(ud);
}

static int l_cairo_save(lua_State* L) {
    cairo_save(checkCairo(L, 1));
    return 0;
}

static int l_cairo_restore(lua_State* L) {
    cairo_restore(checkCairo(L, 1));
    return 0;
}

static int l_cairo_translate(lua_State* L) {
    cairo_translate(checkCairo(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3));
    return 0;
}

static int l_cairo_rotate(lua_State* L) {
    cairo_rotate(checkCairo(L, 1), luaL_checknumber(L, 2));
    return 0;
}

static int l_cairo_scale(lua_State* L) {
    cairo_scale(checkCairo(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3));
    return 0;
}

static int l_cairo_move_to(lua_State* L) {
    cairo_move_to(checkCairo(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3));
    return 0;
}

static int l_cairo_line_to(lua_State* L) {
    cairo_line_to(checkCairo(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3));
    return 0;
}

static int l_cairo_curve_to(lua_State* L) {
    cairo_curve_to(checkCairo(L, 1),
                   luaL_checknumber(L, 2), luaL_checknumber(L, 3),
                   luaL_checknumber(L, 4), luaL_checknumber(L, 5),
                   luaL_checknumber(L, 6), luaL_checknumber(L, 7));
    return 0;
}

static int l_cairo_quad_to(lua_State* L) {
    quadTo(checkCairo(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3),
           luaL_checknumber(L, 4), luaL_checknumber(L, 5));
    return 0;
}

static int l_cairo_arc(lua_State* L) {
    cairo_arc(checkCairo(L, 1),
              luaL_checknumber(L, 2), luaL_checknumber(L, 3),
              luaL_checknumber(L, 4), luaL_checknumber(L, 5), luaL_checknumber(L, 6));
    return 0;
}

static int l_cairo_circle(lua_State* L) {
    circle(checkCairo(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4));
    return 0;
}

static int l_cairo_ellipse(lua_State* L) {
    ellipse(checkCairo(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3),
            luaL_checknumber(L, 4), luaL_checknumber(L, 5), luaL_optnumber(L, 6, 0.0));
    return 0;
}

static int l_cairo_rectangle(lua_State* L) {
    cairo_rectangle(checkCairo(L, 1),
                    luaL_checknumber(L, 2), luaL_checknumber(L, 3),
                    luaL_checknumber(L, 4), luaL_checknumber(L, 5));
    return 0;
}

static int l_cairo_close_path(lua_State* L) {
    cairo_close_path(checkCairo(L, 1));
    return 0;
}

static int l_cairo_new_path(lua_State* L) {
    cairo_new_path(checkCairo(L, 1));
    return 0;
}

static int l_cairo_set_source_rgb(lua_State* L) {
    cairo_set_source_rgb(checkCairo(L, 1),
                         luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4));
    return 0;
}

static int l_cairo_set_source_rgba(lua_State* L) {
    cairo_set_source_rgba(checkCairo(L, 1),
                          luaL_checknumber(L, 2), luaL_checknumber(L, 3),
                          luaL_checknumber(L, 4), luaL_checknumber(L, 5));
    return 0;
}

static int l_cairo_set_source_hex(lua_State* L) {
    cairo_t* cr = checkCairo(L, 1);
    uint32_t hex = (uint32_t)luaL_checkinteger(L, 2);
    double a = luaL_optnumber(L, 3, 1.0);
    Color c = rgb(hex, a);
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
    return 0;
}

static int l_cairo_set_line_width(lua_State* L) {
    cairo_set_line_width(checkCairo(L, 1), luaL_checknumber(L, 2));
    return 0;
}

static int l_cairo_fill(lua_State* L) {
    cairo_fill(checkCairo(L, 1));
    return 0;
}

static int l_cairo_stroke(lua_State* L) {
    cairo_stroke(checkCairo(L, 1));
    return 0;
}

static int l_cairo_fill_preserve(lua_State* L) {
    cairo_fill_preserve(checkCairo(L, 1));
    return 0;
}

static int l_cairo_stroke_preserve(lua_State* L) {
    cairo_stroke_preserve(checkCairo(L, 1));
    return 0;
}

static void pushCairo(lua_State* L, cairo_t* cr) {
    cairo_t** ud = static_cast<cairo_t**>(lua_newuserdata(L, sizeof(cairo_t*)));
    *ud = cr;
    luaL_getmetatable(L, CAIRO_MT);
    lua_setmetatable(L, -2);
}

static void registerCairoMetatable(lua_State* L) {
    if (luaL_newmetatable(L, CAIRO_MT)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        const luaL_Reg methods[] = {
            {"save", l_cairo_save},
            {"restore", l_cairo_restore},
            {"translate", l_cairo_translate},
            {"rotate", l_cairo_rotate},
            {"scale", l_cairo_scale},
            {"move_to", l_cairo_move_to},
            {"line_to", l_cairo_line_to},
            {"curve_to", l_cairo_curve_to},
            {"quad_to", l_cairo_quad_to},
            {"arc", l_cairo_arc},
            {"circle", l_cairo_circle},
            {"ellipse", l_cairo_ellipse},
            {"rectangle", l_cairo_rectangle},
            {"close_path", l_cairo_close_path},
            {"new_path", l_cairo_new_path},
            {"set_source_rgb", l_cairo_set_source_rgb},
            {"set_source_rgba", l_cairo_set_source_rgba},
            {"set_source_hex", l_cairo_set_source_hex},
            {"set_line_width", l_cairo_set_line_width},
            {"fill", l_cairo_fill},
            {"stroke", l_cairo_stroke},
            {"fill_preserve", l_cairo_fill_preserve},
            {"stroke_preserve", l_cairo_stroke_preserve},
            {nullptr, nullptr}
        };
        luaL_register(L, nullptr, methods);
    }
    lua_pop(L, 1);
}

static void pushPetTable(lua_State* L, Pet& pet) {
    lua_newtable(L);

    // body
    lua_newtable(L);
    lua_pushnumber(L, pet.body.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, pet.body.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, pet.body.vx); lua_setfield(L, -2, "vx");
    lua_pushnumber(L, pet.body.vy); lua_setfield(L, -2, "vy");
    lua_pushnumber(L, pet.body.angle); lua_setfield(L, -2, "angle");
    lua_pushnumber(L, pet.body.burst); lua_setfield(L, -2, "burst");
    lua_setfield(L, -2, "body");

    // mouse
    lua_newtable(L);
    lua_pushnumber(L, pet.mouse.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, pet.mouse.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, pet.mouse.vx); lua_setfield(L, -2, "vx");
    lua_pushnumber(L, pet.mouse.vy); lua_setfield(L, -2, "vy");
    lua_pushnumber(L, pet.mouse.speed); lua_setfield(L, -2, "speed");
    lua_setfield(L, -2, "mouse");

    lua_pushnumber(L, pet.time);
    lua_setfield(L, -2, "time");

    // Methods
    lua_pushlightuserdata(L, &pet);
    lua_pushcclosure(L, [](lua_State* ls) -> int {
        Pet* p = static_cast<Pet*>(lua_touserdata(ls, lua_upvalueindex(1)));
        p->emit((float)luaL_checknumber(ls, 2), (float)luaL_checknumber(ls, 3),
                (float)luaL_checknumber(ls, 4), (float)luaL_checknumber(ls, 5),
                (float)luaL_optnumber(ls, 6, 1.0));
        return 0;
    }, 1);
    lua_setfield(L, -2, "emit");

    lua_pushlightuserdata(L, &pet);
    lua_pushcclosure(L, [](lua_State* ls) -> int {
        Pet* p = static_cast<Pet*>(lua_touserdata(ls, lua_upvalueindex(1)));
        const char* key = luaL_checkstring(ls, 2);
        float def = (float)luaL_optnumber(ls, 3, 0.0);
        lua_pushnumber(ls, p->param(key, def));
        return 1;
    }, 1);
    lua_setfield(L, -2, "param");

    lua_pushlightuserdata(L, &pet);
    lua_pushcclosure(L, [](lua_State* ls) -> int {
        Pet* p = static_cast<Pet*>(lua_touserdata(ls, lua_upvalueindex(1)));
        lua_pushnumber(ls, p->random());
        return 1;
    }, 1);
    lua_setfield(L, -2, "random");
}

class LuaAccessory final : public Accessory {
    LuaAccessoryContext ctx_;

public:
    explicit LuaAccessory(LuaAccessoryContext ctx) : ctx_(ctx) {}
    ~LuaAccessory() override = default;

    void update(Pet& pet) override {
        if (ctx_.updateRef == LUA_NOREF) return;
        lua_State* L = ctx_.L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, ctx_.updateRef);
        pushPetTable(L, pet);
        if (lua_pcall(L, 1, 0, 0) != 0) {
            std::fprintf(stderr, "[mousepet] Lua update error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        } else {
            // sync changes back
        }
    }

    void lateUpdate(Pet& pet) override {
        if (ctx_.lateUpdateRef == LUA_NOREF) return;
        lua_State* L = ctx_.L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, ctx_.lateUpdateRef);
        pushPetTable(L, pet);
        if (lua_pcall(L, 1, 0, 0) != 0) {
            std::fprintf(stderr, "[mousepet] Lua lateUpdate error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    void onBurst(Pet& pet) override {
        if (ctx_.burstRef == LUA_NOREF) return;
        lua_State* L = ctx_.L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, ctx_.burstRef);
        pushPetTable(L, pet);
        if (lua_pcall(L, 1, 0, 0) != 0) {
            std::fprintf(stderr, "[mousepet] Lua onBurst error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    void render(cairo_t* cr, const Pet& pet) override {
        if (ctx_.renderRef == LUA_NOREF) return;
        lua_State* L = ctx_.L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, ctx_.renderRef);
        pushCairo(L, cr);
        pushPetTable(L, const_cast<Pet&>(pet));
        if (lua_pcall(L, 2, 0, 0) != 0) {
            std::fprintf(stderr, "[mousepet] Lua render error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    void bounds(const Pet& pet, Bounds& b) const override {
        addBox(pet, b, 35.0f * (1.0f + pet.body.burst * 0.3f));
    }
};

}  // namespace

void loadLuaAccessories(const std::string& dirPath) {
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() <= 4 || name.substr(name.size() - 4) != ".lua") continue;

        std::string fullPath = dirPath + "/" + name;
        lua_State* L = luaL_newstate();
        luaL_openlibs(L);
        registerCairoMetatable(L);

        if (luaL_dofile(L, fullPath.c_str()) != 0) {
            std::fprintf(stderr, "[mousepet] Failed to load Lua accessory %s: %s\n",
                         fullPath.c_str(), lua_tostring(L, -1));
            lua_close(L);
            continue;
        }

        if (!lua_istable(L, -1)) {
            std::fprintf(stderr, "[mousepet] Lua accessory %s must return a table\n", fullPath.c_str());
            lua_close(L);
            continue;
        }

        AccessoryDef def;
        lua_getfield(L, -1, "id");
        def.id = lua_isstring(L, -1) ? lua_tostring(L, -1) : name.substr(0, name.size() - 4);
        lua_pop(L, 1);

        lua_getfield(L, -1, "name");
        def.name = lua_isstring(L, -1) ? lua_tostring(L, -1) : def.id;
        lua_pop(L, 1);

        lua_getfield(L, -1, "color");
        if (lua_isnumber(L, -1)) def.swatch = rgb((uint32_t)lua_tointeger(L, -1));
        else def.swatch = rgb(0x38bdf8);
        lua_pop(L, 1);

        // Body hints
        lua_getfield(L, -1, "body");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "orbitX"); if (lua_isnumber(L, -1)) def.body.orbitX = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, -1, "orbitY"); if (lua_isnumber(L, -1)) def.body.orbitY = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, -1, "sway"); if (lua_isnumber(L, -1)) def.body.sway = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, -1, "pull"); if (lua_isnumber(L, -1)) def.body.pull = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, -1, "lift"); if (lua_isnumber(L, -1)) def.body.lift = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, -1, "kick"); if (lua_isnumber(L, -1)) def.body.kick = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, -1, "sparks"); if (lua_isnumber(L, -1)) def.body.sparks = (int)lua_tointeger(L, -1); lua_pop(L, 1);
            lua_getfield(L, -1, "tilt"); if (lua_isnumber(L, -1)) def.body.tilt = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        }
        lua_pop(L, 1);

        // Rope
        lua_getfield(L, -1, "rope");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "color"); if (lua_isnumber(L, -1)) def.rope.color = rgb((uint32_t)lua_tointeger(L, -1)); lua_pop(L, 1);
            lua_getfield(L, -1, "highlight"); if (lua_isnumber(L, -1)) def.rope.highlight = rgb((uint32_t)lua_tointeger(L, -1)); lua_pop(L, 1);
            lua_getfield(L, -1, "width"); if (lua_isnumber(L, -1)) def.rope.width = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        }
        lua_pop(L, 1);

        // Particle
        lua_getfield(L, -1, "particle");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "color"); if (lua_isnumber(L, -1)) def.particle.color = rgb((uint32_t)lua_tointeger(L, -1)); lua_pop(L, 1);
            lua_getfield(L, -1, "size"); if (lua_isnumber(L, -1)) def.particle.size = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, -1, "gravity"); if (lua_isnumber(L, -1)) def.particle.gravity = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        }
        lua_pop(L, 1);

        LuaAccessoryContext ctx;
        ctx.L = L;
        ctx.tableRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops the table

        lua_rawgeti(L, LUA_REGISTRYINDEX, ctx.tableRef);
        lua_getfield(L, -1, "update"); if (lua_isfunction(L, -1)) ctx.updateRef = luaL_ref(L, LUA_REGISTRYINDEX); else lua_pop(L, 1);
        lua_getfield(L, -1, "lateUpdate"); if (lua_isfunction(L, -1)) ctx.lateUpdateRef = luaL_ref(L, LUA_REGISTRYINDEX); else lua_pop(L, 1);
        lua_getfield(L, -1, "onBurst"); if (lua_isfunction(L, -1)) ctx.burstRef = luaL_ref(L, LUA_REGISTRYINDEX); else lua_pop(L, 1);
        lua_getfield(L, -1, "render"); if (lua_isfunction(L, -1)) ctx.renderRef = luaL_ref(L, LUA_REGISTRYINDEX); else lua_pop(L, 1);
        lua_pop(L, 1); // pop table

        def.create = [ctx](Pet&) -> std::unique_ptr<Accessory> {
            return std::make_unique<LuaAccessory>(ctx);
        };

        std::fprintf(stderr, "[mousepet] Loaded Lua accessory '%s' from %s\n", def.id.c_str(), fullPath.c_str());
        registerAccessory(std::move(def));
    }
    closedir(dir);
}

}  // namespace mousepet
