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
	const uint32_t test_entity_id = 1;
	OBX_model* model = obx_model_create();
	obx_model_entity(model, "TestEntity", test_entity_id, 10001);
		obx_model_property(model, "id", OBXPropertyType_Long, 1, 100010001);
			obx_model_property_flags(model, OBXPropertyFlags_ID);
		obx_model_property(model, "simpleBoolean", OBXPropertyType_Bool, 2, 100010002);
		obx_model_property(model, "simpleByte", OBXPropertyType_Byte, 3, 100010003);
		obx_model_property(model, "simpleShort", OBXPropertyType_Short, 4, 100010004);
		obx_model_property(model, "simpleInt", OBXPropertyType_Int, 5, 100010005);
		obx_model_property(model, "simpleLong", OBXPropertyType_Long, 6, 100010006);
		obx_model_property(model, "simpleFloat", OBXPropertyType_Float, 7, 100010007);
		obx_model_property(model, "simpleDouble", OBXPropertyType_Double, 8, 100010008);
		obx_model_property(model, "simpleString", OBXPropertyType_String, 9, 100010009);
		obx_model_property(model, "simpleByteArray", OBXPropertyType_ByteVector, 10, 100010010);
		obx_model_property(model, "simpleDate", OBXPropertyType_Date, 11, 100010011);
		//obx_model_property(model, "simpleStringArray", OBXPropertyType_StringVector, 12, 100010012);
		obx_model_entity_last_property_id(model, 12, 100010012);
	obx_model_last_entity_id(model, test_entity_id, 10001);
	
	// create store to write the model to a new MDB directory
	OBX_store* store = obx_store_open(model, NULL);
	if(store == NULL) {
		error_print();
		exit(1);
	}
	obx_store_close(store);
	
	return 0;
}
