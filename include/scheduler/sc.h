#pragma once

#include <stdint.h>
#include <csp/csp_crc32.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_ELEMENT_SIZE 0x200
#define SCH_ELEMENT_SIZE 0x100

/* Each message use the default param header, in which the first byte indicate the msg type, 
   and the second byte indicate if the message ends the transmission 
   All of the following structs can be added multiple times in one message, the message length
   indicates the number of elements */

typedef csp_crc32_t param_hash_t;

typedef enum {
    SC_TYPE_CMD = 1,
    SC_TYPE_SCH = 2,
} sc_type_e;

typedef uint8_t sc_type_t;

typedef enum {
    SCH_STATUS_OK = 0,
    SCH_STATUS_SCHEDULED = 0,
    SCH_STATUS_COMPLETED = 1,
    SCH_STATUS_FAILED = 2,
    SCH_STATUS_OVERDUE = 3,
    SCH_STATUS_CORRUPTED = 4,
    SCH_STATUS_MISSING_CMD = 5,
    SCH_STATUS_CORRUPTED_CMD = 6,
    SCH_STATUS_EXISTS = 7,
    SCH_STATUS_FULL = 8,
} sch_status_e;
typedef uint8_t sch_status_t;

typedef struct sc_queue_s {
	uint16_t used;
	uint16_t node;
	uint8_t version;
	char name[20];
} __attribute__((__packed__)) sc_queue_t;

/* Request a particular command or schedule to be downloaded, executed or removed from storage 
   PARAM_COMMAND_EXECUTE_REQUEST_V2
   PARAM_COMMAND_DOWNLOAD_REQUEST_V2
   PARAM_COMMAND_REMOVE_REQUEST_V2
   PARAM_SCHEDULE_REMOVE_REQUEST_V2
   PARAM_SCHEDULE_SHOW_REQUEST_V2
   */
typedef struct {
    param_hash_t hash;
} __attribute__((__packed__)) param_sc_req_t;

/* Response to command, schedule, removal or execute requests 
   PARAM_COMMAND_UPLOAD_RESPONSE_V2
   PARAM_COMMAND_EXECUTE_RESPONSE_V2
   PARAM_COMMAND_LIST_RESPONSE_V2
   PARAM_SCHEDULE_PUSH_RESPONSE_V2
   PARAM_SCHEDULE_COMMAND_RESPONSE_V2
   */
typedef struct {
    param_hash_t hash;
    int8_t result;
} __attribute__((__packed__)) param_sc_rsp_t;

/* Upload a command queue
   Potentially for storage and/or immediate execution 
   PARAM_COMMAND_UPLOAD_REQUEST_V2
   */
typedef struct {
    sc_queue_t param_queue;
    uint8_t execute;
    uint8_t store;
    char param_buffer[];
} __attribute__((__packed__)) param_cmd_upload_t;

/* Response to a download request of a single or all commands 
   PARAM_COMMAND_DOWNLOAD_REQUEST_V2
   */
typedef struct {
    param_hash_t hash;
    int8_t result;
    sc_queue_t param_queue;
    char param_buffer[];
} __attribute__((__packed__)) param_cmd_download_t;

/* Upload a command schedule, with the command queue included
   PARAM_SCHEDULE_PUSH_REQUEST_V2
   */
typedef struct {
    uint32_t timestamp_s;
    uint32_t latency_buffer_s;
    sc_queue_t param_queue;
    char param_buffer[];
} __attribute__((__packed__)) param_sch_push_t;

/* Schedule a command already existing in the target module 
   PARAM_SCHEDULE_COMMAND_REQUEST_V2
   */
typedef struct {
    uint32_t timestamp_s;
    uint32_t latency_buffer_s;
    param_hash_t command_hash;
} __attribute__((__packed__)) param_sch_command_t;

typedef struct {
    param_hash_t sch_hash;
    uint32_t timestamp;
    uint32_t latency_buffer_s;
    param_hash_t cmd_hash;
    uint16_t retries;
    uint8_t status;
} __attribute__((__packed__)) param_sch_list_t;

typedef struct {
    param_hash_t sch_hash;
    uint32_t timestamp;
    uint32_t latency_buffer_s;
    param_hash_t cmd_hash;
    uint16_t retries;
    uint8_t status;
    sc_queue_t param_queue;
    char param_buffer[];
} __attribute__((__packed__)) param_sch_show_t;

/* Delete completed schedule elements older than a given time
   and commands not referenced by any schedule element
   SCHEDULE_CLEANUP_REQUEST_V2
    */
typedef struct {
    int32_t preserve_from;
    uint8_t remove_failed;
    uint8_t remove_unused_commands;
} __attribute__((__packed__)) param_sch_cleanup_t;

/* Response to a schedule cleanup request
   SCHEDULE_CLEANUP_RESPONSE_V2
   */
typedef struct {
    uint32_t num_cmd;
    uint32_t num_sch;
} __attribute__((__packed__)) param_sch_status_t;

typedef enum {
    COMMAND_UPLOAD_REQUEST_V2 = 40,
    COMMAND_UPLOAD_RESPONSE_V2 = 41,
    COMMAND_EXECUTE_REQUEST_V2 = 42,
    COMMAND_EXECUTE_RESPONSE_V2 = 43,
    COMMAND_LIST_REQUEST_V2 = 44,
    COMMAND_LIST_RESPONSE_V2 = 45,
    COMMAND_DOWNLOAD_REQUEST_V2 = 46,
    COMMAND_DOWNLOAD_RESPONSE_V2 = 47,
    COMMAND_REMOVE_REQUEST_V2 = 48,
    COMMAND_REMOVE_RESPONSE_V2 = 49,

    SCHEDULE_PUSH_REQUEST_V2 = 50,
    SCHEDULE_PUSH_RESPONSE_V2 = 51,
    SCHEDULE_COMMAND_REQUEST_V2 = 52,
    SCHEDULE_COMMAND_RESPONSE_V2 = 53,
    SCHEDULE_REMOVE_REQUEST_V2 = 58,
    SCHEDULE_REMOVE_RESPONSE_V2 = 59,
    SCHEDULE_LIST_REQUEST_V2 = 60,
    SCHEDULE_LIST_RESPONSE_V2 = 61,
    SCHEDULE_SHOW_REQUEST_V2 = 62,
    SCHEDULE_SHOW_RESPONSE_V2 = 63,
    SCHEDULE_CLEANUP_REQUEST_V2 = 64,
    SCHEDULE_CLEANUP_RESPONSE_V2 = 65,
} sc_packet_type_e;

void sc_server_init();

#ifdef __cplusplus
}
#endif
