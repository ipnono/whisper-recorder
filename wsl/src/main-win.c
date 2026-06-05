/*
 * Whisper Recorder - WSL Build (main.c)
 * 
 * Note: This file uses POSIX sockets. Compile for WSL/Linux.
 * For Windows, use main-win.c with Winsock2.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#define VERSION "0.1.0"
#define DEFAULT_PORT 8765
#define BUFFER_SIZE 1024

// ============================================================================
// Forward declarations
// ============================================================================

void events_emit(const char *event, const char *data);

// ============================================================================
// Configuration
// ============================================================================

typedef struct {
    char model_path[512];
    bool auto_download;
    int sample_rate;
    int channels;
    int bit_depth;
    float vad_threshold;
    int vad_min_speech_ms;
    int vad_min_silence_ms;
    int vad_speech_pad_ms;
    int session_silence_timeout_ms;
    int max_session_duration_ms;
    char output_dir[512];
    bool create_date_dirs;
    char server_host[64];
    int server_port;
    char language[8];
    char log_level[16];
    char log_file[256];
} config_t;

static config_t g_config;

static void config_set_defaults(config_t *cfg) {
    strncpy(cfg->model_path, "~/.whisper/models/ggml-base.bin", sizeof(cfg->model_path) - 1);
    cfg->auto_download = true;
    cfg->sample_rate = 16000;
    cfg->channels = 1;
    cfg->bit_depth = 16;
    cfg->vad_threshold = 0.5f;
    cfg->vad_min_speech_ms = 250;
    cfg->vad_min_silence_ms = 100;
    cfg->vad_speech_pad_ms = 30;
    cfg->session_silence_timeout_ms = 60000;
    cfg->max_session_duration_ms = 600000;
    strncpy(cfg->output_dir, "D:/recordings", sizeof(cfg->output_dir) - 1);
    cfg->create_date_dirs = true;
    strncpy(cfg->server_host, "127.0.0.1", sizeof(cfg->server_host) - 1);
    cfg->server_port = DEFAULT_PORT;
    strncpy(cfg->language, "zh", sizeof(cfg->language) - 1);
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

// ============================================================================
// Event system
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

void events_emit(const char *event, const char *data) {
    log_event(event, data);
}

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
    printf("  -m, --model <path>     Model file path\n");
    printf("  -o, --output <path>     Output directory\n");
    printf("  -l, --language <lang>   Language (en/zh)\n");
    printf("  -p, --port <port>       TCP server port (default: %d)\n", DEFAULT_PORT);
    printf("\n");
    printf("Commands (via TCP):\n");
    printf("  START:<name>    Start recording session\n");
    printf("  STOP           Stop recording\n");
    printf("  PAUSE          Pause recording\n");
    printf("  RESUME         Resume recording\n");
    printf("  STATUS         Get status\n");
    printf("\n");
}

static void print_version(void) {
    printf("whisper-recorder %s\n", VERSION);
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

static int g_listen_fd = -1;

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
    
    memset(&addr, 0, sizeof(addr));
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

static void handle_command(const char *cmd, int client_fd) {
    char response[BUFFER_SIZE];
    
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
            state_to_string(g_state), g_session_name, g_config.language);
        send(client_fd, response, strlen(response), 0);
        
    } else {
        snprintf(response, sizeof(response), "ERROR: Unknown command\n");
        send(client_fd, response, strlen(response), 0);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[]) {
    config_set_defaults(&g_config);
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
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
            if (i + 1 < argc) g_config.server_port = atoi(argv[++i]);
        }
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    g_listen_fd = create_server(g_config.server_port);
    if (g_listen_fd < 0) {
        fprintf(stderr, "Failed to create server on port %d\n", g_config.server_port);
        return 1;
    }
    
    printf("Whisper Recorder v%s\n", VERSION);
    printf("Listening on 127.0.0.1:%d\n", g_config.server_port);
    printf("Press Ctrl+C to stop\n");
    events_emit("INFO", "Server started");
    
    struct pollfd fds[1];
    fds[0].fd = g_listen_fd;
    fds[0].events = POLLIN;
    
    while (g_running) {
        fds[0].revents = 0;
        int ready = poll(fds, 1, 1000);
        if (ready < 0) {
            if (!g_running) break;
            continue;
        }
        if (ready == 0) continue;
        
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(g_listen_fd, (struct sockaddr *)&client_addr, &client_len);
            
            if (client_fd < 0) continue;
            
            printf("Client connected from %s:%d\n",
                   inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port));
            
            char buffer[BUFFER_SIZE];
            ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            
            if (n > 0) {
                buffer[n] = '\0';
                while (n > 0 && (buffer[n-1] == '\n' || buffer[n-1] == '\r')) {
                    buffer[--n] = '\0';
                }
                printf("Command: %s\n", buffer);
                handle_command(buffer, client_fd);
            }
            close(client_fd);
        }
    }
    
    printf("\nShutting down...\n");
    if (g_listen_fd >= 0) close(g_listen_fd);
    return 0;
}
