#include <stdlib.h>
#include <csp/csp_cmp.h>
#include <csp/csp_types.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>
#include <param/param_string.h>
#include <param/param_server.h>
#include <param/param_serializer.h>
#include <vmem/vmem_client.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "lsi.h"

#include <sys/queue.h>
#include <csp/csp.h>
#include <csp/csp_hooks.h>

// #define LPARAM_DEBUG

typedef struct lua_param_s {
    param_t *param;
    int *is_set;
    SLIST_ENTRY(lua_param_s) next;
} lua_param_t;

typedef struct lua_vmem_s {
    uint16_t node;
    uint64_t addr;

    int use_peek_poke;
    int version;
    int timeout;
} lua_vmem_t;

typedef SLIST_HEAD(global_param_list_head_s, lua_param_s) global_param_list_head_t;

typedef struct queue_list_entry_s {
    lua_param_t *lparam;
    SLIST_ENTRY(queue_list_entry_s) next;
} queue_list_entry_t;

typedef SLIST_HEAD(queue_list_head_s, queue_list_entry_s) queue_list_head_t;

lua_param_t * lua_param_list_find_id(int node, int id, global_param_list_head_t * head);
void add_param_to_list(queue_list_head_t *head, lua_param_t *lparam);

union ValueBuffer {
    LUA_INTEGER intparam;
    LUA_NUMBER doubleparam;
    char str[128];
} __attribute__((aligned(16)));

static int luaparam_add(lua_State * L) {

    char * name = "lparam";
    unsigned int typeid = PARAM_TYPE_INVALID;
    int size = 1;
    int nargs = lua_gettop(L);

    lua_getfield(L, LUA_REGISTRYINDEX, "__paramlist");
    global_param_list_head_t *g_param_list_head = luaL_checkudata(L, -1, "global_param_list_m");
    lua_pop(L, 1);

    int id = luaL_checkinteger(L, 1);
    int node = luaL_checkinteger(L, 2);
    if(lua_isinteger(L, 3)){
        typeid = luaL_checkinteger(L, 3);
    } else {
        const char * typestr = luaL_checkstring(L, 3);
        typeid = param_typestr_to_typeid((char *)typestr);
    }

    if(typeid >= PARAM_TYPE_INVALID){
        luaL_error(L, "Invalid type param type %d\n", typeid);
    }

    if(nargs > 3){
        size = luaL_checkinteger(L, 4);
    }

    lua_param_t *lparam = lua_param_list_find_id(node, id, g_param_list_head);

    if (lparam == NULL) {
        param_t *param = param_list_create_remote(id, node, typeid, 0, size, name, NULL, NULL, -1);
        if (param == NULL) {
            luaL_error(L, "Didn't find existing param and couldn't create new\n");
        }
        lparam = malloc(sizeof(*lparam));
        if (lparam == NULL) {
            luaL_error(L, "Malloc failed");
        }
        lparam->param = param;
        int *is_set = malloc(size * sizeof(int));
        if (is_set == NULL) {
            luaL_error(L, "Malloc failed");
        }
        memset(is_set, 0, size * sizeof(int));
        lparam->is_set = is_set;
        SLIST_INSERT_HEAD(g_param_list_head, lparam, next);
    }

    lua_param_t **ud = lua_newuserdata(L, sizeof(*ud));
    if (!ud) {
        free(lparam);  // TODO Kevin: When should we free `lparam` here?
        return luaL_error(L, "Failed to allocate ud lua_param_t");
    }
    *ud = lparam;

    if (size == 1) {
        luaL_getmetatable(L, "param_single_m");
        lua_setmetatable(L, -2);

    } else {
        luaL_getmetatable(L, "param_array_m");
        lua_setmetatable(L, -2);
    }
    return 1;
}

static int luaparam_queue(lua_State * L) {

    queue_list_head_t *list_head = malloc(sizeof(*list_head));
    if (!list_head) {
        return luaL_error(L, "Failed to allocate queue list head");
    }
    SLIST_INIT(list_head);
    int nargs = lua_gettop(L);

    queue_list_head_t **ud = lua_newuserdata(L, sizeof(*ud));
    if (!ud) {
        free(list_head);
        return luaL_error(L, "Failed to allocate ud q_list_head");
    }
    *ud = list_head;

    int argType = lua_type(L, 1);

    if(nargs == 1){
        if (argType == LUA_TUSERDATA) {
            lua_param_t ** ud = lua_touserdata(L, 1);  /* Expect either `param_m|param_single_m|param_array_m` here */
            add_param_to_list(list_head, *ud);

        } else if (argType == LUA_TTABLE) {
            int len = luaL_len(L, 1);
            for (int i = 1; i <= len; i++) {
                lua_geti(L, 1, i);
                lua_param_t ** ud = lua_touserdata(L, -1);  /* Expect either `param_m|param_single_m|param_array_m` here */
                add_param_to_list(list_head, *ud);
                lua_pop(L, 1);
            }
        }
    }
    luaL_getmetatable(L, "queue_m");
    lua_setmetatable(L, -2);
    return 1;
}

static int luaparam_push(lua_State * L, param_t * param, int index) {

    switch (param->type) {
        case PARAM_TYPE_UINT8:
        case PARAM_TYPE_XINT8: {
            lua_pushinteger(L, param_get_uint8_array(param, index));
            return 1;
        }
        case PARAM_TYPE_UINT16:
        case PARAM_TYPE_XINT16: {
            lua_pushinteger(L, param_get_uint16_array(param, index));
            return 1;
        }
        case PARAM_TYPE_UINT32:
        case PARAM_TYPE_XINT32: {
            lua_pushinteger(L, param_get_uint32_array(param, index));
            return 1;
        }
        case PARAM_TYPE_UINT64:
        case PARAM_TYPE_XINT64: {
            lua_pushinteger(L, param_get_uint64_array(param, index));
            return 1;
        }
        case PARAM_TYPE_INT8: {
            lua_pushinteger(L, param_get_int8_array(param, index));
            return 1;
        }
        case PARAM_TYPE_INT16: {
            lua_pushinteger(L, param_get_int16_array(param, index));
            return 1;
        }
        case PARAM_TYPE_INT32: {
            lua_pushinteger(L, param_get_int32_array(param, index));
            return 1;
        }
        case PARAM_TYPE_INT64: {
            lua_pushinteger(L, param_get_int64_array(param, index));
            return 1;
        }
        case PARAM_TYPE_FLOAT: {
            lua_pushnumber(L, param_get_float_array(param, index));
            return 1;
        }
        case PARAM_TYPE_DOUBLE: {
            lua_pushnumber(L, param_get_double_array(param, index));
            return 1;
        }
        case PARAM_TYPE_DATA:
        case PARAM_TYPE_STRING: {
            const int size = (index == -1) ? param_size(param) : 1;
            char buf[size+1];
            buf[size] = '\0';
            
            if (size == 1) {
                /* Get single char */
                buf[0] = param_get_uint8_array(param, index);
            } else {
                /* Get whole array */
                param_get_string(param, buf, size);
            }
            lua_pushlstring(L, buf, size);
            return 1;
        }
        default: {
            /* Default case to make the compiler happy. Set error and return */
            return -1;
        }
    }
    return -1;
}

lua_param_t * lua_param_list_find_id(int node, int id, global_param_list_head_t *head) {
    
    if (node < 0)
        node = 0;

    lua_param_t *found = NULL;
    lua_param_t *lparam;

    SLIST_FOREACH(lparam, head, next){
        if (*(lparam->param->node) != node){
            continue;
        }

        if (lparam->param->id == id) {
            found = lparam;
            break;
        }
    }
    return found;
}

int lua_param_queue_apply(param_queue_t *queue, global_param_list_head_t * head, int from) {

    int return_code = 0;
    mpack_reader_t reader;
    mpack_reader_init_data(&reader, queue->buffer, queue->used);
    while(reader.data < reader.end) {
        int id, node, offset = -1;
        csp_timestamp_t timestamp = { .tv_sec = 0, .tv_nsec = 0 };
        param_deserialize_id(&reader, &id, &node, &timestamp, &offset, queue);

        /* If the from address is set, and the nodeid is 0, substitue with the source address */
        if (node == 0)
            node = from;

        /* First we search on the specified node in the request or response */
        lua_param_t * lparam = lua_param_list_find_id(node, id, head);

        if (lparam) {
            *lparam->param->timestamp = timestamp;
            param_deserialize_from_mpack_to_param(NULL, queue, lparam->param, offset, &reader);
        } else{
            printf("queue apply could not find param id: %d node: %d\n", id, node);
            /* Move reader forward to skip values */
            mpack_discard(&reader);
        }
    }
    return return_code;
}

static void lua_param_cb(csp_packet_t *response, int verbose, int version, void * context) {

    param_queue_t queue;
    csp_timestamp_t time_now;
    csp_clock_get_time(&time_now);
    param_queue_init(&queue, &response->data[2], response->length - 2, response->length - 2, PARAM_QUEUE_TYPE_SET, version);
    queue.last_node = response->id.src;
    queue.last_timestamp = time_now;

    /* Write data to local memory */
    lua_param_queue_apply(&queue, context, response->id.src);

    csp_buffer_free(response);
}

int lua_param_pull_single(lua_State * L, param_t *param, int offset, int host) {

    csp_packet_t * packet = csp_buffer_get(0);
    if (packet == NULL)
        return -1;

    packet->data[0] = PARAM_PULL_REQUEST_V2;
    packet->data[1] = 0;

    param_queue_t queue;
    param_queue_init(&queue, &packet->data[2], PARAM_SERVER_MTU - 2, 0, PARAM_QUEUE_TYPE_GET, 2);
    param_queue_add(&queue, param, offset, NULL);

    lua_getfield(L, LUA_REGISTRYINDEX, "__paramlist");
    global_param_list_head_t *g_param_list_head = luaL_checkudata(L, -1, "global_param_list_m");
    lua_pop(L, 1);

    packet->length = queue.used + 2;
    packet->id.pri = CSP_PRIO_NORM;
    return param_transaction(packet, host, get_timeout(L), lua_param_cb, 0, 2, g_param_list_head);
}

int lua_param_push_queue(lua_State * L, param_queue_t *queue, int host, int timeout) {

	if ((queue == NULL) || (queue->used == 0))
		return 0;

	csp_packet_t * packet = csp_buffer_get(0);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_PUSH_REQUEST_V2;
	packet->data[1] = 1; // ack with pull flag

	memcpy(&packet->data[2], queue->buffer, queue->used);

    lua_getfield(L, LUA_REGISTRYINDEX, "__paramlist");
    global_param_list_head_t *g_param_list_head = luaL_checkudata(L, -1, "global_param_list_m");
    lua_pop(L, 1);

	packet->length = queue->used + 2;
	packet->id.pri = CSP_PRIO_NORM;
	return param_transaction(packet, host, timeout, lua_param_cb, 0, 2, g_param_list_head);
}

int lua_param_pull_queue(lua_State * L, param_queue_t *queue, int host, int timeout) {

	if ((queue == NULL) || (queue->used == 0))
		return 0;

	csp_packet_t * packet = csp_buffer_get(0);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_PULL_REQUEST_V2;
	packet->data[1] = 0;

	memcpy(&packet->data[2], queue->buffer, queue->used);

    lua_getfield(L, LUA_REGISTRYINDEX, "__paramlist");
    global_param_list_head_t *g_param_list_head = luaL_checkudata(L, -1, "global_param_list_m");
    lua_pop(L, 1);

	packet->length = queue->used + 2;
	packet->id.pri = CSP_PRIO_NORM;
	return param_transaction(packet, host, timeout, lua_param_cb, 0, 2, g_param_list_head);
}

int luaparam_get(lua_State * L) {

    lua_param_t ** ud = lua_touserdata(L, 1);  /* Expect either `param_m|param_single_m|param_array_m` here */
    param_t * param = (*ud)->param;

    int index = -1;
	int count = (param->array_size > 0) ? param->array_size : 1;
    int nargs = lua_gettop(L);
    if(nargs > 1){
        index = lua_tointeger(L, 2);
    }
    if (index >= count) {
        luaL_error(L, "index out of bounds");
    }
    int res = 0;
    res = lua_param_pull_single(L, param, index, *(param->node));
    if (res < 0) {
        luaL_error(L, "param_pull_single failed err code %d\n", res);
    }

    const int is_str = (param->type == PARAM_TYPE_STRING || param->type == PARAM_TYPE_DATA);

    if(index == -1 && count != 1 && !is_str){

        lua_newtable(L);
        for (int i = 0; i < count; i++) {
            (*ud)->is_set[i] = 0;
            res = luaparam_push(L, param, i);
            if (res < 0) {
                luaL_error(L, "param_get failed\n", res);
            }
            lua_rawseti(L, -2, i + 1);
        }
    } else {
        if (!is_str) {
            index = 0;
        }
        (*ud)->is_set[index] = 0;
        res = luaparam_push(L, param, index);
        if (res < 0) {
            luaL_error(L, "param_get failed\n", res);
        }
    }

    return 1;
}

int luaparam_set(lua_State *L) {

    lua_param_t ** ud = lua_touserdata(L, 1);  /* Expect either `param_m|param_single_m|param_array_m` here */
    param_t * param = (*ud)->param;

    int index = 0;
    int val_stack_n = 3;
    int nargs = lua_gettop(L);
    if(nargs == 2){
        val_stack_n = 2;
    } else {
        index = luaL_checkinteger(L, 2);
        val_stack_n = 3;
    }

    if (index >= param->array_size) {
        luaL_error(L, "index out of bounds");
    }

    union ValueBuffer valuebuf = {0};

    switch (param->type) {
        case PARAM_TYPE_UINT8:
        case PARAM_TYPE_XINT8:
        case PARAM_TYPE_UINT16:
        case PARAM_TYPE_XINT16:
        case PARAM_TYPE_UINT32:
        case PARAM_TYPE_XINT32:
        case PARAM_TYPE_UINT64:
        case PARAM_TYPE_XINT64:
        case PARAM_TYPE_INT8:
        case PARAM_TYPE_INT16:
        case PARAM_TYPE_INT32:
        case PARAM_TYPE_INT64:
            valuebuf.intparam = luaL_checkinteger(L, val_stack_n);
            break;
        case PARAM_TYPE_FLOAT:
        case PARAM_TYPE_DOUBLE:
            valuebuf.doubleparam = luaL_checknumber(L, val_stack_n);
            break;
        case PARAM_TYPE_STRING:
        case PARAM_TYPE_DATA:
            strncpy(valuebuf.str, luaL_checkstring(L, val_stack_n), 128);
            break;
        default:
            return 0;
    }

    param_set(param, index, &valuebuf);

    int res = param_push_single(param, index, CSP_PRIO_NORM, &valuebuf, 0, *(param->node), get_timeout(L), 2, false);
    if(res < 0){
        luaL_error(L, "param_pull_single failed err code %d\n", res);
    }
    (*ud)->is_set[index] = 0;
    return 0; 
}

#ifdef LPARAM_DEBUG
static void stackDump (lua_State *L) {

    int i;
    int top = lua_gettop(L);
    for (i = 1; i <= top; i++) {  /* repeat for each level */
    int t = lua_type(L, i);
    switch (t) {

        case LUA_TSTRING:  /* strings */
        printf("`%s'", lua_tostring(L, i));
        break;

        case LUA_TBOOLEAN:  /* booleans */
        printf(lua_toboolean(L, i) ? "true" : "false");
        break;

        case LUA_TNUMBER:  /* numbers */
        printf("%g", lua_tonumber(L, i));
        break;

        default:  /* other values */
        printf("%s", lua_typename(L, t));
        break;

    }
    printf("  ");  /* put a separator */
    }
    printf("\n");  /* end the listing */
}
#endif

static int __index_param_array_m(lua_State * L) {

    lua_param_t ** ud = luaL_checkudata(L, 1, "param_array_m");
    param_t * param = (*ud)->param;

    if (lua_isstring(L, 2)) {
        const char * key = lua_tostring(L, 2);
        if (strcmp(key, "node") == 0) {
            lua_pushinteger(L, *(param->node));
        } else if (strcmp(key, "cache") == 0){
            lua_newtable(L);
            lua_pushvalue(L, 1);
            lua_setfield(L, -2, "__param");

            luaL_getmetatable(L, "cache_m");
            lua_setmetatable(L, -2);
        } else if (strcmp(key, "set") == 0){
            lua_pushcfunction(L, luaparam_set);
        } else if (strcmp(key, "get") == 0){
            lua_pushcfunction(L, luaparam_get);
        } else if (strcmp(key, "id") == 0) {
            lua_pushinteger(L, param->id);
        } else if (strcmp(key, "type") == 0) {
            lua_pushinteger(L, param->type);
        } else if (strcmp(key, "length") == 0) {
            lua_pushinteger(L, param->array_size);
        } else {
            luaL_error(L, "Invalid key\n");
        }
    } else {
        luaL_error(L, "Invalid key type\n");
    }
    return 1;
}

static int __index_param_single_m(lua_State * L) {

    lua_param_t ** ud = luaL_checkudata(L, 1, "param_single_m");
    param_t * param = (*ud)->param;

    if (lua_isstring(L, 2)) {
        const char * key = lua_tostring(L, 2);
        if (strcmp(key, "node") == 0) {
            lua_pushinteger(L, *(param->node));
        } else if (strcmp(key, "cache") == 0){
            int res = luaparam_push(L, param, 0);
            if (res < 0) {
                luaL_error(L, "param_get failed\n", res);
            }
        } else if (strcmp(key, "set") == 0){
            lua_pushcfunction(L, luaparam_set);
        } else if (strcmp(key, "get") == 0){
            lua_pushcfunction(L, luaparam_get);
        } else if (strcmp(key, "id") == 0) {
            lua_pushinteger(L, param->id);
        } else if (strcmp(key, "type") == 0) {
            lua_pushinteger(L, param->type);
        } else if (strcmp(key, "length") == 0) {
            lua_pushinteger(L, param->array_size);
        } else {
            luaL_error(L, "Invalid key\n");
        }
    } else {
        luaL_error(L, "Invalid key type\n");
    }
    return 1;
}

static int __len_param_m(lua_State *L) {

    lua_param_t ** ud = lua_touserdata(L, 1);  /* Expect either `param_m|param_single_m|param_array_m` here */
    param_t * param = (*ud)->param;

    lua_pushinteger(L, param->array_size);
    return 1;
}

static int queue_gc(lua_State *L) {

    queue_list_head_t **ud = luaL_checkudata(L, 1, "queue_m");

    if(ud && *ud){

        queue_list_head_t *list_head = *ud;
        queue_list_entry_t *entry;
        while (!SLIST_EMPTY(list_head)) {
            entry = SLIST_FIRST(list_head);
            SLIST_REMOVE_HEAD(list_head, next);
            free(entry);
        }

        free(list_head);
    }
    return 0;
}

void add_param_to_list(queue_list_head_t *head, lua_param_t *lparam) {

    queue_list_entry_t *new_entry = malloc(sizeof(*new_entry));
    if (!new_entry) {
        return;
    }
    
    new_entry->lparam = lparam;
    
    SLIST_INSERT_HEAD(head, new_entry, next);
}

void lparam_queue_remove(queue_list_head_t *head, lua_param_t *lparam) {

    queue_list_entry_t *curelm = NULL;

    if (!head || !head->slh_first) {
        return;
    }

    if (head->slh_first->lparam == lparam) {
        queue_list_entry_t *temp = head->slh_first;
        SLIST_REMOVE_HEAD(head, next);
        free(temp);
    } else {
        curelm = head->slh_first;
        while (curelm->next.sle_next != NULL && curelm->next.sle_next->lparam != lparam) {
            curelm = curelm->next.sle_next;
        }

        if (curelm->next.sle_next != NULL) {
            queue_list_entry_t *toRemove = curelm->next.sle_next;
            curelm->next.sle_next = curelm->next.sle_next->next.sle_next;
            free(toRemove);
        }
    }
}

static int lua_queue_remove(lua_State *L) {

    queue_list_head_t **list_head = luaL_checkudata(L, 1, "queue_m");

    int argType = lua_type(L, 2);

    if (argType == LUA_TUSERDATA) {
        lua_param_t **ud = lua_touserdata(L, 2);  /* Expect either `param_m|param_single_m|param_array_m` here */

        lparam_queue_remove(*list_head, *ud);

    } else if (argType == LUA_TTABLE) {
        int len = luaL_len(L, 2);
        for (int i = 1; i <= len; i++) {
            lua_geti(L, 2, i);
            lua_param_t **ud = lua_touserdata(L, -1);  /* Expect either `param_m|param_single_m|param_array_m` here */
            lparam_queue_remove(*list_head, *ud);
            lua_pop(L, 1);
        }
    }
    return 0;
}

static int lua_queue_add(lua_State *L) {

    queue_list_head_t **list_head = luaL_checkudata(L, 1, "queue_m");

    int argType = lua_type(L, 2);

    if (argType == LUA_TUSERDATA) {
        lua_param_t **ud = lua_touserdata(L, 2);  /* Expect either `param_m|param_single_m|param_array_m` here */
        add_param_to_list(*list_head, *ud);
    } else if (argType == LUA_TTABLE) {
        int len = luaL_len(L, 2);
        for (int i = 1; i <= len; i++) {
            lua_geti(L, 2, i);
            lua_param_t **ud = lua_touserdata(L, -1);  /* Expect either `param_m|param_single_m|param_array_m` here */
            add_param_to_list(*list_head, *ud);
            lua_pop(L, 1);
        }
    }
    return 0;
}

static int lua_queue_push(lua_State *L) {
    queue_list_head_t **list_head = luaL_checkudata(L, 1, "queue_m");
    if(SLIST_FIRST(*list_head) == NULL){
        luaL_error(L, "queue empty");
    }

    char queue_buf[PARAM_SERVER_MTU]; // stack TODO?
    param_queue_t param_queue = { .buffer = queue_buf, .buffer_size = PARAM_SERVER_MTU, .type = PARAM_QUEUE_TYPE_SET, .version = 2 };
    queue_list_entry_t * queue_entry = NULL;
    int node = -1;
    int timeout = get_timeout(L);

    queue_list_head_t temp_head;
    queue_list_entry_t *temp_q_entry = NULL;
    temp_head.slh_first = (*list_head)->slh_first;

    SLIST_FOREACH(queue_entry, *list_head, next) {
        if(node == -1) {
            node = *(queue_entry->lparam->param->node);
        } else if (node != *(queue_entry->lparam->param->node)) {
            luaL_error(L, "all remote params in param list need to be on same remote");
        }

        for(int i = 0; i < queue_entry->lparam->param->array_size; i++){
            if(queue_entry->lparam->is_set[i]){
                /* MAYBE TODO ? create another mpack pack param so we can use the pack as array */
                int res = param_queue_add(&param_queue, queue_entry->lparam->param, i, NULL);
                if(res < 0) {
                    int res = lua_param_push_queue(L, &param_queue, node, timeout);
                    if(res < 0){
                        luaL_error(L, "push queue fail");
                    }
                    SLIST_FOREACH(temp_q_entry, &temp_head, next) {
                        /* if entry is last to be added only reset is_set up to actual indices added to queue */
                        int array_size = temp_q_entry == queue_entry ? i + 1 : temp_q_entry->lparam->param->array_size;
                        for(int j = 0; j < array_size; j++){
                            temp_q_entry->lparam->is_set[j] = 0;
                        }
                        if(temp_q_entry == queue_entry){
                            break;
                        }
                    }
                    /* set temp head to current queue entry not next as there could be missing indices that needs to be reset */
                    temp_head.slh_first = queue_entry;
                    param_queue.used = 0;
                }
            }
        }
    }
    if(param_queue.used > 0){
        int res = lua_param_push_queue(L, &param_queue, node, timeout);
        if(res < 0){
            luaL_error(L, "push queue fail");
        }
        SLIST_FOREACH(temp_q_entry, &temp_head, next) {
            for(int j = 0; j < temp_q_entry->lparam->param->array_size; j++){
                temp_q_entry->lparam->is_set[j] = 0;
            }
        }
    }
    return 0;
}

static int lua_queue_pull(lua_State *L) {

    queue_list_head_t **list_head = luaL_checkudata(L, 1, "queue_m");
    if(SLIST_FIRST(*list_head) == NULL){
        luaL_error(L, "queue empty");
    }

    char queue_buf[PARAM_SERVER_MTU]; // stack TODO?
    param_queue_t param_queue = { .buffer = queue_buf, .buffer_size = PARAM_SERVER_MTU, .type = PARAM_QUEUE_TYPE_GET, .version = 2 };
    queue_list_entry_t * queue_entry = NULL;
    int node = -1;

    queue_list_head_t temp_head;
    queue_list_entry_t *temp_q_entry = NULL;
    temp_head.slh_first = (*list_head)->slh_first;

    SLIST_FOREACH(queue_entry, *list_head, next){
        if(node == -1) {
            node = *(queue_entry->lparam->param->node);
        } else if (node != *(queue_entry->lparam->param->node)) {
            luaL_error(L, "all remote params in param list need to be on same remote");
        }

        int res = param_queue_add(&param_queue, queue_entry->lparam->param, -1, NULL);
        if(res < 0){
            // pull and reset queue buf continue
            int res = lua_param_pull_queue(L, &param_queue, node, get_timeout(L));
            if(res < 0){
                luaL_error(L, "pull queue fail");
            }
            SLIST_FOREACH(temp_q_entry, &temp_head, next) {
                /* if entry is last to be added only reset is_set up to actual indices added to queue */
                int array_size = temp_q_entry->lparam->param->array_size;
                for(int j = 0; j < array_size; j++){
                    temp_q_entry->lparam->is_set[j] = 0;
                }
                if(temp_q_entry == queue_entry){
                    break;
                }
            }
            temp_head.slh_first = queue_entry;
            param_queue.used = 0;
        }
    }
    if(param_queue.used > 0){
        int res = lua_param_pull_queue(L, &param_queue, node, get_timeout(L));
        if(res < 0){
            luaL_error(L, "pull queue fail");
        }
        SLIST_FOREACH(temp_q_entry, &temp_head, next) {
            for(int j = 0; j < temp_q_entry->lparam->param->array_size; j++){
                temp_q_entry->lparam->is_set[j] = 0;
            }
        }
    }

    return 0;
}

#ifdef LPARAM_DEBUG
static int lua_queue_print(lua_State *L){

    queue_list_head_t **list_head = luaL_checkudata(L, 1, "queue_m");
    if(SLIST_FIRST(*list_head) == NULL){
        printf("queue empty\n");
        return 0;
    }
    queue_list_entry_t * queue_entry = NULL;

    SLIST_FOREACH(queue_entry, *list_head, next){
		param_print(queue_entry->lparam->param, -1, NULL, 0, 2, 0);
        printf("is_set:");
        for(int i = 0; i < queue_entry->lparam->param->array_size; i++){
            printf("[%d]", queue_entry->lparam->is_set[i]);
        }
        printf("\n");
    }

    return 0;
}
#endif

static int __index_cache_m(lua_State *L) {

    lua_getfield(L, 1, "__param");
    param_t **ud = lua_touserdata(L, -1);  /* Expect either `param_m|param_single_m|param_array_m` here */
    lua_pop(L, 1);
    param_t * param = *ud;

    int index = luaL_checkinteger(L, 2);
    int res = 0;
    res = luaparam_push(L, param, index);
    if (res < 0) {
        luaL_error(L, "param_get failed\n", res);
    }
    return 1;
}

static int __newindex_param_m(lua_State *L) {

    lua_param_t **ud = lua_touserdata(L, 1);  /* Expect either `param_m|param_single_m|param_array_m` here */
    param_t *param = (*ud)->param;
    union ValueBuffer valuebuf = {0};

    const char* key = luaL_checkstring(L, 2);

    if(strcmp(key, "cache") == 0){
        switch (param->type) {
            case PARAM_TYPE_UINT8:
            case PARAM_TYPE_XINT8:
            case PARAM_TYPE_UINT16:
            case PARAM_TYPE_XINT16:
            case PARAM_TYPE_UINT32:
            case PARAM_TYPE_XINT32:
            case PARAM_TYPE_UINT64:
            case PARAM_TYPE_XINT64: 
            case PARAM_TYPE_INT8:
            case PARAM_TYPE_INT16:
            case PARAM_TYPE_INT32:
            case PARAM_TYPE_INT64:
                valuebuf.intparam = luaL_checkinteger(L, 3);
                break;
            case PARAM_TYPE_FLOAT:
            case PARAM_TYPE_DOUBLE:
                valuebuf.doubleparam = luaL_checknumber(L, 3);
                break;
            case PARAM_TYPE_STRING:
            case PARAM_TYPE_DATA:
                strncpy(valuebuf.str, luaL_checkstring(L, 3), 128);
                break;
            default:
                return 0;
        }

        param_set(param, 0, &valuebuf);
        *(*ud)->is_set = 1;
    }
    return 0;
}

static int __newindex_cache_m(lua_State *L) {

    lua_getfield(L, 1, "__param");
    lua_param_t **ud = luaL_checkudata(L, -1, "cache_m");
    lua_pop(L, 1);
    param_t * param = (*ud)->param;

    int index = luaL_checkinteger(L, 2);
    if (index < param->array_size) {

        union ValueBuffer valuebuf = {0};

        switch (param->type) {
            case PARAM_TYPE_UINT8:
            case PARAM_TYPE_XINT8:
            case PARAM_TYPE_UINT16:
            case PARAM_TYPE_XINT16:
            case PARAM_TYPE_UINT32:
            case PARAM_TYPE_XINT32:
            case PARAM_TYPE_UINT64:
            case PARAM_TYPE_XINT64: 
            case PARAM_TYPE_INT8:
            case PARAM_TYPE_INT16:
            case PARAM_TYPE_INT32:
            case PARAM_TYPE_INT64:
                valuebuf.intparam = luaL_checkinteger(L, 3);
                break;
            case PARAM_TYPE_FLOAT:
            case PARAM_TYPE_DOUBLE:
                valuebuf.doubleparam = luaL_checknumber(L, 3);
                break;
            case PARAM_TYPE_STRING:
            case PARAM_TYPE_DATA:
                strncpy(valuebuf.str, luaL_checkstring(L, 3), 128);
                break;
            default:
                return 0;
        }

        param_set(param, index, &valuebuf);
        (*ud)->is_set[index] = 1;
    }

    return 0;
}

#if 0  /* Kept as simple examples of bindings */
static int lua_vmem_upload(lua_State *L) {

    const uint16_t node = luaL_checkinteger(L, 1);
    const uint64_t address = luaL_checkinteger(L, 2);
    
    // Parse binary data and its length from the 4th argument
    size_t idata_len = 0;
    const char * idata = luaL_checklstring(L, 3, &idata_len);

    const int version = luaL_optinteger(L, 4, 2);
    const int timeout = luaL_optinteger(L, 5, get_timeout(L));


    int res = vmem_upload(node, timeout, address, (char*)idata, idata_len, version);
    if (res < 0) {
        return luaL_error(L, "Upload failed addr=%d node=%d version=%d", address, node, version);
	}

    lua_pushinteger(L, res);

    return 1;
}

static int lua_vmem_download(lua_State *L) {

    const uint16_t node = luaL_checkinteger(L, 1);
    const uint64_t address = luaL_checkinteger(L, 2);
    const uint64_t length = luaL_checkinteger(L, 3);

    const int version = luaL_optinteger(L, 4, 2);
    const int timeout = luaL_optinteger(L, 5, get_timeout(L));
    const int use_rdp = luaL_optinteger(L, 5, 1);
    
    // Allocate writable buffer as Lua userdata
    char *dataout = lua_newuserdata(L, length);
    if (!dataout) {  /* Pop all args before erroring. I suspect that's good practice? */
        return luaL_error(L, "Failed to allocate dataout");
    }


    int res = vmem_download(node, timeout, address, length, dataout, version, use_rdp);
    if (res < 0) {
        return luaL_error(L, "vmem_download() failed addr=%d node=%d version=%d", address, node, version);
	}

    lua_pushlstring(L, dataout, length);

    return 1;
}
#endif


static int luavmem_add(lua_State *L) {

    const uint16_t node = luaL_checkinteger(L, 1);
    const uint64_t address = luaL_checkinteger(L, 2);

    const int use_peek_poke = luaL_optinteger(L, 3, 0);
    const int version = luaL_optinteger(L, 4, (use_peek_poke ? 1 : 2));
    const int timeout = luaL_optinteger(L, 5, get_timeout(L));

    lua_vmem_t *vmem = malloc(sizeof(*vmem));
    if (vmem == NULL) {
        luaL_error(L, "lua_vmem_t malloc failed");
    }

    vmem->node = node;
    vmem->addr = address;
    vmem->use_peek_poke = use_peek_poke;
    vmem->version = version;
    vmem->timeout = timeout;

    lua_vmem_t **ud = lua_newuserdata(L, sizeof(*ud));
    if (!ud) {
        free(vmem);
        return luaL_error(L, "Failed to allocate ud lua_vmem_t");
    }
    *ud = vmem;

    luaL_getmetatable(L, "vmem_m");
    lua_setmetatable(L, -2);

    return 1;
}

static int lua_vmem_poke(lua_State *L, uint16_t node, uint64_t address, const char * idata, unsigned int data_str_len, int version, int timeout) {

    if (version < 1 || version > 2) {
		return luaL_error(L, "Unsupported version: %d, only supports 1 (32-bit) and 2 (64-bit)\n", version);
	}

	struct csp_cmp_message message;

	if(data_str_len % 2 == 1){
		return luaL_error(L, "Invalid length, needs to be whole bytes in hex\n");
	}

    int res = CSP_ERR_TX;

	switch (version) {
		case 1:
		{
			if (address > 0x00000000FFFFFFFFULL) {
				return luaL_error(L, "Poke address out of 32-bit addressing range for version 1, try version 2.\n");
			}

			if(data_str_len > CSP_CMP_POKE_MAX_LEN){
				return luaL_error(L, "Max poke length %u\n", CSP_CMP_PEEK_MAX_LEN);
			}

			message.poke.addr = htobe32((address & 0x00000000FFFFFFFFULL));
			memcpy(message.poke.data, idata, data_str_len);
			message.poke.len = data_str_len;
			//printf("Poke at address 0x%"PRIx32"\n", (uint32_t) (address & 0x00000000FFFFFFFFULL));
			//csp_hex_dump(NULL, message.poke.data, data_str_len);

            res = csp_cmp_poke(node, timeout, &message);
			if (res != CSP_ERR_NONE) {
				return luaL_error(L, "No response (node=%d timeout=%d, ver=%d)\n", node, timeout, version);
			}
		}
		break;
		case 2:
		{
			if(data_str_len > CSP_CMP_POKE_V2_MAX_LEN){
				return luaL_error(L, "Max poke length %u\n", CSP_CMP_POKE_V2_MAX_LEN);
			}
			message.poke_v2.vaddr = htobe64(address);
			message.poke_v2.len = data_str_len;
            memcpy(message.poke_v2.data, idata, data_str_len);
			//printf("Poke at address 0x%"PRIx64"\n", address);
			//csp_hex_dump(NULL, message.poke_v2.data, data_str_len);

            res = csp_cmp_poke_v2(node, timeout, &message);
			if (res != CSP_ERR_NONE) {
				return luaL_error(L, "No response (node=%d timeout=%d, ver=%d)\n", node, timeout, version);
			}
		}
		break;
	}

    lua_pushinteger(L, res);

    return 1;
}

static int lua_vmem_peek(lua_State *L, uint16_t node, uint64_t address, char *dataout, int version, uint8_t length, int timeout) {

    if (version < 1 || version > 2) {
		return luaL_error(L, "Unsupported version: %d, only supports 1 (32-bit) and 2 (64-bit)\n", version);
	}

	struct csp_cmp_message message;

	switch (version) {
		case 1:
		{
			if (address > 0x00000000FFFFFFFFULL) {
				return luaL_error(L, "Peek address out of 32-bit addressing range for version 1, try version 2.");
			}

			if(length > CSP_CMP_PEEK_MAX_LEN){
				return luaL_error(L, "Max peek length %u", CSP_CMP_PEEK_MAX_LEN);
			}

			message.peek.addr = htobe32(address);
			message.peek.len = length;

			if (csp_cmp_peek(node, timeout, &message) != CSP_ERR_NONE) {
				return luaL_error(L, "No response (node=%d timeout=%d, ver=%d)\n", node, timeout, version);
			}

			//printf("Peek at address 0x%"PRIx32" len %u\n", (uint32_t) address, length);
			//csp_hex_dump(NULL, message.peek.data, length);
            memcpy(dataout, message.peek.data, length);
		}
		break;
		case 2:
		{
			if(length > CSP_CMP_PEEK_V2_MAX_LEN){
				return luaL_error(L, "Max peek length %u", CSP_CMP_PEEK_V2_MAX_LEN);
			}

			message.peek_v2.vaddr = htobe64(address);
			message.peek_v2.len = length;

			if (csp_cmp_peek_v2(node, timeout, &message) != CSP_ERR_NONE) {
				return luaL_error(L, "No response (node=%d timeout=%d, ver=%d)\n", node, timeout, version);
			}

			//printf("Peek at address 0x%"PRIx64" len %u\n", address, length);
			//csp_hex_dump(NULL, message.peek_v2.data, length);
            memcpy(dataout, message.peek_v2.data, length);
		}
		break;
	}

    return 1;
}

static int lua_vmem_write_m(lua_State *L) {

    const lua_vmem_t *const*const vmem_ud = luaL_checkudata(L, 1, "vmem_m");

    if (!vmem_ud && !*vmem_ud) {
        return luaL_error(L, "Expected VMEM metatable, got something else");
    }

    const lua_vmem_t *vmem = *vmem_ud;

    const uint16_t node = vmem->node;
    const uint64_t address = vmem->addr;

    const int version = vmem->version;
    const int timeout = vmem->timeout;

    // Parse binary data and its length from the 4th argument
    size_t idata_len = 0;
    const char * idata = luaL_checklstring(L, 2, &idata_len);

    /* TODO Kevin: Maybe check for bounds of `vmem` here? Not that libparam cares. */
    const int offset = luaL_optinteger(L, 3, 0);

    int res = vmem->use_peek_poke ? lua_vmem_poke(L, node, address+offset, idata, idata_len, version, timeout) : vmem_upload(node, timeout, address+offset, (char*)idata, idata_len, version);
    if (res < 0) {
        return luaL_error(L, "Upload failed addr=%d node=%d version=%d", address+offset, node, version);
	}

    lua_pushinteger(L, res);

    return 1;
}

static int lua_vmem_read_m(lua_State *L) {

    const lua_vmem_t *const*const vmem_ud = luaL_checkudata(L, 1, "vmem_m");

    if (!vmem_ud && !*vmem_ud) {
        return luaL_error(L, "Expected VMEM metatable, got something else");
    }

    const lua_vmem_t *vmem = *vmem_ud;

    const uint16_t node = vmem->node;
    const uint64_t address = vmem->addr;

    const int version = vmem->version;
    const int timeout = vmem->timeout;

    const uint64_t length = luaL_checkinteger(L, 2);
    const uint64_t offset = luaL_optinteger(L, 3, 0);
    const int use_rdp = luaL_optinteger(L, 4, 1);

    // Allocate writable buffer as Lua userdata
    char *dataout = lua_newuserdata(L, length);
    if (!dataout) {  /* Pop all args before erroring. I suspect that's good practice? */
        return luaL_error(L, "Failed to allocate dataout");
    }

    int res = vmem->use_peek_poke ? lua_vmem_peek(L, node, address+offset, dataout, version, length, timeout) : vmem_download(node, timeout, address+offset, length, dataout, version, use_rdp);
    if (res < 0) {
        return luaL_error(L, "vmem_download() failed addr=%d node=%d version=%d", address+offset, node, version);
	}

    lua_pushlstring(L, dataout, length);

    return 1;
}

static int vmem_gc(lua_State *L) {

    lua_vmem_t **ud = luaL_checkudata(L, 1, "vmem_m");

    if(ud && *ud){
        free(*ud);
    }
    return 0;
}

static void setup_vmem_metatables(lua_State * L) {

    luaL_newmetatable(L, "vmem_m");
    lua_pushcfunction(L, vmem_gc);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, lua_vmem_write_m);
    lua_setfield(L, -2, "write");

    lua_pushcfunction(L, lua_vmem_read_m);
    lua_setfield(L, -2, "read");

    // Ensure the metatable is its own __index so methods can be called via userdata:method()
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);  // Pop metatable "vmem_m"
}


static void setup_param_metatables(lua_State * L) {

    luaL_newmetatable(L, "param_array_m");
    lua_pushstring(L, "__index");
    lua_pushcfunction(L, __index_param_array_m);
    lua_settable(L, -3);

    lua_pushstring(L, "__len");
    lua_pushcfunction(L, __len_param_m);
    lua_settable(L, -3);

    lua_pop(L, 1);

    luaL_newmetatable(L, "param_single_m");
    lua_pushstring(L, "__index");
    lua_pushcfunction(L, __index_param_single_m);
    lua_settable(L, -3);

    lua_pushstring(L, "__newindex");
    lua_pushcfunction(L, __newindex_param_m);
    lua_settable(L, -3);

    lua_pushstring(L, "__len");
    lua_pushcfunction(L, __len_param_m);
    lua_settable(L, -3);
    lua_pop(L, 1);

    luaL_newmetatable(L, "queue_m");
    lua_pushcfunction(L, queue_gc);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, lua_queue_add);
    lua_setfield(L, -2, "add");

    lua_pushcfunction(L, lua_queue_remove);
    lua_setfield(L, -2, "remove");

    lua_pushcfunction(L, lua_queue_push);
    lua_setfield(L, -2, "push");

    lua_pushcfunction(L, lua_queue_pull);
    lua_setfield(L, -2, "pull");

#ifdef LPARAM_DEBUG
    lua_pushcfunction(L, lua_queue_print);
    lua_setfield(L, -2, "print");
#endif
    
    // Ensure the metatable is its own __index so methods can be called via userdata:method()
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    luaL_newmetatable(L, "cache_m");

    lua_pushstring(L, "__index");
    lua_pushcfunction(L, __index_cache_m);
    lua_settable(L, -3);

    lua_pushstring(L, "__newindex");
    lua_pushcfunction(L, __newindex_cache_m);
    lua_settable(L, -3);
    lua_pop(L, 1);

}

static int __gc_global_param_list(lua_State *L) {

    global_param_list_head_t *list_head = luaL_checkudata(L, -1, "global_param_list_m");

    if (list_head) {
        while (!SLIST_EMPTY(list_head)) {
            struct lua_param_s *lparam = SLIST_FIRST(list_head);
            SLIST_REMOVE_HEAD(list_head, next);
            param_list_destroy(lparam->param);
            free(lparam->is_set);
            free(lparam);
        }
        // free(list_head);
    }

    return 0;
}

LUAMOD_API int luaopen_param_list(lua_State *L) {

    global_param_list_head_t *list_head = lua_newuserdata(L, sizeof(*list_head));
    if (!list_head) {
        return luaL_error(L, "Failed to allocate g_param_list_head");
    }
    SLIST_INIT(list_head);

    if(luaL_newmetatable(L, "global_param_list_m")){
        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, __gc_global_param_list);
        lua_settable(L, -3);
    }

    lua_setmetatable(L, -2);
    lua_setfield(L, LUA_REGISTRYINDEX, "__paramlist");

    return 0;
}

LUAMOD_API int luaopen_param(lua_State * L) {

    luaopen_param_list(L);

    setup_param_metatables(L);

    lua_pushinteger(L, PARAM_TYPE_UINT8);
    lua_setglobal(L, "UINT8");

    lua_pushinteger(L, PARAM_TYPE_UINT16);
    lua_setglobal(L, "UINT16");

    lua_pushinteger(L, PARAM_TYPE_UINT32);
    lua_setglobal(L, "UINT32");

    lua_pushinteger(L, PARAM_TYPE_UINT64);
    lua_setglobal(L, "UINT64");

    lua_pushinteger(L, PARAM_TYPE_INT8);
    lua_setglobal(L, "INT8");

    lua_pushinteger(L, PARAM_TYPE_INT16);
    lua_setglobal(L, "INT16");

    lua_pushinteger(L, PARAM_TYPE_INT32);
    lua_setglobal(L, "INT32");

    lua_pushinteger(L, PARAM_TYPE_INT64);
    lua_setglobal(L, "INT64");

    lua_pushinteger(L, PARAM_TYPE_XINT8);
    lua_setglobal(L, "XINT8");

    lua_pushinteger(L, PARAM_TYPE_XINT16);
    lua_setglobal(L, "XINT16");

    lua_pushinteger(L, PARAM_TYPE_XINT32);
    lua_setglobal(L, "XINT32");

    lua_pushinteger(L, PARAM_TYPE_XINT64);
    lua_setglobal(L, "XINT64");

    lua_pushinteger(L, PARAM_TYPE_FLOAT);
    lua_setglobal(L, "FLOAT");

    lua_pushinteger(L, PARAM_TYPE_DOUBLE);
    lua_setglobal(L, "DOUBLE");

    lua_pushinteger(L, PARAM_TYPE_STRING);
    lua_setglobal(L, "STRING");

    lua_pushinteger(L, PARAM_TYPE_DATA);
    lua_setglobal(L, "DATA");

    lua_pushcfunction(L, luaparam_add);
    lua_setglobal(L, "parameter");

    lua_pushcfunction(L, luaparam_queue);
    lua_setglobal(L, "parameter_list");


    setup_vmem_metatables(L);

    lua_pushcfunction(L, luavmem_add);
    lua_setglobal(L, "vmem");

#if 0  /* And this is where you will register your global function bindings */
    lua_pushcfunction(L, lua_vmem_upload);
    lua_setglobal(L, "vmem_upload");

    lua_pushcfunction(L, lua_vmem_download);
    lua_setglobal(L, "vmem_download");
#endif

    return 1;
}
