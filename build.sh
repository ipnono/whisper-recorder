#!/bin/bash
# Whisper Recorder - one-shot build script
# Run from MSYS2 "MinGW 64-bit" terminal, at the project root.

set -e

echo "=== Whisper Recorder Build ==="
echo ""

if [ ! -d "mingw" ]; then
    echo "ERROR: run this script from the project root (where mingw/ lives)."
    exit 1
fi

# 1. Vendor whisper.cpp source
if [ ! -d "mingw/third_party/whisper.cpp/CMakeLists.txt" ]; then
    echo "[1/3] Cloning whisper.cpp (shallow)..."
    mkdir -p "mingw/third_party"
    git clone --depth 1 https://github.com/ggml-org/whisper.cpp.git \
        "mingw/third_party/whisper.cpp"
else
    echo "[1/3] whisper.cpp source already present"
fi

# 2. Download ggml-base.bin if missing
echo ""
echo "[2/3] Ensuring whisper model is present..."
( cd "mingw" && make download-model )

# 3. Build
echo ""
echo "[3/3] Building whisper-recorder.exe..."
( cd "mingw" && make )

echo ""
echo "=== Build complete ==="
echo "Binary: mingw/build/whisper-recorder.exe"
echo ""
echo "Quick check:"
echo "  mingw/build/whisper-recorder.exe --version"
echo ""
echo "Run server (after plugging in the Windows UI binary in windows/):"
echo "  mingw/build/whisper-recorder.exe -m mingw/third_party/whisper.cpp/models/ggml-base.bin"
