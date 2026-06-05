# Issue 5: Whisper Transcription

**Type:** AFK  
**Blocked by:** #4 (Audio + VAD Pipeline)

## What to build

Integrate whisper.cpp to transcribe VAD-detected speech segments.

**Model loading:**
- Load `ggml-base.bin` via `whisper_init_from_file()`
- Auto-download if not present (see #10)
- Support `--model` CLI arg override

**Transcription pipeline:**
- On `speech_end` event with segment audio:
  1. Call `whisper_full()` with segment audio
  2. Extract text from `whisper_full_get_segment_text()`
  3. Emit `transcription` event with text

**Language support:**
- Default to `zh` (Chinese)
- Support `--language en` for English
- `SET_LANG` command changes language at runtime

**Performance:**
- Process segments in background thread
- Don't block VAD detection
- Queue transcriptions if needed

**Output format per segment:**
```c
struct transcription {
    const char *text;
    float start_time;
    float end_time;
    const char *language;
};
```

## Acceptance criteria

- [ ] Model loads successfully
- [ ] Chinese text transcribed correctly
- [ ] English text transcribed correctly
- [ ] `transcription` event emitted with text
- [ ] Transcription thread-safe (non-blocking)
- [ ] Graceful handling of model errors
