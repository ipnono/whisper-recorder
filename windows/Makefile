CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -luser32 -lws2_32 -lwinmm -lgdi32 -lshell32 -lcomctl32

BIN = hotkey.exe

.PHONY: all clean

all: $(BIN)

$(BIN): hotkey.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(BIN)
