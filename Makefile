CC = gcc
CFLAGS = -g
# CFLAGS += -Wall -Wextra
LOADLIBES=-lm -I/usr/include/SDL2 -D_REENTRANT -lSDL2 -lm -lSDL2_image -lSDL2_ttf -lSDL2_mixer

EXECUTABLES=vlk3d

.PHONY: all
all: $(EXECUTABLES)

.PHONY: clean
clean:
	rm -vf *.o $(EXECUTABLES)
