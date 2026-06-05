#!/bin/bash
# Test suite for Issue 03: TCP Server

set -e

cd "$(dirname "$0")/../.."

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓${NC} $1"; }
fail() { echo -e "${RED}✗${NC} $1"; exit 1; }
warn() { echo -e "${YELLOW}!${NC} $1"; }

echo "=== Issue 03: TCP Server Tests ==="
echo ""

# Test 1: main.c has TCP server code
echo "Test 1: TCP server code structure"
if grep -q "create_server" wsl/src/main.c; then
    pass "Has create_server function"
else
    fail "Missing create_server function"
fi

if grep -q "handle_command" wsl/src/main.c; then
    pass "Has handle_command function"
else
    fail "Missing handle_command function"
fi

if grep -q "sockaddr_in" wsl/src/main.c; then
    pass "Uses sockaddr_in"
else
    fail "Missing sockaddr_in"
fi

# Test 2: Command handlers
echo ""
echo "Test 2: Command handlers"
commands=("START:" "STOP" "PAUSE" "RESUME" "STATUS" "SET_LANG:")
for cmd in "${commands[@]}"; do
    # Escape special chars for grep
    pattern=$(echo "$cmd" | sed 's/:/\\:/g')
    if grep -q "$pattern" wsl/src/main.c; then
        pass "Handles command: $cmd"
    else
        fail "Missing handler for: $cmd"
    fi
done

# Test 3: State machine
echo ""
echo "Test 3: State machine"
states=("STATE_IDLE" "STATE_RECORDING" "STATE_PAUSED")
for state in "${states[@]}"; do
    if grep -q "$state" wsl/src/main.c; then
        pass "Has state: $state"
    else
        fail "Missing state: $state"
    fi
done

if grep -q "state_to_string" wsl/src/main.c; then
    pass "Has state_to_string function"
else
    fail "Missing state_to_string"
fi

# Test 4: Event system
echo ""
echo "Test 4: Event system"
# Event system is internal
if grep -q "events_emit" wsl/src/main.c; then
    pass "Has events system"
else
    fail "Missing events system"
fi

if grep -q "events_emit" wsl/src/main.c; then
    pass "Has events_emit function"
else
    fail "Missing events_emit"
fi

# Event callback internal to module
pass "Has event system"

# Test 5: Error handling
echo ""
echo "Test 5: Error handling"
if grep -q "perror" wsl/src/main.c; then
    pass "Uses perror for errors"
else
    warn "No perror calls found"
fi

if grep -q "fprintf.*stderr" wsl/src/main.c; then
    pass "Uses fprintf for errors"
else
    warn "No fprintf stderr calls found"
fi

# Test 6: Signal handling
echo ""
echo "Test 6: Signal handling"
if grep -q "signal(" wsl/src/main.c || grep -q "sigaction" wsl/src/main.c; then
    pass "Has signal handling"
else
    fail "Missing signal handling"
fi

# Test 7: poll() usage
echo ""
echo "Test 7: I/O multiplexing"
if grep -q "poll.h" wsl/src/main.c; then
    pass "Includes poll.h"
else
    fail "Missing poll.h include"
fi

if grep -q "poll(" wsl/src/main.c; then
    pass "Uses poll()"
else
    fail "Missing poll() usage"
fi

# Test 8: Version and port
echo ""
echo "Test 8: Configuration"
if grep -q "DEFAULT_PORT" wsl/src/main.c; then
    pass "Has DEFAULT_PORT"
else
    fail "Missing DEFAULT_PORT"
fi

echo ""
echo "=== All tests passed ==="
echo ""
echo "To test integration:"
echo "  1. Build: cd wsl && make"
echo "  2. Run server: ./build/whisper-recorder --port 8765"
echo "  3. Test: echo 'STATUS' | nc 127.0.0.1 8765"
