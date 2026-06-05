# Whisper Recorder - MinGW Build
# Run from MSYS2 "MinGW 64-bit" terminal.

CC = gcc
CXX = g++
CFLAGS  = -Wall -Wextra -std=c11   -O2 -g
CXXFLAGS = -Wall -Wextra -std=c++17 -O2 -g
LDFLAGS = -lm -lpthread -lws2_32 -lgomp -lwinmm

# ---- whisper.cpp (vendored under third_party/) ----
# Note: libwhisper.a references ggml-* symbols but does NOT statically link
# them in, so we must list every .a on the link line in the right order.
WHISPER_DIR   = third_party/whisper.cpp
WHISPER_BUILD = $(WHISPER_DIR)/build
WHISPER_INC   = -I$(WHISPER_DIR)/include -I$(WHISPER_DIR)/ggml/include
WHISPER_LIB   = $(WHISPER_BUILD)/src/libwhisper.a \
                $(WHISPER_BUILD)/ggml/src/ggml-base.a \
                $(WHISPER_BUILD)/ggml/src/ggml.a \
                $(WHISPER_BUILD)/ggml/src/ggml-cpu.a

# ---- application sources ----
# Explicit list — no wildcard. Prevents main() collisions if a stray
# .c file with its own main() ever lands in src/.
SRC_DIR  = src
SOURCES  = main.c \
           audio.c \
           download.c \
           session.c \
           transcribe.c \
           writer.c
OBJECTS  = $(SOURCES:%.c=$(BUILD_DIR)/%.o)

BUILD_DIR = build
BIN       = whisper-recorder.exe

MODEL     = $(WHISPER_DIR)/models/ggml-base.bin

# Hugging Face endpoint — override for users behind firewalls / in China.
# This is the env var name Hugging Face itself defines; hf-mirror.com reads
# the same one, so setting HF_ENDPOINT=https://hf-mirror.com works for both.
#
# NOTE: The whisper.cpp repo moved to the "ggml-org" org, but many China-side
# mirrors (incl. hf-mirror.com) only have the old "ggerganov" path synced.
# Using "ggerganov" works on both the official HF host and most mirrors.
HF_ENDPOINT ?= https://huggingface.co
MODEL_URL    = $(HF_ENDPOINT)/ggerganov/whisper.cpp/resolve/main/ggml-base.bin

.PHONY: all clean test help cmake build-whisper download-model

# ---- top-level targets ----

# Default: build whisper.cpp, then the app
all: build-whisper $(BUILD_DIR)/$(BIN)

# Configure whisper.cpp with CMake (skipped if already configured)
cmake:
	@if [ ! -f "$(WHISPER_BUILD)/CMakeCache.txt" ]; then \
		echo ">>> cmake: configuring whisper.cpp"; \
		mkdir -p "$(WHISPER_BUILD)" && \
		cd "$(WHISPER_BUILD)" && \
		cmake .. -DCMAKE_BUILD_TYPE=Release \
		         -DWHISPER_SDL2=OFF \
		         -DWHISPER_CUBLAS=OFF \
		         -DWHISPER_METAL=OFF \
		         -DBUILD_SHARED_LIBS=OFF \
		         -DBUILD_TESTS=OFF \
		         -DBUILD_EXAMPLES=OFF \
		         -G "Unix Makefiles"; \
	else \
		echo ">>> cmake: already configured"; \
	fi

# Build whisper.cpp static library
build-whisper: cmake
	@echo ">>> build-whisper: building libwhisper.a"
	$(MAKE) -C "$(WHISPER_BUILD)" -j4 whisper

# Link the application
$(BUILD_DIR)/$(BIN): $(OBJECTS)
	@echo ">>> link: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) -o $@ $(WHISPER_LIB) $(LDFLAGS)

# Compile a single .c -> .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(WHISPER_INC) -c $< -o $@

# Download ggml-base.bin (one-time, ~150MB)
# Honours $HF_ENDPOINT for users behind firewalls (e.g. HF_ENDPOINT=https://hf-mirror.com).
download-model:
	@mkdir -p "$(dir $(MODEL))"
	@if [ ! -f "$(MODEL)" ]; then \
		echo ">>> download-model: $(MODEL_URL)"; \
		curl -L --fail --retry 3 --retry-delay 5 \
		     --connect-timeout 30 --max-time 1800 \
		     -o "$(MODEL)" "$(MODEL_URL)"; \
	else \
		echo ">>> download-model: already present ($(MODEL))"; \
	fi

# Smoke tests
test: $(BUILD_DIR)/$(BIN)
	@./$(BUILD_DIR)/$(BIN) --help  > /dev/null && echo "  ok  --help"
	@./$(BUILD_DIR)/$(BIN) --version > /dev/null && echo "  ok  --version"

clean:
	rm -rf "$(BUILD_DIR)"

help:
	@echo "Targets:"
	@echo "  all              Build everything (default)"
	@echo "  cmake            Configure whisper.cpp with CMake (auto)"
	@echo "  build-whisper    Build whisper.cpp static library"
	@echo "  download-model   Fetch ggml-base.bin"
	@echo "  test             Run --help / --version smoke tests"
	@echo "  clean            Remove build artifacts"
	@echo ""
	@echo "Typical first run:"
	@echo "  make download-model   # one-time ~150MB"
	@echo "  make                  # builds libwhisper.a + whisper-recorder.exe"
