STATIC_FILES=$(shell find static -type f)
STATIC_FILES+=static/config.json

build: src/static.ino src/rolek.ino
	pio run

upload: src/static.ino src/rolek.ino
	pio run --target upload

server:
	cd static; python3 -m http.server

clean:
	pio run --target clean
	rm -f src/static.ino static/config.json

static/config.json: config.py
	python3 $< > $@

src/static.ino: $(STATIC_FILES)
	./bin2ino.py static > $@

.PHONY: build upload server clean
