/*
 * Whisper Transcription Module
 *
 * Transcribes audio segments using whisper.cpp
 */

#ifndef TRANSCRIBE_C
#define TRANSCRIBE_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#include "whisper.h"

// ============================================================================
// Types
// ============================================================================

typedef struct {
    char text[4096];
    float start_time;
    float end_time;
    char language[8];
} transcription_t;

typedef enum {
    TRANSCRIBE_STATE_IDLE,
    TRANSCRIBE_STATE_PROCESSING,
    TRANSCRIBE_STATE_ERROR
} transcribe_state_t;

// ============================================================================
// State
// ============================================================================

static struct whisper_context *g_ctx = NULL;
static transcribe_state_t g_state = TRANSCRIBE_STATE_IDLE;
static char g_model_path[512] = {0};
static char g_language[8] = "zh";
static bool g_model_loaded = false;

// ============================================================================
// Event System (forward declaration)
// ============================================================================

#ifndef EVENTS_EMIT_DECLARED
#define EVENTS_EMIT_DECLARED
extern void events_emit(const char *event, const char *data);
#endif

// ============================================================================
// Model Loading
// ============================================================================

bool transcription_init(const char *model_path, const char *language) {
    strncpy(g_model_path, model_path, sizeof(g_model_path) - 1);
    strncpy(g_language, language, sizeof(g_language) - 1);
    
    if (g_model_loaded) {
        transcription_shutdown();
    }
    
    // Load whisper model
    struct whisper_context_params params = whisper_context_default_params();
    params.use_gpu = false;  // CPU only by default
    
    g_ctx = whisper_init_from_file_with_params(model_path, params);
    
    if (!g_ctx) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Failed to load model: %s", model_path);
        events_emit("ERROR", buf);
        g_state = TRANSCRIBE_STATE_ERROR;
        return false;
    }
    
    g_model_loaded = true;
    g_state = TRANSCRIBE_STATE_IDLE;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Model loaded: %s, Language: %s", model_path, language);
    events_emit("INFO", buf);
    events_emit("model_loaded", model_path);
    
    return true;
}

void transcription_shutdown(void) {
    if (g_ctx) {
        whisper_free(g_ctx);
        g_ctx = NULL;
    }
    g_model_loaded = false;
    g_state = TRANSCRIBE_STATE_IDLE;
}

// ============================================================================
// Transcription
// ============================================================================

static transcription_t g_last_result = {0};

bool transcription_transcribe_sync(float *samples, size_t n_samples,
                                   float start_time, float end_time,
                                   transcription_t *result) {
    if (!g_model_loaded || !g_ctx || !result) {
        return false;
    }
    
    g_state = TRANSCRIBE_STATE_PROCESSING;
    
    // Get language ID
    int lang_id = whisper_lang_id(g_language);
    if (lang_id < 0) {
        lang_id = whisper_lang_id("en");  // Fallback to English
    }
    
    // Full transcription parameters
    struct whisper_full_params params = whisper_full_default_params(
        WHISPER_SAMPLING_GREEDY  // Greedy sampling for speed
    );
    params.language = whisper_lang_str(lang_id);
    params.n_threads = 4;
    
    // Run transcription
    int ret = whisper_full(g_ctx, params, samples, (int)n_samples);
    
    if (ret != 0) {
        events_emit("ERROR", "Transcription failed");
        g_state = TRANSCRIBE_STATE_ERROR;
        return false;
    }
    
    // Extract text from all segments
    result->text[0] = '\0';
    result->start_time = start_time;
    result->end_time = end_time;
    strncpy(result->language, g_language, sizeof(result->language) - 1);
    
    int n_segments = whisper_full_n_segments(g_ctx);
    
    for (int i = 0; i < n_segments; i++) {
        const char *text = whisper_full_get_segment_text(g_ctx, i);
        strncat(result->text, text, sizeof(result->text) - strlen(result->text) - 1);
        if (i < n_segments - 1) {
            strncat(result->text, " ", sizeof(result->text) - strlen(result->text) - 1);
        }
    }
    
    g_last_result = *result;
    g_state = TRANSCRIBE_STATE_IDLE;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Transcribed: %s", result->text);
    events_emit("transcription", result->text);
    
    return true;
}

bool transcription_transcribe_async(float *samples, size_t n_samples,
                                    float start_time, float end_time) {
    if (!g_model_loaded) {
        return false;
    }
    
    // For simplicity, do synchronous transcription
    // In production, could use a thread pool
    transcription_t result;
    bool ok = transcription_transcribe_sync(samples, n_samples, start_time, end_time, &result);
    
    return ok;
}

// ============================================================================
// Configuration
// ============================================================================

void transcription_set_language(const char *language) {
    strncpy(g_language, language, sizeof(g_language) - 1);
    
    char buf[64];
    snprintf(buf, sizeof(buf), "Language set to: %s", language);
    events_emit("INFO", buf);
}

const char *transcription_get_language(void) {
    return g_language;
}

bool transcription_is_ready(void) {
    return g_model_loaded && g_ctx && (g_state != TRANSCRIBE_STATE_ERROR);
}

transcribe_state_t transcription_get_state(void) {
    return g_state;
}

const transcription_t *transcription_get_last_result(void) {
    return &g_last_result;
}

// ============================================================================
// Model Info
// ============================================================================

void transcription_print_info(void) {
    if (!g_ctx) {
        printf("No model loaded\n");
        return;
    }
    
    printf("Model info:\n");
    printf("  Is multilingual: %s\n", whisper_is_multilingual(g_ctx) ? "yes" : "no");
    printf("  Language: %s\n", g_language);
    printf("  Model loaded: %s\n", g_model_path);
}

#endif // TRANSCRIBE_C
