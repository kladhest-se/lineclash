CC      = cc
CFLAGS  = -Wall -Wextra -O2 -I../shared/include
SHARED  = ../shared/src/field.c ../shared/src/piece.c \
          ../shared/src/protocol.c ../shared/src/net.c

include ../version.mk
PYTHON ?= python3
SERVER ?= ../server/lineclashd
UNAME := $(shell uname -s 2>/dev/null || echo Windows)
LDFLAGS ?=
ifneq ($(filter Windows MINGW% MSYS% CYGWIN%,$(UNAME)),)
LDFLAGS += -lws2_32
WEB_LIBS = -lwinhttp
C_INTEGRATION =
else
WEB_LIBS = -lcurl
C_INTEGRATION = test_integration
endif

.PHONY: all unit integration clean

all: unit integration

unit: test_field test_net test_auth
	@echo "--- Unit tests ---"
	./test_field
	./test_net
	./test_auth

integration: $(C_INTEGRATION)
	@echo "--- Integration tests ---"
	$(if $(C_INTEGRATION),./test_integration $(SERVER),@echo "POSIX process harness skipped; portable protocol tests run below")
	$(PYTHON) test_http.py $(SERVER)
	$(PYTHON) test_protocol.py $(SERVER)

test_field: test_field.c $(SHARED)
	$(CC) $(CFLAGS) $^ -o $@ -lm $(LDFLAGS)

test_integration: $(C_INTEGRATION).c $(SHARED)
	$(CC) $(CFLAGS) $^ -o $@ -lm $(LDFLAGS)

clean:
	rm -f test_field test_integration test_net test_auth test_gui test_music test_music_ui test_music_decoder test_discovery

test_net: test_net.c $(SHARED)
	$(CC) $(CFLAGS) $^ -o $@ -lm $(LDFLAGS)

test_auth: test_auth.c ../server/src/auth.c ../server/src/config.c $(SHARED)
	$(CC) $(CFLAGS) $^ -o $@ -lm $(LDFLAGS) -lsqlite3

.PHONY: gui client-integration
gui:
	$(MAKE) -C ../clients/gui
	$(CC) $(CFLAGS) test_gui.c ../clients/gui/src/client.o ../clients/gui/src/directory.o ../clients/gui/src/render_sdl.o ../clients/gui/src/sound.o ../clients/gui/src/music.o ../clients/gui/src/music_decoder.o ../clients/gui/src/mod.o ../clients/gui/build/*.o -o test_gui $(LDFLAGS) $$(sdl2-config --libs) $(WEB_LIBS) -lm
	./test_gui

client-integration:
	$(PYTHON) test_cli_reconnect.py ../clients/cli/lineclash-cli

.PHONY: music
music:
	$(PYTHON) ../scripts/check_music.py
	$(MAKE) -C ../clients/gui
	$(CC) $(CFLAGS) $$(sdl2-config --cflags) test_music.c ../clients/gui/src/sound.o ../clients/gui/src/music.o ../clients/gui/src/music_decoder.o -o test_music $(LDFLAGS) $$(sdl2-config --libs) $(WEB_LIBS) -lm
	./test_music

.PHONY: music-ui
music-ui:
	$(MAKE) -C ../clients/gui
	$(CC) $(CFLAGS) $(VERSION_CFLAGS) $$(sdl2-config --cflags) test_music_ui.c ../clients/gui/src/client.o ../clients/gui/src/directory.o ../clients/gui/src/sound.o ../clients/gui/src/music.o ../clients/gui/src/music_decoder.o ../clients/gui/src/mod.o ../clients/gui/build/*.o -o test_music_ui $(LDFLAGS) $$(sdl2-config --libs) $(WEB_LIBS) -lm
	./test_music_ui

.PHONY: decoder-sanitize
decoder-sanitize:
	$(CC) -g -O1 -I../shared/include -fsanitize=address,undefined -fno-omit-frame-pointer test_music_decoder.c ../clients/gui/src/music_decoder.c -o test_music_decoder -lm
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./test_music_decoder

.PHONY: discovery
discovery:
	$(MAKE) -C ../clients/gui
	$(CC) $(CFLAGS) $$(sdl2-config --cflags) test_discovery.c ../clients/gui/src/directory.o -o test_discovery $(LDFLAGS) $$(sdl2-config --libs) $(WEB_LIBS)
	$(PYTHON) test_discovery.py
