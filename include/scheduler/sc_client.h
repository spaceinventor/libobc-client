#pragma once

#include <stdint.h>
#include <param/param_queue.h>
#include <scheduler/sc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CMD and Scheduler client Public API */

/* Type definitions */

typedef void (*on_sch_cmd_cb_t)(param_sc_rsp_t * rsp_element);
/**
 * @brief Set this to a function that will be called when a "schedule cmd" request has completed
 * @attention "completed" does NOT NECESSARILY mean successful, check the "result" field of the response
 */
extern on_sch_cmd_cb_t on_sch_cmd_cb;

typedef void (*on_cmd_rm_cb_t)(param_sc_rsp_t * rsp_element);
/**
 * @brief Set this to a function that will be called when a remove cmd request has completed
 */
extern on_cmd_rm_cb_t on_cmd_rm_cb;

typedef void (*on_sch_list_cb_t)(param_sch_list_t * rsp_element);
/**
 * @brief Set this to a function that will be called for each element in the downloaded command list
 */
extern on_sch_list_cb_t on_sch_list_cb;

typedef void (*on_cmd_download_cb_t)(param_cmd_download_t * resp);
/**
 * @brief Set this to a function that will be called when a command queue is downloaded
 * @note check the "result" field of the given "param_cmd_download_t * resp"!
 */
extern on_cmd_download_cb_t on_cmd_download_cb;

typedef void (*on_cmd_list_element_cb_t)(param_sc_rsp_t * e, bool more);
/**
 * @brief Set this to a function that will be called for each element in the downloaded command list
 * @note check the "result" field of the given "param_sc_rsp_t *e"!
 */
extern on_cmd_list_element_cb_t on_cmd_list_element_cb;

typedef void (*on_cmd_execute_cb_t)(param_sc_rsp_t * rsp_element);
/**
 * @brief Set this to a function that will be called when a "execute cmd" request has completed
 * @attention "completed" does NOT NECESSARILY mean successful, check the "result" field of the response
 */
extern on_cmd_execute_cb_t on_cmd_execute_cb;

typedef void (*on_cmd_upload_cb_t)(param_sc_rsp_t * rsp_element);
/**
 * @brief Set this to a function that will be called for each element in the uploaded command list
 */
extern on_cmd_upload_cb_t on_cmd_upload_cb;

typedef void (*on_sch_show_cb_t)(param_sch_show_t * rsp_element);
/**
 * @brief Set this to a function that will be called when a "schedule show" request has completed
 * @attention "completed" does NOT NECESSARILY mean successful, check the "result" field of the response
 */
extern on_sch_show_cb_t on_sch_show_cb;

typedef void (*on_sch_push_cb_t)(param_sc_rsp_t * rsp_element);
/**
 * @brief Set this to a function that will be called when a "shedule push " request has completed
 * @attention "completed" does NOT NECESSARILY mean successful, check the "result" field of the response
 */
extern on_sch_push_cb_t on_sch_push_cb;

typedef void (*on_sch_rm_cb_t)(param_sc_rsp_t * rsp_element);
/**
 * @brief Set this to a function that will be called when a "schedule rm" request has completed
 * @attention "completed" does NOT NECESSARILY mean successful, check the "result" field of the response
 */
extern on_sch_rm_cb_t on_sch_rm_cb;

/* Command related APIs */

/**
 * @brief Upload a cmd queue to the given server
 * @param param_queue 
 * @param server 
 * @param timeout 
 * @return 0 if success, -1 if the server transaction failed, -2 if the CSP request could not be allocated, 
 * -3 if the param_queue version is not 2
 */
int sc_cmd_upload_client(param_queue_t * param_queue, uint16_t server,
                         unsigned int timeout);
/**
 * @brief Send an execute request for the given cmd queue to the given server
 * @param param_queue 
 * @param server 
 * @param timeout 
 * @return 0 if success, -1 if the server transaction failed, -2 if the CSP request could not be allocated, 
 * -3 if the param_queue version is not 2
 */
int sc_cmd_execute_client(param_queue_t * param_queue, uint16_t server,
                          unsigned int timeout);

/**
 * @brief Download the cmd queue associated with the given hash from the given server
 * @param hash 
 * @param server 
 * @param timeout 
 * @return 0 if success, -1 if the server transaction failed, -2 if the CSP request could not be allocated, 
 */
int sc_cmd_download_client(param_hash_t hash, uint16_t server,
                           unsigned int timeout);

/**
 * @brief Obtain a list of the comnd queues stored on the given server
 * @param server 
 * @param timeout 
 * @return 0 if success, -1 if the server transaction failed, -2 if the CSP request could not be allocated, 
 */
int sc_cmd_list_client(uint16_t server, unsigned int timeout);

/**
 * @brief Remove the given cmd queue from the given server
 * @param hash 
 * @param server 
 * @param timeout 
 * @return 0 if success, -1 if the server transaction failed, -2 if the CSP request could not be allocated, 
 */
int sc_cmd_remove_client(param_hash_t hash, uint16_t server,
                         unsigned int timeout);

/* Scheduling related APIs */

/**
 * @brief Push the given cmd queue for execution at the given schedule to the given server
 * @param queue 
 * @param time 
 * @param latency_buffer_s 
 * @param server 
 * @param timeout 
 * @return 0 if success, -1 if the server transaction failed, -2 if the CSP request could not be allocated, 
 * -3 if the param_queue version is not 2
 */
int sc_sch_push_client(param_queue_t * queue, uint32_t time,
                       uint32_t latency_buffer_s, uint16_t server,
                       unsigned int timeout);
/**
 * @brief Remove the schedule matching the given hash from the given server
 * @param hash 
 * @param server 
 * @param timeout 
 * @return 0 if success, -1 if the server transaction failed, -2 if the CSP request could not be allocated, 
 * @note only the *schedule information* will be removed, not the actual cmd queue
 */
int sc_sch_remove_client(param_hash_t hash, uint16_t server,
                         unsigned int timeout);
/**
 * @brief List the schedules stored on the given server
 * @param server 
 * @param timeout 
 * @return 0 if success, -1 if the server transaction failed, -2 if the CSP request could not be allocated, 
 */
int sc_sch_list_client(uint16_t server, unsigned int timeout);
/**
 * @brief 
 * @param hash 
 * @param server 
 * @param timeout 
 * @return 0 if success, -1 if the server transaction failed, -2 if the CSP request could not be allocated, 
 */
int sc_sch_show_client(param_hash_t hash, uint16_t server,
                       unsigned int timeout);

/**
 * @brief Schedule the existing cmd queue associated with the given hash for execution at the given schedule on the given server
 * @param cmd_hash 
 * @param time UNIX Timestamp in seconds since 01-01-1970
 * @param latency_buffer_s in seconds
 * @param server 
 * @param timeout 
 * @return 0 if success, -1 if the server transaction failed, -2 if the CSP request could not be allocated, 
 */
int sc_sch_cmd_client(param_hash_t cmd_hash, uint32_t time,
                      uint32_t latency_buffer_s, uint16_t server,
                      unsigned int timeout);

/**
 * @brief Request the list of scheduled commands on the given server
 * @param server 
 * @param timeout 
 * @return 0 if success, -1 if the server transaction failed, -2 if the CSP request could not be allocated, 
 */
int sc_sch_list_client(uint16_t server, unsigned int timeout);
#ifdef __cplusplus
}
#endif
