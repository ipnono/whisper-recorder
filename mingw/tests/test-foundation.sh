#!/bin/bash
# Test suite for Issue 01: Project Foundation

set -e

# Navigate to project root
cd "$(dirname "$0")/../.."
PROJECT_ROOT="$(pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓${NC} $1"; }
fail() { echo -e "${RED}✗${NC} $1"; exit 1; }
warn() { echo -e "${YELLOW}!${NC} $1"; }

echo "=== Issue 01: Project Foundation Tests ==="
echo ""

# Test 1: Directory structure exists
echo "Test 1: Directory structure"
dirs=(
    "windows"
    "wsl/src"
    "doc"
    "doc/issues"
)
for d in "${dirs[@]}"; do
    if [ -d "$d" ]; then
        pass "Directory exists: $d"
    else
        fail "Missing directory: $d"
    fi
done

# Test 2: Required files exist
echo ""
echo "Test 2: Required files"
files=(
    "wsl/Makefile"
    "wsl/src/main.c"
    "wsl/README.md"
    "windows/Makefile"
    "windows/hotkey.c"
    "config.json"
    ".gitignore"
    "README.md"
    "doc/PRD.md"
)
for f in "${files[@]}"; do
    if [ -f "$f" ]; then
        pass "File exists: $f"
    else
        fail "Missing file: $f"
    fi
done

# Test 3: config.json is valid JSON
echo ""
echo "Test 3: config.json validity"
# Use relative path since we're already in project root after cd
if command -v node &> /dev/null; then
    if node -e "JSON.parse(require('fs').readFileSync('config.json'))" 2>/dev/null; then
        pass "config.json is valid JSON"
    else
        fail "config.json is invalid JSON"
    fi
elif command -v python3 &> /dev/null; then
    if python3 -c "import json; json.load(open('config.json'))" 2>/dev/null; then
        pass "config.json is valid JSON"
    else
        fail "config.json is invalid JSON"
    fi
else
    warn "No JSON validator available, skipping"
fi

# Test 4: Issue files exist
echo ""
echo "Test 4: Issue files (10 required)"
issue_count=$(ls -1 doc/issues/[0-9]*.md 2>/dev/null | wc -l)
if [ "$issue_count" -ge 10 ]; then
    pass "Found $issue_count issue files (expected 10)"
else
    fail "Found $issue_count issue files (expected 10)"
fi

# Test 5: main.c has expected functions
echo ""
echo "Test 5: main.c structure"
if grep -q "print_usage" "wsl/src/main.c"; then
    pass "Has print_usage function"
else
    fail "Missing print_usage function"
fi

if grep -q "print_version" "wsl/src/main.c"; then
    pass "Has print_version function"
else
    fail "Missing print_version function"
fi

if grep -q "\-\-help" "wsl/src/main.c"; then
    pass "Handles --help flag"
else
    fail "Missing --help handling"
fi

if grep -q "\-\-version" "wsl/src/main.c"; then
    pass "Handles --version flag"
else
    fail "Missing --version handling"
fi

# Test 6: Build system
echo ""
echo "Test 6: Build system"
if grep -q "all:" "wsl/Makefile"; then
    pass "Makefile has all target"
else
    fail "Makefile missing all target"
fi

if grep -q "clean:" "wsl/Makefile"; then
    pass "Makefile has clean target"
else
    fail "Makefile missing clean target"
fi

if grep -q "test:" "wsl/Makefile"; then
    pass "Makefile has test target"
else
    fail "Makefile missing test target"
fi

# Test 7: VERSION defined
echo ""
echo "Test 7: Version constant"
if grep -q 'VERSION "' "wsl/src/main.c"; then
    pass "VERSION is defined"
else
    fail "VERSION not defined"
fi

echo ""
echo "=== All tests passed ==="
echo ""
echo "Project structure is ready."
echo "Build: cd wsl && make"
echo "Test:  cd wsl && make test"
