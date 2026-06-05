#!/bin/bash
# Test suite for Issue 09: Windows Floating UI

set -e

cd "$(dirname "$0")/../.."

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓${NC} $1"; }
fail() { echo -e "${RED}✗${NC} $1"; exit 1; }

echo "=== Issue 09: Windows Floating UI Tests ==="
echo ""

# Test 1: Window creation
echo "Test 1: Window creation"
if grep -qe "CreateWindowEx" windows/hotkey.c; then
    pass "Uses CreateWindowEx"
else
    fail "Missing CreateWindowEx"
fi

if grep -qe "WS_EX_TOPMOST" windows/hotkey.c; then
    pass "Uses WS_EX_TOPMOST"
else
    fail "Missing WS_EX_TOPMOST"
fi

# Test 2: Hotkey registration
echo ""
echo "Test 2: Hotkey registration"
if grep -qe "RegisterHotKey" windows/hotkey.c; then
    pass "Uses RegisterHotKey"
else
    fail "Missing RegisterHotKey"
fi

if grep -qe "WM_HOTKEY" windows/hotkey.c; then
    pass "Handles WM_HOTKEY"
else
    fail "Missing WM_HOTKEY handler"
fi

if grep -qe "MOD_CONTROL" windows/hotkey.c; then
    pass "Uses MOD_CONTROL"
else
    fail "Missing MOD_CONTROL"
fi

if grep -qe "VK_SPACE" windows/hotkey.c; then
    pass "Uses VK_SPACE"
else
    fail "Missing VK_SPACE"
fi

# Test 3: System tray
echo ""
echo "Test 3: System tray"
if grep -qe "Shell_NotifyIcon" windows/hotkey.c; then
    pass "Uses Shell_NotifyIcon"
else
    fail "Missing Shell_NotifyIcon"
fi

if grep -qe "NOTIFYICONDATA" windows/hotkey.c; then
    pass "Uses NOTIFYICONDATA"
else
    fail "Missing NOTIFYICONDATA"
fi

if grep -qe "WM_TRAYICON" windows/hotkey.c; then
    pass "Defines WM_TRAYICON"
else
    fail "Missing WM_TRAYICON"
fi

# Test 4: Tray menu
echo ""
echo "Test 4: Tray menu"
if grep -qe "CreatePopupMenu" windows/hotkey.c; then
    pass "Uses CreatePopupMenu"
else
    fail "Missing CreatePopupMenu"
fi

if grep -qe "TrackPopupMenu" windows/hotkey.c; then
    pass "Uses TrackPopupMenu"
else
    fail "Missing TrackPopupMenu"
fi

# Test 5: Network client
echo ""
echo "Test 5: Network client"
if grep -qe "WSAStartup" windows/hotkey.c; then
    pass "Uses WSAStartup"
else
    fail "Missing WSAStartup"
fi

if grep -qe "socket(" windows/hotkey.c; then
    pass "Creates socket"
else
    fail "Missing socket creation"
fi

if grep -qe "connect(" windows/hotkey.c; then
    pass "Connects to server"
else
    fail "Missing connect"
fi

if grep -qe "send(" windows/hotkey.c; then
    pass "Sends commands"
else
    fail "Missing send"
fi

# Test 6: Log buffer
echo ""
echo "Test 6: Log buffer"
if grep -qe "log_buffer_t" windows/hotkey.c; then
    pass "Has log_buffer_t"
else
    fail "Missing log_buffer_t"
fi

if grep -qe "log_add" windows/hotkey.c; then
    pass "Has log_add function"
else
    fail "Missing log_add"
fi

# Test 7: Status display
echo ""
echo "Test 7: Status display"
if grep -qe "STATE_RECORDING" windows/hotkey.c; then
    pass "Has STATE_RECORDING"
else
    fail "Missing STATE_RECORDING"
fi

if grep -qe "update_status" windows/hotkey.c; then
    pass "Has update_status function"
else
    fail "Missing update_status"
fi

# Test 8: Commands
echo ""
echo "Test 8: Commands"
if grep -qe "START:" windows/hotkey.c; then
    pass "Sends START command"
else
    fail "Missing START command"
fi

if grep -qe '"STOP"' windows/hotkey.c; then
    pass "Sends STOP command"
else
    fail "Missing STOP command"
fi

echo ""
echo "=== All tests passed ==="
