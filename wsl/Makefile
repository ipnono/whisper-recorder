# Whisper Recorder - WSL Build

CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -std=c11 -O2 -g
CXXFLAGS = -Wall -Wextra -std=c++11 -O2 -g
LDFLAGS = -lm -lpthread -lws2_32 -lgomp -lwinmm

# Whisper.cpp (built via CMake)
WHISPER_DIR = third_party/whisper.cpp
WHISPER_BUILD = $(WHISPER_DIR)/build
WHISPER_INC = -I$(WHISPER_DIR)/include -I$(WHISPER_DIR)/ggml/include
WHISPER_LIB = $(WHISPER_BUILD)/src/libwhisper.a \
              $(WHISPER_BUILD)/ggml/src/ggml-base.a \
              $(WHISPER_BUILD)/ggml/src/ggml.a \
              $(WHISPER_BUILD)/ggml/src/ggml-cpu.a

# Source files
SRC_DIR = src
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

BUILD_DIR = build
BIN = whisper-recorder

# VAD model (small, bundled)
VAD_MODEL = $(WHISPER_DIR)/models/ggml-base.bin

.PHONY: all clean test help download-model cmake build-whisper

# Default target
all: cmake build-whisper $(BUILD_DIR)/$(BIN)

# Build whisper.cpp with CMake (only once)
cmake:
	cd $(WHISPER_DIR) && rm -rf build && mkdir -p build && cd build && \
		cmake .. -DCMAKE_BUILD_TYPE=Release -DWHISPER_SDL2=OFF \
		         -DWHISPER_CUBLAS=OFF -DWHISPER_METAL=OFF \
		         -G "MinGW Makefiles"

build-whisper:
	cd $(WHISPER_DIR)/build && mingw32-make -j4

# Build application objects
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(WHISPER_INC) -c $< -o $@

# Link everything
$(BUILD_DIR)/$(BIN): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(WHISPER_LIB) $(LDFLAGS)

# Download VAD model
download-model vad-model:
	@mkdir -p $(dir $(VAD_MODEL))
	@if [ ! -f $(VAD_MODEL) ]; then \
		curl -L -o $(VAD_MODEL) \
		"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin"; \
	fi

# Run tests
test: $(BUILD_DIR)/$(BIN)
	@echo "Running basic tests..."
	@./$(BUILD_DIR)/$(BIN) --help > /dev/null && echo "✓ --help works"
	@./$(BUILD_DIR)/$(BIN) --version > /dev/null && echo "✓ --version works"

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Targets:"
	@echo "  all          - Build everything (default)"
	@echo "  cmake        - Build whisper.cpp with CMake"
	@echo "  build-whisper - Build just whisper.cpp"
	@echo "  vad-model    - Download the VAD model"
	@echo "  clean        - Remove build artifacts"
	@echo "  test         - Run basic tests"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Usage:"
	@echo "  make                    # Build everything"
	@echo "  make cmake              # Build whisper.cpp first"
	@echo "  make vad-model          # Download model"
	@echo "  make test               # Run tests"
	@echo "  ./build/whisper-recorder --help"
