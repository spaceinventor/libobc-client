#include "sc_client_internal.h"
#include "scheduler/sc_client.h"
#include "scheduler/sc_utils.h"
#include <param/param_server.h>
#include <csp/csp.h>
#include <time.h>
#include <string.h>
#define _BSD_SOURCE
#include <endian.h>

typedef void (*transaction_callback_f)(csp_packet_t *response, int verbose, int version);

int sc_transaction(csp_packet_t *packet, int host, int timeout, transaction_callback_f callback, int verbose, int version, void * context) {

    /* Parameters can be setup with a special nodeid, which caused all transaction to be ignored
       and return failure immediately */
    if (host == PARAM_REMOTE_NODE_IGNORE) {
        csp_buffer_free(packet);
        return -1;
    }

    csp_conn_t * conn = csp_connect(packet->id.pri, host, 11, 0, CSP_O_CRC32);
    if (conn == NULL) {
        printf("param transaction failure\n");
        csp_buffer_free(packet);
        return -1;
    }

    csp_send(conn, packet);

    if (timeout == -1) {
        printf("param transaction failure\n");
        csp_close(conn);
        return -1;
    }

    int result = -1;
    while((packet = csp_read(conn, timeout)) != NULL) {

        int end = (packet->data[1] == PARAM_FLAG_END);

        if (callback) {
            callback(packet, verbose, version);
            csp_buffer_free(packet);
        }

        if (end) {
            result = 0;
            break;
        }

    }

    csp_close(conn);
    return result;
}

on_cmd_upload_cb_t on_cmd_upload_cb = 0;
void sc_cmd_upload_client_cb(csp_packet_t *response, int verbose, int version) {

    uint32_t unpacked_len = 2;
    while (unpacked_len < response->length) {
        param_sc_rsp_t* rsp_element = (param_sc_rsp_t*)&response->data[unpacked_len];
        rsp_element->hash = htobe32(rsp_element->hash);
        unpacked_len += sizeof(*rsp_element);
        if(on_cmd_upload_cb) {
            on_cmd_upload_cb(rsp_element);
        }
    }
}

on_cmd_execute_cb_t on_cmd_execute_cb = 0;
void sc_cmd_execute_client_cb(csp_packet_t *response, int verbose, int version) {

    uint32_t unpacked_len = 2;
    while (unpacked_len < response->length) {
        param_sc_rsp_t* rsp_element = (param_sc_rsp_t*)&response->data[unpacked_len];
        rsp_element->hash = htobe32(rsp_element->hash);
        unpacked_len += sizeof(param_sc_rsp_t);
        if(on_cmd_execute_cb) {
            on_cmd_execute_cb(rsp_element);
        }
    }
}


csp_packet_t *sc_create_cmd_req(param_queue_t* queue, bool execute) {
    csp_packet_t *packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet != NULL) {
        packet->data[0] = execute?COMMAND_EXECUTE_REQUEST_V2:COMMAND_UPLOAD_REQUEST_V2;
        packet->data[1] = PARAM_FLAG_END;

        param_cmd_upload_t* cmd = (param_cmd_upload_t*)&packet->data[2];
        memset(cmd->param_queue.name, '\0', sizeof(cmd->param_queue.name));
        strncpy(cmd->param_queue.name, queue->name, sizeof(cmd->param_queue.name));
        cmd->param_queue.node = htobe16(queue->last_node);
        cmd->param_queue.used = htobe16(queue->used);
        cmd->param_queue.version = 2;

        memcpy(&cmd->param_buffer, queue->buffer, queue->used);

        cmd->execute = execute;
        cmd->store = !execute;

        packet->length = 2 + sizeof(*cmd) + queue->used;
    }
    return packet;
}

csp_packet_t *sc_create_cmd_execute_req(param_queue_t* queue) {
    csp_packet_t *packet = sc_create_cmd_req(queue, true);
    return packet;
}

int sc_cmd_execute_client(param_queue_t* queue, uint16_t server, unsigned int timeout) {

    if(queue->version != 2) {
        return -3;
    }
    csp_packet_t * packet = sc_create_cmd_upload_req(queue);
    if (packet == NULL)
        return -2;
    int result = sc_transaction(packet, server, timeout, sc_cmd_upload_client_cb, 0, queue->version, NULL);

    return result;
}

csp_packet_t *sc_create_cmd_upload_req(param_queue_t* queue) {
    csp_packet_t *packet = sc_create_cmd_req(queue, false);
    return packet;
}

int sc_cmd_upload_client(param_queue_t* queue, uint16_t server, unsigned int timeout) {

    if(queue->version != 2) {
        return -3;
    }
    csp_packet_t * packet = sc_create_cmd_upload_req(queue);
    if (packet == NULL)
        return -2;
    int result = sc_transaction(packet, server, timeout, sc_cmd_upload_client_cb, 0, queue->version, NULL);

    return result;
}

on_cmd_list_element_cb_t on_cmd_list_element_cb = 0;

void sc_cmd_list_client_cb(csp_packet_t *response, int verbose, int version) {

    uint32_t unpacked_len = 2;
    if (unpacked_len < response->length) {
        while (unpacked_len < response->length) {
            param_sc_rsp_t *rsp_element = (param_sc_rsp_t*)&response->data[unpacked_len];
            rsp_element->hash = be32toh(rsp_element->hash);
            unpacked_len += sizeof(param_sc_rsp_t);
            if(on_cmd_list_element_cb) {
                on_cmd_list_element_cb(rsp_element, (unpacked_len < response->length) | !(response->data[1] & PARAM_FLAG_END));
            }
        }
    } else {
        if(on_cmd_list_element_cb) {
            static param_sc_rsp_t empty_list = {
                .hash = 0,
                .result = SCH_STATUS_FULL
            };
            on_cmd_list_element_cb(&empty_list, false);
        }
    }
}

csp_packet_t *sc_create_cmd_list_req() {
    csp_packet_t *packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet != NULL) {
        packet->data[0] = COMMAND_LIST_REQUEST_V2;
        packet->data[1] = PARAM_FLAG_END;
        packet->length = 2;
        packet->data32[1] = htobe32(0); /* Current cmd offset, 0 for beginning */
    }
    return packet;
}

int sc_cmd_list_client(uint16_t server, unsigned int timeout) {
    csp_packet_t *packet = sc_create_cmd_list_req();
    if (packet == NULL)
        return -2;
    int result = sc_transaction(packet, server, timeout, sc_cmd_list_client_cb, 0, 2, NULL);
    return result;
}

on_cmd_download_cb_t on_cmd_download_cb = 0;

void sc_cmd_download_cb(csp_packet_t *response, int verbose, int version) {

    uint32_t unpacked_len = 2;
    while (unpacked_len < response->length) {
        param_cmd_download_t* rsp_element = (param_cmd_download_t*)&response->data[unpacked_len];
        rsp_element->hash = be32toh(rsp_element->hash);
        rsp_element->param_queue.used = be16toh(rsp_element->param_queue.used);
        unpacked_len += sizeof(param_cmd_download_t) + rsp_element->param_queue.used;
        if(on_cmd_download_cb) {
            on_cmd_download_cb(rsp_element);
        }
    }
}

csp_packet_t *sc_create_cmd_download_req(param_hash_t hash) {
    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet) {
        packet->data[0] = COMMAND_DOWNLOAD_REQUEST_V2;
        packet->data[1] = PARAM_FLAG_END;
        packet->length = 2;

        param_sc_req_t* cmd = (param_sc_req_t*)&packet->data[packet->length];
        packet->length += sizeof(*cmd);

        cmd->hash = htobe32(hash);
    }
    return packet;
}

int sc_cmd_download_client(param_hash_t hash, uint16_t server, unsigned int timeout) {

    csp_packet_t * packet = sc_create_cmd_download_req(hash);
    if (packet == NULL)
        return -2;

    int result = sc_transaction(packet, server, timeout, sc_cmd_download_cb, 0, 2, NULL);

    return result;
}

on_cmd_rm_cb_t on_cmd_rm_cb = 0;

void sc_cmd_rm_cb(csp_packet_t *response, int verbose, int version) {
    uint32_t unpacked_len = 2;
    while (unpacked_len < response->length) {
        param_sc_rsp_t* rsp_element = (param_sc_rsp_t*)&response->data[unpacked_len];
        rsp_element->hash = be32toh(rsp_element->hash);
        unpacked_len += sizeof(*rsp_element);
        if(on_cmd_rm_cb) {
            on_cmd_rm_cb(rsp_element);
        }
    }
}

csp_packet_t *sc_create_cmd_remove_req(param_hash_t hash) {
    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet) {
        packet->data[0] = COMMAND_REMOVE_REQUEST_V2;
        packet->data[1] = PARAM_FLAG_END;
        packet->length = 2;

        param_sc_req_t* cmd = (param_sc_req_t*)&packet->data[2];
        packet->length += sizeof(*cmd);

        cmd->hash = htobe32(hash);
    }
    return packet;
}

int sc_cmd_remove_client(param_hash_t hash, uint16_t server, unsigned int timeout) {
    csp_packet_t * packet = sc_create_cmd_remove_req(hash);
    if (packet == NULL)
        return -2;

    int result = sc_transaction(packet, server, timeout, sc_cmd_rm_cb, 0, 2, NULL);

    return result;
}


on_sch_push_cb_t on_sch_push_cb = 0;

void sc_sch_push_client_cb(csp_packet_t *response, int verbose, int version) {
    uint32_t unpacked_len = 2;

    while (unpacked_len < response->length) {

        param_sc_rsp_t * rsp_element = (param_sc_rsp_t *)&response->data[unpacked_len];
        unpacked_len += sizeof(*rsp_element);
        rsp_element->hash = be32toh(rsp_element->hash);
        if(on_sch_push_cb) {
            on_sch_push_cb(rsp_element);
        }
    }
}
csp_packet_t *sc_create_sch_push_req(param_queue_t* queue, uint32_t time, uint32_t latency_buffer_s) {
    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet) {
        packet->data[0] = SCHEDULE_PUSH_REQUEST_V2;
        packet->data[1] = PARAM_FLAG_END;

        param_sch_push_t* cmd = (param_sch_push_t*)&packet->data[2];

        cmd->timestamp_s = htobe32(time);
        cmd->latency_buffer_s = htobe32(latency_buffer_s);
        memset(cmd->param_queue.name, '\0', sizeof(cmd->param_queue.name));
        strncpy(cmd->param_queue.name, queue->name, sizeof(cmd->param_queue.name));
        cmd->param_queue.node = htobe16(queue->last_node);
        cmd->param_queue.used = htobe16(queue->used);
        cmd->param_queue.version = 2;
        memcpy(&cmd->param_buffer, queue->buffer, queue->used);

        packet->length = 2 + sizeof(*cmd) + queue->used;
    }
    return packet;
}

int sc_sch_push_client(param_queue_t* queue, uint32_t time, uint32_t latency_buffer_s, uint16_t server, unsigned int timeout) {
    if (queue->version != 2) {
        return -3;
    }

    csp_packet_t * packet = sc_create_sch_push_req(queue, time, latency_buffer_s);
    if (packet == NULL)
        return -2;

    int result = sc_transaction(packet, server, timeout, sc_sch_push_client_cb, 0, queue->version, NULL);

    return result;
}

on_sch_list_cb_t on_sch_list_cb = 0;
void sc_sch_list_client_cb(csp_packet_t *response, int verbose, int version) {

    uint32_t unpacked_len = 2;
    while (unpacked_len < response->length) {

        param_sch_list_t* rsp_element = (param_sch_list_t*)&response->data[unpacked_len];
        rsp_element->cmd_hash = be32toh(rsp_element->cmd_hash);
        rsp_element->latency_buffer_s = be32toh(rsp_element->latency_buffer_s);
        rsp_element->retries = be16toh(rsp_element->retries);
        rsp_element->sch_hash = be32toh(rsp_element->sch_hash);
        rsp_element->timestamp = be32toh(rsp_element->timestamp);
        unpacked_len += sizeof(*rsp_element);
        if(on_sch_list_cb) {
            on_sch_list_cb(rsp_element);
        }
        // char s[100];
        // time_t timestamp = rsp_element->timestamp;
        // struct tm * p = localtime(&timestamp);
        // strftime(s, sizeof(s), "%B %d %Y at %X", p);

        // printf("SCH hash 0x%08"PRIX32", CMD hash 0x%08"PRIX32":\n", rsp_element->sch_hash, rsp_element->cmd_hash);
        // printf("  Status is %s with %"PRIu16" retries\n", sch_str_status(rsp_element->status), rsp_element->retries);
        // printf("  Execution time %s local time (%"PRIu32" s UNIX time) with max latency %"PRIu32" s\n", s, rsp_element->timestamp, rsp_element->latency_buffer_s);
    }
}

csp_packet_t *sc_create_sch_list_req() {
    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet) {
        packet->data[0] = SCHEDULE_LIST_REQUEST_V2;
        packet->data[1] = PARAM_FLAG_END;
        packet->data32[1] = 0;
        packet->length = 2;
    }
    return packet;
}

int sc_sch_list_client(uint16_t server, unsigned int timeout) {

    csp_packet_t * packet = sc_create_sch_list_req();
    if (packet == NULL)
        return -2;

    int result = sc_transaction(packet, server, timeout, sc_sch_list_client_cb, 0, 2, NULL);

    return result;
}

on_sch_show_cb_t on_sch_show_cb = 0;

void sc_sch_show_client_cb(csp_packet_t *response, int verbose, int version) {

    uint32_t unpacked_len = 2;

    while (unpacked_len < response->length) {

        param_sch_show_t* rsp_element = (param_sch_show_t*)&response->data[unpacked_len];
        unpacked_len += sizeof(*rsp_element);
        rsp_element->cmd_hash = be32toh(rsp_element->cmd_hash);
        rsp_element->latency_buffer_s = be32toh(rsp_element->latency_buffer_s);
        rsp_element->param_queue.node = be16toh(rsp_element->param_queue.node);
        rsp_element->param_queue.used = be16toh(rsp_element->param_queue.used);
        rsp_element->retries = be16toh(rsp_element->retries);
        rsp_element->sch_hash = be32toh(rsp_element->sch_hash);
        rsp_element->timestamp = be32toh(rsp_element->timestamp);
        if(on_sch_show_cb) {
            on_sch_show_cb(rsp_element);
        }
        unpacked_len += sizeof(param_sch_show_t) + rsp_element->param_queue.used;
    }
}

csp_packet_t *sc_create_sch_show_req(param_hash_t hash) {
    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet) {
        packet->data[0] = SCHEDULE_SHOW_REQUEST_V2;
        packet->data[1] = PARAM_FLAG_END;
        packet->length = 2;

        param_sc_req_t* cmd = (param_sc_req_t*)&packet->data[2];
        packet->length += sizeof(*cmd);

        cmd->hash = htobe32(hash);
    }
    return packet;
}

int sc_sch_show_client(param_hash_t hash, uint16_t server, unsigned int timeout) {

    csp_packet_t * packet = sc_create_sch_show_req(hash);
    if (packet == NULL)
        return -2;

    int result = sc_transaction(packet, server, timeout, sc_sch_show_client_cb, 0, 2, NULL);

    return result;
}


on_sch_cmd_cb_t on_sch_cmd_cb = 0;

void sc_sch_cmd_client_cb(csp_packet_t *response, int verbose, int version) {
    uint32_t unpacked_len = 2;

    while (unpacked_len < response->length) {

        param_sc_rsp_t * rsp_element = (param_sc_rsp_t *)&response->data[unpacked_len];
        rsp_element->hash = htobe32(rsp_element->hash);
        unpacked_len += sizeof(*rsp_element);
        if(on_sch_cmd_cb) {
            on_sch_cmd_cb(rsp_element);
        }
    }
}

csp_packet_t *sc_create_sch_cmd_req(param_hash_t cmd_hash, uint32_t time, uint32_t latency_buffer_s) {
    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet) {
        packet->data[0] = SCHEDULE_COMMAND_REQUEST_V2;
        packet->data[1] = PARAM_FLAG_END;

        param_sch_command_t* cmd = (param_sch_command_t*)&packet->data[2];

        cmd->timestamp_s = htobe32(time);
        cmd->latency_buffer_s = htobe32(latency_buffer_s);
        cmd->command_hash = htobe32(cmd_hash);

        packet->length = 2 + sizeof(*cmd);
    }
    return packet;
}

int sc_sch_cmd_client(param_hash_t cmd_hash, uint32_t time, uint32_t latency_buffer_s, uint16_t server, unsigned int timeout) {

    csp_packet_t * packet = sc_create_sch_cmd_req(cmd_hash, time, latency_buffer_s);
    if (packet == NULL)
        return -2;

    int result = sc_transaction(packet, server, timeout, sc_sch_cmd_client_cb, 0, 2, NULL);

    return result;
}

on_sch_rm_cb_t on_sch_rm_cb = 0;

void sc_sch_rm_client_cb(csp_packet_t *response, int verbose, int version) {
    uint32_t unpacked_len = 2;

    while (unpacked_len < response->length) {

        param_sc_rsp_t * rsp_element = (param_sc_rsp_t *)&response->data[unpacked_len];
        rsp_element->hash = be32toh(rsp_element->hash);
        unpacked_len += sizeof(*rsp_element);
        if(on_sch_rm_cb) {
            on_sch_rm_cb(rsp_element);
        }
    }
}

csp_packet_t *sc_create_sch_remove_req(param_hash_t hash) {
    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet) {
        packet->data[0] = SCHEDULE_REMOVE_REQUEST_V2;
        packet->data[1] = PARAM_FLAG_END;
        packet->length = 2;

        param_sc_req_t* cmd = (param_sc_req_t*)&packet->data[2];
        packet->length += sizeof(*cmd);

        cmd->hash = htobe32(hash);
    }
    return packet;
}

int sc_sch_remove_client(param_hash_t hash, uint16_t server, unsigned int timeout) {

    csp_packet_t * packet = sc_create_sch_remove_req(hash);
    if (packet == NULL)
        return -2;

    int result = sc_transaction(packet, server, timeout, sc_sch_rm_client_cb, 0, 2, NULL);

    return result;
}
