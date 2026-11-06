CC = gcc

CFLAGS = -Wall -Wextra -std=c17 -g
CFLAGS += $(shell sdl2-config --cflags)

LDLIBS = $(shell sdl2-config --libs)

TARGET = pacman-emu

SOURCES = \
	src/main.c \
	src/emulator/emulator.c \
	src/memory/memory.c \
	src/video/video.c \
	src/video/palette.c \
	src/video/tilemap.c \
	src/video/sprites.c \
	src/plataform/sdl_video.c \
	src/bus/bus.c \
	src/cpu/z80/z80.c \
	src/rom/rom_loader.c \
	src/rom/rom_db.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
