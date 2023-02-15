all: build webui

build: src/rolek.cpp
	pio run

webui:
	cd webui && vue build
	find data/ui/ -type f -print0 | xargs -0 -n1 gzip -9

upload: src/rolek.cpp
	pio run --target upload

uploadfs: data/config.json
	pio run --target uploadfs

server: data/config.json
	cd data; python3 -m http.server

clean:
	pio run --target clean

.PHONY: webui build upload server clean
