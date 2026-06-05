# Whisper Recorder

Real-time voice transcription using whisper.cpp.

## Architecture

```
┌──────────────┐  TCP :8765  ┌─────────────────┐
│ Windows UI   │ ─────────>  │ Recorder        │
│ hotkey.exe   │  localhost   │ whisper-recorder.exe │
│ (Ctrl+Space) │              │ + Microphone     │
└──────────────┘              │ + Transcription  │
                               │ + Markdown Out  │
                               └─────────────────┘
```

Both binaries are **native Windows .exe files**. The TCP socket is `localhost:8765`.

The recorder binary (`whisper-recorder.exe`) is built on Windows with **MSYS2 + MinGW-w64**
(no WSL required). The UI binary (`hotkey.exe`) is built with MinGW-w64 / gcc.

## Components

| Component | Path | Built with |
|-----------|------|------------|
| **Recorder** | `mingw/build/whisper-recorder.exe` | gcc + MinGW-w64 (MSYS2) |
| **Windows UI** | `windows/build/hotkey.exe` | gcc + MinGW-w64 |
| **Model** | `mingw/third_party/whisper.cpp/models/ggml-base.bin` | downloaded |

## Prerequisites

Install **MSYS2** once: <https://www.msys2.org/>. Then in the **MSYS2 MinGW 64-bit**
terminal:

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

## Build

From the project root, in the **MSYS2 MinGW 64-bit** terminal:

```bash
./build.sh
```

This will (idempotently):
1. Clone whisper.cpp into `mingw/third_party/whisper.cpp/`
2. Download `ggml-base.bin` (~150MB) on first run
3. Build `mingw/build/whisper-recorder.exe`

## Run

```bash
# from project root, in MSYS2 MinGW 64-bit
mingw/build/whisper-recorder.exe -m mingw/third_party/whisper.cpp/models/ggml-base.bin
```

## Test with PowerShell

```powershell
$tcp = New-Object System.Net.Sockets.TcpClient
$tcp.Connect("127.0.0.1", 8765)
$stream = $tcp.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$reader = New-Object System.IO.StreamReader($stream)

$writer.WriteLine("STATUS");  $writer.Flush(); Write-Host $reader.ReadLine()
$writer.WriteLine("START:test"); $writer.Flush(); Write-Host $reader.ReadLine()
$writer.WriteLine("STOP");    $writer.Flush()
$tcp.Close()
```

## TCP Commands

| Command | Description |
|---------|-------------|
| `START:<name>` | Start recording session |
| `STOP` | Stop recording |
| `PAUSE` | Pause recording |
| `RESUME` | Resume recording |
| `STATUS` | Get status (JSON) |

## Output

Sessions are saved as Markdown files:

```
D:/recordings/
├── 2026-06-05/
│   └── session-10-30-00.md
└── 2026-06-06/
│   └── session-09-15-30.md
```

## Options

```
-h, --help              Show help
-v, --version           Show version
-m, --model <path>     Model file path
-o, --output <path>    Output directory (default: D:/recordings)
-l, --language <lang>   Language (en/zh)
-p, --port <port>      TCP port (default: 8765)
-d, --device <id>      Audio device ID
--list-devices         List audio devices
```

## Project Structure

```
whisper-recorder/
├── mingw/                          # Recorder server (MinGW build)
│   ├── build/
│   │   └── whisper-recorder.exe    # build output
│   ├── src/                        # C source
│   ├── third_party/whisper.cpp/    # vendored, gitignored
│   ├── Makefile
│   └── README.md
├── windows/                        # Windows UI
│   ├── build/
│   │   └── hotkey.exe              # build output
│   └── hotkey.c
├── config.json
├── build.sh                        # top-level build script
└── README.md
```

## Status

- [x] Recorder server - Working
- [x] TCP commands - Working
- [x] Audio capture - Working (WinMM)
- [x] Whisper model - Working
- [x] Session management - Working
- [x] Markdown output - Working
- [ ] Windows UI (hotkey.exe) - Not built
