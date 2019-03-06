#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <objectbox.h>



obx_err error_print() {
    printf("Unexpected error: %d, %d (%s)\n", obx_last_error_code(), obx_last_error_secondary(), obx_last_error_message());
    return obx_last_error_code();
}

int main(int argc, char** argv) {
	// create model
	const uint32_t compression_demo_entity_id = 1;
	OBX_model* model = obx_model_create();
	obx_model_entity(model, "CompressionDemoEntity", compression_demo_entity_id, 10001);
		obx_model_property(model, "id", OBXPropertyType_Long, 1, 100010001);
			obx_model_property_flags(model, OBXPropertyFlags_ID);
		obx_model_property(model, "timestamp", OBXPropertyType_Date, 2, 100010002);
		obx_model_property(model, "userData", OBXPropertyType_ByteVector, 3, 100010003);
		obx_model_entity_last_property_id(model, 3, 100010003);
	obx_model_last_entity_id(model, compression_demo_entity_id, 10001);
	
	// create store to write the model to a new MDB directory
	OBX_store* store = obx_store_open(model, NULL);
	if(store == NULL) {
		error_print();
		exit(1);
	}
	obx_store_close(store);
	
	return 0;
}
