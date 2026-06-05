#!/bin/bash
# Build script for Windows UI

set -e

echo "=== Building Windows UI ==="
echo ""

cd windows

# Check for gcc
if ! command -v gcc &> /dev/null; then
    echo "Error: gcc not found"
    echo "Please install MinGW-w64"
    exit 1
fi

echo "Compiling..."
gcc hotkey.c -o hotkey.exe \
    -luser32 -lws2_32 -lwinmm -lgdi32 -lshell32 -lcomctl32 \
    -Wall -Wextra

if [ -f "hotkey.exe" ]; then
    echo "✓ Build successful: hotkey.exe"
else
    echo "✗ Build failed"
    exit 1
fi

echo ""
echo "=== Build Complete ==="
echo ""
echo "Run with: ./hotkey.exe"
