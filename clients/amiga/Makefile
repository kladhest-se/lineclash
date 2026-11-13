AMIGA_TOOLS ?= /Users/frossmant/Development/tools/amiga/m68k-amigaos-gcc-16.2
CC = $(AMIGA_TOOLS)/bin/m68k-amigaos-gcc
CPU ?= 68020
CFLAGS = -std=gnu99 -O2 -m$(CPU) -msoft-float -noixemul -DPLATFORM_AMIGA -I../gui/src -I../../shared/include -MMD -MP -Wall
LDFLAGS = -m$(CPU) -msoft-float -noixemul
SOURCES = main.c render.c audio.c directory.c https.c clock.c ../gui/src/client.c ../../shared/src/net.c ../../shared/src/field.c ../../shared/src/piece.c ../../shared/src/protocol.c
OBJECTS = $(addprefix build/,$(notdir $(SOURCES:.c=.o)))
.PHONY: all clean
all: LineClash
LineClash: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ -lm -lamisslstubs
build:
	mkdir -p build
build/%.o: %.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/%.o: ../gui/src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/%.o: ../../shared/src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -rf build LineClash
-include $(OBJECTS:.o=.d)
