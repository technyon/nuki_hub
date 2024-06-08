# Extract board names from platformio.ini
PLATFORMIO_INI := platformio.ini
BOARDS := $(shell grep -oP '(?<=\[env:)[^\]]+' $(PLATFORMIO_INI) | grep -v '_dbg')
DEBUG_BOARDS := $(shell grep -oP '(?<=\[env:)[^\]]+' $(PLATFORMIO_INI) | grep '_dbg')

# Default target
.PHONY: default
default: esp32

.PHONY: release
release: $(BOARDS)

.PHONY: debug
debug: $(DEBUG_BOARDS)

# Target to build all boards in both release and debug modes
.PHONY: all
all: release debug

# Alias
.PHONY: esp32
esp32: esp32dev

esp%:
	@echo "Building $@"
	pio run --environment $@

# Help target to display available build targets
.PHONY: help
help:
	@echo "Makefile targets:"
	@echo "  make                  - Default build (ESP32 in release mode)"
	@echo "  make deps             - Install software dependencies (PlatformIO)"
	@echo "  make all              - Build all boards in both release and debug modes"
	@$(foreach board,$(BOARDS),echo "  make $(board)       - Build $(board) in release mode";)
	@$(foreach board,$(DEBUG_BOARDS),echo "  make $(board)       - Build $(board) in debug mode";)
	@echo "Available boards:"
	@echo "  $(BOARDS)"
	@echo "  $(DEBUG_BOARDS)"

# Utility target to clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@-rm -rf release debug .pio/build

# Install dependencies
.PHONY: deps
deps:
	@echo "Installing dependencies..."
	pip install --upgrade platformio esptool
