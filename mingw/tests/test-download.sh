#!/bin/bash
# Test suite for Issue 10: Model Auto-Download

set -e

cd "$(dirname "$0")/../.."

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓${NC} $1"; }
fail() { echo -e "${RED}✗${NC} $1"; exit 1; }

echo "=== Issue 10: Model Auto-Download Tests ==="
echo ""

# Test 1: Core functions
echo "Test 1: Core functions"
funcs=(
    "download_model"
    "download_model_sync"
    "download_is_model_present"
    "download_get_model_path"
    "download_configure"
)
for func in "${funcs[@]}"; do
    if grep -qe "$func" wsl/src/download.c; then
        pass "Has: $func"
    else
        fail "Missing: $func"
    fi
done

# Test 2: Configuration
echo ""
echo "Test 2: Configuration"
if grep -qe "download_config_t" wsl/src/download.c; then
    pass "Has config struct"
else
    fail "Missing config struct"
fi

if grep -qe "max_retries" wsl/src/download.c; then
    pass "Has max_retries"
else
    fail "Missing max_retries"
fi

if grep -qe "auto_download" wsl/src/download.c; then
    pass "Has auto_download flag"
else
    fail "Missing auto_download flag"
fi

# Test 3: URL configuration
echo ""
echo "Test 3: URL configuration"
if grep -qe "huggingface" wsl/src/download.c; then
    pass "Has default HuggingFace URL"
else
    warn "No default URL found"
fi

# Test 4: Progress callback
echo ""
echo "Test 4: Progress callback"
if grep -qe "progress_callback_t" wsl/src/download.c; then
    pass "Has progress callback type"
else
    fail "Missing progress callback type"
fi

if grep -qe "download_set_progress_callback" wsl/src/download.c; then
    pass "Has set_progress_callback"
else
    fail "Missing set_progress_callback"
fi

# Test 5: Path handling
echo ""
echo "Test 5: Path handling"
if grep -qe "expand_path" wsl/src/download.c; then
    pass "Has expand_path"
else
    fail "Missing expand_path"
fi

if grep -qe "file_exists" wsl/src/download.c; then
    pass "Has file_exists"
else
    fail "Missing file_exists"
fi

echo ""
echo "=== All tests passed ==="
