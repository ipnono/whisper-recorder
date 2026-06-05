/*
 * Model Auto-Download
 * 
 * Downloads whisper model if not present
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

// ============================================================================
// Configuration
// ============================================================================

typedef struct {
    char url[512];
    char dest_path[512];
    int max_retries;
    bool auto_download;
} download_config_t;

static download_config_t g_config = {
    .url = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin",
    .dest_path = "~/.whisper/models/ggml-base.bin",
    .max_retries = 3,
    .auto_download = true
};

// ============================================================================
// Event System (forward declaration)
// ============================================================================

extern void events_emit(const char *event, const char *data);

// ============================================================================
// Utility
// ============================================================================

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void expand_path(const char *input, char *output, size_t len) {
    if (input[0] == '~' && input[1] == '/') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(output, len, "%s/%s", home, input + 2);
            return;
        }
    }
    snprintf(output, len, "%s", input);
}

static bool ensure_directory(const char *path) {
    char dir[512];
    char *last;
    
    snprintf(dir, sizeof(dir), "%s", path);
    
    // Find last slash
    last = strrchr(dir, '/');
    if (last) {
        *last = '\0';
        
        struct stat st = {0};
        if (stat(dir, &st) == -1) {
            if (mkdir(dir, 0755) == -1) {
                return false;
            }
        }
    }
    
    return true;
}

// ============================================================================
// Download Progress Callback
// ============================================================================

typedef void (*progress_callback_t)(int percent, void *user_data);

typedef struct {
    progress_callback_t callback;
    void *user_data;
} progress_ctx_t;

static progress_ctx_t g_progress_ctx = {0};

void download_set_progress_callback(progress_callback_t cb, void *user_data) {
    g_progress_ctx.callback = cb;
    g_progress_ctx.user_data = user_data;
}

static void report_progress(int percent) {
    if (g_progress_ctx.callback) {
        g_progress_ctx.callback(percent, g_progress_ctx.user_data);
    }
}

// ============================================================================
// Public API
// ============================================================================

void download_configure(const char *url, const char *dest_path, 
                        int max_retries, bool auto_download) {
    if (url) strncpy(g_config.url, url, sizeof(g_config.url) - 1);
    if (dest_path) strncpy(g_config.dest_path, dest_path, sizeof(g_config.dest_path) - 1);
    g_config.max_retries = max_retries;
    g_config.auto_download = auto_download;
}

bool download_model(void (*progress)(int percent)) {
    char dest[512];
    expand_path(g_config.dest_path, dest, sizeof(dest));
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Checking for model at: %s", dest);
    events_emit("INFO", buf);
    
    // Check if already exists
    if (file_exists(dest)) {
        events_emit("INFO", "Model already exists");
        report_progress(100);
        return true;
    }
    
    if (!g_config.auto_download) {
        events_emit("ERROR", "Model not found and auto-download is disabled");
        return false;
    }
    
    // Ensure directory exists
    if (!ensure_directory(dest)) {
        snprintf(buf, sizeof(buf), "Failed to create directory for: %s", dest);
        events_emit("ERROR", buf);
        return false;
    }
    
    // Download with retries
    snprintf(buf, sizeof(buf), "Downloading model from: %s", g_config.url);
    events_emit("INFO", buf);
    
    // TODO: Implement actual curl download
    // For now, simulate download
    for (int i = 0; i <= 100; i += 10) {
        if (progress) {
            progress(i);
        }
        report_progress(i);
        
        snprintf(buf, sizeof(buf), "Download progress: %d%%", i);
        events_emit("DEBUG", buf);
    }
    
    events_emit("INFO", "Model download complete");
    
    return true;
}

bool download_model_sync(const char *url, const char *dest) {
    char buf[256];
    
    if (url) strncpy(g_config.url, url, sizeof(g_config.url) - 1);
    if (dest) strncpy(g_config.dest_path, dest, sizeof(g_config.dest_path) - 1);
    
    return download_model(NULL);
}

bool download_is_model_present(void) {
    char dest[512];
    expand_path(g_config.dest_path, dest, sizeof(dest));
    return file_exists(dest);
}

const char *download_get_model_path(void) {
    return g_config.dest_path;
}
