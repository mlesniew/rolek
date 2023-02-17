build: src/rolek.cpp
	pio run

build-webui:
	cd webui && npm run build
	find data/ui/ -type f -not -name '*.gz' -print0 | xargs -r -0 -n1 gzip

upload: src/rolek.cpp
	pio run --target upload

uploadfs:
	pio run --target uploadfs

clean:
	pio run --target clean

.PHONY: build upload server clean
