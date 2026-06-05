# Whisper Recorder

Real-time voice transcription using whisper.cpp.

## Architecture

```
┌──────────────┐  TCP :8765  ┌─────────────────┐
│ Windows UI   │ ─────────>  │ WSL Server      │
│ hotkey.exe   │  localhost   │ whisper-recorder │
│ (Ctrl+Space) │              │ + Microphone     │
└──────────────┘              │ + Transcription │
                              │ + Markdown Out  │
                              └─────────────────┘
```

## Components

| Component | Path | Description |
|-----------|------|-------------|
| **WSL Server** | `wsl/build/whisper-recorder.exe` | Main transcription server |
| **Windows UI** | `windows/build/hotkey.exe` | Global hotkey launcher |
| **Model** | `wsl/third_party/whisper.cpp/models/ggml-base.bin` | Whisper base model (148MB) |

## Quick Start

### 1. Build

```bash
# Build whisper.cpp
cd wsl
make cmake
make build-whisper

# Download model
make download-model

# Build application
make
```

### 2. Run Server

```bash
cd wsl
./build/whisper-recorder.exe -m third_party/whisper.cpp/models/ggml-base.bin
```

### 3. Test with PowerShell

```powershell
$tcp = New-Object System.Net.Sockets.TcpClient
$tcp.Connect("127.0.0.1", 8765)
$stream = $tcp.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$reader = New-Object System.IO.StreamReader($stream)

$writer.WriteLine("STATUS")
$writer.Flush()
Write-Host $reader.ReadLine()

$writer.WriteLine("START:test")
$writer.Flush()
Write-Host $reader.ReadLine()

$writer.WriteLine("STOP")
$writer.Flush()

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
    └── session-09-15-30.md
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

## Audio Devices

```bash
./build/whisper-recorder.exe --list-devices
```

## Project Structure

```
wisper/
├── wsl/                    # WSL Server
│   ├── build/
│   │   └── whisper-recorder.exe
│   ├── src/
│   │   └── main.c          # Main server
│   ├── third_party/
│   │   └── whisper.cpp/    # whisper.cpp + models
│   └── Makefile
├── windows/                 # Windows UI
│   ├── build/
│   │   └── hotkey.exe
│   └── hotkey.c
└── doc/                    # Documentation
    ├── PRD.md
    └── issues/
```

## Status

- [x] WSL Server - Working
- [x] TCP Commands - Working
- [x] Audio Capture - Working (WinMM)
- [x] Whisper Model - Working
- [x] Session Management - Working
- [x] Markdown Output - Working
- [ ] Windows UI (hotkey.exe) - Not built
