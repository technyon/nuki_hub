# Extract board names from platformio.ini
PLATFORMIO_INI := platformio.ini
BOARDS := $(shell grep -oP '(?<=\[env:)[^\]]+' $(PLATFORMIO_INI) | grep -v '_dbg')
DEBUG_BOARDS := $(shell grep -oP '(?<=\[env:)[^\]]+' $(PLATFORMIO_INI) | grep '_dbg')
UPDATER_BOARDS := $(shell grep -oP '(?<=\[env:)[^\]]+' $(PLATFORMIO_INI) | grep -v '_dbg' | sed 's/^/updater_/')

# Default target
.PHONY: default
default: esp32

.PHONY: release
release: $(BOARDS)

.PHONY: updater
updater: $(UPDATER_BOARDS)

.PHONY: debug
debug: $(DEBUG_BOARDS)

# Target to build all boards in release, updater and debug modes
.PHONY: all
all: release updater debug

# Alias
esp%:
	@echo "Building $@"
	pio run --environment $@

updater_esp%:
	@echo "Building $@"
	pio run -d updater --environment $@

# Help target to display available build targets
.PHONY: help
help:
	@echo "Makefile targets:"
	@echo "  make                  - Default build (ESP32 in release mode)"
	@echo "  make deps             - Install software dependencies (PlatformIO)"
	@echo "  make all              - Build all boards in both release and debug modes"
	@$(foreach board,$(BOARDS),echo "  make $(board)       - Build $(board) in release mode";)
	@$(foreach board,$(UPDATER_BOARDS),echo "  make $(board)       - Build updater for $(board) in release mode";)
	@$(foreach board,$(DEBUG_BOARDS),echo "  make $(board)       - Build $(board) in debug mode";)
	@echo "Available boards:"
	@echo "  $(BOARDS)"
	@echo "  updater_$(BOARDS)"
	@echo "  $(DEBUG_BOARDS)"

# Utility target to clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@-rm -rf release debug .pio/build updater/.pio/build dependencies.lock updater/dependencies.lock managed_components updater/managed_components

# Install dependencies
.PHONY: deps
deps:
	@echo "Installing dependencies..."
	pip install --upgrade "https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip"
	pip install --upgrade "littlefs-python>=0.16.0" "fatfs-ng>=0.1.14" "pyyaml>=6.0.2" \
		"rich-click>=1.8.6" "zopfli>=0.2.2" "intelhex>=2.3.0" "rich>=14.0.0" "urllib3<2" \
		"cryptography>=45.0.3" "certifi>=2025.8.3" "ecdsa>=0.19.1" "bitstring>=4.3.1" \
		"reedsolo>=1.5.3,<1.8" "esp-idf-size>=2.0.0" "esp-coredump>=1.14.0"
