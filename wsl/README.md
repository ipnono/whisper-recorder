# Whisper Recorder - WSL Component

WSL-side application for real-time voice transcription using whisper.cpp.

## Build

```bash
# Build whisper.cpp first
make cmake
make build-whisper

# Download model
make download-model

# Build application
make
```

## Run

```bash
# List audio devices
./build/whisper-recorder --list-devices

# Run with default settings
./build/whisper-recorder

# Specify model and language
./build/whisper-recorder -m models/ggml-base.bin -l zh

# Specify output directory
./build/whisper-recorder -o D:/transcripts
```

## Options

```
-h, --help              Show help message
-v, --version           Show version
-m, --model <path>      Model file path (default: third_party/whisper.cpp/models/ggml-base.bin)
-o, --output <path>     Output directory (default: D:/recordings)
-l, --language <lang>   Language code: en, zh, etc. (default: zh)
-p, --port <port>       TCP server port (default: 8765)
-d, --device <id>       Audio input device ID (default: -1 = default)
--list-devices          List available audio input devices
```

## TCP Commands

Connect to `127.0.0.1:8765` and send commands:

```
START:<name>    Start recording session (name optional)
STOP            Stop recording
PAUSE           Pause recording
RESUME          Resume recording
STATUS          Get current status (JSON)
```

## Architecture

```
┌──────────────┐  TCP :8765  ┌─────────────────┐
│ Windows UI   │ ─────────>  │ WSL Server      │
│ hotkey.exe   │  localhost   │ whisper-recorder │
│ (Ctrl+Space) │              │ (Transcription) │
└──────────────┘              └─────────────────┘
```

## Output

Sessions are saved as Markdown files:
```
D:/recordings/
├── 2026-06-05/
│   └── session-10-30-00.md
└── 2026-06-06/
    └── session-09-15-30.md
```

## Test

```bash
make test
```
