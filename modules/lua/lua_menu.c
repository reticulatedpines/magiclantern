/***
 Functions for interacting with the ML menu
 
 @author Magic Lantern Team
 @copyright 2014
 @license GPL
 @module menu
 */

#include <dryos.h>
#include <string.h>
#include <menu.h>
#include <property.h>

#include "lua_common.h"

extern int lua_running;
static int lua_run_arg_count = 0;
static lua_State * running_script = NULL;

static int luaCB_menu_index(lua_State * L);
static int luaCB_menu_newindex(lua_State * L);
static int luaCB_menu_remove(lua_State * L);
static void load_menu_entry(lua_State * L, struct script_menu_entry * script_entry, struct menu_entry * menu_entry, const char * default_name);

static void lua_run_task(int unused)
{
    lua_running = 1;
    if(running_script)
    {
        lua_State * L = running_script;
        console_printf("running script...\n");
        if(docall(L, lua_run_arg_count, 0))
        {
            console_printf("script failed:\n %s\n", lua_tostring(L, -1));
        }
        else
        {
            console_printf("script finished\n");
        }
    }
    lua_running = 0;
}

//copied from lua.c
static int msghandler (lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == NULL) {  /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
            lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
            return 1;  /* that is the message */
        else
            msg = lua_pushfstring(L, "(error object is a %s value)",
                                  luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
    return 1;  /* return the traceback */
}

int docall (lua_State *L, int narg, int nres) {
    int status;
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler);  /* push message handler */
    lua_insert(L, base);  /* put it under function and args */
    status = lua_pcall(L, narg, nres, base);
    lua_remove(L, base);  /* remove message handler from the stack */
    return status;
}

static MENU_SELECT_FUNC(script_menu_select)
{
    if(lua_running)
    {
        console_printf("script error: another script is currently running\n");
        return;
    }
    
    struct script_menu_entry * script_entry = priv;
    if(script_entry && script_entry->L && script_entry->select_ref != LUA_NOREF)
    {
        lua_State * L = script_entry->L;
        if(lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->select_ref) == LUA_TFUNCTION)
        {
            lua_run_arg_count = 1;
            lua_pushinteger(L, delta);
            running_script = L;
            lua_running = 1;
            task_create("lua_task", 0x1c, 0x8000, lua_run_task, (void*) 0);
        }
        else
        {
            luaL_error(L, "select is not a function");
        }
    }
}

static MENU_UPDATE_FUNC(script_menu_update)
{
    if(lua_running)
    {
        console_printf("script error: another script is currently running\n");
        return;
    }
    
    struct script_menu_entry * script_entry = entry->priv;
    if(script_entry && script_entry->L)
    {
        lua_State * L = script_entry->L;
        if(script_entry->update_ref != LUA_NOREF)
        {
            if(lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->update_ref) == LUA_TFUNCTION)
            {
                if(!docall(L, 0, 1))
                {
                    MENU_SET_VALUE("%s", lua_tostring(L, -1));
                }
            }
            else if(lua_isstring(L, -1))
            {
                MENU_SET_VALUE("%s", lua_tostring(L, -1));
            }
            lua_pop(L,1);
        }
        if(script_entry->info_ref != LUA_NOREF)
        {
            if(lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->info_ref) == LUA_TFUNCTION)
            {
                if(!docall(L, 0, 1))
                {
                    MENU_SET_WARNING(MENU_WARN_INFO, "%s", lua_tostring(L, -1));
                }
            }
            else if(lua_isstring(L, -1))
            {
                MENU_SET_VALUE("%s", lua_tostring(L, -1));
            }
            lua_pop(L,1);
        }
        if(script_entry->warning_ref != LUA_NOREF)
        {
            if(lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->warning_ref) == LUA_TFUNCTION)
            {
                if(!docall(L, 0, 1))
                {
                    MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "%s", lua_tostring(L, -1));
                }
            }
            lua_pop(L,1);
        }
    }
}

static int lua_hasfield(lua_State * L, int idx, const char * name, int expected_type)
{
    int result = lua_getfield(L, -1, name) == expected_type;
    lua_pop(L, 1);
    return result;
}

static int get_index_for_choices(struct menu_entry * menu_entry, const char * value)
{
    int i;
    for(i = 0; i < menu_entry->max; i++)
    {
        if(!strcmp(menu_entry->choices[i], value))
        {
            return i;
        }
    }
    return 0;
}

/*** Creates a new menu item
 @tparam table definition
 @function new
 @return a menu object
 @usage
mymenu = menu.new
{
    parent = "LUA",
    name = "Run Test Script",
    help = "Run the test script.",
    submenu =
    { 
        {
            name = "Run",
            help = "Run this script.",
            icon_type = 5,
            update = "",
        },
        {
            name = "param1",
            help = "help for param1",
            min = 0,
            max = 100,
            warning = function() return "this param doesn't work right :P" end,
        },
        {
            name = "param2",
            help = "help for param2",
            min = 0,
            max = 10,
            value = 5,
            info = function() return "click it baby!" end,
        },
        {
            name = "dec test",
            min = 0,
            max = 10000,
            unit = 7,
        },
        {
            name = "choices test",
            choices = { "choice1", "choice2", "choice3" },
        }
    },
    update = function() return mymenu.submenu["choices test"].value end,
}

mymenu.submenu["Run"].select = function()
    console.show()
    print("param1= "..mymenu.submenu["param1"].value)
    print("param2= "..mymenu.submenu["param2"].value)
    print("dec test= "..mymenu.submenu["dec test"].value)
    print("choices test= "..mymenu.submenu["choices test"].value)
    print("script run finished!")
end
*/
static int luaCB_menu_new(lua_State * L)
{
    if(!lua_istable(L, 1)) return luaL_argerror(L, 1, "expected table");
    
    lua_pushvalue(L, 1);
    const char * parent = LUA_FIELD_STRING("parent", "LUA");
    lua_pop(L, 1);
    
    struct script_menu_entry * new_entry = lua_newuserdata(L, sizeof(struct script_menu_entry));
    //add a metatable to the userdata object for value lookups and to store submenu
    lua_newtable(L);
    lua_pushcfunction(L, luaCB_menu_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, luaCB_menu_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, luaCB_menu_remove);
    lua_setfield(L, -2, "remove");
    lua_pushstring(L, parent);
    lua_setfield(L, -2, "parent");
    lua_setmetatable(L, -2);
    
    lua_pushvalue(L, 1);
    load_menu_entry(L, new_entry, NULL, "unknown");
    menu_add(parent, new_entry->menu_entry, 1);
    lua_pop(L, 1);
    
    return 1; //return the userdata object
}

/// Represents a menu item
// @type menu

static int luaCB_menu_index(lua_State * L)
{
    if(!lua_isuserdata(L, 1)) return luaL_argerror(L, 1, NULL);
    struct script_menu_entry * script_entry = lua_touserdata(L, 1);
    if(!script_entry || !script_entry->menu_entry) return luaL_argerror(L, 1, "internal error: userdata was NULL");
    
    LUA_PARAM_STRING_OPTIONAL(key, 2, "");
    /// current value of the menu item
    // @tfield integer value
    if(!strcmp(key, "value"))
    {
        if(script_entry->menu_entry->choices)
        {
            lua_pushstring(L, script_entry->menu_entry->choices[script_entry->menu_value]);
        }
        else
        {
            lua_pushinteger(L, script_entry->menu_value);
        }
    }
    /// Name for the menu item
    // @tfield string name
    else if(!strcmp(key, "name")) lua_pushstring(L, script_entry->menu_entry->name);
    /// Help text for the menu item (line 1)
    // @tfield string help
    else if(!strcmp(key, "help")) lua_pushstring(L, script_entry->menu_entry->help);
    /// Help text for the menu item (line 2)
    // @tfield string help2
    else if(!strcmp(key, "help2")) lua_pushstring(L, script_entry->menu_entry->help2);
    /// Advanced setting in submenus
    // @tfield boolean advanced
    else if(!strcmp(key, "advanced")) lua_pushinteger(L, script_entry->menu_entry->advanced);
    /// Dependencies for this menu item.
    // If the dependecies are not met, the item will be greyed out and a warning will appear at the bottom of the screen.
    // @tfield integer depends_on @{constants.DEPENDS_ON}
    else if(!strcmp(key, "depends_on")) lua_pushinteger(L, script_entry->menu_entry->depends_on);
    /// Editing mode for the menu item
    // @tfield integer edit_mode
    else if(!strcmp(key, "edit_mode")) lua_pushinteger(L, script_entry->menu_entry->edit_mode);
    /// Hidden from main menu
    // @tfield integer hidden
    else if(!strcmp(key, "hidden")) lua_pushinteger(L, script_entry->menu_entry->hidden);
    /// The type of icon to use for this menu item
    // @tfield integer icon_type @{constants.ICON_TYPE}
    else if(!strcmp(key, "icon_type")) lua_pushinteger(L, script_entry->menu_entry->icon_type);
    /// Hidden from junkie menu
    // @tfield boolean jhidden
    else if(!strcmp(key, "jhidden")) lua_pushboolean(L, script_entry->menu_entry->jhidden);
    /// The maximum value the menu item can have
    // @tfield integer max
    else if(!strcmp(key, "max")) lua_pushinteger(L, script_entry->menu_entry->max);
    /// The minimum value the menu item can have
    // @tfield integer min
    else if(!strcmp(key, "min")) lua_pushinteger(L, script_entry->menu_entry->min);
    /// Whether or not the menu is selected
    // @tfield integer selected
    else if(!strcmp(key, "selected")) lua_pushboolean(L, script_entry->menu_entry->selected);
    /// Special hide, not toggleable by user
    // @tfield boolean shidden
    else if(!strcmp(key, "shidden")) lua_pushboolean(L, script_entry->menu_entry->shidden);
    /// Present in "my menu"
    // @tfield boolean starred
    else if(!strcmp(key, "starred")) lua_pushboolean(L, script_entry->menu_entry->starred);
    /// Submenu Height
    // @tfield integer submenu_height
    else if(!strcmp(key, "submenu_height")) lua_pushinteger(L, script_entry->menu_entry->submenu_height);
    /// Submenu Width
    // @tfield integer submenu_width
    else if(!strcmp(key, "submenu_width")) lua_pushinteger(L, script_entry->menu_entry->submenu_width);
    /// The unit for the menu item's value
    // @tfield integer unit @{constants.UNIT}
    else if(!strcmp(key, "unit")) lua_pushinteger(L, script_entry->menu_entry->unit);
    /// Suggested operating mode for this menu item
    // @tfield integer works_best_in @{constants.DEPENDS_ON}
    else if(!strcmp(key, "works_best_in")) lua_pushinteger(L, script_entry->menu_entry->works_best_in);
    /// Function called when menu is toggled
    // @tparam integer delta
    // @function select
    else if(!strcmp(key, "select")) lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->select_ref);
    /// Function called when menu is displayed. Return a string to be displayed.
    // @return string
    // @function update
    else if(!strcmp(key, "update")) lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->update_ref);
    /// Function called when menu is displayed. Return a string to be displayed in the info area (in green).
    // @return string
    // @function info
    else if(!strcmp(key, "info")) lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->info_ref);
    /// Function called when menu is displayed. Return a string when there is a warning (menu will be greyed out).
    // @return string
    // @function select
    else if(!strcmp(key, "warning")) lua_rawgeti(L, LUA_REGISTRYINDEX, script_entry->warning_ref);
    else
    {
        //retrieve the key from the metatable
        if(lua_getmetatable(L, 1))
        {
            lua_pushvalue(L, 2);
            lua_rawget(L, -2);
        }
        else
        {
            lua_pushnil(L);
        }
    }
    return 1;
}

static int luaCB_menu_newindex(lua_State * L)
{
    if(!lua_isuserdata(L, 1)) return luaL_argerror(L, 1, NULL);
    struct script_menu_entry * script_entry = lua_touserdata(L, 1);
    if(!script_entry || !script_entry->menu_entry) return luaL_argerror(L, 1, "internal error: userdata was NULL");
    
    LUA_PARAM_STRING_OPTIONAL(key, 2, "");
    if(!strcmp(key, "value"))
    {
        if(script_entry->menu_entry->choices)
        {
            LUA_PARAM_STRING(value, 3);
            script_entry->menu_value = get_index_for_choices(script_entry->menu_entry, value);
        }
        else
        {
            LUA_PARAM_INT(value, 3);
            script_entry->menu_value = value;
        }
    }
    else if(!strcmp(key, "name")) { LUA_PARAM_STRING(value, 3); script_entry->menu_entry->name = value; }
    else if(!strcmp(key, "help")) { LUA_PARAM_STRING(value, 3); script_entry->menu_entry->help = value; }
    else if(!strcmp(key, "help2")) { LUA_PARAM_STRING(value, 3); script_entry->menu_entry->help2 = value; }
    else if(!strcmp(key, "advanced")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->advanced = value; }
    else if(!strcmp(key, "depends_on")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->depends_on = value; }
    else if(!strcmp(key, "edit_mode")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->edit_mode = value; }
    else if(!strcmp(key, "hidden")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->hidden = value; }
    else if(!strcmp(key, "icon_type")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->icon_type = value; }
    else if(!strcmp(key, "jhidden")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->jhidden = value; }
    else if(!strcmp(key, "max")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->max = value; }
    else if(!strcmp(key, "min")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->min = value; }
    else if(!strcmp(key, "selected")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->selected = value; }
    else if(!strcmp(key, "shidden")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->shidden = value; }
    else if(!strcmp(key, "starred")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->starred = value; }
    else if(!strcmp(key, "submenu_height")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->submenu_height = value; }
    else if(!strcmp(key, "submenu_width")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->submenu_width = value; }
    else if(!strcmp(key, "unit")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->unit = value; }
    else if(!strcmp(key, "works_best_in")) { LUA_PARAM_INT(value, 3); script_entry->menu_entry->works_best_in = value; }
    else if(!strcmp(key, "select"))
    {
        if(script_entry->select_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, script_entry->select_ref);
        if(!lua_isfunction(L, 3))
        {
            script_entry->select_ref = LUA_NOREF;
            script_entry->menu_entry->select = NULL;
        }
        else
        {
            script_entry->select_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            script_entry->menu_entry->select = script_menu_select;
        }
    }
    else if(!strcmp(key, "update"))
    {
        if(script_entry->update_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, script_entry->update_ref);
        if(!lua_isfunction(L, 3)) script_entry->update_ref = LUA_NOREF;
        else
        {
            script_entry->update_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            script_entry->menu_entry->update = script_menu_update;
        }
    }
    else if(!strcmp(key, "info"))
    {
        if(script_entry->info_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, script_entry->info_ref);
        if(!lua_isfunction(L, 3)) script_entry->info_ref = LUA_NOREF;
        else
        {
            script_entry->info_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            script_entry->menu_entry->update = script_menu_update;
        }
    }
    else if(!strcmp(key, "warning"))
    {
        if(script_entry->warning_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, script_entry->warning_ref);
        if(!lua_isfunction(L, 3)) script_entry->warning_ref = LUA_NOREF;
        else
        {
            script_entry->warning_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            script_entry->menu_entry->update = script_menu_update;
        }
    }
    else
    {
        //set the key in the metatable
        if(lua_getmetatable(L, 1))
        {
            lua_pushvalue(L, 2);
            lua_rawget(L, -2);
        }
        else
        {
            lua_pushnil(L);
        }
    }
    return 0;
}

static int get_function_ref(lua_State * L, const char * name)
{
    if(lua_getfield(L, -1, name) == LUA_TFUNCTION)
    {
        return luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        lua_pop(L,1);
        return LUA_NOREF;
    }
}

static void load_menu_entry(lua_State * L, struct script_menu_entry * script_entry, struct menu_entry * menu_entry, const char * default_name)
{
    if(!menu_entry)
    {
        menu_entry = malloc(sizeof(struct menu_entry));
        if(!menu_entry)
        {
            luaL_error(L, "malloc error creating menu_entry");
            return;
        }
    }
    memset(menu_entry, 0, sizeof(struct menu_entry));
    script_entry->L = L;
    script_entry->menu_entry = menu_entry;
    menu_entry->priv = script_entry;
    menu_entry->name = LUA_FIELD_STRING("name", default_name);
    menu_entry->help = LUA_FIELD_STRING("help", "");
    menu_entry->help2 = LUA_FIELD_STRING("help2", "");
    menu_entry->depends_on = LUA_FIELD_INT("depends_on", 0);
    menu_entry->icon_type = LUA_FIELD_INT("icon_type", 0);
    menu_entry->unit = LUA_FIELD_INT("unit", 0);
    menu_entry->min = LUA_FIELD_INT("min", 0);
    menu_entry->max = LUA_FIELD_INT("max", 0);
    menu_entry->works_best_in = LUA_FIELD_INT("works_best_in", 0);
    menu_entry->submenu_width = LUA_FIELD_INT("submenu_width", 0);
    menu_entry->submenu_height = LUA_FIELD_INT("submenu_height", 0);
    menu_entry->hidden = LUA_FIELD_INT("hidden", 0);
    menu_entry->jhidden = LUA_FIELD_INT("jhidden", 0);
    menu_entry->shidden = LUA_FIELD_INT("shidden", 0);
    menu_entry->starred = LUA_FIELD_INT("starred", 0);
    /// List of strings to display as choices in the menu item
    // @tfield table choices
    if(lua_getfield(L, -1, "choices") == LUA_TTABLE)
    {
        int choices_count = luaL_len(L, -1);
        menu_entry->choices = malloc(sizeof(char*) * choices_count);
        if(menu_entry->choices)
        {
            int choice_index = 0;
            for (choice_index = 0; choice_index < choices_count; choice_index++)
            {
                if(lua_geti(L, -1, choice_index + 1) == LUA_TSTRING) //lua arrays are 1 based
                {
                    menu_entry->choices[choice_index] = lua_tostring(L, -1);
                }
                else
                {
                    console_printf("invalid choice[%d]\n", choice_index);
                    menu_entry->choices[choice_index] = NULL;
                    choices_count = choice_index;
                }
                lua_pop(L, 1);
            }
            menu_entry->min = 0;
            menu_entry->max = choices_count - 1;
        }
    }
    lua_pop(L, 1);
    
    if((script_entry->select_ref = get_function_ref(L, "select")) != LUA_NOREF) menu_entry->select = script_menu_select;
    if((script_entry->update_ref = get_function_ref(L, "update")) != LUA_NOREF) menu_entry->update = script_menu_update;
    if((script_entry->warning_ref = get_function_ref(L, "warning")) != LUA_NOREF) menu_entry->update = script_menu_update;
    if((script_entry->info_ref = get_function_ref(L, "info")) != LUA_NOREF) menu_entry->update = script_menu_update;
    
    /// Table of more menu tables that define a submenu
    // @tfield table submenu
    if(lua_getfield(L, -1, "submenu") == LUA_TTABLE)
    {
        int submenu_count = luaL_len(L, -1);
        if(submenu_count > 0)
        {
            int submenu_index = 0;
            script_entry->menu_value = 1;
            script_entry->menu_entry->icon_type = IT_SUBMENU;
            script_entry->menu_entry->select = menu_open_submenu;
            script_entry->menu_entry->children = malloc(sizeof(struct menu_entry) * (1 + submenu_count));
            memset(script_entry->menu_entry->children, 0, sizeof(struct menu_entry) * (1 + submenu_count));
            
            if(lua_getmetatable(L, -3))
            {
                //create a new submenu table
                lua_newtable(L);
                lua_setfield(L, -2, "submenu");
                lua_pop(L, 1);
            }
            else
            {
                console_printf("warning: could not create metatable submenu");
            }
            
            for (submenu_index = 0; submenu_index < submenu_count; submenu_index++)
            {
                if(lua_geti(L, -1, submenu_index + 1) == LUA_TTABLE) //lua arrays are 1 based
                {
                    struct script_menu_entry * new_entry = lua_newuserdata(L, sizeof(struct script_menu_entry));
                    
                    //add a metatable to the userdata object for value lookups and to store submenu
                    lua_newtable(L);
                    lua_pushcfunction(L, luaCB_menu_index);
                    lua_setfield(L, -2, "__index");
                    lua_pushcfunction(L, luaCB_menu_newindex);
                    lua_setfield(L, -2, "__newindex");
                    lua_setmetatable(L, -2);
                    
                    lua_pushvalue(L, -2);
                    load_menu_entry(L, new_entry, &(script_entry->menu_entry->children[submenu_index]), "unknown");
                    lua_pop(L, 1);
                    
                    //add the new userdata object to the submenu table of the parent metatable, using the menu name as a key
                    if(lua_getmetatable(L, -5))
                    {
                        if(lua_getfield(L, -1, "submenu") == LUA_TTABLE)
                        {
                            lua_pushvalue(L, -3);
                            lua_setfield(L, -2, script_entry->menu_entry->children[submenu_index].name);
                        }
                        else
                        {
                            console_printf("warning: could not get metatable submenu");
                        }
                        lua_pop(L, 2);
                    }
                    else
                    {
                        console_printf("warning: could not get parent metatable");
                    }
                    
                    lua_pop(L, 1);//userdata
                }
                else
                {
                    console_printf("invalid submenu[%d]\n", submenu_index);
                }
                lua_pop(L, 1);
            }
            script_entry->menu_entry->children[submenu_index].priv = MENU_EOL_PRIV;
        }
    }
    lua_pop(L, 1);
    
    //load default 'value' so our index metamethod works
    if(menu_entry->choices)
    {
        const char * str_value = LUA_FIELD_STRING("value", "");
        script_entry->menu_value = get_index_for_choices(menu_entry, str_value);
    }
    else
    {
        script_entry->menu_value = LUA_FIELD_INT("value", 0);
    }
    
}

/// Removes this menu entry
// @function remove
static int luaCB_menu_remove(lua_State * L)
{
    if(!lua_isuserdata(L, 1)) return luaL_argerror(L, 1, NULL);
    struct script_menu_entry * script_entry = lua_touserdata(L, 1);
    if(!script_entry || !script_entry->menu_entry) return luaL_argerror(L, 1, "internal error: userdata was NULL");
    if(lua_getmetatable(L, 1))
    {
        const char * parent = LUA_FIELD_STRING("parent", "LUA");
        menu_remove(parent, script_entry->menu_entry, 1);
    }
    else
    {
        return luaL_error(L, "could not get metatable for userdata");
    }
    return 0;
}

const luaL_Reg menulib[] =
{
    {"new", luaCB_menu_new},
    {NULL, NULL}
};

int luaopen_menu(lua_State * L)
{
    lua_newtable(L);
    luaL_setfuncs(L, menulib, 0);
    return 1;
}