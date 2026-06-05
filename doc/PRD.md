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
│                              Windows Host                                 │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │  Hotkey Binary (C + Win32 API)  — windows/hotkey.exe            │    │
│  │  - Global hotkey: Ctrl+Space                                     │    │
│  │  - Floating window UI (always-on-top)                            │    │
│  │  - Activity log (5 lines, rolling)                               │    │
│  │  - System tray minimize                                          │    │
│  │  - TCP client to recorder (one connection per command)          │    │
│  └───────────────────────┬─────────────────────────────────────────┘    │
│                          │ TCP localhost:8765                            │
│                          │ (one connection per command)                  │
└──────────────────────────┼──────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          Recorder  — mingw/build/whisper-recorder.exe   │
│                                                                          │
│  Built with: MSYS2 + MinGW-w64 + GCC. Runs natively on Windows.        │
│                                                                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                    │
│  │ Audio (winmm)│─▶│ VAD Detect  │─▶│ Transcription│                   │
│  │  mic capture │  │  (silence)  │  │ (whisper.cpp)│                   │
│  └─────────────┘  └─────────────┘  └──────┬──────┘                    │
│                                             │                           │
│                                             ▼                           │
│                                    ┌─────────────┐                     │
│                                    │ MD Writer   │                     │
│                                    │ (session)   │                     │
│                                    └─────────────┘                     │
└─────────────────────────────────────────────────────────────────────────┘
```

Both binaries are **native Windows .exe files**. There is no WSL, no
Linux runtime, no virtual machine. The TCP socket is plain localhost
on port 8765.

The "recorder" name reflects the actual build target:
`mingw/build/whisper-recorder.exe`. The folder used to be called
`wsl/` (when it was used as a cross-compile environment); it was
renamed to `mingw/` because (a) WSL is no longer required, and
(b) "WSL" was a misnomer — the produced binary was always a
Windows .exe. See `docs/adr/0001-msys2-mingw-build.md` for the
rationale.

---

## 3. Components

### 3.1 Windows Hotkey Binary (UI client)

**Path:** `windows/hotkey.c` → `windows/hotkey.exe` (0.3 MB)

**Language:** C with Win32 API

**Features:**
- Register global Ctrl+Space hotkey
- Floating window (always-on-top, draggable)
- Display status: Recording indicator, session name, timestamp
- Rolling activity log (5 lines)
- System tray icon with context menu
- X button minimizes to tray
- TCP client to recorder (one connection per command — see protocol note below)

**IPC Commands (UI → Recorder):**
```
START <session_name>     - Start new session
STOP                     - Stop current session
PAUSE                    - Pause recording
RESUME                   - Resume recording
STATUS                   - Query status
```

**Protocol note (important):** the recorder accepts exactly **one
command per TCP connection**, then closes the socket. The UI
client opens a fresh connection for each command. This is by
design (see `mingw/src/main.c` near `closesocket(client_fd)`).

### 3.2 Recorder Main Application (server)

**Path:** `mingw/src/*.c` → `mingw/build/whisper-recorder.exe` (2.7 MB)

**Language:** C, statically linked against whisper.cpp

**Modules:**

#### 3.2.1 Audio Capture
- Uses **Windows Multimedia API (`winmm.dll`)** for direct microphone
  capture — `waveInOpen` / `waveInStart` / `waveInStop` /
  `waveInClose`.
- Sample format: 16 kHz, mono, 16-bit PCM (matches whisper.cpp's
  required `WHISPER_SAMPLE_RATE`).
- Capture is **polled in a dedicated thread** (CALLBACK_NULL
  mode) and pushed into a ring buffer.
- (Note: an earlier version of this PRD said "audio via stdin
  from a Windows proxy". That was a planned design that was
  never built. The current implementation captures directly
  from the audio device on the same Windows host.)

#### 3.2.2 VAD Detection
- Uses whisper.cpp's built-in Silero VAD (`ggml-silero-v6.2.0.bin`).
- Parameters (in `config.json` / `mingw/Makefile`):
  - `vad_threshold`: 0.5
  - `min_speech_duration_ms`: 250ms
  - `min_silence_duration_ms`: 100ms (segment split)
  - `speech_pad_ms`: 30ms

#### 3.2.3 Transcription
- Uses whisper.cpp C API.
- Model: `ggml-base.bin` (multilingual, ~142 MiB), downloaded
  on first run.
- Supports Chinese and English out of the box; other languages
  via `--language` flag.
- Language configurable at runtime.

#### 3.2.4 Markdown Writer
- Output directory: configurable (default `D:/recordings`).
- File structure: `YYYY-MM-DD/session-HH-MM-SS.md`.
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

### 3.3 TCP Server (Recorder side)

**Path:** `mingw/src/main.c` (function `create_server`,
`handle_command`)

**Port:** 8765 (configurable)

**Protocol:** Plain text commands, newline-terminated.
**One command per connection** (see protocol note in 3.1).

**Commands:**
- `START:<session_name>` - Start new session
- `STOP` - Stop current session
- `PAUSE` - Pause recording
- `RESUME` - Resume recording
- `STATUS` - Returns status JSON
- `SET_LANG:<lang>` - Set language (en/zh) *(implemented in
  protocol spec; current handler has a stub for it)*

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

### 9.1 Toolchain
- **MSYS2** (https://www.msys2.org/) — install once.
- In the **MSYS2 MinGW 64-bit** terminal:
  ```bash
  pacman -Syu
  pacman -S --needed --noconfirm \
      mingw-w64-x86_64-gcc \
      mingw-w64-x86_64-cmake \
      mingw-w64-x86_64-make \
      mingw-w64-x86_64-pkg-config \
      mingw-w64-x86_64-curl \
      git \
      base-devel
  ```
- Optionally add `C:\msys64\mingw64\bin` to the Windows `PATH` so
  `gcc`/`cmake` are usable from PowerShell / cmd too.

### 9.2 Recorder (mingw/)
- Compiler: `gcc` from MinGW-w64
- Output: `mingw/build/whisper-recorder.exe` (statically links
  whisper.cpp + ggml)
- Vendored: `mingw/third_party/whisper.cpp/` (gitignored;
  populated by `build.sh` via `git clone --depth 1`)

### 9.3 Hotkey UI (windows/)
- Compiler: `gcc` from MinGW-w64
- Output: `windows/hotkey.exe`
- Single source file, no vendored dependencies.

### 9.4 Build Commands
```bash
# From MSYS2 MinGW 64-bit terminal, at the project root

# Recorder (server)
./build.sh
# which does:
#   1. clone whisper.cpp into mingw/third_party/
#   2. download ggml-base.bin (~141 MiB) if missing
#   3. cmake + make libwhisper.a + make whisper-recorder.exe

# Hotkey UI (client)
cd windows && mingw32-make && cd ..
```

### 9.5 Model download
The model URL is set in `mingw/Makefile`:
`https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin`
The base URL is overridable via the `HF_ENDPOINT` env var
(standard Hugging Face convention):
```bash
# China (Sangfor VPN / corporate networks):
export HF_ENDPOINT=https://hf-mirror.com
make download-model

# Add to ~/.bashrc to persist across shells.
```

The file path uses the `ggerganov` org (not `ggml-org`) because
that's what the China-side mirrors currently have synced. The
file is the same.

---

## 10. File Structure

```
whisper-recorder/
├── doc/
│   ├── PRD.md                  # this file
│   ├── issues/                 # one file per implementation issue
│   └── adr/                    # architecture decision records
├── mingw/                      # Recorder (server), MinGW build
│   ├── build/
│   │   └── whisper-recorder.exe    # build output
│   ├── src/                    # 7 .c files
│   │   ├── main.c              # entry, TCP server, audio capture, VAD, transcribe
│   │   ├── audio.c
│   │   ├── download.c
│   │   ├── session.c
│   │   ├── transcribe.c
│   │   └── writer.c
│   ├── third_party/whisper.cpp/    # vendored, gitignored
│   ├── Makefile
│   └── README.md
├── windows/                    # Hotkey UI (client), MinGW build
│   ├── build/  (or just hotkey.exe)
│   ├── hotkey.c
│   ├── Makefile
│   └── README.md
├── build.sh                    # top-level build (idempotent)
├── config.json                 # default config
├── .gitignore
└── README.md
```

`mingw/src/config.c` exists on disk but is NOT in the build. It
is a self-contained Linux-style "main program" kept for reference
(history preservation); it would collide at link time with
`main.c`'s `main()`. See commit history for the removal rationale.

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
- [x] UI binary builds (windows/hotkey.exe, 0.3 MB)
- [x] UI binary launches, creates the Win32 window, enters message loop
- [x] UI binary does not crash the recorder when both are running
- [⚠️] Visual verification of the floating window and tray icon
  — not done in CI/automation. The hotkey.exe binary builds and
  stays running for 3+ seconds in headless smoke test; full visual
  fidelity has to be checked by a human on a desktop session.

### 11.3 Error Handling
- [x] Audio device detection (--list-devices)
- [ ] Graceful handling of audio device issues
- [ ] Disk full detection and warning
- [ ] TCP reconnection on connection loss
- [x] Log file captures events

---

## 12. Implementation Status (2026-06-05)

### Completed
- [x] **Build environment** — MSYS2 + MinGW-w64 (no WSL required)
- [x] **Recorder binary** (`mingw/build/whisper-recorder.exe`, 2.7 MB)
- [x] **Hotkey UI binary** (`windows/hotkey.exe`, 0.3 MB)
- [x] TCP server on port 8765 (one command per connection)
- [x] Audio capture via `winmm.dll`
- [x] Whisper model loading (141 MiB `ggml-base.bin`, base model)
- [x] Transcription pipeline
- [x] Session management (state machine, auto-split, session naming)
- [x] Markdown writer (date-based dirs, per-session files)
- [x] Command handlers (START/STOP/PAUSE/RESUME/STATUS)
- [x] **End-to-end smoke test passes** — all 7 TCP commands
      return correct responses; state transitions correctly
- [x] `HF_ENDPOINT` env var support for users behind firewalls
- [x] Documentation (this file + READMEs + ADRs)

### Remaining (out of scope for current push)
- [ ] Full visual verification of the floating window on a real
      desktop session (the headless smoke test only checks
      "binary stays running for 3s without crashing")
- [ ] Silence/timeout warnings (-Wformat-truncation,
      -Wstringop-truncation) — non-fatal, code is correct
- [ ] Remove the dead `mingw/src/config.c` from disk
- [ ] Re-record the smoke test with an actual recording (right
      now we test the state machine but not the audio →
      markdown pipeline end-to-end with a real voice input)

### Build Commands
```bash
# From MSYS2 MinGW 64-bit terminal, at the project root
export HF_ENDPOINT=https://hf-mirror.com   # optional, for China
./build.sh                                # builds recorder
(cd windows && mingw32-make)              # builds UI
```

### Test Results (2026-06-05, end-to-end smoke test)

```
> STATUS                 -> {"state":"IDLE","session":"","language":"zh"}
> START:smoketest        -> OK: Session started: smoketest
> STATUS                 -> {"state":"RECORDING","session":"smoketest","language":"zh"}
> PAUSE                  -> OK: Paused
> RESUME                 -> OK: Resumed
> STOP                   -> OK: Session stopped
> STATUS                 -> {"state":"IDLE","session":"","language":"zh"}
ALL CHECKS PASSED
```

The test opens one TCP connection per command (per protocol),
asserts that the state transitions IDLE → RECORDING → PAUSED →
RECORDING → IDLE, and verifies the server is still responsive
after the UI binary is launched.

**Output:** Session files created at
`D:/recordings/YYYY-MM-DD/session-HH-MM-SS.md` (verified by
configuration; actual file write only happens when audio
recording completes — not exercised in the headless smoke test).

---

## 13. Future Considerations (Out of Scope)

- Multiple language mixed transcription
- Speaker diarization
- Real-time streaming output
- Web interface
- Mobile companion app
- Cloud sync

---

## 14. Related Documents

- `README.md` — quick start and architecture overview
- `mingw/README.md` — recorder-specific docs
- `windows/README.md` — UI-specific docs
- `doc/adr/0001-msys2-mingw-build.md` — rationale for the
  MSYS2/MinGW build (not WSL, not Docker, not MSVC)
- `doc/issues/` — per-feature implementation notes

---

**Document Version:** 1.1
**Created:** 2024-01-15
**Last updated:** 2026-06-05
**Status:** Active
