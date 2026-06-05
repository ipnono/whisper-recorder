# Whisper Recorder - Recorder (MinGW component)

This is the **recorder / transcription server**: a Windows .exe built with
MSYS2 + MinGW-w64 that listens on a TCP port and transcribes microphone audio
using whisper.cpp.

It is **not** a WSL component. Despite the previous name, this folder was
historically used as a MinGW cross-compile environment only.

## Build

All commands run from the **MSYS2 MinGW 64-bit** terminal.

```bash
# From this directory
make download-model   # one-time, ~150MB
make                  # builds libwhisper.a + whisper-recorder.exe
```

Or from the project root:

```bash
./build.sh
```

## Run

```bash
# List audio devices
./build/whisper-recorder.exe --list-devices

# Run with default settings
./build/whisper-recorder.exe

# Specify model and language
./build/whisper-recorder.exe -m third_party/whisper.cpp/models/ggml-base.bin -l zh

# Specify output directory
./build/whisper-recorder.exe -o D:/transcripts
```

## Options

```
-h, --help              Show help message
-v, --version           Show version
-m, --model <path>      Model file path
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
│ Windows UI   │ ─────────>  │ Recorder        │
│ hotkey.exe   │  localhost   │ whisper-recorder.exe │
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

## Source layout

```
mingw/
├── src/
│   ├── main.c          # entry point, TCP server, audio, VAD, transcription
│   ├── audio.c
│   ├── config.c
│   ├── download.c
│   ├── session.c
│   ├── transcribe.c
│   └── writer.c
├── third_party/
│   └── whisper.cpp/    # gitignored — populated by build.sh
├── build/              # gitignored — build output
└── Makefile
```
