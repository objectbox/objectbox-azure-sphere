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

#include <Grove.h>
#include <Sensors/GroveLightSensor.h>
#include <Sensors/GroveTempHumiSHT31.h>
#include <Sensors/GroveAD7992.h>

#include <curl/curl.h>
#include <objectbox.h>

#include "SensorDemoEntity_builder.h"
#include "SensorDemoEntity_reader.h"

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

float fix_float(float x) {
	if (isinf(x) || isnan(x))
		return 0.0f;
	return x;
}

uint64_t get_current_time_ns() {
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return (uint64_t)t.tv_sec * 1000000000L + (uint64_t)t.tv_nsec;
}

void transmit_sensor_values(OBXC_store* store, float light_intensity, float temperature, float humidity) {
	OBXC_bytes mem;
	int new_id;

	// initialize the flatbuffers structure
	flatcc_builder_t builder;
	flatcc_builder_init(&builder);
	SensorDemoEntity_start_as_root(&builder);

	// set all attributes
	uint64_t measured_at = get_current_time_ns();
	SensorDemoEntity_id_add(&builder, -1);
	SensorDemoEntity_lightIntensity_add(&builder, light_intensity);
	SensorDemoEntity_temperature_add(&builder, temperature);
	SensorDemoEntity_humidity_add(&builder, humidity);
	SensorDemoEntity_measuredAt_add(&builder, measured_at);

	// finish populating the entity's attributes and insert it at the server
	SensorDemoEntity_end_as_root(&builder);
	mem.data = flatcc_builder_get_direct_buffer(&builder, &mem.size);
	obxc_data_insert(store, 1, &mem, &new_id);
	Log_Debug("inserted new item with %d bytes, it got id %d\n", mem.size, new_id);
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

	// test light sensor (example from https://github.com/Seeed-Studio/MT3620_Grove_Shield#usage-of-the-library-see-example---temp-and-huminidy-sht31)
	int i2c_fd;
	GroveShield_Initialize(&i2c_fd, 115200);
	void* light_sensor = GroveLightSensor_Init(i2c_fd, 0);
	void* temp_humi_sensor = GroveTempHumiSHT31_Open(i2c_fd);
	for (int i = 0; i < 10; ++i) {
		GroveTempHumiSHT31_Read(temp_humi_sensor);
		transmit_sensor_values(store, GroveAD7992_ConvertToMillisVolt(GroveLightSensor_Read(light_sensor)), GroveTempHumiSHT31_GetTemperature(temp_humi_sensor), GroveTempHumiSHT31_GetHumidity(temp_humi_sensor));
		usleep(500000);
	}

	// eventually close store
	obxc_store_close(store);
	Log_Debug("application exiting...\n");

	return 0;
}
