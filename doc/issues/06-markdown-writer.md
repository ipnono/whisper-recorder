# Issue 6: Markdown Writer

**Type:** AFK  
**Blocked by:** #5 (Whisper Transcription)

## What to build

Write transcribed text to Markdown files organized by session and date.

**File structure:**
```
{output_dir}/
└── YYYY-MM-DD/
    └── session-HH-MM-SS.md
```

**Markdown template:**
```markdown
# Session: session-2024-01-15-10-30

**Started:** 2024-01-15 10:30:00  
**Language:** zh  
**Duration:** 00:00:00

---

{transcribed text appended here}

---

**Ended:** 2024-01-15 10:45:32
**Transcribed segments:** 12
```

**Operations:**
- `session_started`: Create new file with header
- `transcription`: Append segment text
- `session_ended`: Update duration and segment count, close file
- Handle file creation errors gracefully

**File locking:**
- Use `flock()` to prevent concurrent writes
- Ensure atomic writes

**Directory creation:**
- Create `YYYY-MM-DD/` if not exists
- Handle permission errors

## Acceptance criteria

- [ ] Files created in correct directory structure
- [ ] Header written on session start
- [ ] Transcriptions appended in real-time
- [ ] Footer written on session end
- [ ] Concurrent sessions don't corrupt files
- [ ] Invalid paths handled gracefully
