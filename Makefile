ARDUINO=~/arduino-1.8.7/arduino
FILE=rolek.ino
STATIC_FILES=$(shell find static -type f)
STATIC_FILES+=static/config.json

all: upload

build: static.ino rolek.ino
	$(ARDUINO) --verify $(FILE)

upload: static.ino rolek.ino
	$(ARDUINO) --port /dev/ttyUSB0 --upload $(FILE)

server:
	cd static; python3 -m http.server

clean:
	rm -f static.ino static/config.json

static/config.json: config.py
	python3 $< > $@

static.ino: $(STATIC_FILES)
	./bin2ino.py $^ > $@

.PHONY: all build upload server clean
