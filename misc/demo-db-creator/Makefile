# set your own HTTP server settings here
HTTP_SERVER = ../../../../objectbox/cbuild/Debug/objectbox/src/main/cpp/objectbox-http-server
HTTP_PORT = 8181

CFLAGS = -Iinclude -Llib -Wl,-rpath,\$${ORIGIN}/../lib

all: sensor-demo-db compression-demo-db

sensor-demo-db: lib/libobjectbox.so src/sensor-demo-entity.c
	mkdir -p bin
	gcc $(CFLAGS) src/sensor-demo-entity.c -o bin/sensor-demo-entity -lobjectbox
	bin/sensor-demo-entity
	rm -rf bin
	rm -rf dbs/sensor-demo-db
	mv objectbox dbs/sensor-demo-db

compression-demo-db: lib/libobjectbox.so src/compression-demo-entity.c
	mkdir -p bin
	gcc $(CFLAGS) src/compression-demo-entity.c -o bin/compression-demo-entity -lobjectbox
	bin/compression-demo-entity
	rm -rf bin
	rm -rf dbs/compression-demo-db
	mv objectbox dbs/compression-demo-db

http-server-sensor: sensor-demo-db
	$(HTTP_SERVER) dbs/sensor-demo-db $(HTTP_PORT)

http-server-compression: compression-demo-db
	$(HTTP_SERVER) dbs/compression-demo-db $(HTTP_PORT)

lib/libobjectbox.so:
	./download.sh
	rm -rf download

.PHONY: sensor-demo-db compression-demo-db
