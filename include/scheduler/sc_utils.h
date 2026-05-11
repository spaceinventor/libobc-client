#pragma once

#include <stdbool.h>
#include <scheduler/sc.h>
#include <vmem/vmem.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute the hash value for the given command schedule queue
 * @param queue command schedule queue
 * @param buffer command schedule queue buffer
 * @return computed hash value
 */
param_hash_t sc_calc_cmd_hash(sc_queue_t* queue, char* buffer);
bool sc_verify_cmd_hash(sc_queue_t* queue, char * buffer, uint32_t addr, param_hash_t hash);

/**
 * @brief Get a pointer to the VMEM object associated with the given schedule storage type
 * @param sc_type SC_TYPE_CMD or SC_TYPE_SCH
 * @return NULL if sc_type parameter is invalid, pointer to a VMEM object otherwise
 */
vmem_t* sc_get_vmem(sc_type_t sc_type);

/**
 * @brief Get the size (in terms of number of items) of the storage allocated for the given type
 * @param sc_type SC_TYPE_CMD or SC_TYPE_SCH
 * @return -1 if sc_type parameter is invalid, aa number of queue items oitherwise
 */
int32_t sc_get_size(sc_type_t sc_type);

/**
 * @brief Find the VMEM address corresponding to the given hash in the given VMEM area
 * @param hash_in hash value to use to lookup address
 * @param sc_type VMEM area to use
 * @return valid VMEM area if hash is found in it, -1 otherwise
 */
int32_t sc_find_addr(param_hash_t hash_in, sc_type_t sc_type);

/**
 * @brief return a free VMEM address if there is any left
 * @param sc_type VMEM area to scan for free address
 * @return valid VMEM address if there is space, -1 otherwise
 */
int32_t sc_find_free_slot(sc_type_t sc_type);

/**
 * @brief Remove (mark as free) the VMEM area corresponding to the given hash in the given VMEM area
 * @param hash hash value to remove
 * @param sc_type VMEM area to use
 * @return SCH_STATUS_OK if hash has been remove, another sch_status_t value describing the error otherwise
 */
sch_status_t sc_remove_entry(param_hash_t hash, sc_type_t sc_type);

/**
 * @brief Retrieve the cmd (hash + queue) at the given VMEM address
 * @param cmd_addr address to look at
 * @param[out] hash will contain the hash of the command if found
 * @param[out] queue will contain the queue meta-data if found
 * @param[out] buffer will contain the queue data if found
 * @return SCH_STATUS_OK if cmd was found, another sch_status_t value describing the error otherwise
 */
sch_status_t sc_get_cmd(int32_t cmd_addr, param_hash_t* hash, sc_queue_t* queue, char* buffer);

/**
 * @brief Read the queue data and meta-data for the given hash
 * @param hash hash value of the command to read
 * @param[out] queue will contain the queue meta-data if found
 * @param[out] buffer will contain the queue data if found
 * @return SCH_STATUS_OK if cmd was found, another sch_status_t value describing the error otherwise
 */
sch_status_t sc_read_cmd(param_hash_t hash, sc_queue_t* queue, char* buffer);

/**
 * @brief Retrieve the cmd (hash + queue meta-data) with the given name
 * @param name name of the command to retrieve
 * @param[out] queue will contain the queue meta-data if found
 * @param[out] hash will contain the hash value if found
 * @return SCH_STATUS_OK if cmd was retrieved, another sch_status_t value describing the error otherwise
 */
sch_status_t sc_find_cmd(char* name, sc_queue_t* queue, param_hash_t *hash);

/**
 * @brief Persist the command with the given hash
 * @param hash hash value of the command to persist
 * @param queue command queue meta-data
 * @param buffer command queue data
 * @return SCH_STATUS_OK if cmd was persisted, another sch_status_t value describing the error otherwise
 */
sch_status_t sc_store_cmd(param_hash_t hash, sc_queue_t* queue, char* buffer);

char* sch_str_status(sch_status_t status);

#ifdef __cplusplus
}
#endif
