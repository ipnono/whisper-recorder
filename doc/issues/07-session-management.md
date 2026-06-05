# Issue 7: Session Management

**Type:** AFK  
**Blocked by:** #6 (Markdown Writer)

## What to build

Implement automatic session splitting based on silence timeout and max duration.

**Session split conditions:**

1. **Silence timeout** (default: 60 seconds)
   - Track last speech timestamp
   - If silence > 60s and session has content:
     - Emit `session_ended`
     - Emit `session_started` with new session name
   - Increment session counter: `session-2024-01-15-10-30`, `session-2024-01-15-10-31`, etc.

2. **Max duration** (default: 10 minutes / 600 seconds)
   - Track session start time
   - If duration > 10min:
     - Same as silence timeout

**Session naming:**
```
session-{YYYY}-{MM}-{DD}-{HH}-{MM}-{SS}
session-2024-01-15-10-30-00
```

**State management:**
- Current session start time
- Current session file path
- Segment count for current session
- Last speech timestamp

**Edge cases:**
- Empty session (< 1 segment): Delete file on split
- Single segment: Keep as-is
- Rapid speech/silence toggles: Debounce with 500ms threshold

## Acceptance criteria

- [ ] New session created after 60s silence
- [ ] New session created after 10min duration
- [ ] Session counter increments correctly
- [ ] Empty sessions cleaned up
- [ ] Config values respected (`session_silence_timeout_ms`, `max_session_duration_ms`)
- [ ] No corrupted files on split
