# Issue 4: Audio + VAD Pipeline

**Type:** AFK  
**Blocked by:** #3 (WSL TCP Server)

## What to build

Receive audio data via TCP from Windows proxy and run VAD detection.

**Audio receiver:**
- Receive raw PCM (16kHz mono 16-bit) over TCP socket
- Buffer audio in ring buffer (30 seconds capacity)
- Use `select()` to multiplex between command socket and audio socket

**VAD integration:**
- Use `whisper_vad_init_*()` from whisper.cpp
- Process audio in chunks (e.g., 100ms windows)
- Call `whisper_vad_detect_speech()` per chunk

**Speech segment detection:**
- Track speech/silence transitions
- Emit events:
  - `speech_start` with timestamp
  - `speech_end` with duration
- Configurable thresholds from `config.json`

**VAD parameters:**
```c
struct whisper_vad_params params = whisper_vad_default_params();
params.threshold = 0.5f;
params.min_speech_duration_ms = 250;
params.min_silence_duration_ms = 100;
params.speech_pad_ms = 30;
```

## Acceptance criteria

- [ ] Audio data received and buffered
- [ ] VAD initialized with whisper.cpp
- [ ] `speech_start` event emitted on speech detection
- [ ] `speech_end` event emitted on silence
- [ ] VAD segments have correct timestamps
- [ ] Memory-stable (no leaks in buffer)
