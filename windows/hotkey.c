/*
 * Whisper Recorder - Windows UI Component
 * 
 * Floating status window with activity log and system tray
 * 
 * Build: gcc hotkey.c -o hotkey.exe -luser32 -lws2_32 -lwinmm -lgdi32 -lshell32 -lcomctl32
 */

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")

// ============================================================================
// Constants
// ============================================================================

#define VERSION "0.1.0"
#define DEFAULT_PORT 8765
#define DEFAULT_HOST "127.0.0.1"
#define WM_TRAYICON (WM_USER + 1)
#define LOG_LINES 5

// ============================================================================
// Types
// ============================================================================

typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PAUSED
} app_state_t;

typedef struct {
    char lines[LOG_LINES][256];
    int head;
    int count;
} log_buffer_t;

// ============================================================================
// Globals
// ============================================================================

static HWND g_hwnd = NULL;
static HWND g_status_hwnd = NULL;
static HWND g_log_hwnd = NULL;
static HWND g_time_hwnd = NULL;
static HMENU g_tray_menu = NULL;
static NOTIFYICONDATA g_nid = {0};
static SOCKET g_sock = INVALID_SOCKET;
static app_state_t g_state = STATE_IDLE;
static char g_session_name[256] = {0};
static char g_server_host[64] = DEFAULT_HOST;
static int g_server_port = DEFAULT_PORT;
static log_buffer_t g_log = {0};
static int g_minimized_to_tray = 0;
static WINDOWPLACEMENT g_wp = {0};

// ============================================================================
// Logging
// ============================================================================

static void log_add(const char *msg) {
    strncpy(g_log.lines[g_log.head], msg, 255);
    g_log.lines[g_log.head][255] = '\0';
    g_log.head = (g_log.head + 1) % LOG_LINES;
    if (g_log.count < LOG_LINES) g_log.count++;
    
    // Update log window
    if (g_log_hwnd) {
        InvalidateRect(g_log_hwnd, NULL, TRUE);
        UpdateWindow(g_log_hwnd);
    }
}

static void log_init(void) {
    memset(&g_log, 0, sizeof(g_log));
}

static const char *log_get(int idx) {
    if (idx >= g_log.count) return "";
    int pos = (g_log.head - g_log.count + idx + LOG_LINES) % LOG_LINES;
    return g_log.lines[pos];
}

// ============================================================================
// Network
// ============================================================================

static int init_network(void) {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return -1;
    }
    return 0;
}

static int connect_to_server(void) {
    if (g_sock != INVALID_SOCKET) {
        return 0;
    }
    
    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_sock == INVALID_SOCKET) {
        return -1;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((USHORT)g_server_port);
    inet_pton(AF_INET, g_server_host, &addr.sin_addr);
    
    if (connect(g_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
        return -1;
    }
    
    return 0;
}

static void disconnect_from_server(void) {
    if (g_sock != INVALID_SOCKET) {
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
    }
}

static int send_command(const char *cmd) {
    if (connect_to_server() != 0) {
        log_add("Not connected to server");
        return -1;
    }
    
    char buf[512];
    snprintf(buf, sizeof(buf), "%s\r\n", cmd);
    
    int len = (int)strlen(buf);
    int sent = send(g_sock, buf, len, 0);
    
    if (sent != len) {
        log_add("Send failed");
        return -1;
    }
    
    // Read response
    char resp[256];
    int n = recv(g_sock, resp, sizeof(resp) - 1, 0);
    if (n > 0) {
        resp[n] = '\0';
        log_add(resp);
    }
    
    return 0;
}

// ============================================================================
// UI Drawing
// ============================================================================

static void draw_log(HWND hwnd, HDC hdc) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    
    // Dark background
    HBRUSH bg = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &rect, bg);
    DeleteObject(bg);
    
    // Text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(200, 200, 200));
    
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SelectObject(hdc, hFont);
    
    int line_height = 18;
    int y = 5;
    
    for (int i = 0; i < LOG_LINES; i++) {
        const char *text = log_get(i);
        TextOutA(hdc, 10, y, text, (int)strlen(text));
        y += line_height;
    }
}

static void update_status(void) {
    if (!g_status_hwnd) return;
    
    char status[64];
    const char *state_str;
    
    switch (g_state) {
        case STATE_RECORDING:
            state_str = "REC";
            break;
        case STATE_PAUSED:
            state_str = "PAUSE";
            break;
        default:
            state_str = "IDLE";
            break;
    }
    
    if (g_state == STATE_RECORDING) {
        snprintf(status, sizeof(status), "● %s  %s", state_str, g_session_name);
    } else {
        snprintf(status, sizeof(status), "○ %s", state_str);
    }
    
    SetWindowText(g_status_hwnd, status);
    
    // Update tooltip
    char tooltip[256];
    snprintf(tooltip, sizeof(tooltip), "Whisper Recorder - %s", state_str);
    strncpy(g_nid.szTip, tooltip, sizeof(g_nid.szTip) - 1);
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
}

// ============================================================================
// Window Procedure
// ============================================================================

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Status text
            g_status_hwnd = CreateWindowEx(
                0, "STATIC", "○ IDLE",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 10, 280, 20,
                hwnd, NULL, GetModuleHandle(NULL), NULL);
            
            // Time display
            g_time_hwnd = CreateWindowEx(
                0, "STATIC", "00:00:00",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                10, 35, 280, 20,
                hwnd, NULL, GetModuleHandle(NULL), NULL);
            
            // Log area
            g_log_hwnd = CreateWindowEx(
                0, "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 60, 280, 90,
                hwnd, NULL, GetModuleHandle(NULL), NULL);
            
            log_add("Whisper Recorder started");
            log_add("Press Ctrl+Space to toggle");
            
            // Set timer for clock update
            SetTimer(hwnd, 1, 1000, NULL);
            return 0;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(g_log_hwnd, &ps);
            draw_log(g_log_hwnd, hdc);
            EndPaint(g_log_hwnd, &ps);
            return 0;
        }
        
        case WM_TIMER: {
            if (wParam == 1) {
                // Update clock
                char time_str[32];
                time_t now = time(NULL);
                struct tm *tm_info = localtime(&now);
                strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
                SetWindowText(g_time_hwnd, time_str);
            }
            return 0;
        }
        
        case WM_HOTKEY: {
            if (wParam == 1) {
                // Toggle recording
                if (g_state == STATE_IDLE) {
                    // Start recording
                    time_t now = time(NULL);
                    struct tm *tm_info = localtime(&now);
                    strftime(g_session_name, sizeof(g_session_name), 
                             "%Y-%m-%d-%H-%M-%S", tm_info);
                    
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd), "START:%s", g_session_name);
                    send_command(cmd);
                    
                    g_state = STATE_RECORDING;
                    log_add("Recording started");
                } else {
                    // Stop recording
                    send_command("STOP");
                    g_state = STATE_IDLE;
                    log_add("Recording stopped");
                }
                update_status();
            }
            return 0;
        }
        
        case WM_CLOSE: {
            // Minimize to tray instead of closing
            ShowWindow(hwnd, SW_HIDE);
            g_minimized_to_tray = 1;
            return 0;
        }
        
        case WM_DESTROY: {
            KillTimer(hwnd, 1);
            UnregisterHotKey(hwnd, 1);
            disconnect_from_server();
            PostQuitMessage(0);
            return 0;
        }
        
        case WM_TRAYICON: {
            if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                TrackPopupMenu(g_tray_menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            } else if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hwnd, SW_RESTORE);
                g_minimized_to_tray = 0;
            }
            return 0;
        }
        
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            
            if (id == 1) {  // Show
                ShowWindow(hwnd, SW_RESTORE);
                g_minimized_to_tray = 0;
            } else if (id == 2) {  // Start
                if (g_state == STATE_IDLE) {
                    time_t now = time(NULL);
                    struct tm *tm_info = localtime(&now);
                    strftime(g_session_name, sizeof(g_session_name), 
                             "%Y-%m-%d-%H-%M-%S", tm_info);
                    
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd), "START:%s", g_session_name);
                    send_command(cmd);
                    
                    g_state = STATE_RECORDING;
                    log_add("Recording started");
                    update_status();
                }
            } else if (id == 3) {  // Stop
                if (g_state != STATE_IDLE) {
                    send_command("STOP");
                    g_state = STATE_IDLE;
                    log_add("Recording stopped");
                    update_status();
                }
            } else if (id == 4) {  // Exit
                DestroyWindow(hwnd);
            }
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Tray Icon
// ============================================================================

static int create_tray_icon(HWND hwnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strncpy(g_nid.szTip, "Whisper Recorder - IDLE", sizeof(g_nid.szTip) - 1);
    
    return Shell_NotifyIcon(NIM_ADD, &g_nid) ? 0 : -1;
}

static void create_tray_menu(HWND hwnd) {
    g_tray_menu = CreatePopupMenu();
    
    AppendMenu(g_tray_menu, MF_STRING, 1, "Show Window");
    AppendMenu(g_tray_menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(g_tray_menu, MF_STRING, 2, "Start Recording");
    AppendMenu(g_tray_menu, MF_STRING, 3, "Stop Recording");
    AppendMenu(g_tray_menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(g_tray_menu, MF_STRING, 4, "Exit");
}

// ============================================================================
// Usage
// ============================================================================

static void print_usage(const char *program) {
    printf("Whisper Recorder UI - Windows Component\n");
    printf("\n");
    printf("Usage: %s [options]\n", program);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help        Show this help\n");
    printf("  --host <addr>     Server host (default: 127.0.0.1)\n");
    printf("  --port <port>     Server port (default: 8765)\n");
    printf("\n");
    printf("Global hotkey: Ctrl+Space\n");
}

// ============================================================================
// Main
// ============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    // Parse command line
    for (int i = 1; i < __argc; i++) {
        char *arg = __argv[i];
        
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(__argv[0]);
            return 0;
        }
        if (strcmp(arg, "--host") == 0 && i + 1 < __argc) {
            strncpy(g_server_host, __argv[++i], sizeof(g_server_host) - 1);
        }
        if (strcmp(arg, "--port") == 0 && i + 1 < __argc) {
            g_server_port = atoi(__argv[++i]);
        }
    }
    
    // Init network
    if (init_network() != 0) {
        MessageBox(NULL, "Failed to initialize network", "Error", MB_ICONERROR);
        return 1;
    }
    
    // Register window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "WhisperRecorder";
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "Failed to register window class", "Error", MB_ICONERROR);
        return 1;
    }
    
    // Create window
    g_hwnd = CreateWindowEx(
        WS_EX_TOPMOST,  // Always on top
        "WhisperRecorder",
        "Whisper Recorder",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        320, 200,  // Width, Height
        NULL, NULL, hInstance, NULL
    );
    
    if (!g_hwnd) {
        MessageBox(NULL, "Failed to create window", "Error", MB_ICONERROR);
        return 1;
    }
    
    // Create tray
    log_init();
    create_tray_icon(g_hwnd);
    create_tray_menu(g_hwnd);
    
    // Register hotkey: Ctrl+Space
    if (!RegisterHotKey(g_hwnd, 1, MOD_CONTROL, VK_SPACE)) {
        MessageBox(NULL, 
                   "Failed to register Ctrl+Space hotkey.\n"
                   "The hotkey may be in use by another application.",
                   "Warning", MB_ICONWARNING);
    }
    
    // Show window
    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Cleanup
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
    disconnect_from_server();
    WSACleanup();
    
    return 0;
}
