/*
 * Simple test binary - verifies build system works
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "0.1.0"

int main(int argc, char *argv[]) {
    printf("Whisper Recorder v%s\n", VERSION);
    printf("Build test successful!\n");
    
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("Version: %s\n", VERSION);
        return 0;
    }
    
    printf("\nUsage: test.exe [--version]\n");
    printf("\nThis is a placeholder binary.\n");
    printf("For full build, compile in WSL with POSIX headers.\n");
    
    return 0;
}
