# =============================================================================
# Makefile for the TinyCPU "Program Layout & Execution" project.
#
# - Compiles the TinyCPU emulator + assembler from src/*.cpp into ./tinycpu
# - Assembles every program in programs/*.asm into build/*.bin
# - Provides convenience targets to run / trace the recursive factorial
# =============================================================================

CXX        := c++
CXXFLAGS   := -std=c++17 -O2 -Wall -Wextra

TARGET     := tinycpu
BUILD_DIR  := build
SRC_DIR    := src
PROG_DIR   := programs

# C++ sources of the emulator + assembler.
SRCS := \
    $(SRC_DIR)/main.cpp          \
    $(SRC_DIR)/control_unit.cpp  \
    $(SRC_DIR)/bus.cpp           \
    $(SRC_DIR)/alu.cpp           \
    $(SRC_DIR)/assembler.cpp     \
    $(SRC_DIR)/isa.cpp

# Every .asm file under programs/ becomes a .bin under build/.
ASM_SRCS := $(wildcard $(PROG_DIR)/*.asm)
BIN_OUT  := $(patsubst $(PROG_DIR)/%.asm,$(BUILD_DIR)/%.bin,$(ASM_SRCS))

.PHONY: all clean run trace debug

# Default target: build the emulator and every program.
all: $(TARGET) $(BIN_OUT)

# Build the emulator.
$(TARGET): $(SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)
	@echo "Built ./$(TARGET)"

# Pattern: assemble programs/foo.asm -> build/foo.bin.
$(BUILD_DIR)/%.bin: $(PROG_DIR)/%.asm $(TARGET) | $(BUILD_DIR)
	./$(TARGET) asm $< -o $@

# Build directory.
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Convenience: run the recursive factorial demo.
run: $(BUILD_DIR)/factorial.bin
	./$(TARGET) run $(BUILD_DIR)/factorial.bin

# Trace mode: one disassembled line per instruction (recursion is visible
# as repeated jumps to the FACTORIAL entry point).
trace: $(BUILD_DIR)/factorial.bin
	./$(TARGET) run $(BUILD_DIR)/factorial.bin --trace

# Debug-style run with a memory dump after halt.
debug: $(BUILD_DIR)/factorial.bin
	./$(TARGET) run $(BUILD_DIR)/factorial.bin --trace --dump-after 0x0000:0x009F

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "Cleaned."
