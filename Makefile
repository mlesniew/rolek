build: src/rolek.cpp
	pio run

build-webui:
	cd webui && vue build
	rm -rf data/*
	cp -r webui/dist/* data/
	find data/ -type f -name '*.map' -delete
	find data -type f -not -name '*.gz' -print0 | xargs -0 -n1 gzip

upload: src/rolek.cpp
	pio run --target upload

uploadfs:
	pio run --target uploadfs

clean:
	pio run --target clean

.PHONY: build upload server clean
