#include "mpack/mpack.h"
#include <stdio.h>
#include <stdlib.h>
#include<inttypes.h>
#include <sys/time.h>
#include <csp/csp.h>
#include <hk/hk.h>
#include <param/param_queue.h>
#include <param/param_serializer.h>
#include <param/param_string.h>

#define MAX_HKS 16

typedef struct local_epoch_s {
	int count;
	time_t local_epoch[MAX_HKS];
	uint16_t node[MAX_HKS];
} local_epoch_t;
static local_epoch_t hks = {0};

typedef struct timesync_nodes_s {
	int count;
	uint16_t node[MAX_HKS];
	uint16_t paramid[MAX_HKS];
} timesync_nodes_t;
static timesync_nodes_t timesync_nodes = {0};

static uint8_t get_throughput_index(double rate);


bool hk_sync_epoch(mpack_reader_t *reader, int node, int src, const param_t *param, csp_timestamp_t *timestamp, time_t *local_epoch) {
    bool res = true;
    for (int i = 0; i < timesync_nodes.count; i++) {
        if (timesync_nodes.node[i] == node && timesync_nodes.paramid[i] == param->id) {
            mpack_tag_t tag = mpack_peek_tag(reader);
            *local_epoch = tag.v.i - timestamp->tv_sec;
            hk_set_epoch(*local_epoch, src, true);
            break;
        }
    }
    if (*local_epoch == -1 && !hk_get_epoch(local_epoch, src)) {
        res = false;
    }
    return res;
}

void hk_set_epoch(time_t epoch, uint16_t node, bool auto_sync) {

	time_t current_epoch;
	time(&current_epoch);

	/* 1577836800: Jan 1st 2020 */
	if (epoch > current_epoch || epoch < 1577836800) {
		char current_epoch_str[32];
		strftime(current_epoch_str, sizeof(current_epoch_str), "%Y-%m-%d %H:%M:%S", gmtime(&epoch));
		printf("HK: Illegal EPOCH %"PRIu64" (%s) received\n", (uint64_t)current_epoch, current_epoch_str);
		return;
	}

	/* update existing */
	for (int i = 0; i < hks.count; i++) {
		if (hks.node[i] == node) {

			if (auto_sync && hks.local_epoch[i] - epoch > 86400) {
				char time[32];
				strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", gmtime(&epoch));
				char time_current[32];
				strftime(time_current, sizeof(time_current), "%Y-%m-%d %H:%M:%S", gmtime(&hks.local_epoch[i]));
				printf("HK: Skipping possible invalid EPOCH %s, current EPOCH for HK node %u is %s (%"PRIu64")\n", time, node, time_current, (uint64_t)hks.local_epoch[i]);
				return;
			}

			if (llabs(hks.local_epoch[i] - epoch) > 1 || !auto_sync) {
				/* get unix time to string time */
				char time[32];
				strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", gmtime(&epoch));
				printf("HK: Updating HK node %u EPOCH by %"PRIu64" sec to %s (%"PRIu64")\n", node, (uint64_t)(hks.local_epoch[i] - epoch), time, (uint64_t)epoch);
			}

			hks.local_epoch[i] = epoch;
			return;
		}
	}

	if (hks.count >= MAX_HKS) {
		printf("HK: Error: Maximum number of HK nodes reached (%d). Cannot set new epoch for node %u\n", MAX_HKS, node);
		return;
	}

	hks.node[hks.count] = node;
	hks.local_epoch[hks.count++] = epoch;

	char new_epoch_str[32];
	strftime(new_epoch_str, sizeof(new_epoch_str), "%Y-%m-%d %H:%M:%S", gmtime(&epoch));
	printf("HK: Setting new hk node %u EPOCH to %s (%"PRIu64")\n", node, new_epoch_str, (uint64_t)epoch);
}

bool hk_get_epoch(time_t * local_epoch, uint16_t node) {

	for (int i = 0; i < hks.count; i++) {
		if (node == hks.node[i]) {
			*local_epoch = hks.local_epoch[i];
			return true;
		}
	}

	return false;
}

hk_retrieve_status_t hk_retrieve(unsigned int node, const char * filename, int32_t timestamp, uint32_t step, double rate, uint32_t num_timestamps, const char * prio, int rdp, int use_offset, int quiet, hk_retrieve_param_callback_f param_callback, void * callback_context) {

    if (prio == NULL) {
        prio = "123";
    }

    if (use_offset && timestamp != 0) {
        printf("Options offset and timestamp cannot be used simultaneously\n");
        return HK_RETRIEVE_EINVAL;
    }

    time_t epoch;
    if (timestamp < 0 && !hk_get_epoch(&epoch, node)) {
        printf("Relative timestamp cannot be used when satellite EPOCH is not known\n");
        return HK_RETRIEVE_EINVAL;
    } else if (timestamp < 0) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        timestamp = tv.tv_sec - epoch + timestamp;
    }

    csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, node, 13, 3000, CSP_O_CRC32 | rdp );
    if (conn == NULL) {
        printf("Could not create connection");
        return HK_RETRIEVE_EIO;
    }

    csp_packet_t * packet = csp_buffer_get(8);
    if (packet == NULL) {
        printf("Could not create packet");
        csp_close(conn);
        return HK_RETRIEVE_EIO;
    }

    uint8_t prio_mask = 0;
    if (strchr(prio, '1') != NULL) prio_mask |= 0b001;
    if (strchr(prio, '2') != NULL) prio_mask |= 0b010;
    if (strchr(prio, '3') != NULL) prio_mask |= 0b100;

    uint8_t throughput = get_throughput_index(rate);

    hk_retrievehdr_t *hk_retrievehdr = (hk_retrievehdr_t*)packet->data;
    hk_retrievehdr->timestamp = htobe32(timestamp);
    hk_retrievehdr->period = htobe16(step);
    hk_retrievehdr->prios = prio_mask;
    hk_retrievehdr->throughput = throughput;
    hk_retrievehdr->num_timestamps = (uint8_t)(num_timestamps & 0xFF);
    packet->length = sizeof(hk_retrievehdr_t);

    csp_send(conn, packet);

    long unsigned int last_timestamp = 0;
    unsigned int param_cnt = 0;

    FILE * file = stdout;
    if (filename != NULL) {
        file = fopen(filename, "a");
    }
    if (file == NULL) {
        printf("File could not be opened, redirecting to stdout\n");
        file = stdout;
    }

    do {
        csp_buffer_free(packet);
        packet = csp_read(conn, 3000);
        if (packet == NULL) {
            printf("No response\n");
            csp_close(conn);
            return HK_RETRIEVE_EIO;
        }

        if (last_timestamp != be32toh(packet->data32[0])) {
            last_timestamp = be32toh(packet->data32[0]);
            if (param_cnt != 0) {
                if (quiet) fprintf(file, "Receieved %u parameters\n", param_cnt);
                param_cnt = 0;
            }
            fprintf(file, "Timestamp %lu\n", last_timestamp);
        }

        if (use_offset) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            hk_set_epoch(tv.tv_sec - last_timestamp, node, false);
            use_offset = 0;
        }

        param_queue_t queue;
        param_queue_init(&queue, &packet->data[5], packet->length - 5, packet->length - 5, PARAM_QUEUE_TYPE_GET, 2);

        mpack_reader_t reader;
        mpack_reader_init_data(&reader, queue.buffer, queue.used);
        while(reader.data < reader.end) {
            int id, node_param, offset = -1;
            csp_timestamp_t timestamp_param = { .tv_sec = 0, .tv_nsec = 0};
            param_deserialize_id(&reader, &id, &node_param, &timestamp_param, &offset, &queue);
            const param_t * param = param_list_find_id(node_param, id);
            if (param == NULL) {
                printf("Parameter with node %d and id %d not found\n", node_param, id);
                mpack_discard(&reader);
                continue;
            }
            param_t param_log = *param;
            // param_log.timestamp = &timestamp_param;
            /* TODO Kevin: Is it not the case that we're updating the list timestamp here?
                Is that what we want? */
            *param_log.timestamp = timestamp_param;

            param_deserialize_from_mpack_to_param(NULL, &queue, &param_log, offset, &reader);

            if (param_callback != NULL) {
                /* `param_log` is stack allocated here so we modify the timestamp without affecting the global list. */
                param_callback(&param_log, callback_context);
            }

            /* Handle received parameter data here */
            if (!quiet) {
                /* TODO Kevin: Find an actual timestamp which is newer than param_log->timestamp,
                    so we can display the parameters in gray without causing valgrind errors due to linking errors. */
                param_print_file(file, &param_log, -1, NULL, 0, 1, UINT32_MAX);
            }
            param_cnt++;
        }

    } while ((packet->data[4] & (1 << 7)) == 0);
    csp_buffer_free(packet);

    if (param_cnt != 0 && quiet) {
        fprintf(file, "Receieved %u parameters\n", param_cnt);
    }

    fprintf(file, "\n");
    if (file != stdout) {
        fflush(file);
        fclose(file);
    }
    
    csp_close(conn);
    return HK_RETRIEVE_SUCCESS;
}

void hk_set_utcparam(unsigned int node, unsigned int paramid) {

	// update existing
	for (int i = 0; i < timesync_nodes.count; i++) {
		if (timesync_nodes.node[i] == node) {
			timesync_nodes.node[i] = node;
			timesync_nodes.paramid[i] = paramid;
			printf("HK: Updating HK UTC parameter from node %u\n", node);
			return;
		}
	}

	if (timesync_nodes.count >= MAX_HKS) {
		printf("HK: Error: Maximum number of HK nodes reached (%d). Cannot set new utcparam for node %u\n", MAX_HKS, node);
		return;
	}

	timesync_nodes.node[timesync_nodes.count] = node;
	timesync_nodes.paramid[timesync_nodes.count++] = paramid;

	printf("HK: Adding HK UTC parameter from node %u\n", node);
}


static const double hk_retrieve_rate_to_index[] = {
    0.37,
    0.45,
    0.56,
    0.69,
    0.85,
    1.04,
    1.29,
    1.58,
    1.95,
    2.40,
    2.95,
    3.64,
    4.48,
    5.51,
    6.79,
    8.36,
    10.29,
    12.67,
    15.60,
    19.20,
    23.64,
    29.10,
    35.83,
    44.11,
    54.31,
    66.86,
    82.31,
    101.34,
    124.76,
    153.60,
    189.10,
};

/* Returns the index of the nearest rate, where index starts at 1 */
static uint8_t get_throughput_index(double rate) {

    unsigned long idx;

    if (rate <= 0.0)
        return 0;

    /* Find the first rate that is larger than the requested rate */
    for (idx = 1; idx < sizeof(hk_retrieve_rate_to_index) / sizeof(double); idx++) {
        if (hk_retrieve_rate_to_index[idx] >= rate)
            break;
    }

    if (idx == sizeof(hk_retrieve_rate_to_index) / sizeof(double))
        return idx; // All rates are smaller

    if ((hk_retrieve_rate_to_index[idx] - rate) < (rate - hk_retrieve_rate_to_index[idx - 1]))
        return idx + 1;
    else
        return idx;
}
