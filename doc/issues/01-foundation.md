# Issue 1: Project Foundation

**Type:** AFK  
**Blocked by:** None - can start immediately

## What to build

Set up the complete project directory structure and build system for both Windows and WSL targets.

**Directory structure:**
```
whisper-recorder/
├── doc/
│   ├── PRD.md
│   └── issues/
├── windows/
│   ├── hotkey.c
│   ├── Makefile
│   └── README.md
├── wsl/
│   ├── src/
│   │   ├── main.c
│   │   ├── audio.c
│   │   ├── vad.c
│   │   ├── transcribe.c
│   │   ├── writer.c
│   │   └── config.c
│   ├── include/
│   ├── third_party/
│   ├── Makefile
│   └── README.md
├── config.json
├── .gitignore
└── README.md
```

**Build requirements:**
- WSL: `gcc`, `cmake`, POSIX sockets
- Windows: `gcc` (MinGW) or `cl` (MSVC)

**Empty main.c** for WSL that:
- Accepts `--help` and `--version` flags
- Prints usage info
- Returns 0

**Empty hotkey.c** for Windows that:
- Shows basic window on run
- Accepts command line args

## Acceptance criteria

- [ ] All directories created
- [ ] WSL Makefile builds with `make`
- [ ] Windows Makefile builds with `gcc -o hotkey.exe hotkey.c`
- [ ] `--help` flag works on both platforms
- [ ] `config.json` exists with defaults

---

## Completion Notes

**Completed:** 2024-01-15

**Deliverables:**
- Directory structure created
- WSL main.c with --help and --version
- Windows hotkey.c with basic framework
- Makefiles for both platforms
- config.json with defaults
- Test suite: wsl/tests/test-foundation.sh

**Test Results:** All 7 tests pass
