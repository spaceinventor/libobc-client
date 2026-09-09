#pragma once

#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include <param/param.h>
#include <param/param_serializer.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 
 * 
 */
typedef enum {
    HK_RETRIEVE_SUCCESS = 0, /**< */
    HK_RETRIEVE_EINVAL = -1, /**< */
    HK_RETRIEVE_EIO = -2, /**< */
    HK_RETRIEVE_EUSAGE = -3, /**< */
    HK_RETRIEVE_MAX = INT_MAX, /**< */
} hk_retrieve_status_t;

/**
 * @brief 
 * 
 */
typedef void (*hk_retrieve_param_callback_f)(param_t *hk_param, void * context);

/**
 * @brief 
 * 
 * @param node 
 * @param filename 
 * @param timestamp 
 * @param step 
 * @param rate 
 * @param num_timestamps 
 * @param prio 
 * @param rdp 
 * @param use_offset 
 * @param quiet 
 * @param param_callback 
 * @param callback_context 
 * @return hk_retrieve_status_t 
 */
hk_retrieve_status_t hk_retrieve(unsigned int node, const char * filename, int32_t timestamp, uint32_t step, double rate, uint32_t num_timestamps, const char * prio, int rdp, int use_offset, int quiet, hk_retrieve_param_callback_f param_callback, void * callback_context);

/**
 * @brief 
 * 
 * @param node 
 * @param paramid 
 */
void hk_set_utcparam(unsigned int node, unsigned int paramid);

/**
 * @brief 
 * 
 * @param reader 
 * @param node 
 * @param src 
 * @param param 
 * @param timestamp 
 * @param local_epoch 
 * @return true 
 * @return false 
 */
bool hk_sync_epoch(mpack_reader_t *reader, int node, int src, const param_t *param, csp_timestamp_t *timestamp, time_t *local_epoch);

/**
 * @brief 
 * 
 * @param epoch 
 * @param node 
 * @return true 
 * @return false 
 */
bool hk_get_epoch(time_t* epoch, uint16_t node);

/**
 * @brief 
 * 
 * @param epoch 
 * @param node 
 * @param auto_sync 
 */
void hk_set_epoch(time_t epoch, uint16_t node, bool auto_sync);

#ifdef __cplusplus
}
#endif