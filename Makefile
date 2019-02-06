ARDUINO=~/arduino-1.8.7/arduino
FILE=rolek.ino
STATIC_FILES=$(shell find static -type f)
STATIC_FILES+=static/config.json

ARDUINO_OPTIONS=--board esp8266:esp8266:nodemcuv2 \
	--pref build.path=build \
	--preserve-temp-files

all: upload

build:
	mkdir -p build

verify: static.ino rolek.ino build
	$(ARDUINO) $(ARDUINO_OPTIONS) --verify $(FILE)

upload: static.ino rolek.ino build
	$(ARDUINO) $(ARDUINO_OPTIONS) --port /dev/ttyUSB0 --upload $(FILE)

server:
	cd static; python3 -m http.server

clean:
	rm -f static.ino static/config.json

static/config.json: config.py
	python3 $< > $@

static.ino: $(STATIC_FILES)
	./bin2ino.py $^ > $@

.PHONY: all build upload server clean
