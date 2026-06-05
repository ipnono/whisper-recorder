# Issue 2: Windows Hotkey Binary

**Type:** AFK  
**Blocked by:** #1 (Project Foundation)

## What to build

Create the Windows-side hotkey binary that registers Ctrl+Space and communicates with the WSL TCP server.

**Core functionality:**
- Register global hotkey `Ctrl+Space` using `RegisterHotKey()`
- Create TCP socket client (WinSock2)
- Connect to `127.0.0.1:8765`
- Send commands: `START:<name>`, `STOP`, `PAUSE`, `RESUME`

**Basic window:**
- WNDCLASSEX with `CS_HREDRAW | CS_VREDRAW`
- `CreateWindow()` with fixed size
- Message loop with `GetMessage()` / `DispatchMessage()`
- Close button (X) sends WM_CLOSE

**Command-line arguments:**
```
hotkey.exe [--host 127.0.0.1] [--port 8765]
```

**Stub UI elements (can be basic):**
- Static text for status
- Basic window that responds to resize

## Acceptance criteria

- [ ] `Ctrl+Space` triggers START/STOP toggle
- [ ] TCP connection established on startup
- [ ] Commands sent to server on hotkey press
- [ ] Window displays and responds to close
- [ ] Compiles with MinGW: `gcc hotkey.c -luser32 -lws2_32 -lwinmm -o hotkey.exe`
