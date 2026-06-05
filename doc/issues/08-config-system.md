# Issue 8: Config System

**Type:** AFK  
**Blocked by:** #1 (Project Foundation)

## What to build

Load configuration from `config.json` and allow CLI argument overrides.

**Config structure:**
```json
{
  "model": {
    "path": "~/.whisper/models/ggml-base.bin",
    "auto_download": true
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

**CLI arguments:**
| Argument | Description |
|----------|-------------|
| `-c, --config <path>` | Config file path |
| `-m, --model <path>` | Model file path |
| `-o, --output <path>` | Output directory |
| `-l, --language <lang>` | Language (en/zh) |
| `-p, --port <port>` | TCP server port |
| `-v, --verbose` | Verbose logging |
| `-d, --daemon` | Run as daemon |

**Config loading:**
1. Load `config.json` from default location (`.` or `~/.config/whisper-recorder/`)
2. Override with `--config <path>` if provided
3. Override individual values with CLI args

**No file = use defaults** (don't error on missing config)

## Acceptance criteria

- [ ] `config.json` loaded on startup
- [ ] CLI args override config values
- [ ] Missing config file uses defaults
- [ ] Invalid config shows warning, uses defaults
- [ ] `--help` shows all options with current values
- [ ] Path `~` expansion works for model path
