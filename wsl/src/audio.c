/*
 * Audio + VAD Pipeline
 *
 * Receives audio from TCP and runs VAD detection using whisper.cpp
 */

#ifndef AUDIO_C
#define AUDIO_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "whisper.h"

// ============================================================================
// Constants
// ============================================================================

#define SAMPLE_RATE WHISPER_SAMPLE_RATE  // 16000 from whisper.h
#define BUFFER_SIZE_MS 30000  // 30 seconds ring buffer
#define CHUNK_SIZE_MS 100     // Process in 100ms chunks

// ============================================================================
// VAD Context
// ============================================================================

static struct whisper_vad_context *g_vad_context = NULL;
static bool g_vad_initialized = false;

// ============================================================================
// Ring Buffer
// ============================================================================

typedef struct {
    float *data;
    size_t capacity;
    size_t write_pos;
    size_t read_pos;
    size_t count;
    pthread_mutex_t mutex;
} ring_buffer_t;

static ring_buffer_t *ring_buffer_create(size_t capacity) {
    ring_buffer_t *rb = calloc(1, sizeof(ring_buffer_t));
    if (!rb) return NULL;
    
    rb->data = calloc(capacity, sizeof(float));
    if (!rb->data) {
        free(rb);
        return NULL;
    }
    
    rb->capacity = capacity;
    pthread_mutex_init(&rb->mutex, NULL);
    return rb;
}

static void ring_buffer_destroy(ring_buffer_t *rb) {
    if (!rb) return;
    pthread_mutex_destroy(&rb->mutex);
    free(rb->data);
    free(rb);
}

static size_t ring_buffer_write(ring_buffer_t *rb, const float *data, size_t n) {
    pthread_mutex_lock(&rb->mutex);
    
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
    
    pthread_mutex_unlock(&rb->mutex);
    return written;
}

static void ring_buffer_clear(ring_buffer_t *rb) {
    pthread_mutex_lock(&rb->mutex);
    rb->write_pos = 0;
    rb->read_pos = 0;
    rb->count = 0;
    pthread_mutex_unlock(&rb->mutex);
}

// ============================================================================
// Audio State
// ============================================================================

typedef struct {
    ring_buffer_t *buffer;
    bool is_speaking;
    size_t speech_start_pos;
    size_t last_speech_pos;
    uint64_t silence_frames;
    uint64_t sample_count;
} audio_state_t;

static audio_state_t g_audio;

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

// ============================================================================
// VAD Parameters
// ============================================================================

static struct whisper_vad_params g_vad_params;

void audio_set_vad_params(float threshold, int min_speech_ms, int min_silence_ms, int pad_ms) {
    g_vad_params.threshold = threshold;
    g_vad_params.min_speech_duration_ms = min_speech_ms;
    g_vad_params.min_silence_duration_ms = min_silence_ms;
    g_vad_params.speech_pad_ms = pad_ms;
}

// ============================================================================
// Event System (forward declaration)
// ============================================================================

#ifndef EVENTS_EMIT_DECLARED
#define EVENTS_EMIT_DECLARED
extern void events_emit(const char *event, const char *data);
#endif

// ============================================================================
// VAD Detection using whisper.cpp
// ============================================================================

static bool init_vad(const char *model_path) {
    if (g_vad_initialized) {
        return true;
    }
    
    struct whisper_vad_context_params params = whisper_vad_default_context_params();
    
    // Load VAD model if provided, otherwise use default
    if (model_path && strlen(model_path) > 0) {
        g_vad_context = whisper_vad_init_from_file_with_params(model_path, params);
    } else {
        // Use built-in VAD (no separate model needed for some configs)
        g_vad_context = whisper_vad_init_with_params(NULL, params);
    }
    
    if (!g_vad_context) {
        fprintf(stderr, "Failed to initialize VAD context\n");
        return false;
    }
    
    g_vad_initialized = true;
    
    // Set default VAD params
    audio_set_vad_params(0.5f, 250, 100, 30);
    
    return true;
}

static void shutdown_vad(void) {
    if (g_vad_context) {
        whisper_vad_free(g_vad_context);
        g_vad_context = NULL;
    }
    g_vad_initialized = false;
}

// ============================================================================
// Segment Tracking
// ============================================================================

typedef struct {
    const char *session;
    uint64_t sample_count;
    uint64_t speech_start_sample;
    uint64_t speech_end_sample;
} speech_segment_t;

static speech_segment_t g_current_segment = {0};
static bool g_in_segment = false;

static void emit_speech_start(audio_state_t *state) {
    if (!g_in_segment) {
        g_current_segment.speech_start_sample = state->speech_start_pos;
        g_in_segment = true;
        
        char buf[128];
        snprintf(buf, sizeof(buf), "Speech start at sample %lu", 
                 (unsigned long)g_current_segment.speech_start_sample);
        events_emit("speech_start", buf);
        events_emit("INFO", "VAD: speech detected");
    }
}

static void emit_speech_end(audio_state_t *state) {
    if (g_in_segment) {
        g_current_segment.speech_end_sample = state->last_speech_pos;
        
        uint64_t duration_samples = g_current_segment.speech_end_sample - 
                                    g_current_segment.speech_start_sample;
        float duration_ms = (float)duration_samples * 1000.0f / SAMPLE_RATE;
        
        if (duration_ms >= g_vad_params.min_speech_duration_ms) {
            char buf[256];
            snprintf(buf, sizeof(buf), 
                     "Speech segment: samples %lu-%lu (%.1fms), %.1fs-%.1fs",
                     (unsigned long)g_current_segment.speech_start_sample,
                     (unsigned long)g_current_segment.speech_end_sample,
                     duration_ms,
                     (float)g_current_segment.speech_start_sample / SAMPLE_RATE,
                     (float)g_current_segment.speech_end_sample / SAMPLE_RATE);
            events_emit("speech_end", buf);
            events_emit("INFO", "VAD: speech segment ready");
            
            // Emit transcription request
            events_emit("transcription_request", buf);
        } else {
            events_emit("DEBUG", "Speech segment too short, ignoring");
        }
        
        g_in_segment = false;
    }
}

// ============================================================================
// Process Audio Chunk with VAD
// ============================================================================

static int g_samples_per_frame = SAMPLE_RATE * CHUNK_SIZE_MS / 1000;

static bool process_vad_chunk(const float *chunk, size_t n) {
    if (!g_vad_initialized || !g_vad_context) {
        // Fallback: simple energy-based detection
        float sum = 0.0f;
        for (size_t i = 0; i < n; i++) {
            sum += fabsf(chunk[i]);
        }
        float energy = sum / n;
        return energy > 0.02f;  // Threshold for speech
    }
    
    // Use whisper VAD
    bool is_speech = whisper_vad_detect_speech(g_vad_context, chunk, (int)n);
    
    return is_speech;
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
            emit_speech_start(state);
        }
    } else {
        state->silence_frames++;
        
        int silence_threshold_frames = g_vad_params.min_silence_duration_ms / CHUNK_SIZE_MS;
        
        if (state->is_speaking && state->silence_frames >= silence_threshold_frames) {
            state->is_speaking = false;
            emit_speech_end(state);
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

bool audio_init(const char *vad_model_path) {
    // Initialize VAD
    if (!init_vad(vad_model_path)) {
        fprintf(stderr, "Warning: VAD initialization failed, using fallback\n");
    }
    
    // Initialize audio state
    audio_state_init(&g_audio);
    
    events_emit("INFO", "Audio pipeline initialized");
    
    return true;
}

void audio_shutdown(void) {
    if (g_in_segment) {
        emit_speech_end(&g_audio);
    }
    audio_state_destroy(&g_audio);
    shutdown_vad();
}

void audio_reset(void) {
    if (g_audio.buffer) {
        ring_buffer_clear(g_audio.buffer);
    }
    g_audio.is_speaking = false;
    g_audio.silence_frames = 0;
    g_in_segment = false;
    
    if (g_vad_context) {
        whisper_vad_reset_state(g_vad_context);
    }
}

void audio_process(const float *samples, size_t n) {
    size_t chunk_size = g_samples_per_frame;
    
    while (n >= chunk_size) {
        process_audio_chunk(&g_audio, samples, chunk_size);
        samples += chunk_size;
        n -= chunk_size;
    }
    
    if (n > 0) {
        process_audio_chunk(&g_audio, samples, n);
    }
}

void audio_get_stats(uint64_t *total_samples, uint64_t *speech_samples, bool *is_speaking) {
    if (total_samples) *total_samples = g_audio.sample_count;
    if (speech_samples) *speech_samples = g_audio.last_speech_pos;
    if (is_speaking) *is_speaking = g_audio.is_speaking;
}

bool audio_is_vad_ready(void) {
    return g_vad_initialized;
}

#endif // AUDIO_C
