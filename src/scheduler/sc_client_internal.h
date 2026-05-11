#pragma once

#include "scheduler/sc.h"
#include "param/param_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

csp_packet_t * sc_create_cmd_upload_req(param_queue_t * queue);
csp_packet_t * sc_create_cmd_execute_req(param_queue_t * queue);
csp_packet_t * sc_create_cmd_download_req(param_hash_t hash);
csp_packet_t * sc_create_cmd_list_req();
csp_packet_t * sc_create_cmd_remove_req(param_hash_t hash);

csp_packet_t * sc_create_sch_push_req(param_queue_t * queue, uint32_t time,
                                      uint32_t latency_buffer_s);
csp_packet_t * sc_create_sch_remove_req(param_hash_t hash);
csp_packet_t * sc_create_sch_list_req();
csp_packet_t * sc_create_sch_show_req(param_hash_t hash);
csp_packet_t * sc_create_sch_cmd_req(param_hash_t cmd_hash, uint32_t time,
                                     uint32_t latency_buffer_s);
csp_packet_t *sc_create_sch_list_req();                                     
void sc_cmd_list_client_cb(csp_packet_t * response, int verbose, int version);
void sc_cmd_upload_client_cb(csp_packet_t * response, int verbose, int version);
void sc_cmd_download_cb(csp_packet_t * response, int verbose, int version);
void sc_cmd_rm_cb(csp_packet_t * response, int verbose, int version);
void sc_cmd_execute_client_cb(csp_packet_t * response, int verbose, int version);
void sc_sch_show_client_cb(csp_packet_t * response, int verbose, int version);
void sc_sch_cmd_client_cb(csp_packet_t * response, int verbose, int version);
void sc_sch_list_client_cb(csp_packet_t * response, int verbose, int version);
void sc_sch_rm_client_cb(csp_packet_t * response, int verbose, int version);
void sc_sch_push_client_cb(csp_packet_t * response, int verbose, int version);

#ifdef __cplusplus
}
#endif
