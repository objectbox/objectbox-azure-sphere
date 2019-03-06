#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <math.h>

#include "applibs_versions.h"
#include "mt3620_rdb.h"
#include <applibs/log.h>

#include <curl/curl.h>
#include <objectbox.h>
#include <zstd.h>

#include "CompressionDemoEntity_builder.h"
#include "CompressionDemoEntity_reader.h"

#define OBX_TEST_SERVER_DB "test-db"
#define OBX_TEST_SERVER_IP "192.168.178.54"
#define OBX_TEST_SERVER_PORT 8181

static volatile sig_atomic_t termination_required = false;
static void termination_handler(int signal_number) {
	termination_required = true;
}

void register_sigterm_handler() {
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = termination_handler;
	sigaction(SIGTERM, &action, NULL);
}

void fail_with_output(const char* msg) {
	Log_Debug(msg);
	exit(1);
}

uint64_t get_current_time_ns() {
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return (uint64_t)t.tv_sec * 1000000000L + (uint64_t)t.tv_nsec;
}

int8_t* generate_byte_array(size_t* len) {
	*len = rand() % 200 + 1;
	int8_t* ret = malloc(*len * sizeof(int8_t));
	if (ret == NULL) {
		Log_Debug("failed to allocate memory in %s!\n", __FUNCTION__);
		*len = 0;
		return NULL;
	}
	for (size_t i = 0; i < *len; ++i)
		ret[i] = (int8_t) (i + 32 + rand() % 2 - 1);
	return ret;
}

flatbuffers_string_ref_t* generate_string_array(flatcc_builder_t* builder, size_t* len) {
	*len = rand() % 50;
	flatbuffers_string_ref_t* ret = malloc(*len * sizeof(flatbuffers_string_ref_t));
	if (ret == NULL) {
		Log_Debug("failed to allocate memory in %s!\n", __FUNCTION__);
		*len = 0;
		return NULL;
	}
	char buf[20];

	for (size_t i = 0; i < *len; ++i) {
		size_t curr_len = rand() % 20 + 1;
		for (size_t j = 0; j < curr_len; ++j)
			buf[j] = (char) ('A' + (rand() % 26));
		ret[i] = flatbuffers_string_create_strn(builder, buf, curr_len);
	}

	return ret;
}

void transmit_entry(OBXC_store* store) {
	OBXC_bytes mem, mem_compr;
	int new_id;

	// initialize the flatbuffers structure
	flatcc_builder_t builder;
	flatcc_builder_init(&builder);
	CompressionDemoEntity_start_as_root(&builder);

	// determine attribute values
	uint64_t timestamp = get_current_time_ns();
	size_t byte_array_len = 0, string_array_len = 0;
	uint8_t* byte_array = generate_byte_array(&byte_array_len);
	flatbuffers_string_ref_t* string_array = generate_string_array(&builder, &string_array_len);
	Log_Debug("having %lu bytes and %lu strings\n", byte_array_len, string_array_len);

	// add all attributes
	CompressionDemoEntity_id_add(&builder, -1);
	CompressionDemoEntity_timestamp_add(&builder, timestamp);
	CompressionDemoEntity_userData_create(&builder, byte_array, byte_array_len);
	CompressionDemoEntity_userStrings_create(&builder, string_array, string_array_len);

	// finish populating the entity's attributes and generate the Flatbuffers bytes
	CompressionDemoEntity_end_as_root(&builder);
	mem.data = flatcc_builder_get_direct_buffer(&builder, &mem.size);
	free(byte_array);
	free(string_array);
	if (mem.data == NULL || mem.size == 0) {
		Log_Debug("mem.data is %p, mem.size is %lu!\n", mem.data, mem.size);
		return;
	}

	// compress the data using Zstandard
	size_t compr_buf_size = ZSTD_compressBound(mem.size) + sizeof(uint32_t);
	mem_compr.data = malloc(compr_buf_size);
	mem_compr.size = ZSTD_compress(mem_compr.data + sizeof(uint32_t), compr_buf_size, mem.data, mem.size, 10) + sizeof(uint32_t);
	*((uint32_t*)mem_compr.data) = mem.size;
	flatcc_builder_clear(&builder);
	Log_Debug("compressed data from %lu bytes to %lu bytes (estimated %lu)\n", mem.size, mem_compr.size, compr_buf_size);

	// insert data into database
	obxc_data_insert(store, 1, &mem_compr, &new_id);
	Log_Debug("inserted new item with %d bytes, it got id %d\n", mem.size, new_id);
	free(mem_compr.data);
}

int main(int argc, char *argv[]) {
	Log_Debug("application starting...\n");
	register_sigterm_handler();
	srand(time(NULL));

	// construct server URL
	char base_url[20 + sizeof(OBX_TEST_SERVER_IP)];
	snprintf(base_url, 128, "http://%s:%d/api/v2", OBX_TEST_SERVER_IP, OBX_TEST_SERVER_PORT);

	// initialize store options and create store
	OBXC_store_options store_options;
	store_options.base_url = base_url;
	store_options.db = OBX_TEST_SERVER_DB;
	store_options.user = "";
	store_options.pass = "";
	store_options.model.data = NULL;
	store_options.model.size = 0;

	// create store (make sure to execute `./objectbox-http-server ../path/to/compression-demo-db/ 8181` on the respective server computer beforehand)
	OBXC_store* store = obxc_store_open(&store_options);
	if (store == NULL)
		fail_with_output("unable to construct ObjectBox client store instance");

	// transmit entries to store
	transmit_entry(store);

	// eventually close store
	obxc_store_close(store);
	Log_Debug("application exiting...\n");
	fflush(stdout);

	return 0;
}
