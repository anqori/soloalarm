ARDUINO_CLI ?= arduino-cli
CLI_CONFIG ?= arduino-cli.yaml
SKETCH_DIR ?= arduino-project
# Default MiniCore target; override FQBN for specific MCU variants/options.
FQBN ?= MiniCore:avr:328
# Default to ATmega328PB variant (ARD ONE C-MC). Override as needed.
BOARD_OPTIONS ?= variant=modelPB
BUILD_DIR ?= build
PORT ?= $(shell $(ARDUINO_CLI) board list | awk 'NR==2 {print $$1}')
BAUD ?= 9600
LIBS ?= "Adafruit SSD1306" "Adafruit GFX Library"

.PHONY: help list deps build upload monitor clean check-port

help:
	@echo "Targets: build upload monitor clean list"
	@echo "Vars: FQBN=... BOARD_OPTIONS=... PORT=... BAUD=... SKETCH_DIR=... BUILD_DIR=..."

list:
	$(ARDUINO_CLI) board list --config-file $(CLI_CONFIG)

deps:
	$(ARDUINO_CLI) lib install $(LIBS)

build:
	$(ARDUINO_CLI) compile --config-file $(CLI_CONFIG) --fqbn $(FQBN) $(if $(BOARD_OPTIONS),--board-options $(BOARD_OPTIONS),) --build-path $(BUILD_DIR) $(SKETCH_DIR)

check-port:
	@test -n "$(PORT)" || (echo "PORT not set. Run: $(ARDUINO_CLI) board list and set PORT=/dev/tty..." && exit 1)

upload: check-port
	$(ARDUINO_CLI) upload --config-file $(CLI_CONFIG) --fqbn $(FQBN) $(if $(BOARD_OPTIONS),--board-options $(BOARD_OPTIONS),) --build-path $(BUILD_DIR) -p $(PORT) $(SKETCH_DIR)

monitor: check-port
	$(ARDUINO_CLI) monitor --config-file $(CLI_CONFIG) --fqbn $(FQBN) -p $(PORT) --config baudrate=$(BAUD)

clean:
	rm -rf $(BUILD_DIR)
