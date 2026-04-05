CC = gcc
CFLAGS = -Wall `pkg-config --cflags gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0` -lpthread

# Targets
all: client_cli client_gui

# Client CLI (Your original code, adapted)
client_cli: ../client.c ../client_shared.c
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

# Client GUI
client_gui: main.c ../client_shared.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f client_cli client_gui
