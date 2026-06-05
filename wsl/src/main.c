/*
 * Whisper Recorder - Main Entry Point
 * Windows-compatible version with Winsock2
 * 
 * Integrates: audio/VAD, transcription, session management, markdown writer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mmsystem.h>
#include <process.h>

// whisper.cpp headers
#include "whisper.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

#define VERSION "0.1.0"
#define DEFAULT_PORT 8765
#define BUFFER_SIZE 1024

// ============================================================================
// Constants
// ============================================================================

#define SAMPLE_RATE WHISPER_SAMPLE_RATE  // 16000 from whisper.h
#define BUFFER_SIZE_MS 30000  // 30 seconds ring buffer
#define CHUNK_SIZE_MS 100     // Process in 100ms chunks

// ============================================================================
// Types
// ============================================================================

typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PAUSED
} state_t;

typedef struct {
    char model_path[512];
    char vad_model_path[512];
    int sample_rate;
    int channels;
    float vad_threshold;
    int vad_min_speech_ms;
    int vad_min_silence_ms;
    int vad_speech_pad_ms;
    int session_silence_timeout_ms;
    int max_session_duration_ms;
    char output_dir[512];
    bool create_date_dirs;
    char language[8];
} config_t;

typedef struct {
    float *data;
    size_t capacity;
    size_t write_pos;
    size_t read_pos;
    size_t count;
} ring_buffer_t;

typedef struct {
    ring_buffer_t *buffer;
    bool is_speaking;
    size_t speech_start_pos;
    size_t last_speech_pos;
    uint64_t silence_frames;
    uint64_t sample_count;
} audio_state_t;

typedef struct {
    char text[4096];
    float start_time;
    float end_time;
    char language[8];
} transcription_t;

typedef struct {
    char session_name[256];
    char file_path[512];
    FILE *file;
    time_t start_time;
    time_t end_time;
    int segment_count;
} writer_session_t;

typedef struct {
    char name[256];
    time_t start_time;
    time_t last_speech_time;
    int segment_count;
    bool has_content;
} session_info_t;

// ============================================================================
// Forward Declarations
// ============================================================================

static void session_end(void);
static void session_on_speech_detected(void);
static void session_on_segment_complete(void);
static void writer_append_text(const char *text);
static void writer_end_session(void);
static void process_audio_chunk(audio_state_t *state, const float *chunk, size_t n);

// ============================================================================
// Global State
// ============================================================================

static config_t g_config;
static volatile int g_running = 1;
static state_t g_state = STATE_IDLE;
static char g_session_name[256] = {0};

// Whisper context
static struct whisper_context *g_whisper_ctx = NULL;
static struct whisper_vad_context *g_vad_context = NULL;
static bool g_vad_initialized = false;

// Audio state
static audio_state_t g_audio;
static struct whisper_vad_params g_vad_params;

// Session state
static session_info_t g_session;
static bool g_in_session = false;
static int g_session_counter = 0;
static time_t g_last_split_time = 0;

// Writer state
static writer_session_t g_writer_session;
static bool g_writer_in_session = false;
static config_t g_writer_config;

// Transcription state
static transcription_t g_last_result;
static bool g_model_loaded = false;

// ============================================================================
// Event System
// ============================================================================

static void log_event(const char *event, const char *data) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    if (data) {
        printf("[%s] %s: %s\n", timestamp, event, data);
    } else {
        printf("[%s] %s\n", timestamp, event);
    }
}

static void events_emit(const char *event, const char *data) {
    log_event(event, data);
}

// ============================================================================
// Ring Buffer
// ============================================================================

static ring_buffer_t *ring_buffer_create(size_t capacity) {
    ring_buffer_t *rb = calloc(1, sizeof(ring_buffer_t));
    if (!rb) return NULL;
    
    rb->data = calloc(capacity, sizeof(float));
    if (!rb->data) {
        free(rb);
        return NULL;
    }
    
    rb->capacity = capacity;
    return rb;
}

static void ring_buffer_destroy(ring_buffer_t *rb) {
    if (!rb) return;
    free(rb->data);
    free(rb);
}

static size_t ring_buffer_write(ring_buffer_t *rb, const float *data, size_t n) {
    size_t written = 0;
    for (size_t i = 0; i < n; i++) {
        rb->data[rb->write_pos] = data[i];
        rb->write_pos = (rb->write_pos + 1) % rb->capacity;
        
        if (rb->count < rb->capacity) {
            rb->count++;
        } else {
            rb->read_pos = (rb->read_pos + 1) % rb->capacity;
        }
        written++;
    }
    return written;
}

static void ring_buffer_clear(ring_buffer_t *rb) {
    rb->write_pos = 0;
    rb->read_pos = 0;
    rb->count = 0;
}

// ============================================================================
// ============================================================================
// Audio Capture (Windows winmm) - Polling Mode
// ============================================================================

static HWAVEIN g_hWaveIn = NULL;
static bool g_AudioCapturing = false;
static bool g_AudioThreadRunning = false;
static HANDLE g_AudioThread = NULL;

static ring_buffer_t *g_CaptureBuffer = NULL;
static CRITICAL_SECTION g_CaptureCS;

static int g_RecordingDevice = -1;  // -1 = default device
static int g_samples_per_frame;  // Forward declaration, defined in Audio/VAD

static void wave_error(MMRESULT mr) {
    char err_buf[256];
    waveOutGetErrorText(mr, err_buf, sizeof(err_buf));
    fprintf(stderr, "Wave error: %s\n", err_buf);
}

// Audio capture thread - uses buffer headers for async capture
static unsigned __stdcall audio_capture_thread(void *param) {
    (void)param;
    
    const int buffer_size = SAMPLE_RATE * CHUNK_SIZE_MS / 1000 * sizeof(short);
    
    // Allocate buffers on heap
    WAVEHDR *hdr = calloc(2, sizeof(WAVEHDR));
    short *buffers = calloc(2, buffer_size);
    
    if (!hdr || !buffers) {
        free(hdr);
        free(buffers);
        return 1;
    }
    
    for (int i = 0; i < 2; i++) {
        hdr[i].lpData = (char *)&buffers[i * buffer_size];
        hdr[i].dwBufferLength = buffer_size;
        hdr[i].dwFlags = 0;
        hdr[i].dwLoops = 0;
        
        MMRESULT mr = waveInPrepareHeader(g_hWaveIn, &hdr[i], sizeof(WAVEHDR));
        if (mr != MMSYSERR_NOERROR) {
            free(hdr);
            free(buffers);
            return 1;
        }
        
        mr = waveInAddBuffer(g_hWaveIn, &hdr[i], sizeof(WAVEHDR));
        if (mr != MMSYSERR_NOERROR) {
            free(hdr);
            free(buffers);
            return 1;
        }
    }
    while (g_AudioThreadRunning) {
        // Check if headers have data
        for (int i = 0; i < 2; i++) {
            if (hdr[i].dwFlags & WHDR_DONE) {
                short *samples = (short *)hdr[i].lpData;
                int num_samples = hdr[i].dwBytesRecorded / sizeof(short);
                
                if (num_samples > 0 && g_state == STATE_RECORDING) {
                    // Convert to float and process
                    EnterCriticalSection(&g_CaptureCS);
                    
                    if (g_CaptureBuffer) {
                        for (int j = 0; j < num_samples; j++) {
                            float f = samples[j] / 32768.0f;
                            ring_buffer_write(g_CaptureBuffer, &f, 1);
                        }
                    }
                    
                    if (num_samples >= g_samples_per_frame) {
                        process_audio_chunk(&g_audio, (const float *)samples, num_samples);
                    }
                    
                    LeaveCriticalSection(&g_CaptureCS);
                }
                
                // Requeue the buffer
                hdr[i].dwBytesRecorded = 0;
                hdr[i].dwFlags = 0;
                waveInAddBuffer(g_hWaveIn, &hdr[i], sizeof(WAVEHDR));
            }
        }
        
        Sleep(10);  // Small delay to prevent CPU spinning
    }
    
    // Cleanup
    for (int i = 0; i < 2; i++) {
        waveInUnprepareHeader(g_hWaveIn, &hdr[i], sizeof(WAVEHDR));
    }
    free(hdr);
    free(buffers);
    
    return 0;
}

static bool audio_capture_start(int device_id) {
    if (g_AudioCapturing) return true;
    
    MMRESULT mr;
    WAVEFORMATEX wf;
    
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = SAMPLE_RATE;
    wf.nAvgBytesPerSec = SAMPLE_RATE * 2;
    wf.nBlockAlign = 2;
    wf.wBitsPerSample = 16;
    wf.cbSize = 0;
    
    // Use CALLBACK_NULL for polling mode
    if (device_id < 0) {
        mr = waveInOpen(&g_hWaveIn, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL);
    } else {
        mr = waveInOpen(&g_hWaveIn, (UINT)device_id, &wf, 0, 0, CALLBACK_NULL);
    }
    
    if (mr != MMSYSERR_NOERROR) {
        wave_error(mr);
        return false;
    }
    
    // Start recording
    mr = waveInStart(g_hWaveIn);
    if (mr != MMSYSERR_NOERROR) {
        wave_error(mr);
        return false;
    }
    
    // Start capture thread
    g_AudioThreadRunning = true;
    g_AudioThread = (HANDLE)_beginthreadex(NULL, 0, audio_capture_thread, NULL, 0, NULL);
    if (!g_AudioThread) {
        waveInStop(g_hWaveIn);
        waveInClose(g_hWaveIn);
        g_hWaveIn = NULL;
        return false;
    }
    
    g_AudioCapturing = true;
    return true;
}

static void audio_capture_stop(void) {
    if (!g_AudioCapturing) return;
    
    // Stop capture thread
    g_AudioThreadRunning = false;
    if (g_AudioThread) {
        WaitForSingleObject(g_AudioThread, 2000);
        CloseHandle(g_AudioThread);
        g_AudioThread = NULL;
    }
    
    // Stop and close wave device
    if (g_hWaveIn) {
        waveInStop(g_hWaveIn);
        waveInReset(g_hWaveIn);
        waveInClose(g_hWaveIn);
        g_hWaveIn = NULL;
    }
    
    g_AudioCapturing = false;
}

static void audio_capture_list_devices(void) {
    UINT num_devs = waveInGetNumDevs();
    printf("Available audio input devices:\n");
    
    for (UINT i = 0; i < num_devs; i++) {
        WAVEINCAPS caps;
        MMRESULT mr = waveInGetDevCaps(i, &caps, sizeof(caps));
        if (mr == MMSYSERR_NOERROR) {
            printf("  [%d] %s\n", i, caps.szPname);
        }
    }
    
    if (num_devs == 0) {
        printf("  No audio input devices found\n");
    }
}

// ============================================================================
// Audio/VAD
// ============================================================================

static int g_samples_per_frame = SAMPLE_RATE * CHUNK_SIZE_MS / 1000;

static void audio_state_init(audio_state_t *state) {
    size_t capacity = SAMPLE_RATE * BUFFER_SIZE_MS / 1000;
    state->buffer = ring_buffer_create(capacity);
    state->is_speaking = false;
    state->speech_start_pos = 0;
    state->last_speech_pos = 0;
    state->silence_frames = 0;
    state->sample_count = 0;
}

static void audio_state_destroy(audio_state_t *state) {
    if (state->buffer) {
        ring_buffer_destroy(state->buffer);
        state->buffer = NULL;
    }
}

static bool init_vad(const char *model_path) {
    if (g_vad_initialized) return true;
    
    struct whisper_vad_context_params params = whisper_vad_default_context_params();
    
    if (model_path && strlen(model_path) > 0) {
        g_vad_context = whisper_vad_init_from_file_with_params(model_path, params);
    } else {
        g_vad_context = whisper_vad_init_with_params(NULL, params);
    }
    
    if (!g_vad_context) {
        fprintf(stderr, "Warning: VAD initialization failed\n");
        return false;
    }
    
    g_vad_initialized = true;
    g_vad_params.threshold = 0.5f;
    g_vad_params.min_speech_duration_ms = 250;
    g_vad_params.min_silence_duration_ms = 100;
    g_vad_params.speech_pad_ms = 30;
    
    return true;
}

static void shutdown_vad(void) {
    if (g_vad_context) {
        whisper_vad_free(g_vad_context);
        g_vad_context = NULL;
    }
    g_vad_initialized = false;
}

static bool process_vad_chunk(const float *chunk, size_t n) {
    if (!g_vad_initialized || !g_vad_context) {
        // Fallback: simple energy-based detection
        float sum = 0.0f;
        for (size_t i = 0; i < n; i++) {
            sum += fabsf(chunk[i]);
        }
        float energy = sum / n;
        return energy > 0.02f;
    }
    
    return whisper_vad_detect_speech(g_vad_context, chunk, (int)n);
}

static void process_audio_chunk(audio_state_t *state, const float *chunk, size_t n) {
    // Write to ring buffer
    ring_buffer_write(state->buffer, chunk, n);
    state->sample_count += n;
    
    // Run VAD
    bool is_speech = process_vad_chunk(chunk, n);
    
    if (is_speech) {
        state->last_speech_pos = state->sample_count;
        state->silence_frames = 0;
        
        if (!state->is_speaking) {
            state->is_speaking = true;
            state->speech_start_pos = state->sample_count - n;
            events_emit("INFO", "VAD: speech detected");
            session_on_speech_detected();
        }
    } else {
        state->silence_frames++;
        
        int silence_threshold_frames = g_vad_params.min_silence_duration_ms / CHUNK_SIZE_MS;
        
        if (state->is_speaking && state->silence_frames >= (uint64_t)silence_threshold_frames) {
            state->is_speaking = false;
            events_emit("INFO", "VAD: speech ended");
            
            // Trigger transcription
            if (g_in_session && g_model_loaded && g_whisper_ctx) {
                // Get audio from buffer and transcribe
                events_emit("transcription", "Processing segment...");
            }
        }
    }
}

static void audio_reset(void) {
    if (g_audio.buffer) {
        ring_buffer_clear(g_audio.buffer);
    }
    g_audio.is_speaking = false;
    g_audio.silence_frames = 0;
    
    if (g_vad_context) {
        whisper_vad_reset_state(g_vad_context);
    }
}

// ============================================================================
// Transcription
// ============================================================================

static bool transcription_transcribe(const float *samples, size_t n_samples) {
    if (!g_model_loaded || !g_whisper_ctx) {
        return false;
    }
    
    int lang_id = whisper_lang_id(g_config.language);
    if (lang_id < 0) {
        lang_id = whisper_lang_id("en");
    }
    
    struct whisper_full_params params = whisper_full_default_params(
        WHISPER_SAMPLING_GREEDY
    );
    params.language = whisper_lang_str(lang_id);
    params.n_threads = 4;
    
    int ret = whisper_full(g_whisper_ctx, params, samples, (int)n_samples);
    
    if (ret != 0) {
        return false;
    }
    
    // Extract text
    g_last_result.text[0] = '\0';
    g_last_result.start_time = 0;
    g_last_result.end_time = 0;
    strncpy(g_last_result.language, g_config.language, sizeof(g_last_result.language) - 1);
    
    int n_segments = whisper_full_n_segments(g_whisper_ctx);
    
    for (int i = 0; i < n_segments; i++) {
        const char *text = whisper_full_get_segment_text(g_whisper_ctx, i);
        strncat(g_last_result.text, text, sizeof(g_last_result.text) - strlen(g_last_result.text) - 1);
        if (i < n_segments - 1) {
            strncat(g_last_result.text, " ", sizeof(g_last_result.text) - strlen(g_last_result.text) - 1);
        }
    }
    
    if (strlen(g_last_result.text) > 0) {
        events_emit("transcription", g_last_result.text);
        writer_append_text(g_last_result.text);
        session_on_segment_complete();
    }
    
    return true;
}

static void transcription_shutdown(void) {
    if (g_whisper_ctx) {
        whisper_free(g_whisper_ctx);
        g_whisper_ctx = NULL;
    }
    g_model_loaded = false;
}

// ============================================================================
// Session Management
// ============================================================================

static long get_time_ms(void) {
    SYSTEMTIME st;
    GetSystemTime(&st);
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    ULONGLONG ms = (((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000;
    return (long)ms;
}

static void session_start(const char *name) {
    if (g_in_session) {
        session_end();
    }
    
    time_t now = time(NULL);
    
    strncpy(g_session.name, name ? name : "", sizeof(g_session.name) - 1);
    g_session.start_time = now;
    g_session.last_speech_time = now;
    g_session.segment_count = 0;
    g_session.has_content = false;
    
    g_in_session = true;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Session started: %s", g_session.name);
    events_emit("INFO", buf);
}

static void session_end(void) {
    if (!g_in_session) return;
    
    g_last_split_time = get_time_ms();
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Session ended: %s (%d segments)",
             g_session.name, g_session.segment_count);
    events_emit("INFO", buf);
    
    g_in_session = false;
}

static void session_on_speech_detected(void) {
    if (!g_in_session) return;
    g_session.last_speech_time = time(NULL);
    g_session.has_content = true;
}

static void session_on_segment_complete(void) {
    if (!g_in_session) return;
    g_session.segment_count++;
}

// ============================================================================
// Markdown Writer
// ============================================================================

static bool ensure_directory(const char *path) {
    if (!CreateDirectoryA(path, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            return false;
        }
    }
    return true;
}

static void get_date_dir(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, len, "%Y-%m-%d", tm_info);
}

static void writer_init(const char *base_dir, bool create_date_dirs) {
    strncpy(g_writer_config.output_dir, base_dir, sizeof(g_writer_config.output_dir) - 1);
    g_writer_config.create_date_dirs = create_date_dirs;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Writer initialized: %s", base_dir);
    events_emit("INFO", buf);
}

static bool writer_start_session(const char *session_name) {
    if (g_writer_in_session) {
        writer_end_session();
    }
    
    // Create directory path
    char dir_path[512];
    
    if (g_writer_config.create_date_dirs) {
        char date_dir[32];
        get_date_dir(date_dir, sizeof(date_dir));
        snprintf(dir_path, sizeof(dir_path), "%s/%s", 
                 g_writer_config.output_dir, date_dir);
    } else {
        snprintf(dir_path, sizeof(dir_path), "%s", g_writer_config.output_dir);
    }
    
    if (!ensure_directory(dir_path)) {
        events_emit("ERROR", "Failed to create directory");
        return false;
    }
    
    // Generate file path
    char time_str[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%H-%M-%S", tm_info);
    
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/session-%s.md",
             dir_path, time_str);
    
    // Open file
    FILE *f = fopen(file_path, "w");
    if (!f) {
        events_emit("ERROR", "Failed to open file");
        return false;
    }
    
    // Write header
    fprintf(f, "# Session: %s\n\n", session_name);
    fprintf(f, "**Started:** %s\n", ctime(&now));
    fprintf(f, "**Language:** %s\n\n", g_config.language);
    fprintf(f, "---\n\n");
    fflush(f);
    
    // Store session info
    strncpy(g_writer_session.session_name, session_name, 
             sizeof(g_writer_session.session_name) - 1);
    strncpy(g_writer_session.file_path, file_path,
             sizeof(g_writer_session.file_path) - 1);
    g_writer_session.file = f;
    g_writer_session.start_time = now;
    g_writer_session.end_time = now;
    g_writer_session.segment_count = 0;
    
    g_writer_in_session = true;
    
    return true;
}

static void writer_append_text(const char *text) {
    if (!g_writer_in_session || !g_writer_session.file) return;
    
    fprintf(g_writer_session.file, "%s\n\n", text);
    fflush(g_writer_session.file);
    
    g_writer_session.segment_count++;
}

static void writer_end_session(void) {
    if (!g_writer_in_session) return;
    
    time_t end = time(NULL);
    g_writer_session.end_time = end;
    
    if (g_writer_session.file) {
        // Write footer
        fprintf(g_writer_session.file, "---\n\n");
        fprintf(g_writer_session.file, "**Ended:** %s\n", ctime(&end));
        fprintf(g_writer_session.file, "**Transcribed segments:** %d\n",
                g_writer_session.segment_count);
        
        fclose(g_writer_session.file);
        g_writer_session.file = NULL;
    }
    
    char buf[512];
    snprintf(buf, sizeof(buf), "Saved: %s", g_writer_session.file_path);
    events_emit("INFO", buf);
    
    g_writer_in_session = false;
}

// ============================================================================
// Configuration
// ============================================================================

static void config_set_defaults(config_t *cfg) {
    strncpy(cfg->model_path, "third_party/whisper.cpp/models/ggml-base.bin", 
             sizeof(cfg->model_path) - 1);
    cfg->vad_model_path[0] = '\0';
    cfg->sample_rate = 16000;
    cfg->channels = 1;
    cfg->vad_threshold = 0.5f;
    cfg->vad_min_speech_ms = 250;
    cfg->vad_min_silence_ms = 100;
    cfg->vad_speech_pad_ms = 30;
    cfg->session_silence_timeout_ms = 60000;
    cfg->max_session_duration_ms = 600000;
    strncpy(cfg->output_dir, "D:/recordings", sizeof(cfg->output_dir) - 1);
    cfg->create_date_dirs = true;
    strncpy(cfg->language, "zh", sizeof(cfg->language) - 1);
}

static void print_usage(const char *program) {
    printf("Whisper Recorder - Real-time voice transcription tool\n");
    printf("\n");
    printf("Usage: %s [options]\n", program);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --version           Show version information\n");
    printf("  -m, --model <path>     Model file path\n");
    printf("  -o, --output <path>     Output directory\n");
    printf("  -l, --language <lang>   Language (en/zh)\n");
    printf("  -p, --port <port>       TCP server port (default: %d)\n", DEFAULT_PORT);
    printf("  -d, --device <id>       Audio input device ID\n");
    printf("  --list-devices          List available audio input devices\n");
    printf("\n");
    printf("Commands (via TCP):\n");
    printf("  START:<name>    Start recording session\n");
    printf("  STOP           Stop recording\n");
    printf("  PAUSE          Pause recording\n");
    printf("  RESUME         Resume recording\n");
    printf("  STATUS         Get status\n");
    printf("\n");
}

static const char *state_to_string(state_t s) {
    switch (s) {
        case STATE_IDLE: return "IDLE";
        case STATE_RECORDING: return "RECORDING";
        case STATE_PAUSED: return "PAUSED";
        default: return "UNKNOWN";
    }
}

static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
    (void)dwCtrlType;
    g_running = 0;
    return TRUE;
}

// ============================================================================
// TCP Server
// ============================================================================

static SOCKET g_listen_socket = INVALID_SOCKET;

static int create_server(int port) {
    SOCKET listen_socket;
    struct sockaddr_in addr;
    int opt = 1;
    
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        fprintf(stderr, "socket failed\n");
        return -1;
    }
    
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons((USHORT)port);
    
    if (bind(listen_socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
        closesocket(listen_socket);
        return -1;
    }
    
    if (listen(listen_socket, 1) == SOCKET_ERROR) {
        fprintf(stderr, "listen failed\n");
        closesocket(listen_socket);
        return -1;
    }
    
    return (int)listen_socket;
}

static void handle_command(const char *cmd, SOCKET client_fd) {
    char response[BUFFER_SIZE];
    
    if (strncmp(cmd, "START:", 6) == 0) {
        const char *name = cmd + 6;
        
        // Generate session name if empty
        if (strlen(name) == 0) {
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            strftime(g_session_name, sizeof(g_session_name), "%Y-%m-%d %H:%M:%S", tm_info);
        } else {
            strncpy(g_session_name, name, sizeof(g_session_name) - 1);
        }
        
        // End previous session
        if (g_in_session) {
            session_end();
            writer_end_session();
        }
        
        // Start new session
        session_start(g_session_name);
        writer_start_session(g_session_name);
        audio_reset();
        
        // Start audio capture
        if (!audio_capture_start(g_RecordingDevice)) {
            events_emit("ERROR", "Failed to start audio capture");
            snprintf(response, sizeof(response), "ERROR: Failed to start audio capture\r\n");
            send(client_fd, response, (int)strlen(response), 0);
            return;
        }
        
        g_state = STATE_RECORDING;
        
        snprintf(response, sizeof(response), "OK: Session started: %s\r\n", g_session_name);
        send(client_fd, response, (int)strlen(response), 0);
        
    } else if (strcmp(cmd, "STOP") == 0) {
        if (g_state != STATE_IDLE) {
            audio_capture_stop();
            session_end();
            writer_end_session();
            audio_reset();
            g_state = STATE_IDLE;
            g_session_name[0] = '\0';
        }
        
        snprintf(response, sizeof(response), "OK: Session stopped\r\n");
        send(client_fd, response, (int)strlen(response), 0);
        
    } else if (strcmp(cmd, "PAUSE") == 0) {
        if (g_state == STATE_RECORDING) {
            g_state = STATE_PAUSED;
            snprintf(response, sizeof(response), "OK: Paused\r\n");
            events_emit("INFO", "Recording paused");
        } else {
            snprintf(response, sizeof(response), "ERROR: Not recording\r\n");
        }
        send(client_fd, response, (int)strlen(response), 0);
        
    } else if (strcmp(cmd, "RESUME") == 0) {
        if (g_state == STATE_PAUSED) {
            g_state = STATE_RECORDING;
            snprintf(response, sizeof(response), "OK: Resumed\r\n");
            events_emit("INFO", "Recording resumed");
        } else {
            snprintf(response, sizeof(response), "ERROR: Not paused\r\n");
        }
        send(client_fd, response, (int)strlen(response), 0);
        
    } else if (strcmp(cmd, "STATUS") == 0) {
        snprintf(response, sizeof(response), 
            "{\"state\":\"%s\",\"session\":\"%s\",\"language\":\"%s\"}",
            state_to_string(g_state), g_session_name, g_config.language);
        send(client_fd, response, (int)strlen(response), 0);
        
    } else {
        snprintf(response, sizeof(response), "ERROR: Unknown command\r\n");
        send(client_fd, response, (int)strlen(response), 0);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[]) {
    
    // Set console ctrl handler first
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
    
    WSADATA wsa_data;
    
    // Init Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
    
    config_set_defaults(&g_config);
    
    // Parse CLI args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            WSACleanup();
            return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("whisper-recorder %s\n", VERSION);
            WSACleanup();
            return 0;
        }
        if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--model") == 0) {
            if (i + 1 < argc) strncpy(g_config.model_path, argv[++i], sizeof(g_config.model_path) - 1);
        }
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) strncpy(g_config.output_dir, argv[++i], sizeof(g_config.output_dir) - 1);
        }
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--language") == 0) {
            if (i + 1 < argc) strncpy(g_config.language, argv[++i], sizeof(g_config.language) - 1);
        }
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) g_config.session_silence_timeout_ms = atoi(argv[++i]);
        }
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
            if (i + 1 < argc) g_RecordingDevice = atoi(argv[++i]);
        }
        if (strcmp(argv[i], "--list-devices") == 0) {
            audio_capture_list_devices();
            WSACleanup();
            return 0;
        }
    }
    
    // Initialize subsystems
    InitializeCriticalSection(&g_CaptureCS);
    g_CaptureBuffer = ring_buffer_create(SAMPLE_RATE * BUFFER_SIZE_MS / 1000);
    
    writer_init(g_config.output_dir, g_config.create_date_dirs);
    audio_state_init(&g_audio);
    
    // Initialize VAD
    // Skipping VAD for now - using simple energy-based detection
    g_vad_initialized = false;
    //init_vad(g_config.vad_model_path[0] ? g_config.vad_model_path : NULL);
    
    events_emit("INFO", "Audio capture initialized (winmm)");
    
    // Initialize whisper model
    events_emit("INFO", "Loading whisper model...");
    
    struct whisper_context_params params = whisper_context_default_params();
    params.use_gpu = false;
    
    g_whisper_ctx = whisper_init_from_file_with_params(g_config.model_path, params);
    
    if (!g_whisper_ctx) {
        fprintf(stderr, "WARNING: Failed to load whisper model from '%s'\n", g_config.model_path);
        fprintf(stderr, "  Transcription will be disabled.\n");
        fprintf(stderr, "  Download the model from:\n");
        fprintf(stderr, "    https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin\n");
        g_model_loaded = false;
    } else {
        g_model_loaded = true;
        events_emit("INFO", "Whisper model loaded successfully");
    }
    
    // Create server
    g_listen_socket = create_server(DEFAULT_PORT);
    if (g_listen_socket == INVALID_SOCKET) {
        fprintf(stderr, "Failed to create server on port %d\n", DEFAULT_PORT);
        audio_state_destroy(&g_audio);
        transcription_shutdown();
        shutdown_vad();
        WSACleanup();
        return 1;
    }
    
    printf("Whisper Recorder v%s\n", VERSION);
    printf("Model: %s\n", g_config.model_path);
    printf("Language: %s\n", g_config.language);
    printf("Output: %s\n", g_config.output_dir);
    printf("Listening on 127.0.0.1:%d\n", DEFAULT_PORT);
    printf("Press Ctrl+C to stop\n");
    events_emit("INFO", "Server started");
    
    // Main loop
    while (g_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET((SOCKET)g_listen_socket, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(0, (fd_set*)&read_fds, NULL, NULL, &timeout);
        if (ready < 0) break;
        if (ready == 0) continue;
        
        if (FD_ISSET((SOCKET)g_listen_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            int client_len = sizeof(client_addr);
            SOCKET client_fd = accept(g_listen_socket, (struct sockaddr *)&client_addr, &client_len);
            
            if (client_fd == INVALID_SOCKET) continue;
            
            printf("Client connected from %s:%d\n",
                   inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port));
            
            char buffer[BUFFER_SIZE];
            int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            
            if (n > 0) {
                buffer[n] = '\0';
                // Remove trailing newlines
                while (n > 0 && (buffer[n-1] == '\n' || buffer[n-1] == '\r')) {
                    buffer[--n] = '\0';
                }
                printf("Command: %s\n", buffer);
                handle_command(buffer, client_fd);
            }
            closesocket(client_fd);
        }
    }
    
    printf("\nShutting down...\n");
    events_emit("INFO", "Server stopped");
    
    // Cleanup
    if (g_in_session) {
        session_end();
        writer_end_session();
    }
    
    if (g_listen_socket != INVALID_SOCKET) {
        closesocket(g_listen_socket);
    }
    
    // Cleanup audio capture
    if (g_AudioCapturing) {
        audio_capture_stop();
    }
    if (g_CaptureBuffer) {
        DeleteCriticalSection(&g_CaptureCS);
        ring_buffer_destroy(g_CaptureBuffer);
    }
    
    audio_state_destroy(&g_audio);
    transcription_shutdown();
    shutdown_vad();
    
    WSACleanup();
    return 0;
}
