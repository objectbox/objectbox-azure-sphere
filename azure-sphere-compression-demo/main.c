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

	// create store (make sure to execute `./objectbox-http-server ../path/to/compression-demo-db/ 8181` on the respective server computer beforehand)
	OBXC_store* store = obxc_store_open(&store_options);
	if (store == NULL)
		fail_with_output("unable to construct ObjectBox client store instance");

	// eventually close store
	obxc_store_close(store);
	Log_Debug("application exiting...\n");

	return 0;
}
