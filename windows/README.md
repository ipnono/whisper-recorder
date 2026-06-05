# Whisper Recorder - Windows UI

## Features

- **Floating status window** (always-on-top)
- **Activity log** (5 lines)
- **System tray** with context menu
- **Global hotkey** (Ctrl+Space)
- **TCP client** for WSL communication

## Build

```bash
mingw32-make
# or
gcc hotkey.c -o hotkey.exe -luser32 -lws2_32 -lwinmm -lgdi32 -lshell32 -lcomctl32
```

## Usage

```bash
./hotkey.exe --host 127.0.0.1 --port 8765
```

## Controls

- **Ctrl+Space**: Toggle recording
- **X button**: Minimize to tray
- **Tray icon**: Right-click for menu

## Tray Menu

- Show Window
- Start Recording
- Stop Recording
- Exit
