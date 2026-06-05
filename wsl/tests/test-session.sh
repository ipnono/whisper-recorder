#!/bin/bash
# Test suite for Issue 07: Session Management

set -e

cd "$(dirname "$0")/../.."

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓${NC} $1"; }
fail() { echo -e "${RED}✗${NC} $1"; exit 1; }

echo "=== Issue 07: Session Management Tests ==="
echo ""

# Test 1: Core functions
echo "Test 1: Core functions"
funcs=(
    "session_start"
    "session_end"
    "session_should_split"
    "session_should_auto_split"
    "session_is_active"
    "session_get_name"
    "session_get_segment_count"
    "session_get_duration"
)
for func in "${funcs[@]}"; do
    if grep -qe "$func" wsl/src/session.c; then
        pass "Has: $func"
    else
        fail "Missing: $func"
    fi
done

# Test 2: Configuration
echo ""
echo "Test 2: Configuration"
if grep -qe "session_config_set_silence_timeout" wsl/src/session.c; then
    pass "Has silence timeout config"
else
    fail "Missing silence timeout config"
fi

if grep -qe "session_config_set_max_duration" wsl/src/session.c; then
    pass "Has max duration config"
else
    fail "Missing max duration config"
fi

if grep -qe "session_config_set_debounce" wsl/src/session.c; then
    pass "Has debounce config"
else
    fail "Missing debounce config"
fi

# Test 3: Events
echo ""
echo "Test 3: Events"
events=(
    "\"session_started\""
    "\"session_ended\""
    "\"session_empty\""
)
for event in "${events[@]}"; do
    if grep -qe "$event" wsl/src/session.c; then
        pass "Emits: $event"
    else
        fail "Missing: $event"
    fi
done

# Test 4: Split conditions
echo ""
echo "Test 4: Split conditions"
if grep -qe "silence_timeout" wsl/src/session.c; then
    pass "Checks silence timeout"
else
    fail "Missing silence timeout check"
fi

if grep -qe "max_duration" wsl/src/session.c; then
    pass "Checks max duration"
else
    fail "Missing max duration check"
fi

# Test 5: Counter management
echo ""
echo "Test 5: Counter management"
if grep -qe "session_next_counter" wsl/src/session.c; then
    pass "Has next_counter"
else
    fail "Missing next_counter"
fi

if grep -qe "session_reset_counter" wsl/src/session.c; then
    pass "Has reset_counter"
else
    fail "Missing reset_counter"
fi

echo ""
echo "=== All tests passed ==="
