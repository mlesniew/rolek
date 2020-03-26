build: src/rolek.ino data/config.json
	pio run

upload: src/rolek.ino
	pio run --target upload

server: data/config.json
	cd data; python3 -m http.server

clean:
	pio run --target clean
	rm -f data/config.json

data/config.json: config.py
	python3 $< > $@

.PHONY: build upload server clean
