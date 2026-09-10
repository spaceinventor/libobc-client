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
 * @brief HK request parameters
 *
 */
typedef struct {
    uint32_t timestamp; /**< Timestamp for newest data to receive */
    uint16_t period; /**< Time between each log */
    uint8_t prios : 3; /**< One bit per priority, i.e. 0b101 for prio 1 and 3 */
    uint8_t throughput : 5; /**< Percentage increase in download throughput of timestamps, leave at 0 for full throughput */
    uint8_t num_timestamps; /**< Number of timestamps to retrieve */
} __attribute__((__packed__)) hk_retrievehdr_t;


/**
 * @brief possible return values for hk_retrieve()
 * Note: previous versions simply returned an "int" so we make sure that the enum has the same type by forcing
 * the last value to INT_MAX
 */
typedef enum {
    HK_RETRIEVE_SUCCESS = 0, /**< */
    HK_RETRIEVE_EINVAL = -1, /**< */
    HK_RETRIEVE_EIO = -2, /**< */
    HK_RETRIEVE_EUSAGE = -3, /**< */
    HK_RETRIEVE_MAX = INT_MAX, /**< */
} hk_retrieve_status_t;

/**
 * @brief type definition for hk_retrieve callback parameter
 * 
 */
typedef void (*hk_retrieve_param_callback_f)(param_t *hk_param, void * context);

/**
 * @brief request House Keeping data from an OBC-P4-HK
 * 
 * @param node CSP address of the OBC-P4-HK to query
 * @param filename house keeping data will be written to this filename or stdout if this is NULL
 * @param timestamp 
 * @param step 
 * @param rate 
 * @param num_timestamps 
 * @param prio 
 * @param rdp 
 * @param use_offset 
 * @param quiet 
 * @param param_callback ptr to a function that will be called for each house keeping parameter received
 * @param callback_context user data passed to the callback above
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