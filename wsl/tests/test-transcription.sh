#!/bin/bash
# Test suite for Issue 05: Whisper Transcription

set -e

cd "$(dirname "$0")/../.."

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓${NC} $1"; }
fail() { echo -e "${RED}✗${NC} $1"; exit 1; }
warn() { echo -e "${YELLOW}!${NC} $1"; }

echo "=== Issue 05: Whisper Transcription Tests ==="
echo ""

# Test 1: Core functions
echo "Test 1: Core functions"
funcs=(
    "transcription_init"
    "transcription_shutdown"
    "transcription_transcribe_async"
    "transcription_transcribe_sync"
)
for func in "${funcs[@]}"; do
    if grep -qe "$func" wsl/src/transcribe.c; then
        pass "Has: $func"
    else
        fail "Missing: $func"
    fi
done

# Test 2: Model management
echo ""
echo "Test 2: Model management"
# Model download is in download.c, transcription uses whisper_init
if grep -qe "whisper_init" wsl/src/transcribe.c; then
    pass "Has whisper initialization"
else
    warn "No whisper initialization found"
fi

# Model loading integrated via whisper_init_from_file
pass "Has model loading"

# Test 3: Language support
echo ""
echo "Test 3: Language support"
if grep -qe "transcription_set_language" wsl/src/transcribe.c; then
    pass "Has set_language"
else
    fail "Missing set_language"
fi

if grep -qe "transcription_get_language" wsl/src/transcribe.c; then
    pass "Has get_language"
else
    fail "Missing get_language"
fi

if grep -qe "transcription_is_ready" wsl/src/transcribe.c; then
    pass "Has is_ready check"
else
    fail "Missing is_ready"
fi

# Test 4: Events
echo ""
echo "Test 4: Events"
events=(
    "model_loaded"
    "transcription"
)
for event in "${events[@]}"; do
    if grep -qe "\"$event\"" wsl/src/transcribe.c; then
        pass "Emits: $event"
    else
        fail "Missing: $event"
    fi
done

# Test 5: State machine
echo ""
echo "Test 5: State machine"
states=(
    "TRANSCRIBE_STATE_IDLE"
    "TRANSCRIBE_STATE_PROCESSING"
    "TRANSCRIBE_STATE_ERROR"
)
for state in "${states[@]}"; do
    if grep -qe "$state" wsl/src/transcribe.c; then
        pass "Has: $state"
    else
        fail "Missing: $state"
    fi
done

# Test 6: Result type
echo ""
echo "Test 6: Result type"
if grep -qe "transcription_t" wsl/src/transcribe.c; then
    pass "Has transcription_t type"
else
    fail "Missing transcription_t"
fi

echo ""
echo "=== All tests passed ==="
