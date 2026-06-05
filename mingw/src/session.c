/*
 * Session Management
 *
 * Handles automatic session splitting based on silence timeout and max duration
 */

#ifndef SESSION_C
#define SESSION_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

// ============================================================================
// Configuration
// ============================================================================

typedef struct {
    int silence_timeout_ms;    // Split after N ms of silence
    int max_duration_ms;      // Split after N ms total
    int debounce_ms;          // Ignore splits shorter than this
} session_config_t;

static session_config_t g_config = {
    .silence_timeout_ms = 60000,   // 60 seconds
    .max_duration_ms = 600000,     // 10 minutes
    .debounce_ms = 500            // 500ms debounce
};

// ============================================================================
// State
// ============================================================================

typedef struct {
    char name[256];
    time_t start_time;
    time_t last_speech_time;
    int segment_count;
    bool has_content;
} session_info_t;

static session_info_t g_session = {0};
static bool g_in_session = false;
static int g_session_counter = 0;
static time_t g_last_split_time = 0;

// ============================================================================
// Event System (forward declaration)
// ============================================================================

#ifndef EVENTS_EMIT_DECLARED
#define EVENTS_EMIT_DECLARED
extern void events_emit(const char *event, const char *data);
#endif

// Forward decl: session_start() calls session_end()
void session_end(void);

// ============================================================================
// Utility
// ============================================================================

static long get_time_ms(void) {
#ifdef _WIN32
    SYSTEMTIME st;
    GetSystemTime(&st);
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    ULONGLONG ms = (((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000;
    return (long)ms;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static void generate_session_name(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d-%H-%M-%S", tm_info);
    
    if (g_session_counter > 0) {
        snprintf(buf, len, "session-%s-%d", time_str, g_session_counter);
    } else {
        snprintf(buf, len, "session-%s", time_str);
    }
}

static bool should_debounce_split(void) {
    if (g_last_split_time == 0) return false;
    
    long now = get_time_ms();
    long elapsed = now - g_last_split_time;
    
    return elapsed < g_config.debounce_ms;
}

// ============================================================================
// Public API
// ============================================================================

void session_config_set_silence_timeout(int ms) {
    g_config.silence_timeout_ms = ms;
}

void session_config_set_max_duration(int ms) {
    g_config.max_duration_ms = ms;
}

void session_config_set_debounce(int ms) {
    g_config.debounce_ms = ms;
}

bool session_should_split(time_t last_speech, int segment_count) {
    // Check debounce
    if (should_debounce_split()) {
        return false;
    }
    
    // Check for content
    if (segment_count < 1) {
        return false;
    }
    
    // Check silence timeout
    if (last_speech > 0) {
        time_t now = time(NULL);
        long silence_seconds = (now - last_speech);
        
        if (silence_seconds * 1000 > g_config.silence_timeout_ms) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Silence timeout: %lds > %dms",
                     silence_seconds, g_config.silence_timeout_ms);
            events_emit("WARN", buf);
            return true;
        }
    }
    
    // Check max duration
    if (g_session.start_time > 0) {
        time_t now = time(NULL);
        long duration_seconds = (now - g_session.start_time);
        
        if (duration_seconds * 1000 > g_config.max_duration_ms) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Max duration: %lds > %dms",
                     duration_seconds, g_config.max_duration_ms);
            events_emit("WARN", buf);
            return true;
        }
    }
    
    return false;
}

void session_start(const char *name) {
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
    events_emit("session_started", g_session.name);
}

void session_end(void) {
    if (!g_in_session) {
        return;
    }
    
    g_last_split_time = get_time_ms();
    
    // Check if session should be deleted (no content)
    if (!g_session.has_content || g_session.segment_count < 1) {
        events_emit("INFO", "Empty session, discarding");
        events_emit("session_empty", g_session.name);
    }
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Session ended: %s (%d segments)",
             g_session.name, g_session.segment_count);
    events_emit("INFO", buf);
    events_emit("session_ended", g_session.name);
    
    g_in_session = false;
}

void session_on_speech_detected(void) {
    if (!g_in_session) {
        return;
    }
    
    g_session.last_speech_time = time(NULL);
    g_session.has_content = true;
}

void session_on_segment_complete(void) {
    if (!g_in_session) {
        return;
    }
    
    g_session.segment_count++;
}

bool session_should_auto_split(void) {
    if (!g_in_session) {
        return false;
    }
    
    return session_should_split(g_session.last_speech_time, 
                                g_session.segment_count);
}

const char *session_get_name(void) {
    return g_session.name;
}

bool session_is_active(void) {
    return g_in_session;
}

int session_get_segment_count(void) {
    return g_session.segment_count;
}

time_t session_get_duration(void) {
    if (!g_in_session || g_session.start_time == 0) {
        return 0;
    }
    return time(NULL) - g_session.start_time;
}

void session_next_counter(void) {
    g_session_counter++;
}

void session_reset_counter(void) {
    g_session_counter = 0;
}

#endif // SESSION_C
