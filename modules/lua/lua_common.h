/*
 * Copyright (C) 2014 David Milligan
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef _lua_common_h
#define _lua_common_h

#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lualib.h"

#define MAX_PATH_LEN 0x80
#define SCRIPTS_DIR "ML/SCRIPTS"
#define EC2RAW(ec) ec * 8 / 10
#define CBR_RET_KEYPRESS_NOTHANDLED 1
#define CBR_RET_KEYPRESS_HANDLED 0
//#define CONFIG_VSYNC_EVENTS

#define LUA_PARAM_INT(name, index)\
if(index > lua_gettop(L) || !lua_isinteger(L, index)) return luaL_argerror(L, index, "expected integer for param '" #name "'");\
int name = lua_tointeger(L, index)

#define LUA_PARAM_INT_OPTIONAL(name, index, default) int name = (index <= lua_gettop(L) && lua_isinteger(L, index)) ? lua_tointeger(L, index) : default

#define LUA_PARAM_BOOL(name, index)\
if(index > lua_gettop(L) || !lua_isboolean(L, index)) return luaL_argerror(L, index, "expected boolean for param '" #name "'");\
int name = lua_toboolean(L, index)

#define LUA_PARAM_BOOL_OPTIONAL(name, index, default) int name = (index <= lua_gettop(L) && lua_isboolean(L, index)) ? lua_toboolean(L, index) : default

#define LUA_PARAM_NUMBER(name, index)\
if(index > lua_gettop(L) || !lua_isnumber(L, index)) return luaL_argerror(L, index, "expected number for param '" #name "'");\
float name = lua_tonumber(L, index)

#define LUA_PARAM_NUMBER_OPTIONAL(name, index, default) float name = (index <= lua_gettop(L) && lua_isnumber(L, index)) ? lua_tonumber(L, index) : default

#define LUA_PARAM_STRING(name, index)\
if(index > lua_gettop(L) || !lua_isstring(L, index)) return luaL_argerror(L, index, "expected string for param '" #name "'");\
const char * name = lua_tostring(L, index)

#define LUA_PARAM_STRING_OPTIONAL(name, index, default) const char * name = (index <= lua_gettop(L) && lua_isstring(L, index)) ? lua_tostring(L, index) : default

#define LUA_FIELD_STRING(field, default) lua_getfield(L, -1, field) == LUA_TSTRING ? lua_tostring(L, -1) : default; lua_pop(L, 1)
#define LUA_FIELD_INT(field, default) lua_getfield(L, -1, field) == LUA_TNUMBER ? lua_tointeger(L, -1) : default; lua_pop(L, 1)

#define LUA_CONSTANT(name, value) lua_pushinteger(L, value); lua_setfield(L, -2, #name)

#define LUA_LIB(name)\
int luaopen_##name(lua_State * L) {\
    lua_newtable(L);\
    luaL_setfuncs(L, name##lib, 0);\
    lua_newtable(L);\
    lua_pushcfunction(L, luaCB_##name##_index);\
    lua_setfield(L, -2, "__index");\
    lua_pushcfunction(L, luaCB_##name##_newindex);\
    lua_setfield(L, -2, "__newindex");\
    lua_setmetatable(L, -2);\
    return 1;\
}

struct script_menu_entry
{
    int menu_value;
    lua_State * L;
    struct menu_entry * menu_entry;
    int select_ref;
    int update_ref;
    int warning_ref;
    int info_ref;
    int submenu_ref;
};

int docall(lua_State *L, int narg, int nres);

int luaopen_globals(lua_State * L);
int luaopen_console(lua_State * L);
int luaopen_camera(lua_State * L);
int luaopen_lv(lua_State * L);
int luaopen_lens(lua_State * L);
int luaopen_movie(lua_State * L);
int luaopen_display(lua_State * L);
int luaopen_key(lua_State * L);
int luaopen_menu(lua_State * L);

int luaopen_MODE(lua_State * L);
int luaopen_ICON_TYPE(lua_State * L);
int luaopen_UNIT(lua_State * L);
int luaopen_DEPENDS_ON(lua_State * L);
int luaopen_FONT(lua_State * L);
int luaopen_COLOR(lua_State * L);
int luaopen_KEY(lua_State * L);

#endif