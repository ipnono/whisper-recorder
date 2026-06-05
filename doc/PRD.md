# Whisper Recorder - Product Requirements Document

## 1. Overview

**Project Name:** Whisper Recorder

**Type:** Real-time voice transcription tool

**Core Functionality:** Listen to microphone input, detect speech segments using VAD, transcribe audio to text using whisper.cpp, and save results as Markdown files organized by session.

**Target Users:** Users who need to capture spoken content (meetings, notes, lectures) into text files, with focus on Chinese and English speech.

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              Windows Side                                 │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │  Hotkey Binary (C + Win32 API)                                  │    │
│  │  - Global hotkey: Ctrl+Space                                     │    │
│  │  - Floating window UI (always-on-top)                            │    │
│  │  - Activity log (5 lines, rolling)                               │    │
│  │  - System tray minimize                                          │    │
│  └───────────────────────┬─────────────────────────────────────────┘    │
│                          │ TCP (localhost)                               │
└──────────────────────────┼──────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                               WSL Side                                   │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │  Main Application (C)                                           │    │
│  │                                                                   │    │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │    │
│  │  │ Audio Proxy │─▶│ VAD Detect  │─▶│ Transcription│              │    │
│  │  │  (stdin)    │  │  (silence)  │  │ (whisper.cpp)│              │    │
│  │  └─────────────┘  └─────────────┘  └──────┬──────┘              │    │
│  │                                             │                      │    │
│  │                                             ▼                      │    │
│  │                                    ┌─────────────┐                │    │
│  │                                    │ MD Writer   │                │    │
│  │                                    │ (session)   │                │    │
│  │                                    └─────────────┘                │    │
│  └─────────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Components

### 3.1 Windows Hotkey Binary

**Language:** C with Win32 API

**Features:**
- Register global Ctrl+Space hotkey
- Floating window (always-on-top, draggable)
- Display status: Recording indicator, session name, timestamp
- Rolling activity log (5 lines)
- System tray icon with context menu
- X button minimizes to tray
- TCP client to send commands to WSL app

**IPC Commands (Windows → WSL):**
```
START <session_name>     - Start new session
STOP                     - Stop current session
PAUSE                    - Pause recording
RESUME                   - Resume recording
STATUS                   - Query status
```

### 3.2 WSL Main Application

**Language:** C

**Modules:**

#### 3.2.1 Audio Proxy
- Receives audio via stdin from Windows proxy
- Converts to 16kHz mono 16-bit PCM
- Feeds audio buffer to VAD module

#### 3.2.2 VAD Detection
- Uses whisper.cpp built-in VAD
- Parameters:
  - `vad_threshold`: 0.5
  - `min_speech_duration_ms`: 250ms
  - `min_silence_duration_ms`: 100ms (segment split)
  - `speech_pad_ms`: 30ms

#### 3.2.3 Transcription
- Uses whisper.cpp C API
- Model: Auto-download `base.bin` (multilingual) on first run
- Supports Chinese and English
- Language configurable at runtime

#### 3.2.4 Markdown Writer
- Output directory: `configurable/recordings/`
- File structure: `YYYY-MM-DD/session-HH-MM-SS.md`
- Format:
```markdown
# Session: session-2024-01-15-10-30

**Started:** 2024-01-15 10:30:00  
**Language:** zh  
**Duration:** 00:15:32

---

Hello, this is a test recording.
Another sentence here.

---

**Ended:** 2024-01-15 10:45:32
**Transcribed segments:** 12
```

### 3.3 TCP Server (WSL Side)

**Port:** 8765 (configurable)

**Protocol:** Plain text commands, newline-terminated

**Commands:**
- `START:<session_name>` - Start new session
- `STOP` - Stop current session
- `PAUSE` - Pause recording
- `RESUME` - Resume recording
- `STATUS` - Returns status JSON
- `SET_LANG:<lang>` - Set language (en/zh)

---

## 4. Session Management

### 4.1 Session Naming
Auto-generated format: `session-YYYY-MM-DD-HH-MM-SS`

### 4.2 Session Split Conditions
| Condition | Behavior |
|-----------|----------|
| Silence > 60s | Auto-split, create new session file |
| Duration > 10 minutes | Auto-split, create new session file |
| Manual STOP | End session |

### 4.3 Session States
```
IDLE → RECORDING → PAUSED → RECORDING → STOPPED
  ↑_______________________________________________|
```

---

## 5. Configuration

### 5.1 Config File (`config.json`)

```json
{
  "model": {
    "path": "~/.whisper/models/ggml-base.bin",
    "auto_download": true,
    "download_url": "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin"
  },
  "audio": {
    "sample_rate": 16000,
    "channels": 1,
    "bit_depth": 16
  },
  "vad": {
    "threshold": 0.5,
    "min_speech_duration_ms": 250,
    "min_silence_duration_ms": 100,
    "speech_pad_ms": 30,
    "session_silence_timeout_ms": 60000,
    "max_session_duration_ms": 600000
  },
  "output": {
    "base_dir": "D:/recordings",
    "create_date_dirs": true
  },
  "server": {
    "host": "127.0.0.1",
    "port": 8765
  },
  "language": "zh",
  "log": {
    "level": "INFO",
    "file": "whisper-recorder.log"
  }
}
```

### 5.2 CLI Arguments (Override Config)

| Argument | Description |
|----------|-------------|
| `-c, --config <path>` | Config file path |
| `-m, --model <path>` | Model file path |
| `-o, --output <path>` | Output directory |
| `-l, --language <lang>` | Language (en/zh) |
| `-p, --port <port>` | TCP server port |
| `-v, --verbose` | Verbose logging |
| `-d, --daemon` | Run as daemon |

---

## 6. User Interface

### 6.1 Floating Window

**Window Properties:**
- Always on top
- Draggable
- Default size: 300x150 pixels
- Starts visible, X minimizes to tray

**Layout:**
```
┌─────────────────────────────────┐
│ ● REC         session-2024-01-15 │  ← Header (status + session name)
│ 10:30:25      10:45:32          │  ← Current time + session duration
├─────────────────────────────────┤
│ ├─ Session started              │
│ ├─ Speech detected              │  ← Activity log (5 lines)
│ ├─ Transcription complete       │
│ ├─ File saved: session-...      │
│ └─ Idle 45s                    │
└─────────────────────────────────┘
```

**Status Indicators:**
- ● REC (red) - Recording active
- ○ PAUSE (yellow) - Paused
- ○ IDLE (gray) - Not recording

### 6.2 System Tray

**Icon:** Microphone icon

**Context Menu:**
- Show Window
- Start Recording
- Stop Recording
- ---
- Exit

---

## 7. Logging

### 7.1 Log Format
```
[2024-01-15 10:30:05] INFO: Session started: session-2024-01-15-10-30
[2024-01-15 10:30:15] INFO: VAD: speech detected
[2024-01-15 10:30:25] INFO: Transcription complete (123ms)
[2024-01-15 10:30:25] INFO: File saved: D:/recordings/2024-01-15/session-10-30-25.md
[2024-01-15 10:31:45] WARN: VAD: silence timeout, session split pending
```

### 7.2 Log Levels
- DEBUG - Detailed debug info
- INFO - Normal operation
- WARN - Recoverable issues
- ERROR - Errors requiring attention

---

## 8. Error Handling

| Scenario | Behavior |
|----------|----------|
| No speech detected | Warn after 30s, continue listening |
| Very long speech (>10min) | Split into new session |
| Audio device disconnect | Retry 3x, then show error, pause |
| Disk full | Warn, stop recording, save buffer |
| Empty transcription | Skip file creation |
| Model not found | Auto-download, or exit with error |
| TCP connection lost | Reconnect, queue commands |

---

## 9. Build & Dependencies

### 9.1 Windows Binary
- Compiler: MSVC or MinGW-w64
- Dependencies: Win32 API (built-in)
- Output: Single .exe file

### 9.2 WSL Application
- Compiler: GCC
- Dependencies:
  - whisper.cpp (included as submodule or vendored)
  - POSIX sockets (built-in)
- Output: Single binary

### 9.3 Build Commands
```bash
# WSL
cd wsl
mkdir -p build && cd build
cmake .. && make

# Windows (MinGW-w64)
cd windows
gcc -o hotkey.exe hotkey.c -luser32 -lgdi32 -lws2_32 -lwinmm
```

---

## 10. File Structure

```
whisper-recorder/
├── doc/
│   └── PRD.md
├── windows/
│   ├── hotkey.c           # Windows hotkey + UI
│   ├── Makefile
│   └── README.md
├── wsl/
│   ├── src/
│   │   ├── main.c         # Entry point, TCP server
│   │   ├── audio.c        # Audio buffer management
│   │   ├── vad.c          # VAD integration
│   │   ├── transcribe.c   # Whisper transcription
│   │   ├── writer.c       # Markdown output
│   │   └── config.c       # Config loading
│   ├── include/
│   │   └── whisper.h      # From whisper.cpp
│   ├── third_party/
│   │   └── whisper.cpp    # Vendored whisper
│   ├── Makefile
│   └── README.md
├── config.json            # Default config
├── .gitignore
└── README.md
```

---

## 11. Acceptance Criteria

### 11.1 Functional Requirements
- [x] TCP server accepts commands (START/STOP/PAUSE/RESUME/STATUS)
- [x] Audio is captured at 16kHz mono 16-bit (winmm)
- [x] VAD detects speech segments (energy-based fallback)
- [x] Chinese and English transcription works (whisper.cpp)
- [x] Sessions auto-split on silence
- [x] Sessions auto-split after duration limit
- [x] Markdown files are created in correct structure
- [x] CLI args override config defaults
- [x] Model loads successfully (ggml-base.bin)

### 11.2 UI Requirements
- [x] TCP server accepts commands from any client
- [ ] Floating window shows current status (not implemented)
- [ ] Activity log updates in real-time (not implemented)
- [ ] Window is always-on-top (not implemented)
- [ ] X button minimizes to tray (not implemented)
- [ ] System tray icon has working context menu (not implemented)

### 11.3 Error Handling
- [x] Audio device detection (--list-devices)
- [ ] Graceful handling of audio device issues
- [ ] Disk full detection and warning
- [ ] TCP reconnection on connection loss
- [x] Log file captures events

---

## 12. Implementation Status (2026-06-05)

### Completed
- [x] WSL Server binary (`whisper-recorder.exe`)
- [x] TCP server on port 8765
- [x] Audio capture via winmm.dll
- [x] Whisper model loading (148MB ggml-base.bin)
- [x] Transcription pipeline
- [x] Session management
- [x] Markdown writer
- [x] Command handlers (START/STOP/PAUSE/RESUME/STATUS)

### In Progress
- [ ] Windows UI (floating window, hotkey)
- [ ] Full VAD integration (using energy fallback)

### Remaining
- [ ] Windows UI (floating window, hotkey)
- [ ] Global hotkey integration
- [ ] Real-time transcription output
- [ ] Error recovery

### Build Commands
```bash
# Build everything
cd wsl
make cmake
make build-whisper
make download-model
make

# Run
./build/whisper-recorder.exe -m third_party/whisper.cpp/models/ggml-base.bin
```

### Test Results (2026-06-05)
```
STATUS → {"state":"IDLE","session":"","language":"zh"} ✅
START:final → OK: Session started ✅
STATUS → {"state":"RECORDING","session":"final","language":"zh"} ✅
STOP → OK: Session stopped ✅
STATUS → {"state":"IDLE","session":"","language":"zh"} ✅
```

**Output:** Session files created at `D:/recordings/YYYY-MM-DD/session-HH-MM-SS.md`

---

## 12. Future Considerations (Out of Scope)

- Multiple language mixed transcription
- Speaker diarization
- Real-time streaming output
- Web interface
- Mobile companion app
- Cloud sync

---

**Document Version:** 1.0  
**Created:** 2024-01-15  
**Status:** Approved for implementation
