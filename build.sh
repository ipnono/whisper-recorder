#!/bin/bash
# Build script for Whisper Recorder

set -e

echo "=== Whisper Recorder Build Script ==="
echo ""

# Check environment
if [ ! -d "wsl" ]; then
    echo "Error: Run this script from the project root"
    exit 1
fi

cd wsl

# Step 1: Build whisper.cpp
echo "Step 1: Building whisper.cpp..."
if [ ! -d "third_party/whisper.cpp/build" ]; then
    echo "  Running cmake..."
    cd third_party/whisper.cpp
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release \
              -DWHISPER_SDL2=OFF \
              -DWHISPER_CUBLAS=OFF \
              -DWHISPER_METAL=OFF \
              -G "MinGW Makefiles"
    cd ../..
fi

echo "  Building whisper libraries..."
cd third_party/whisper.cpp/build
mingw32-make -j4
cd ../../..

echo "  whisper.cpp built successfully"

# Step 2: Download model
echo ""
echo "Step 2: Checking model..."
if [ ! -f "third_party/whisper.cpp/models/ggml-base.bin" ]; then
    echo "  Downloading ggml-base.bin..."
    mkdir -p third_party/whisper.cpp/models
    curl -L -o third_party/whisper.cpp/models/ggml-base.bin \
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin"
else
    echo "  Model already exists"
fi

# Step 3: Build application
echo ""
echo "Step 3: Building whisper-recorder..."
g++ build/main.o -o build/whisper-recorder.exe \
    ./libcombined.a \
    -lm -lpthread -lws2_32 -lgomp -lwinmm

echo ""
echo "=== Build Complete ==="
echo ""
echo "Binary: wsl/build/whisper-recorder.exe"
echo ""
echo "To run:"
echo "  cd wsl"
echo "  ./build/whisper-recorder.exe -m third_party/whisper.cpp/models/ggml-base.bin"
