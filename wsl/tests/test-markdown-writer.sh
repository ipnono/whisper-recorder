#!/bin/bash
# Test suite for Issue 06: Markdown Writer

set -e

cd "$(dirname "$0")/../.."

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓${NC} $1"; }
fail() { echo -e "${RED}✗${NC} $1"; exit 1; }
warn() { echo -e "${YELLOW}!${NC} $1"; }

echo "=== Issue 06: Markdown Writer Tests ==="
echo ""

# Test 1: Core functions
echo "Test 1: Core functions"
funcs=(
    "writer_init"
    "writer_shutdown"
    "writer_start_session"
    "writer_append_text"
    "writer_end_session"
)
for func in "${funcs[@]}"; do
    if grep -qe "$func" wsl/src/writer.c; then
        pass "Has: $func"
    else
        fail "Missing: $func"
    fi
done

# Test 2: Session management
echo ""
echo "Test 2: Session management"
if grep -qe "writer_is_in_session" wsl/src/writer.c; then
    pass "Has is_in_session"
else
    fail "Missing is_in_session"
fi

if grep -qe "writer_get_current_file" wsl/src/writer.c; then
    pass "Has get_current_file"
else
    fail "Missing get_current_file"
fi

if grep -qe "writer_get_segment_count" wsl/src/writer.c; then
    pass "Has get_segment_count"
else
    fail "Missing get_segment_count"
fi

# Test 3: Directory creation
echo ""
echo "Test 3: Directory handling"
if grep -qe "ensure_directory" wsl/src/writer.c; then
    pass "Has ensure_directory"
else
    fail "Missing ensure_directory"
fi

if grep -qe "create_date_dirs" wsl/src/writer.c; then
    pass "Has create_date_dirs config"
else
    fail "Missing create_date_dirs"
fi

# Test 4: Markdown format
echo ""
echo "Test 4: Markdown format"
# Check for markdown headers
if grep -qe "# Session:" wsl/src/writer.c; then
    pass "Uses session header"
else
    warn "No session header"
fi

# Check for footer
if grep -qe "Transcribed segments" wsl/src/writer.c; then
    pass "Has segment count footer"
else
    warn "No segment count footer"
fi

# Test 5: Events
echo ""
echo "Test 5: Events"
if grep -qe "\"session_started\"" wsl/src/writer.c; then
    pass "Emits session_started"
else
    fail "Missing session_started event"
fi

if grep -qe "\"session_ended\"" wsl/src/writer.c; then
    pass "Emits session_ended"
else
    fail "Missing session_ended event"
fi

# Test 6: Path handling
echo ""
echo "Test 6: Path handling"
if grep -qe "base_dir" wsl/src/writer.c; then
    pass "Has base_dir"
else
    fail "Missing base_dir"
fi

if grep -qe "get_date_dir" wsl/src/writer.c; then
    pass "Has date directory formatting"
else
    warn "No date directory formatting"
fi

echo ""
echo "=== All tests passed ==="
