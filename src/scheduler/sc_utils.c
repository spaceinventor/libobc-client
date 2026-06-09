
#include "scheduler/sc_utils.h"
#include <stdlib.h> /* strtoul */
#include <string.h>
#include <inttypes.h>

vmem_t vmem_cmd_hash __attribute__((weak));
vmem_t vmem_cmd_hash __attribute__((weak));
vmem_t vmem_cmd_store __attribute__((weak));
vmem_t vmem_sch_hash __attribute__((weak));
vmem_t vmem_sch_store __attribute__((weak));

param_hash_t sc_calc_cmd_hash(sc_queue_t* queue, char* buffer) {

    csp_crc32_t crc_obj;
    csp_crc32_init(&crc_obj);
    csp_crc32_update(&crc_obj, queue, sizeof(*queue));
    csp_crc32_update(&crc_obj, buffer, queue->used);
    return csp_crc32_final(&crc_obj);
}

bool sc_verify_cmd_hash(sc_queue_t* queue, char * buffer, uint32_t addr, param_hash_t hash) {

    csp_crc32_t calc_hash = sc_calc_cmd_hash(queue, buffer);

    if (calc_hash != hash) {
        printf("Command number %"PRIX32" is obstructed (0x%"PRIX32" vs 0x%"PRIX32")\n", addr, hash, calc_hash);
        return false;
    }
    return true;
}

vmem_t* sc_get_vmem(sc_type_t sc_type) {

    switch (sc_type) {
        case SC_TYPE_CMD:
            return &vmem_cmd_hash;
        case SC_TYPE_SCH:
            return &vmem_sch_hash;
        default:
            return NULL;
        }
}

int32_t sc_get_size(sc_type_t sc_type) {
    vmem_t* vmem = sc_get_vmem(sc_type);
    int32_t res = -1;
    if(NULL != vmem) {
        res = vmem->size / sizeof(param_hash_t);
    }
	return res;
}

int32_t sc_find_addr(param_hash_t hash_in, sc_type_t sc_type) {

    vmem_t* vmem = sc_get_vmem(sc_type);
    int32_t addr = -1;
    if(0 != hash_in) {
        if (vmem != NULL) {
            param_hash_t hash;

            /* Commands are stored with their hash in one VMEM supporting random access
                A hash value 0 indicate that the particular position is not used
                The actual command is stored in a corresponding position in a separate VMEM supporting block writes */
            do {
                addr++;
                if (addr*sizeof(hash) >= vmem_cmd_hash.size) {
                    return -1;
                }
                vmem_read(&hash, (vmem->vaddr + addr*sizeof(hash)), sizeof(hash));
            } while (hash != hash_in);
        }
    }

    return addr;
}

int32_t sc_find_free_slot(sc_type_t sc_type) {

    vmem_t* vmem = sc_get_vmem(sc_type);
    int32_t addr = -1;
    if (vmem != NULL) {
        param_hash_t hash;

        /* Commands are stored with their hash in one VMEM supporting random access
            A hash value 0 indicate that the particular position is not used
            The actual command is stored in a corresponding position in a separate VMEM supporting block writes */
        do {
            addr++;
            if (addr*sizeof(hash) >= vmem_cmd_hash.size) {
                return -1;
            }
            vmem_read(&hash, vmem->vaddr + addr*sizeof(hash), sizeof(hash));
        } while (hash != 0);
    }

    return addr;
}

sch_status_t sc_remove_entry(param_hash_t hash, sc_type_t sc_type) {
    sch_status_t result = SCH_STATUS_MISSING_CMD;
    vmem_t* vmem = sc_get_vmem(sc_type);

    if (vmem == NULL) {
        result = SCH_STATUS_FAILED;
    } else {
        if (hash == 0x0) {
            /* Remove all entries */
            param_hash_t hash_tmp = 0;
            for (uint32_t addr = 0; addr < vmem->size/sizeof(param_hash_t); addr++) {
                vmem_read(&hash_tmp, vmem->vaddr+addr*sizeof(hash), sizeof(hash_tmp));
                if (hash_tmp != 0) {
                    vmem_write(vmem->vaddr+addr*sizeof(param_hash_t), &hash, sizeof(hash));
                }
            }
            result = SCH_STATUS_OK;
        } else {
            /* Remove specific entry */
            param_hash_t hash_tmp = 0;
            for (uint32_t addr = 0; addr < vmem->size/sizeof(hash); addr++) {
                vmem_read(&hash_tmp, vmem->vaddr+addr*sizeof(param_hash_t), sizeof(hash_tmp));
                if (hash_tmp == hash) {
                    hash_tmp = 0;
                    vmem_write(vmem->vaddr+addr*sizeof(param_hash_t), &hash_tmp, sizeof(hash_tmp));
                    result = SCH_STATUS_OK;
                    break;
                }
            }
        }
    }
    return result;
}

sch_status_t sc_get_cmd(int32_t cmd_addr, param_hash_t* hash, sc_queue_t* queue, char* buffer) {

    if (cmd_addr < 0) {
        return SCH_STATUS_FAILED;
    }

	vmem_read(hash, vmem_cmd_hash.vaddr+cmd_addr*sizeof(param_hash_t), sizeof(*hash));

	if (*hash == 0x0) {
        return SCH_STATUS_FAILED;
	}

    vmem_read(queue, vmem_cmd_store.vaddr+cmd_addr*CMD_ELEMENT_SIZE, sizeof(*queue));
    if (queue->used > CMD_ELEMENT_SIZE - sizeof(sc_queue_t)) {
        *hash = 0;
        vmem_write(vmem_cmd_hash.vaddr+cmd_addr*sizeof(param_hash_t), hash, sizeof(*hash));
        printf("Invalid queue size %u in addr %"PRId32"\n", queue->used, cmd_addr);
        return SCH_STATUS_CORRUPTED_CMD;
    }
    vmem_read(buffer, vmem_cmd_store.vaddr+cmd_addr*CMD_ELEMENT_SIZE+sizeof(sc_queue_t), queue->used);
    if (!sc_verify_cmd_hash(queue, buffer, cmd_addr, *hash)) {
        *hash = 0;
        vmem_write(vmem_cmd_hash.vaddr+cmd_addr*sizeof(param_hash_t), hash, sizeof(*hash));
        printf("Found invalid hash in addr %"PRId32", skipping\n", cmd_addr);
        return SCH_STATUS_CORRUPTED_CMD;
    }

    return SCH_STATUS_OK;
}

sch_status_t sc_read_cmd(param_hash_t hash, sc_queue_t* queue, char* buffer) {

    int32_t cmd_addr = sc_find_addr(hash, SC_TYPE_CMD);
    if (cmd_addr < 0) {
        return SCH_STATUS_MISSING_CMD;
    }

    vmem_read(queue, vmem_cmd_store.vaddr+cmd_addr*CMD_ELEMENT_SIZE, sizeof(*queue));
    if (queue->used > CMD_ELEMENT_SIZE - sizeof(sc_queue_t)) {
        hash = 0;
        vmem_write(vmem_cmd_hash.vaddr+cmd_addr*sizeof(param_hash_t), &hash, sizeof(hash));
        printf("Invalid queue size %u in addr %"PRId32"\n", queue->used, cmd_addr);
        return SCH_STATUS_CORRUPTED_CMD;
    }
    vmem_read(buffer, vmem_cmd_store.vaddr+cmd_addr*CMD_ELEMENT_SIZE+sizeof(sc_queue_t), queue->used);
    if (!sc_verify_cmd_hash(queue, buffer, cmd_addr, hash)) {
        hash = 0;
        vmem_write(vmem_cmd_hash.vaddr+cmd_addr*sizeof(param_hash_t), &hash, sizeof(hash));
        printf("Found invalid hash in addr %"PRId32", skipping\n", cmd_addr);
        return SCH_STATUS_CORRUPTED_CMD;
    }

    return SCH_STATUS_OK;
}

static bool sc_hash_exists(param_hash_t hash) {
    return sc_find_addr(hash, SC_TYPE_CMD) >= 0;
}

sch_status_t sc_find_cmd(char* name, sc_queue_t* queue, param_hash_t* hash) {

    vmem_t* vmem = sc_get_vmem(SC_TYPE_CMD);
    if (vmem == NULL) {
        *hash = 0;
        return SCH_STATUS_FAILED;
    }
    for (uint32_t addr = 0; addr < vmem->size/sizeof(param_hash_t); addr++) {
        vmem_read(queue, vmem_cmd_store.vaddr+addr*CMD_ELEMENT_SIZE, sizeof(*queue));
        if(strncmp(queue->name, name, sizeof(queue->name)) == 0) {
            vmem_read(hash, vmem_cmd_hash.vaddr+addr*sizeof(param_hash_t), sizeof(hash));
            return SCH_STATUS_OK;
        }
    }
    /* Not found by name, was it a hash ? */
    char *endptr = 0;
    *hash = strtoul(name, &endptr, 16);
    if (*endptr != '\0') {
        *hash = 0;
        return SCH_STATUS_MISSING_CMD;
    }
    if(true == sc_hash_exists(*hash)) {
        return SCH_STATUS_OK;
    }
    return SCH_STATUS_MISSING_CMD;
}

sch_status_t sc_store_cmd(param_hash_t hash, sc_queue_t* queue, char* buffer) {

    int32_t addr = sc_find_addr(hash, SC_TYPE_CMD);
    if (addr >= 0 && sc_verify_cmd_hash(queue, buffer, addr, hash)) {
        return SCH_STATUS_EXISTS;
    }

    addr = sc_find_free_slot(SC_TYPE_CMD);
    if (addr < 0) {
        return SCH_STATUS_FULL;
    }

    vmem_write(vmem_cmd_hash.vaddr+addr*sizeof(hash), &hash, sizeof(hash));
    vmem_write(vmem_cmd_store.vaddr+addr*CMD_ELEMENT_SIZE, queue, sizeof(*queue));
    vmem_write(vmem_cmd_store.vaddr+addr*CMD_ELEMENT_SIZE+sizeof(*queue), buffer, queue->used);

    return SCH_STATUS_OK;
}

char* sch_str_status(sch_status_t status) {

    switch (status) {
        case SCH_STATUS_SCHEDULED: return "Scheduled";
        case SCH_STATUS_COMPLETED: return "Completed";
        case SCH_STATUS_FAILED: return "Failed";
        case SCH_STATUS_OVERDUE: return "Overdue";
        case SCH_STATUS_CORRUPTED: return "Corrupted";
        case SCH_STATUS_MISSING_CMD: return "Missing CMD";
        case SCH_STATUS_CORRUPTED_CMD: return "Corrupted CMD";
        case SCH_STATUS_EXISTS: return "CMD exists";
        case SCH_STATUS_FULL: return "Queue is full";
        default: return "Unknown";
    }
}
