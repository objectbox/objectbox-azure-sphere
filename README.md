# ObjectBox client for Azure Sphere

For a general introduction to Azure Sphere and use cases in combination with [ObjectBox](https://objectbox.io/), check the [announcement post](https://objectbox.io/objectbox-on-azure-sphere-efficient-handling-of-persistent-iot-data-on-tiny-devices/). 

## Setup

For setting up the basic environment, please check out [the official installation instructions](https://docs.microsoft.com/en-us/azure-sphere/install/overview) for Azure Sphere by Microsoft. Next, do the following to run the demo project `azure-sphere-test`:

1. Download the HTTP server by running `./download.sh` on Linux or `download.bat` on Windows in this project's root directory.
   Alternatively, you can check the [releases](https://github.com/objectbox/objectbox-azure-sphere/releases) and download manually.
2. Execute `http-server/objectbox-http-server 8181` on Linux or `http-server\objectbox-http-server.exe 8181` on Windows.
3. Identify the local IP address of the computer you just started the server on.
4. Open this project's `objectbox-azure-sphere.sln` solution in Visual Studio.
5. Edit the value of [`Capabilities.AllowedConnections` in `azure-sphere-test/app_manifest.json`](azure-sphere-test/app_manifest.json#L8) to contain your IP address instead.
6. Set the value of the [`OBX_TEST_SERVER_IP` define in `azure-sphere-test/main.c`](azure-sphere-test/main.c#L19) to your IP address as well.
7. Connect the Azure Sphere board to your computer and make sure it's in the same local network.
8. Run the `azure-sphere-test` project, i.e. click on the _Remote GDB Debugger_ button in Visual Studio. After a short time, the application will start on the Azure Sphere board and some unit test results are output.

Note that steps 1 to 3 can be done on Linux or Windows; steps 4 to 8 strictly require Windows because of Visual Studio. Also, these two sections do not need to be executed on the same computer.

## Next steps

### The sensor demo

After having successfully run the demo, you are ready to check out the other demo project, `azure-sphere-sensor-demo`:

1. Buy the [_Grove Starter Kit for Azure Sphere_](https://www.seeedstudio.com/Grove-Starter-Kit-for-Azure-Sphere-MT3620-Development-Kit-p-3150.html) and connect it to your Azure Sphere board. Also attach the light and temperature/humidity sensors.
2. Repeat steps 5 and 6 from the setup instruction above for the respective files in the `azure-sphere-sensor-demo` project.
3. Run the `azure-sphere-sensor-demo` project. It will continously read from the attached sensors and write the values, together with the current timestamp, to the HTTP server's database.

### Working with the HTTP server

As you might have already guessed, running the HTTP server requires you to specify a port, these demos always use port 8181. If you'd like to change it, make sure to reflect the change in the corresponding `OBX_TEST_SERVER_PORT` define, for example [here](azure-sphere-test/main.c#L20).

ObjectBox will create the database files (i.e. `data.mdb` and `lock.mdb`) _in the current directory_. This means that you can call the HTTP server from another, empty directory if you prefer. Additionally, if these database files already exist in the current directory, they will be reused, i.e. none of their data is lost.

You can also look at the data currently residing in the database by using the ObjectBox Browser: just enter `http://localhost:8181` (of course depending on which port you chose) into the URL bar of a web browser of your choice.

## Architecture

![REST connection illustration](misc/azure-sphere-objectbox.png)

The image above illustrates how the Azure Sphere connects to a device running the ObjectBox HTTP server.
The "server" could also be an app running on a mobile phone;
example setups include direct connections via Wifi for smart home use cases.

During development, you can connect the Azure Sphere dev board to a server running on the development machine PC.
Just ensure both devices are running the same local network.
And, of course, you can connect the Azure Sphere to a production server on-premise or in the cloud.

The library `objectbox-client-azure-sphere` provided in this repository then connects to this server and exchanges data with it via a simple REST protocol.
Specifically, `GET` requests are used to get the number of entries in an entity or get a single or all entries.
`POST` is used for inserting data, updating entries uses `PUT`, and deleting them uses `DELETE`.

All payload of the various requests uses [FlatBuffers](https://google.github.io/flatbuffers/),
specifically [flatcc](https://github.com/dvidelabs/flatcc) for data serialization and deserialization.
The application running on the Azure Sphere device needs to know the data model in advance, e.g. by already being built with the compiled model.


## API

`objectbox-client-azure-sphere` gives developers the option to interact with the HTTP server using the following operations.
All methods, structs and error codes special to this library are prefixed with `obxc_`, `OBXC_` and `OBXC_`, respectively.
Identifiers that are used in other ObjectBox libraries as well, for example [objectbox-c](https://github.com/objectbox/objectbox-c),
use `obx_`, `OBX_` and `OBX_`, respectively.

### Initialization

First, an instance of the `OBXC_store_options` structure must be created. The following attributes must be set for that:

- `base_url`: The HTTP server's API base URL, e.g. `"http://192.168.178.54:8181/api/v2"`.
- `db`: The name of the desired database to connect to.
- `user` and `pass`: Username and password, respectively, needed to access the database.
  May be the empty string if no authentication is needed.
- `model.data` and `model.size`: For now always `NULL` and `0`, respectively.

After that, an instance of `OBXC_store*` can be created from these options using the function `OBXC_store* obxc_store_open(const OBXC_store_options* options)`.
It needs a pointer to a `OBXC_store_options` instance as its first and only parameter.
If the return value is `NULL`, creating the instance failed and further information may be obtained using the error handling methods presented below.

Finally, `obxc_store_close` must be used to correctly close the connection and deallocate all data associated with the store instance.


### General operations

All operations need a valid pointer to a `OBXC_store` instance as their first parameter to unambiguously identify the targeted database.

*`obx_err obxc_data_count(OBXC_store* store, int entityId, uint64_t* count)`*:
Counts the number of entries in the entity with id `entityId` and writes the result to the 64 bit integer value pointed to by `count`.


### Data retrieval

*`obx_err obxc_data_get(OBXC_store* store, int entityId, int id, OBXC_bytes* dest)`*
fetches the serialized FlatBuffers bytes of an entry with id `id` in the entity with id `entityId` into the buffer pointed to by `dest`.
The bytes can then be interpreted by feeding them through flatcc.
The result eventually needs to be deallocated using `obxc_bytes_free`.

*`obx_err obxc_data_get_all(OBXC_store* store, int entityId, OBXC_bytes_array* dest)`*
is similar to the previous function, but gets all entries associated with one entity.
This results also needs to be freed using `obxc_bytes_free`.


## Data modification

*`obx_err obxc_data_insert(OBXC_store* store, int entityId, const OBXC_bytes* src, int* id)`*
inserts a chunk of FlatBuffers-serialized data into the entity with id `entityId`.
The server will automatically assign a new, unique ID to the inserted entry, which will be returned by setting the integer pointed to by the `id` parameter.

*`obx_err obxc_data_update(OBXC_store* store, int entityId, int id, const OBXC_bytes* src)`* updates an entry in an entity with the given data.

*`obx_err obxc_data_delete(OBXC_store* store, int entityId, int id)`* deletes the respective entry from an entity.


### Error handling

All operations return [an error code](objectbox-client-azure-sphere/Inc/Public/objectbox.h#L42), which allows unified error handling.
A return value that is not `OBX_SUCCESS` indicates failure.
Alternatively, the last error code, as well as some more specific information, may be retrieved using the functions `obxc_last_error_code`, `obxc_last_error_message`, `obxc_last_error_secondary`, `obxc_last_error_clear`.
See the [demo application's source](azure-sphere-test/main.c) for a detailed example on how to correctly handle errors.

Note that all operations use the secondary error and the error message to store errors returned by the HTTP server.
This is, a HTTP code and a message explaining why the server rejected your request.

# Used libraries

- _MT3620 Grove Shield_, Copyright 2018 Seeed Studio, License [here](external/MT3620_Grove_Shield_Library/LICENSE.txt)
- _flatcc_, Copyright 2015 Mikkel F. Jørgensen, dvide.com, License [here](external/flatcc/LICENSE.txt)

# License

    Copyright 2019 ObjectBox Ltd. All rights reserved.
    
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
    
        http://www.apache.org/licenses/LICENSE-2.0
    
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
