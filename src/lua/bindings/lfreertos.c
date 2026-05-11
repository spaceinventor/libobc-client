#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <FreeRTOS.h>
#include <task.h>

static int lua_vTaskDelay(lua_State *L) {
    // Check if the argument is a number and get it
    int delay = luaL_checkinteger(L, 1);

    // Call the FreeRTOS vTaskDelay function
    vTaskDelay(pdMS_TO_TICKS(delay)); // Assuming delay is in milliseconds

    return 0;
}

LUAMOD_API int luaopen_freertos(lua_State * L) {
    lua_pushcfunction(L, lua_vTaskDelay);
    lua_setglobal(L, "sleep");
    return 1;
}