ARDUINO=~/arduino-1.8.7/arduino
FILE=rolek.ino

all: upload

build: index.ino config.ino index.ino
	$(ARDUINO) --verify $(FILE)

upload: index.ino config.ino index.ino
	$(ARDUINO) --port /dev/ttyUSB0 --upload $(FILE)

server:
	python3 -m http.server

clean:
	rm -f index.mini.html config.ino index.ino config.json

index.mini.html: index.html
	minify $< > $@

index.ino: index.mini.html
	xxd -i $< | sed -r 's#unsigned#const unsigned#' | sed 's#unsigned char#unsigned char PROGMEM#' | sed 's#mini_##' > $@

config.json: config.py
	python3 $< > $@

config.ino: config.json
	xxd -i $< | sed -r 's#unsigned#const unsigned#' | sed 's#unsigned char#unsigned char PROGMEM#' > $@

.PHONY: all build upload server clean
