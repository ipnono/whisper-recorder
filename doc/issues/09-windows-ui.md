# Issue 9: Windows Floating UI

**Type:** AFK  
**Blocked by:** #2 (Windows Hotkey Binary)

## What to build

Create the floating status window with activity log and system tray integration.

**Window properties:**
- Size: 300x150 pixels (resizable)
- Always on top: `WS_EX_TOPMOST`
- Draggable title bar
- Background: Dark theme (#1e1e1e)
- Font: Consolas or system mono

**Layout:**
```
┌─────────────────────────────────┐
│ ● REC         session-2024-01-15 │  ← Header bar
│ 10:30:25      Duration: 00:15:32 │  ← Time info
├─────────────────────────────────┤
│ ├─ Session started              │
│ ├─ Speech detected              │  ← 5-line log
│ ├─ Transcription complete       │
│ ├─ File saved                   │
│ └─ Idle 45s                    │
└─────────────────────────────────┘
```

**Status indicators:**
- `● REC` (red #ff4444) - Recording
- `○ PAUSE` (yellow #ffcc00) - Paused
- `○ IDLE` (gray #888888) - Not recording

**Activity log:**
- Rolling buffer of 5 most recent events
- Auto-scroll to newest
- Color-coded by level:
  - INFO: White
  - WARN: Yellow
  - ERROR: Red

**System tray:**
- Icon: Microphone or app icon
- Tooltip: Shows current status
- Context menu:
  ```
  Show Window
  Start Recording
  Stop Recording
  ---
  Exit
  ```

**X button behavior:**
- `WM_CLOSE` → Hide window (not destroy)
- Show tray icon if not already visible

**Status updates:**
- Receive status via TCP from WSL
- Parse JSON: `{"state":"RECORDING","session":"name","started":"ts","events":["msg1","msg2"]}`

## Acceptance criteria

- [ ] Window is always-on-top
- [ ] Status indicator shows correct state
- [ ] Activity log updates in real-time
- [ ] X minimizes to tray
- [ ] Tray icon shows with context menu
- [ ] "Exit" from tray actually exits
- [ ] Window can be shown/hidden multiple times
