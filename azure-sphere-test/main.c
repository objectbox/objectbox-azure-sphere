#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>

#include "applibs_versions.h"
#include "mt3620_rdb.h"
#include <applibs/log.h>
#include <curl/curl.h>
#include <objectbox.h>

#include "TestEntity_builder.h"
#include "TestEntity_reader.h"

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

#define OBXC_ERROR_DUMP_SIZE 1024
char obxc_error_dump[OBXC_ERROR_DUMP_SIZE];
char* obxc_dump_errors() {
	snprintf(obxc_error_dump, OBXC_ERROR_DUMP_SIZE,
		"last error code:      %d\n"
		"last secondary error: %d\n"
		"last error message:   %s\n",
		obxc_last_error_code(), obxc_last_error_secondary(), obxc_last_error_message());
	return obxc_error_dump;
}

#define OBX_REQUIRE(CALL)                                                         \
	{                                                                             \
		obxc_last_error_clear();                                                  \
		obx_err r = CALL;                                                         \
		if (r != OBX_SUCCESS) {                                                   \
			Log_Debug("call in line %d returned invalid code %d\n", __LINE__, r); \
			Log_Debug("%s", obxc_dump_errors());                                  \
			exit(1);                                                              \
		}                                                                         \
	}

#define REQUIRE(EXPR)                                                \
	{                                                                \
		if (!(EXPR)) {                                               \
			Log_Debug("expression in line %d is false\n", __LINE__); \
			exit(1);                                                 \
		}                                                            \
	}

#define OBX_REQUIRE_ERROR(CALL, MAIN, SECONDARY, MSG)         \
    {                                                         \
        obxc_last_error_clear();                              \
        obx_err r = CALL;                                     \
        REQUIRE(r == MAIN);                                   \
        REQUIRE(obxc_last_error_code() == MAIN);              \
        REQUIRE(obxc_last_error_secondary() == SECONDARY);    \
        REQUIRE(strcmp(obxc_last_error_message(), MSG) == 0); \
    }

char* data_hex_string(OBXC_bytes* mem) {
	char* ret = (char*)malloc(mem->size * 2 + 1);
	for (int i = 0; i < mem->size; ++i)
		snprintf(ret + i * 2, 3, "%02x", ((char*)mem->data)[i] & 0xFF);
	return ret;
}

int compare_bytes_and_hex(OBXC_bytes* mem, const char* expc_hex) {
	char* mem_hex = data_hex_string(mem);
	int res = strcmp(mem_hex, expc_hex);
	free(mem_hex);
	return res == 0;
}

uint64_t get_current_time_ns() {
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return (uint64_t)t.tv_sec * 1000000000L + (uint64_t)t.tv_nsec;
}

void test_obxc_data_count(OBXC_store* store) {
	uint64_t count;
	OBX_REQUIRE(obxc_data_count(store, 1, &count));
	Log_Debug("[%s] count is %d\n", __FUNCTION__, (int)count);
}

void test_obxc_data_get(OBXC_store* store) {
	OBXC_bytes mem;

	// expected item data
	const char itemHex[] =
		"240000000000000000001a00280004002300220020001c00140000000000240000000c001a00"
		"00000100000000000000a4c0410c1b221c1500000000000000009bffffff9aff990104000000"
		"150000005465737420656e7469747920666f7220636f756e74000000";

	// load one item
	OBX_REQUIRE(obxc_data_get(store, 1, 1, &mem));
	REQUIRE(compare_bytes_and_hex(&mem, itemHex));
	Log_Debug("[%s] got %d bytes of item 1\n", __FUNCTION__, mem.size);
	obxc_bytes_free(&mem);
}

void test_obxc_data_remove_update(OBXC_store* store) {
	OBXC_bytes mem;

	// expected item data
	const char itemHex[] =
		"240000000000000000001a00280004002300220020001c00140000000000240000000c001a00"
		"00000100000000000000a4c0410c1b221c1500000000000000009bffffff9aff990104000000"
		"150000005465737420656e7469747920666f7220636f756e74000000";

	// load one item
	OBX_REQUIRE(obxc_data_get(store, 1, 1, &mem));
	REQUIRE(compare_bytes_and_hex(&mem, itemHex));
	Log_Debug("[%s] got and validated %d bytes of item 1\n", __FUNCTION__, mem.size);

	// delete item, check if it's really deleted
	OBX_REQUIRE(obxc_data_delete(store, 1, 1));
	OBX_REQUIRE_ERROR(obxc_data_delete(store, 1, 1), OBX_ERROR_ILLEGAL_RESPONSE, 404,
		"Object with the given ID doesn't exist");
	Log_Debug("[%s] deleted item 1 and made sure that it has really been deleted\n", __FUNCTION__);

	// finally reinsert the item and check if that was done correctly
	OBX_REQUIRE(obxc_data_update(store, 1, 1, &mem));
	obxc_bytes_free(&mem);
	OBX_REQUIRE(obxc_data_get(store, 1, 1, &mem));
	REQUIRE(compare_bytes_and_hex(&mem, itemHex));
	Log_Debug("[%s] got and validated %d bytes of item 1 again\n", __FUNCTION__, mem.size);
	obxc_bytes_free(&mem);
}

void test_obxc_data_insert(OBXC_store* store) {
	OBXC_bytes mem;
	int newId;

	const char oldItemHex[] =
		"240000000000000000001a00280004002300220020001c00140000000000240000000c001a00"
		"00000100000000000000a4c0410c1b221c1500000000000000009bffffff9aff990104000000"
		"150000005465737420656e7469747920666f7220636f756e74000000";

	// to test the insert, we get one of the existing items and create a copy
	OBX_REQUIRE(obxc_data_get(store, 1, 1, &mem));
	REQUIRE(compare_bytes_and_hex(&mem, oldItemHex));
	Log_Debug("[%s] got and validated %d bytes of item 1\n", __FUNCTION__, mem.size);

	// the server will automatically update the ID in the copy to the new one
	OBX_REQUIRE(obxc_data_insert(store, 1, &mem, &newId));
	Log_Debug("[%s] inserted a new item with %d bytes, it got id %d\n", __FUNCTION__, mem.size, newId);
	obxc_bytes_free(&mem);

	// check if the new data is correct
	OBX_REQUIRE(obxc_data_get(store, 1, newId, &mem));
	Log_Debug("[%s] got %d bytes of item %d\n", __FUNCTION__, mem.size, newId);
	obxc_bytes_free(&mem);

	// finally delete the item again and check if it's really deleted
	OBX_REQUIRE(obxc_data_delete(store, 1, newId));
	OBX_REQUIRE_ERROR(obxc_data_delete(store, 1, newId), OBX_ERROR_ILLEGAL_RESPONSE, 404,
		"Object with the given ID doesn't exist");
	Log_Debug("[%s] deleted item %d and made sure that it has really been deleted\n", __FUNCTION__, newId);
}

void test_flatcc_reader(OBXC_store* store, int id, int simpleBooleanVal, int simpleIntVal, float simpleFloatVal, const char* simpleStringVal, uint64_t simpleDateVal) {
	OBXC_bytes mem;

	// get some data first, then initialize the flatcc entity
	OBX_REQUIRE(obxc_data_get(store, 1, id, &mem));
	TestEntity_table_t entity = TestEntity_as_root(mem.data);
	REQUIRE(entity);

	// output some attributes of that entity
	Log_Debug("[%s, id=%d] id            = %" PRId64 "\n", __FUNCTION__, id, TestEntity_id(entity));
	Log_Debug("[%s, id=%d] simpleBoolean = %d\n", __FUNCTION__, id, TestEntity_simpleBoolean(entity));
	Log_Debug("[%s, id=%d] simpleInt     = %d\n", __FUNCTION__, id, TestEntity_simpleInt(entity));
	Log_Debug("[%s, id=%d] simpleFloat   = %f\n", __FUNCTION__, id, TestEntity_simpleFloat(entity));
	Log_Debug("[%s, id=%d] simpleString  = %s\n", __FUNCTION__, id, TestEntity_simpleString(entity));
	Log_Debug("[%s, id=%d] simpleDate    = %" PRId64 "\n", __FUNCTION__, id, TestEntity_simpleDate(entity));

	// if ID 1 is given, ensure that all attributes have the correct values
	REQUIRE(TestEntity_id(entity) == id);
	REQUIRE(TestEntity_simpleBoolean(entity) == simpleBooleanVal);
	REQUIRE(TestEntity_simpleInt(entity) == simpleIntVal);
	REQUIRE(TestEntity_simpleFloat(entity) == simpleFloatVal);
	REQUIRE(TestEntity_simpleString(entity) && strcmp(TestEntity_simpleString(entity), simpleStringVal) == 0);
	REQUIRE(TestEntity_simpleDate(entity) == simpleDateVal);
	Log_Debug("[%s, id=%d] all attribute values are correct\n", __FUNCTION__, id);

	// eventually free the received bytes
	obxc_bytes_free(&mem);
}

void test_flatcc_writer(OBXC_store* store) {
	OBXC_bytes mem, memRecv;
	int newId;

	// initialize the flatbuffers structure
	flatcc_builder_t builder;
	flatcc_builder_init(&builder);
	TestEntity_start_as_root(&builder);

	// set some attributes (note that ID is set to the dummy value 0 here, the actual ID is set automatically by the server upon insertion)
	uint64_t creationTime = get_current_time_ns();
	TestEntity_id_add(&builder, -1);
	TestEntity_simpleInt_add(&builder, 42);
	TestEntity_simpleFloat_add(&builder, 3.14159f);
	TestEntity_simpleString_create_str(&builder, "Don't believe his lies");
	TestEntity_simpleDate_add(&builder, creationTime);

	// finish populating the attributes of the entity and insert it at the server
	TestEntity_end_as_root(&builder);
	mem.data = flatcc_builder_get_direct_buffer(&builder, &mem.size);
	OBX_REQUIRE(obxc_data_insert(store, 1, &mem, &newId));
	Log_Debug("[%s] inserted a new item with %d bytes, it got id %d\n", __FUNCTION__, mem.size, newId);

	// read the item, then delete it and clean up flatcc
	test_flatcc_reader(store, newId, 0, 42, 3.14159f, "Don't believe his lies", creationTime);
	OBX_REQUIRE(obxc_data_delete(store, 1, newId));
	flatcc_builder_clear(&builder);
}

int main(int argc, char *argv[]) {
    Log_Debug("application starting...\n");
	register_sigterm_handler();

	// construct server URL
	char base_url[128];
	snprintf(base_url, 128, "http://%s:%d/api/v2", OBX_TEST_SERVER_IP, OBX_TEST_SERVER_PORT);

	// initialize store options and create store
	OBXC_store_options store_options;
	store_options.base_url = base_url;
	store_options.db = OBX_TEST_SERVER_DB;
	store_options.user = "";
	store_options.pass = "";
	store_options.model.data = NULL;
	store_options.model.size = 0;

	// create store (make sure to execute `./objectbox-http-server ../path/to/test-db/ 8181` on the respective server computer beforehand)
	OBXC_store* store = obxc_store_open(&store_options);
	if (store == NULL)
		fail_with_output("unable to construct ObjectBox client store instance");

	// execute all OBXC-only test cases
	test_obxc_data_count(store);
	test_obxc_data_get(store);
	test_obxc_data_remove_update(store);
	test_obxc_data_insert(store);

	// execute test cases with flatcc
	test_flatcc_reader(store, 1, 1, -101, 0.0f, "Test entity for count", 1521128273709482148L);
	test_flatcc_writer(store);

	// eventually close store
	obxc_store_close(store);
    Log_Debug("application exiting...\n");
	sleep(2);

    return 0;
}
