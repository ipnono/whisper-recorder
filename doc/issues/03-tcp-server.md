# Issue 3: WSL TCP Server

**Type:** AFK  
**Blocked by:** #1 (Project Foundation)

## What to build

Create the TCP server that receives commands from the Windows binary and manages recording state.

**TCP Server:**
- Bind to `127.0.0.1:8765` (configurable via `--port`)
- Listen with `listen()` backlog of 1
- Accept single client connection
- Non-blocking with `select()` or poll for readability

**Command protocol (newline-terminated):**
```
START:<session_name>
STOP
PAUSE
RESUME
STATUS
SET_LANG:<en|zh>
```

**State machine:**
```
IDLE → RECORDING → PAUSED → RECORDING → IDLE
  ↑____________________________________________|
```

**Command handlers:**
- `START` - Transition to RECORDING, emit "session_started" event
- `STOP` - Transition to IDLE, emit "session_ended" event
- `PAUSE` - Transition to PAUSED
- `RESUME` - Transition to RECORDING
- `STATUS` - Send JSON response: `{"state":"RECORDING","session":"name","started":"ts"}`

**Event system (for other modules):**
```c
typedef void (*event_callback_t)(const char *event, const char *data);
void events_on(event_callback_t cb);
void events_emit(const char *event, const char *data);
```

## Acceptance criteria

- [ ] Server accepts connections on port 8765
- [ ] Commands parsed correctly
- [ ] State transitions work
- [ ] STATUS returns valid JSON
- [ ] Multiple connect/disconnect cycles handled
- [ ] Compiles: `gcc main.c -o server -lpthread`
