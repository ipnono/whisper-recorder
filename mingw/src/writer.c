/*
 * Markdown Writer
 *
 * Writes transcribed text to session-based Markdown files
 */

#ifndef WRITER_C
#define WRITER_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <errno.h>
#endif

// ============================================================================
// Types
// ============================================================================

typedef struct {
    char session_name[256];
    char file_path[512];
    FILE *file;
    time_t start_time;
    time_t end_time;
    int segment_count;
} session_t;

typedef struct {
    char base_dir[512];
    bool create_date_dirs;
} writer_config_t;

// ============================================================================
// State
// ============================================================================

static session_t g_current_session = {0};
static writer_config_t g_writer_config = {0};
static bool g_in_session = false;

// ============================================================================
// Event System (forward declaration)
// ============================================================================

#ifndef EVENTS_EMIT_DECLARED
#define EVENTS_EMIT_DECLARED
extern void events_emit(const char *event, const char *data);
#endif

// Forward decl: writer_shutdown() calls writer_end_session()
void writer_end_session(void);

// ============================================================================
// Utility
// ============================================================================

static bool ensure_directory(const char *path) {
#ifdef _WIN32
    if (!CreateDirectoryA(path, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            fprintf(stderr, "CreateDirectory failed: %lu\n", err);
            return false;
        }
    }
    return true;
#else
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            perror("mkdir");
            return false;
        }
    }
    return true;
#endif
}

static void get_date_dir(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, len, "%Y-%m-%d", tm_info);
}

static void get_timestamp(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm_info);
}

static void format_duration(char *buf, size_t len, time_t start, time_t end) {
    int total_seconds = (int)(end - start);
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    
    if (hours > 0) {
        snprintf(buf, len, "%02d:%02d:%02d", hours, minutes, seconds);
    } else {
        snprintf(buf, len, "%02d:%02d", minutes, seconds);
    }
}

// ============================================================================
// Public API
// ============================================================================

void writer_init(const char *base_dir, bool create_date_dirs) {
    strncpy(g_writer_config.base_dir, base_dir, sizeof(g_writer_config.base_dir) - 1);
    g_writer_config.create_date_dirs = create_date_dirs;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Writer initialized: %s", base_dir);
    events_emit("INFO", buf);
}

void writer_shutdown(void) {
    if (g_in_session) {
        writer_end_session();
    }
}

bool writer_start_session(const char *session_name) {
    if (g_in_session) {
        writer_end_session();
    }
    
    // Create directory path
    char dir_path[512];
    
    if (g_writer_config.create_date_dirs) {
        char date_dir[32];
        get_date_dir(date_dir, sizeof(date_dir));
        
        snprintf(dir_path, sizeof(dir_path), "%s/%s",
                 g_writer_config.base_dir, date_dir);
    } else {
        snprintf(dir_path, sizeof(dir_path), "%s", g_writer_config.base_dir);
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
        char buf[256];
        snprintf(buf, sizeof(buf), "Failed to open file: %s", file_path);
        events_emit("ERROR", buf);
        return false;
    }
    
    // Write header
    fprintf(f, "# Session: %s\n\n", session_name);
    fprintf(f, "**Started:** %s  \n", ctime(&now));
    fprintf(f, "**Language:** %s  \n", "zh");  // TODO: get from config
    fprintf(f, "**Duration:** 00:00\n\n");
    fprintf(f, "---\n\n");
    fflush(f);
    
    // Store session info
    strncpy(g_current_session.session_name, session_name, 
             sizeof(g_current_session.session_name) - 1);
    strncpy(g_current_session.file_path, file_path,
             sizeof(g_current_session.file_path) - 1);
    g_current_session.file = f;
    g_current_session.start_time = now;
    g_current_session.end_time = now;
    g_current_session.segment_count = 0;
    
    g_in_session = true;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Session started: %s", session_name);
    events_emit("INFO", buf);
    events_emit("session_started", session_name);
    
    return true;
}

void writer_append_text(const char *text) {
    if (!g_in_session || !g_current_session.file) {
        return;
    }
    
    fprintf(g_current_session.file, "%s\n\n", text);
    fflush(g_current_session.file);
    
    g_current_session.segment_count++;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Appended text (segment #%d)", 
             g_current_session.segment_count);
    events_emit("DEBUG", buf);
}

void writer_end_session(void) {
    if (!g_in_session) {
        return;
    }
    
    time_t end = time(NULL);
    g_current_session.end_time = end;
    
    if (g_current_session.file) {
        // Write footer
        fprintf(g_current_session.file, "---\n\n");
        fprintf(g_current_session.file, "**Ended:** %s\n", ctime(&end));
        fprintf(g_current_session.file, "**Transcribed segments:** %d\n",
                g_current_session.segment_count);
        
        fclose(g_current_session.file);
        g_current_session.file = NULL;
    }
    
    char buf[512];
    snprintf(buf, sizeof(buf), "Session ended: %s (%d segments)", 
             g_current_session.session_name,
             g_current_session.segment_count);
    events_emit("INFO", buf);
    events_emit("session_ended", g_current_session.session_name);
    
    g_in_session = false;
}

const char *writer_get_current_file(void) {
    if (g_in_session) {
        return g_current_session.file_path;
    }
    return NULL;
}

bool writer_is_in_session(void) {
    return g_in_session;
}

int writer_get_segment_count(void) {
    return g_current_session.segment_count;
}

#endif // WRITER_C
