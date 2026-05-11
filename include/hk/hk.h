#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t timestamp; /* Timestamp for newest data to receive */
    uint16_t period; /* Time between each log */
    uint8_t prios : 3; /* One bit per priority, i.e. 0b101 for prio 1 and 3 */
    uint8_t throughput : 5; /* Percentage increase in download throughput of timestamps, leave at 0 for full throughput */
    uint8_t num_timestamps; /* Number of timestamps to retrieve */
} __attribute__((__packed__)) hk_retrievehdr_t;
