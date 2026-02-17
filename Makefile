# CircuitC Makefile
# Electronic Circuit Simulator in C

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -Isrc
LDFLAGS = -lm

# Directories
SRC_DIR = src
BUILD_DIR = build
EXAMPLES_DIR = examples

# Source files
SOURCES = $(SRC_DIR)/solver.c $(SRC_DIR)/simulation.c $(SRC_DIR)/elements.c
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Example programs
EXAMPLE_SOURCES = $(wildcard $(EXAMPLES_DIR)/*.c)
EXAMPLE_BINS = $(patsubst $(EXAMPLES_DIR)/%.c,$(BUILD_DIR)/%,$(EXAMPLE_SOURCES))

# Targets
.PHONY: all clean examples test

all: $(BUILD_DIR) $(EXAMPLE_BINS)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/circuit.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build example programs
$(BUILD_DIR)/%: $(EXAMPLES_DIR)/%.c $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(OBJECTS) -o $@ $(LDFLAGS)

# Build examples
examples: $(EXAMPLE_BINS)

# Run test/example
test: $(BUILD_DIR)/examples
	@echo "Running circuit examples..."
	@./$(BUILD_DIR)/examples

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Help
help:
	@echo "CircuitC Build System"
	@echo "===================="
	@echo "Targets:"
	@echo "  all      - Build all examples (default)"
	@echo "  examples - Build example programs"
	@echo "  test     - Run example programs"
	@echo "  clean    - Remove build artifacts"
	@echo "  help     - Show this help message"
