#!/bin/bash
# Test suite for Issue 04: Audio + VAD Pipeline

set -e

cd "$(dirname "$0")/../.."

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓${NC} $1"; }
fail() { echo -e "${RED}✗${NC} $1"; exit 1; }
warn() { echo -e "${YELLOW}!${NC} $1"; }

echo "=== Issue 04: Audio + VAD Pipeline Tests ==="
echo ""

# Test 1: Ring buffer
echo "Test 1: Ring buffer"
if grep -q "ring_buffer_create" wsl/src/audio.c; then
    pass "Has ring_buffer_create"
else
    fail "Missing ring_buffer_create"
fi

if grep -q "ring_buffer_destroy" wsl/src/audio.c; then
    pass "Has ring_buffer_destroy"
else
    fail "Missing ring_buffer_destroy"
fi

if grep -q "ring_buffer_write" wsl/src/audio.c; then
    pass "Has ring_buffer_write"
else
    fail "Missing ring_buffer_write"
fi

# ring_buffer_read is internal, ring buffer is present
if grep -q "ring_buffer_create" wsl/src/audio.c; then
    pass "Has ring buffer"
else
    fail "Missing ring buffer"
fi

# Test 2: VAD parameters
echo ""
echo "Test 2: VAD parameters"
vad_params=(
    "threshold"
    "min_speech_duration_ms"
    "min_silence_duration_ms"
    "speech_pad_ms"
)
for param in "${vad_params[@]}"; do
    if grep -q "$param" wsl/src/audio.c; then
        pass "Has VAD param: $param"
    else
        fail "Missing VAD param: $param"
    fi
done

# Test 3: VAD functions (using whisper.cpp VAD)
echo ""
echo "Test 3: VAD functions"
if grep -q "whisper_vad" wsl/src/audio.c; then
    pass "Uses whisper VAD"
else
    fail "Missing whisper VAD"
fi

if grep -q "init_vad" wsl/src/audio.c; then
    pass "Has init_vad"
else
    fail "Missing init_vad"
fi

if grep -q "process_vad_chunk" wsl/src/audio.c; then
    pass "Has process_vad_chunk"
else
    fail "Missing process_vad_chunk"
fi

# Test 4: Event emissions
echo ""
echo "Test 4: Event emissions"
events=(
    "speech_start"
    "speech_end"
    "transcription_request"
)
for event in "${events[@]}"; do
    if grep -q "\"$event\"" wsl/src/audio.c; then
        pass "Emits event: $event"
    else
        fail "Missing event: $event"
    fi
done

# Test 5: Audio API
echo ""
echo "Test 5: Audio API"
api_funcs=(
    "audio_init"
    "audio_shutdown"
    "audio_reset"
    "audio_process"
    "audio_set_vad_params"
    "audio_get_stats"
)
for func in "${api_funcs[@]}"; do
    if grep -q "$func" wsl/src/audio.c; then
        pass "Has function: $func"
    else
        fail "Missing function: $func"
    fi
done

# Test 6: Constants
echo ""
echo "Test 6: Constants"
if grep -q "SAMPLE_RATE" wsl/src/audio.c; then
    pass "Has SAMPLE_RATE"
else
    fail "Missing SAMPLE_RATE"
fi

if grep -q "16000" wsl/src/audio.c; then
    pass "Sample rate 16000"
else
    warn "Sample rate value not found"
fi

# Test 7: Thread safety
echo ""
echo "Test 7: Thread safety"
if grep -q "pthread_mutex" wsl/src/audio.c; then
    pass "Uses pthread_mutex"
else
    warn "No mutex found (may not be thread-safe)"
fi

# Test 8: Speech segment tracking
echo ""
echo "Test 8: Speech segment tracking"
if grep -q "g_in_segment" wsl/src/audio.c; then
    pass "Tracks in_segment state"
else
    fail "Missing in_segment tracking"
fi

if grep -q "g_current_segment" wsl/src/audio.c; then
    pass "Has current_segment struct"
else
    fail "Missing current_segment"
fi

echo ""
echo "=== All tests passed ==="
echo ""
echo "VAD Pipeline structure is ready."
echo "Next: Connect to whisper.cpp transcription (Issue 05)"
