# Issue 10: Model Auto-Download

**Type:** AFK  
**Blocked by:** #5 (Whisper Transcription)

## What to build

Automatically download the whisper model if not present, with progress feedback.

**Default model:**
- URL: `https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin`
- Fallback: `https://huggingface.co/datasets/ggerganov/whisper.cpp/resolve/main/ggml-base.bin`

**Download logic:**
```c
int download_model(const char *url, const char *dest, 
                  void (*progress)(int percent));

// 1. Check if dest exists → skip download
// 2. Create temp file: dest + ".tmp"
// 3. Download to temp with progress callback
// 4. On success: rename temp → dest
// 5. On failure: delete temp
```

**Progress callback:**
- Emit log event: `Downloading model...`
- Emit log event: `Downloading model... 45%`
- Emit log event: `Model downloaded to: /path/to/model`

**Curl integration:**
```bash
# Build requires: apt install libcurl4-openssl-dev
# Link with: -lcurl
```

**Error handling:**
- Network error: Retry 3x with exponential backoff
- Disk full: Show error, exit
- Invalid URL: Show error, exit
- Checksum mismatch: Retry download

**Config integration:**
```json
{
  "model": {
    "path": "~/.whisper/models/ggml-base.bin",
    "auto_download": true,
    "download_url": "https://..."
  }
}
```

**Manual override:**
- `--model path/to/model.bin` skips download
- `--no-auto-download` disables auto-download

## Acceptance criteria

- [ ] Model downloaded on first run if missing
- [ ] Download progress shown in logs
- [ ] Existing model skipped (no re-download)
- [ ] Network errors handled gracefully
- [ ] Retry logic works (3 attempts)
- [ ] Download path configurable
- [ ] `--no-auto-download` prevents automatic download
