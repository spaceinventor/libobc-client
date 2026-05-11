#include <csp/csp.h>
#include <csp/csp_crc32.h>
#include <FreeRTOS.h>
#include <task.h>
#include <inttypes.h>
#include <lua.h>
#include <stdio.h>

#include "portmacro.h"
#include "vmem/vmem_fram.h"

#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <string.h>
#include <portmacro.h>
#include <lstate.h>

#include "lparam.h"
#include "lcsp.h"
#include "lfreertos.h"
#include "lsi.h"
#include "lua_conf.h"

#include <param/param.h>

#include <semphr.h>


// todo make compile time meson option?
#define LUA_STACK_SIZE 1024
#define LUA_MAX_SIZE 0x10000 
#define LUA_SLOTS 10

static void lua_callback(param_t * param, int offset);
static void lua_print_cb(param_t * param, int offset);

static int8_t _lua_run[LUA_TASKS] = {0};
static int32_t _lua_print_list = 0;
static uint32_t _lua_size = LUA_MAX_SIZE;
static int32_t _lua_errno[LUA_TASKS] = {0};
static uint32_t _lua_state[LUA_TASKS] = {0};
static StaticTask_t lua_tcb[LUA_TASKS] __attribute__((section(".noinit")));
static StackType_t lua_stack[LUA_TASKS][LUA_STACK_SIZE] __attribute__((section(".noinit")));
static TaskHandle_t lua_thandle[LUA_TASKS];

static StaticSemaphore_t Lbuf_lock_buf;
static SemaphoreHandle_t Lbuf_lock = NULL;
static uint8_t Lbuf[LUA_MAX_SIZE] __attribute__((section(".noinit")));

#define LUA_ERRNO_SIZE -1 /* Lua script exceeded max size */
#define LUA_ERRNO_CRC -2 /* Lua bytecode crc32 does not match header crc32 */
#define LUA_ERRNO_PCALL -3 /* Lua script threw an error, is printed to stdbuf */
#define LUA_ERRNO_TASK -4 /* Task creation erro */
#define LUA_ERRNO_INTERNAL -5 /* Lua state creation returned NULL */
#define LUA_ERRNO_VMEM_INVALID -6 /* Target vmem addr for lua script is not valid */
#define LUA_ERRNO_VERSION -7 /* Version mismatch between module and script */

#define PARAMID_LUA_RUN                         130
#define PARAMID_LUA_SIZE                        131
#define PARAMID_LUA_SLOTS                       132
#define PARAMID_LUA_ERRNO                       133
#define PARAMID_LUA_SLOTS_ADDR                  134
#define PARAMID_LUA_ARGS                        135
#define PARAMID_LUA_STATUS_CODE                 136
#define PARAMID_LUA_PRINT_LIST                  137

#define PM_LUA    (1 << (PM_USER_FLAGS_OFFSET + 1))

extern vmem_t vmem_lua;
extern vmem_t vmem_lbyte;

PARAM_DEFINE_STATIC_RAM(PARAMID_LUA_RUN, lua_run, PARAM_TYPE_INT8, LUA_TASKS, sizeof(int8_t), PM_CONF|PM_LUA, lua_callback, NULL, &_lua_run, "Set to slot index to start running slot in lua task. -1 when not running");
PARAM_DEFINE_STATIC_VMEM(PARAMID_LUA_SLOTS_ADDR, lua_vmem_slots, PARAM_TYPE_XINT32, LUA_SLOTS, sizeof(uint32_t), PM_CONF, NULL, "", lua, 0x0, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_LUA_ARGS, lua_args, PARAM_TYPE_UINT32, LUA_TASKS, sizeof(uint32_t), PM_CONF|PM_LUA, NULL, "", lua, (LUA_SLOTS * sizeof(uint32_t)), "Pass arg to lua script inside global var 'Larg'");
PARAM_DEFINE_STATIC_RAM(PARAMID_LUA_SIZE, lua_max_size, PARAM_TYPE_UINT32, 1, sizeof(uint32_t), PM_READONLY, NULL, NULL, &_lua_size, "");
PARAM_DEFINE_STATIC_RAM(PARAMID_LUA_ERRNO, lua_errno, PARAM_TYPE_INT32, LUA_TASKS, sizeof(int32_t), PM_READONLY|PM_LUA, NULL, NULL, &_lua_errno, "Negative value indicate error during Lua execution");
PARAM_DEFINE_STATIC_RAM(PARAMID_LUA_STATUS_CODE, lua_state, PARAM_TYPE_UINT32, LUA_TASKS, sizeof(uint32_t), PM_READONLY|PM_LUA, NULL, NULL, &_lua_state, "Lua scripts can use set_state() to reflect internal state while running");
PARAM_DEFINE_STATIC_RAM(PARAMID_LUA_PRINT_LIST, lua_print_list, PARAM_TYPE_INT32, 1, sizeof(int32_t), PM_DEBUG, lua_print_cb, NULL, &_lua_print_list, "Set to trigger print of lua script list");

static bool Lbuf_lock_take(int block_time_ticks)
{
	if (xSemaphoreTake(Lbuf_lock, block_time_ticks) == pdTRUE)
		return true;
	return false;
}

static void Lbuf_lock_give(void) {
	xSemaphoreGive(Lbuf_lock);
}

static void Lbuf_lock_init(void) {
	if (Lbuf_lock == NULL)
		Lbuf_lock = xSemaphoreCreateMutexStatic(&Lbuf_lock_buf);
}

static void lua_callback(param_t * param, int offset){
    if(offset < LUA_TASKS){
        xTaskNotifyGive(lua_thandle[offset]);
    }
}

static void lua_print_cb(param_t * param, int offset){
    lua_header_t header = {0};

    for(int i = 0; i < LUA_SLOTS; i++){
        void * vmem_slot_p = (void *)param_get_uint32_array(&lua_vmem_slots, i);
        if((intptr_t)vmem_slot_p >= vmem_lbyte.vaddr && (intptr_t)vmem_slot_p < (intptr_t)(vmem_lbyte.vaddr + vmem_lbyte.size)){
            vmem_memcpy((void *)&header, vmem_slot_p, sizeof(header));
            if(header.size){
                printf("slot: %d name: %s size: %"PRIu32" crc: 0x%08"PRIX32", version: %"PRIu8"\n", i, header.name, header.size, header.crc, header.lua_si_version);
            }
        } else {
            printf("slot: %d invalid vmem addr\n", i);
        }
    }
}

static int openLibs(lua_State *L){
    luaL_openlibs(L);
    return 0;
}

void lua_task(void * param) {

    int offset = (int)param;

    /* TODO refactor to use goto when error 
       TODO create libparam vmem function to check if addr is valid vmem
    */
    while(1){

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        Lbuf_lock_take(portMAX_DELAY);

        lua_State * L = NULL;
        int invalid_vmem = 1;

        int8_t sel_slot = param_get_int8_array(&lua_run, offset);
        if(sel_slot < 0 || sel_slot > LUA_SLOTS - 1){
            Lbuf_lock_give();
            continue;
        }
        void * vmem_addr = (void *)param_get_uint32_array(&lua_vmem_slots, sel_slot);

        if((intptr_t)vmem_addr >= vmem_lbyte.vaddr && (intptr_t)(vmem_addr + sizeof(lua_header_t)) < (vmem_lbyte.vaddr + vmem_lbyte.size)){
            invalid_vmem = 0;
        }

        if(invalid_vmem){
            printf("LUA: Invalid VMEM addr %p\n", vmem_addr);
            param_set_int32_array(&lua_errno, offset, LUA_ERRNO_VMEM_INVALID);
            param_set_int8_array_nocallback(&lua_run, offset, -1);
            Lbuf_lock_give();
            continue;
        }

        vmem_memcpy(Lbuf, vmem_addr, sizeof(lua_header_t));
        lua_header_t * header = (lua_header_t *)&Lbuf;
        uint32_t size = header->size;
        uint32_t crc = header->crc;
        uint8_t version = header->lua_si_version;
        char name[32];
        strncpy(name, header->name, 32);

        if(version != LUA_SI_VERSION){
            printf("LUA: Invalid version %u, running version %u\n", version, LUA_SI_VERSION);
            param_set_int32_array(&lua_errno, offset, LUA_ERRNO_VERSION);
            param_set_int8_array_nocallback(&lua_run, offset, -1);
            Lbuf_lock_give();
            continue;
        }
        if(size > LUA_MAX_SIZE && size != 0){
            printf("LUA: Invalid size %"PRIu32"\n", size);
            param_set_int32_array(&lua_errno, offset, LUA_ERRNO_SIZE);
            param_set_int8_array_nocallback(&lua_run, offset, -1);
            Lbuf_lock_give();
            continue;
        }

        invalid_vmem = 1;
        if(((intptr_t)vmem_addr + size) < (vmem_lbyte.vaddr + vmem_lbyte.size)){
            invalid_vmem = 0;
        }
        if(invalid_vmem){
            printf("LUA: Invalid VMEM addr %p\n", vmem_addr);
            param_set_int32_array(&lua_errno, offset, LUA_ERRNO_VMEM_INVALID);
            param_set_int8_array_nocallback(&lua_run, offset, -1);
            Lbuf_lock_give();
            continue;
        }
        vmem_memcpy(Lbuf + sizeof(lua_header_t), vmem_addr + sizeof(lua_header_t), size);
        uint32_t crc_cal = csp_crc32_memory(Lbuf + sizeof(lua_header_t), size);
        if (crc != crc_cal) {
            printf("LUA: CRC mismatch crc expected: %"PRIu32"  crc got %"PRIu32"\n", crc, crc_cal);
            param_set_int32_array(&lua_errno, offset, LUA_ERRNO_CRC);
            param_set_int8_array_nocallback(&lua_run, offset, -1);
            Lbuf_lock_give();
            continue;
        }

        L = luaL_newstate();
        if(!L){
            param_set_int32_array(&lua_errno, offset, LUA_ERRNO_INTERNAL);
            param_set_int8_array_nocallback(&lua_run, offset, -1);
            Lbuf_lock_give();
            continue;
        }

        lua_pushcfunction(L, openLibs);

        if(lua_pcall(L, 0, 0, 0) != LUA_OK){
            param_set_int32_array(&lua_errno, offset, LUA_ERRNO_PCALL);
            printf("LUA: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
            lua_close(L);
            Lbuf_lock_give();
            continue;
        }

        luaopen_si(L, offset);
        luaopen_freertos(L);
        luaopen_param(L);
        luaopen_csp(L);

        lua_pushinteger(L, param_get_uint32_array(&lua_args, offset));
        lua_setglobal(L, "Larg");

        if(luaL_loadbuffer(L, (const char *)Lbuf + sizeof(lua_header_t), size, "lua") != LUA_OK){
            param_set_int32_array(&lua_errno, offset, LUA_ERRNO_INTERNAL);
            param_set_int8_array_nocallback(&lua_run, offset, -1);
            Lbuf_lock_give();
            lua_close(L);
            continue;
        }

        Lbuf_lock_give();

        printf("LUA: task %d started %s\n", offset, name);
        if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
            param_set_int32_array(&lua_errno, offset, LUA_ERRNO_PCALL);
            printf("LUA: %s\n", lua_tostring(L, -1));
        } 

        lua_close(L);
        param_set_int8_array_nocallback(&lua_run, offset, -1);
        printf("LUA: task %d ended %s\n", offset, name);
    }
}


void lua_init(void) {

    Lbuf_lock_init();

    for (int i = 0; i < LUA_TASKS; i++) {
        param_set_int8_array_nocallback(&lua_run, i, -1);
        lua_thandle[i] = xTaskCreateStatic(lua_task, "LUATASK", LUA_STACK_SIZE, (void *)i, 1, lua_stack[i], &lua_tcb[i]);
        if (lua_thandle[i] == NULL) {
            printf("Failed to create lua task\n");
            param_set_int32_array(&lua_errno, i, LUA_ERRNO_TASK);
        }
    }
}
