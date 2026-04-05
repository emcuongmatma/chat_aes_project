CC = gcc
CFLAGS = -Wall `pkg-config --cflags gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0` -lpthread

# Paths
SHARED = ../client_shared.c
CLI_SRC = ../client.c
GUI_SRC = main.c

all: client_cli client_gui

# Client CLI
client_cli: $(CLI_SRC) $(SHARED)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

# Client GUI
client_gui: $(GUI_SRC) $(SHARED)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f client_cli client_gui
