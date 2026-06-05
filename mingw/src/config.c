#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define VERSION "0.1.0"
#define DEFAULT_PORT 8765
#define BUFFER_SIZE 1024

// ============================================================================
// Configuration
// ============================================================================

typedef struct {
    // Model
    char model_path[512];
    bool auto_download;
    char download_url[512];
    
    // Audio
    int sample_rate;
    int channels;
    int bit_depth;
    
    // VAD
    float vad_threshold;
    int vad_min_speech_duration_ms;
    int vad_min_silence_duration_ms;
    int vad_speech_pad_ms;
    int session_silence_timeout_ms;
    int max_session_duration_ms;
    
    // Output
    char output_dir[512];
    bool create_date_dirs;
    
    // Server
    char server_host[64];
    int server_port;
    
    // Language
    char language[8];
    
    // Log
    char log_level[16];
    char log_file[256];
} config_t;

static config_t g_config;

static void config_set_defaults(config_t *cfg) {
    // Model
    strncpy(cfg->model_path, "~/.whisper/models/ggml-base.bin", sizeof(cfg->model_path) - 1);
    cfg->auto_download = true;
    strncpy(cfg->download_url, 
            "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin",
            sizeof(cfg->download_url) - 1);
    
    // Audio
    cfg->sample_rate = 16000;
    cfg->channels = 1;
    cfg->bit_depth = 16;
    
    // VAD
    cfg->vad_threshold = 0.5f;
    cfg->vad_min_speech_duration_ms = 250;
    cfg->vad_min_silence_duration_ms = 100;
    cfg->vad_speech_pad_ms = 30;
    cfg->session_silence_timeout_ms = 60000;
    cfg->max_session_duration_ms = 600000;
    
    // Output
    strncpy(cfg->output_dir, "D:/recordings", sizeof(cfg->output_dir) - 1);
    cfg->create_date_dirs = true;
    
    // Server
    strncpy(cfg->server_host, "127.0.0.1", sizeof(cfg->server_host) - 1);
    cfg->server_port = DEFAULT_PORT;
    
    // Language
    strncpy(cfg->language, "zh", sizeof(cfg->language) - 1);
    
    // Log
    strncpy(cfg->log_level, "INFO", sizeof(cfg->log_level) - 1);
    strncpy(cfg->log_file, "whisper-recorder.log", sizeof(cfg->log_file) - 1);
}

// ============================================================================
// State machine
// ============================================================================

typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PAUSED
} state_t;

static volatile sig_atomic_t g_running = 1;
static state_t g_state = STATE_IDLE;
static char g_session_name[256] = {0};
static char g_language[8] = "zh";
static int g_listen_port = DEFAULT_PORT;

// ============================================================================
// Event system
// ============================================================================

typedef void (*event_callback_t)(const char *event, const char *data);
static event_callback_t g_event_callback = NULL;

void events_on(event_callback_t cb) {
    g_event_callback = cb;
}

void events_emit(const char *event, const char *data) {
    if (g_event_callback) {
        g_event_callback(event, data);
    }
    printf("[%s] %s\n", event, data ? data : "");
}

// ============================================================================
// Forward declarations
// ============================================================================

static void print_usage(const char *program);
static const char *state_to_string(state_t s);
static void handle_command(const char *cmd, int client_fd);

// ============================================================================
// CLI / Config
// ============================================================================

static void print_usage(const char *program) {
    printf("Whisper Recorder - Real-time voice transcription tool\n");
    printf("\n");
    printf("Usage: %s [options]\n", program);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --version           Show version information\n");
    printf("  -c, --config <path>     Config file path (default: config.json)\n");
    printf("  -m, --model <path>     Model file path\n");
    printf("  -o, --output <path>    Output directory\n");
    printf("  -l, --language <lang>  Language (en/zh, default: zh)\n");
    printf("  -p, --port <port>      TCP server port (default: %d)\n", DEFAULT_PORT);
    printf("  -v, --verbose           Enable verbose logging\n");
    printf("  -d, --daemon           Run as daemon\n");
    printf("\n");
    printf("Commands (via TCP):\n");
    printf("  START:<name>   Start recording session\n");
    printf("  STOP          Stop recording\n");
    printf("  PAUSE         Pause recording\n");
    printf("  RESUME        Resume recording\n");
    printf("  STATUS        Get current status (JSON)\n");
    printf("  SET_LANG:<lang> Set language\n");
    printf("\n");
}

static void print_version(void) {
    printf("whisper-recorder %s\n", VERSION);
}

static void print_config(const config_t *cfg) {
    printf("Current configuration:\n");
    printf("  Model: %s\n", cfg->model_path);
    printf("  Auto-download: %s\n", cfg->auto_download ? "yes" : "no");
    printf("  Sample rate: %d\n", cfg->sample_rate);
    printf("  Channels: %d\n", cfg->channels);
    printf("  VAD threshold: %.2f\n", cfg->vad_threshold);
    printf("  Output dir: %s\n", cfg->output_dir);
    printf("  Server: %s:%d\n", cfg->server_host, cfg->server_port);
    printf("  Language: %s\n", cfg->language);
}

static const char *state_to_string(state_t s) {
    switch (s) {
        case STATE_IDLE: return "IDLE";
        case STATE_RECORDING: return "RECORDING";
        case STATE_PAUSED: return "PAUSED";
        default: return "UNKNOWN";
    }
}

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

// ============================================================================
// TCP Server
// ============================================================================

static void handle_command(const char *cmd, int client_fd) {
    char response[BUFFER_SIZE];
    
    // Parse command
    if (strncmp(cmd, "START:", 6) == 0) {
        const char *name = cmd + 6;
        strncpy(g_session_name, name, sizeof(g_session_name) - 1);
        g_state = STATE_RECORDING;
        
        snprintf(response, sizeof(response), "OK: Session started: %s\n", name);
        send(client_fd, response, strlen(response), 0);
        
        events_emit("INFO", "Session started");
        events_emit("session_started", name);
        
    } else if (strcmp(cmd, "STOP") == 0) {
        g_state = STATE_IDLE;
        
        snprintf(response, sizeof(response), "OK: Session stopped\n");
        send(client_fd, response, strlen(response), 0);
        
        events_emit("INFO", "Session stopped");
        events_emit("session_ended", g_session_name);
        
    } else if (strcmp(cmd, "PAUSE") == 0) {
        if (g_state == STATE_RECORDING) {
            g_state = STATE_PAUSED;
            snprintf(response, sizeof(response), "OK: Paused\n");
            events_emit("INFO", "Recording paused");
        } else {
            snprintf(response, sizeof(response), "ERROR: Not recording\n");
        }
        send(client_fd, response, strlen(response), 0);
        
    } else if (strcmp(cmd, "RESUME") == 0) {
        if (g_state == STATE_PAUSED) {
            g_state = STATE_RECORDING;
            snprintf(response, sizeof(response), "OK: Resumed\n");
            events_emit("INFO", "Recording resumed");
        } else {
            snprintf(response, sizeof(response), "ERROR: Not paused\n");
        }
        send(client_fd, response, strlen(response), 0);
        
    } else if (strcmp(cmd, "STATUS") == 0) {
        snprintf(response, sizeof(response),
            "{\"state\":\"%s\",\"session\":\"%s\",\"language\":\"%s\"}\n",
            state_to_string(g_state),
            g_session_name,
            g_language);
        send(client_fd, response, strlen(response), 0);
        
    } else if (strncmp(cmd, "SET_LANG:", 9) == 0) {
        const char *lang = cmd + 9;
        strncpy(g_language, lang, sizeof(g_language) - 1);
        strncpy(g_config.language, lang, sizeof(g_config.language) - 1);
        
        snprintf(response, sizeof(response), "OK: Language set to %s\n", lang);
        send(client_fd, response, strlen(response), 0);
        
        events_emit("INFO", "Language changed");
        
    } else if (strcmp(cmd, "CONFIG") == 0) {
        // Return current config as JSON
        snprintf(response, sizeof(response),
            "{\"model\":\"%s\",\"output\":\"%s\",\"language\":\"%s\",\"port\":%d}\n",
            g_config.model_path,
            g_config.output_dir,
            g_config.language,
            g_config.server_port);
        send(client_fd, response, strlen(response), 0);
        
    } else {
        snprintf(response, sizeof(response), "ERROR: Unknown command: %s\n", cmd);
        send(client_fd, response, strlen(response), 0);
    }
}

static int create_server(int port) {
    int sockfd;
    struct sockaddr_in addr;
    int opt = 1;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons((unsigned short)port);
    
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }
    
    if (listen(sockfd, 1) < 0) {
        perror("listen");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[]) {
    int daemon_mode = 0;
    int verbose = 0;
    char *config_path = NULL;
    
    // Set defaults
    config_set_defaults(&g_config);
    
    // Parse args (overrides config)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                config_path = argv[++i];
            }
        }
        if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--model") == 0) {
            if (i + 1 < argc) {
                strncpy(g_config.model_path, argv[++i], sizeof(g_config.model_path) - 1);
            }
        }
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                strncpy(g_config.output_dir, argv[++i], sizeof(g_config.output_dir) - 1);
            }
        }
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--language") == 0) {
            if (i + 1 < argc) {
                strncpy(g_config.language, argv[++i], sizeof(g_config.language) - 1);
                strncpy(g_language, argv[i], sizeof(g_language) - 1);
            }
        }
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                g_config.server_port = atoi(argv[++i]);
                g_listen_port = g_config.server_port;
            }
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
            strncpy(g_config.log_level, "DEBUG", sizeof(g_config.log_level) - 1);
        }
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--daemon") == 0) {
            daemon_mode = 1;
        }
        if (strcmp(argv[i], "--print-config") == 0) {
            print_config(&g_config);
            return 0;
        }
    }
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create server socket
    int sockfd = create_server(g_listen_port);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to create server on port %d\n", g_listen_port);
        return 1;
    }
    
    printf("Whisper Recorder v%s\n", VERSION);
    if (verbose) {
        print_config(&g_config);
    }
    printf("Listening on 127.0.0.1:%d\n", g_listen_port);
    printf("Press Ctrl+C to stop\n");
    events_emit("INFO", "Server started");
    
    // Main loop
    struct pollfd fds[2];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;
    
    while (g_running) {
        fds[0].revents = 0;
        
        int ready = poll(fds, 1, 1000);  // 1 second timeout
        if (ready < 0) {
            if (!g_running) break;
            perror("poll");
            continue;
        }
        
        if (ready == 0) {
            // Timeout - check for running flag
            continue;
        }
        
        if (fds[0].revents & POLLIN) {
            // New connection
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
            
            if (client_fd < 0) {
                perror("accept");
                continue;
            }
            
            printf("Client connected from %s:%d\n",
                   inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port));
            
            // Handle client in a simple echo-like manner
            char buffer[BUFFER_SIZE];
            ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            
            if (n > 0) {
                buffer[n] = '\0';
                
                // Remove trailing newlines/carriage returns
                while (n > 0 && (buffer[n-1] == '\n' || buffer[n-1] == '\r')) {
                    buffer[--n] = '\0';
                }
                
                printf("Received command: %s\n", buffer);
                handle_command(buffer, client_fd);
            }
            
            close(client_fd);
        }
    }
    
    printf("\nShutting down...\n");
    events_emit("INFO", "Server stopped");
    close(sockfd);
    
    return 0;
}
