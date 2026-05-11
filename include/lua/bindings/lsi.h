#pragma once

#define LUA_SI_VERSION 2

int get_timeout(lua_State * L);

LUAMOD_API int luaopen_si(lua_State * L, int task_offset);

typedef struct lua_header_s {
    uint32_t crc;
    uint32_t size;
    char name[32];
    uint8_t lua_si_version;
    uint8_t bytecode[];
} __attribute__((__packed__)) lua_header_t;