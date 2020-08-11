build: src/rolek.cpp
	pio run

upload: src/rolek.cpp
	pio run --target upload

uploadfs: data/config.json
	pio run --target uploadfs

server: data/config.json
	cd data; python3 -m http.server

clean:
	pio run --target clean

.PHONY: build upload server clean
