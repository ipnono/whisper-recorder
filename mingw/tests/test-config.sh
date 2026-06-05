#!/bin/bash
# Test suite for Issue 08: Config System

set -e

cd "$(dirname "$0")/../.."

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓${NC} $1"; }
fail() { echo -e "${RED}✗${NC} $1"; exit 1; }
warn() { echo -e "${YELLOW}!${NC} $1"; }

echo "=== Issue 08: Config System Tests ==="
echo ""

# Test 1: Config structure
echo "Test 1: Config structure"
if grep -q "typedef struct" wsl/src/config.c | head -1; then
    pass "Has config_t struct"
else
    fail "Missing config_t struct"
fi

# Test 2: Required config fields
echo ""
echo "Test 2: Config fields"
fields=(
    "model_path"
    "auto_download"
    "sample_rate"
    "vad_threshold"
    "output_dir"
    "server_port"
    "language"
)
for field in "${fields[@]}"; do
    if grep -q "$field" wsl/src/config.c; then
        pass "Has field: $field"
    else
        fail "Missing field: $field"
    fi
done

# Test 3: Config defaults
echo ""
echo "Test 3: Config defaults function"
if grep -q "config_set_defaults" wsl/src/config.c; then
    pass "Has config_set_defaults function"
else
    fail "Missing config_set_defaults"
fi

# Test 4: CLI arguments
echo ""
echo "Test 4: CLI arguments"
# Use -e to prevent option parsing issues
if grep -qe "--help" wsl/src/config.c; then pass "Handles --help"; else fail "--help"; fi
if grep -qe "--version" wsl/src/config.c; then pass "Handles --version"; else fail "--version"; fi
if grep -qe "--config" wsl/src/config.c; then pass "Handles --config"; else fail "--config"; fi
if grep -qe "--model" wsl/src/config.c; then pass "Handles --model"; else fail "--model"; fi
if grep -qe "--output" wsl/src/config.c; then pass "Handles --output"; else fail "--output"; fi
if grep -qe "--language" wsl/src/config.c; then pass "Handles --language"; else fail "--language"; fi
if grep -qe "--port" wsl/src/config.c; then pass "Handles --port"; else fail "--port"; fi
if grep -qe "--verbose" wsl/src/config.c; then pass "Handles --verbose"; else fail "--verbose"; fi
if grep -qe "--daemon" wsl/src/config.c; then pass "Handles --daemon"; else fail "--daemon"; fi

# Test 5: print_config function
echo ""
echo "Test 5: Config display"
if grep -q "print_config" wsl/src/config.c; then
    pass "Has print_config function"
else
    fail "Missing print_config"
fi

# Test 6: Log level support
echo ""
echo "Test 6: Log configuration"
if grep -q "log_level" wsl/src/config.c; then
    pass "Has log_level"
else
    fail "Missing log_level"
fi

# Test 7: Config command via TCP
echo ""
echo "Test 7: CONFIG command"
if grep -q '"CONFIG"' wsl/src/config.c; then
    pass "Handles CONFIG command"
else
    fail "Missing CONFIG command handler"
fi

# Test 8: Default values
echo ""
echo "Test 8: Default values"
if grep -q '16000' wsl/src/config.c; then
    pass "Default sample rate 16000"
else
    warn "Sample rate default not found"
fi

if grep -q '8765' wsl/src/config.c || grep -q 'DEFAULT_PORT' wsl/src/config.c; then
    pass "Default port configured"
else
    warn "Default port not found"
fi

if grep -q '"zh"' wsl/src/config.c; then
    pass "Default language zh"
else
    warn "Default language not found"
fi

echo ""
echo "=== All tests passed ==="
