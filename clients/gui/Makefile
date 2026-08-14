# lineclash - SDL2 Client Makefile
UNAME := $(shell uname -s 2>/dev/null || echo Windows)
.DEFAULT_GOAL := all

# ---- Version — extracted from CHANGELOG.md via get-version.sh ----
# Can be overridden on command line: make VERSION=1.0
ifndef VERSION
  VERSION     := $(shell "$(CURDIR)/../get-version.sh" 2>/dev/null || echo dev)
  VERSION_NUM := $(shell "$(CURDIR)/../get-version.sh" num 2>/dev/null || echo 0)
  BUILD_DATE  := $(shell "$(CURDIR)/../get-version.sh" date 2>/dev/null || echo unknown)
  ifeq ($(VERSION),)
    VERSION     := dev
    VERSION_NUM := 0
    BUILD_DATE  := unknown
  endif
endif
include ../version.mk

.PHONY: all clean run gen-buildnum

gen-buildnum:
	@SCRIPT=$(CURDIR)/../get-version.sh; BN=$$($$SCRIPT buildnum 2>/dev/null || echo 0); BD=$$($$SCRIPT date 2>/dev/null || date -u +%Y-%m-%d); { echo '/* auto-generated */'; echo "#define BUILD_NUMBER $$BN"; echo "#define BUILD_DATE \"$$BD\""; } > src/buildnum.h


# ---- SDL2 detection (POSIX/Windows hosts) ----
SDL2_CONFIG := $(shell which sdl2-config 2>/dev/null || \
                        ls /opt/homebrew/bin/sdl2-config 2>/dev/null || \
                        ls /usr/local/bin/sdl2-config 2>/dev/null)

ifneq ($(SDL2_CONFIG),)
    SDL2_CFLAGS := $(shell $(SDL2_CONFIG) --cflags)
    SDL2_LIBS   := $(shell $(SDL2_CONFIG) --libs)
else
    SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
    SDL2_LIBS   := $(shell pkg-config --libs   sdl2 2>/dev/null)
    ifeq ($(SDL2_LIBS),)
        SDL2_CFLAGS := -I/opt/homebrew/include -I/usr/local/include
        SDL2_LIBS   := -L/opt/homebrew/lib -L/usr/local/lib -lSDL2
    endif
endif

# ---- Platform settings ----
ifeq ($(UNAME),Darwin)
    CC      = cc
    CFLAGS  = -Wall -Wextra -O2 -DPLATFORM_POSIX $(VERSION_CFLAGS)
    LDFLAGS = -lm
    TARGET  = lineclash
endif
ifeq ($(UNAME),Linux)
    CC      = gcc
    CFLAGS  = -Wall -Wextra -O2 -DPLATFORM_POSIX $(VERSION_CFLAGS)
    LDFLAGS = -lm
    TARGET  = lineclash
endif
ifeq ($(UNAME),Windows)
    CC      = gcc
    CFLAGS  = -Wall -O2 -Wno-unknown-pragmas -Wno-stringop-truncation -Wno-format-truncation $(VERSION_CFLAGS)
    LDFLAGS = -lws2_32 -lmingw32
    TARGET  = lineclash.exe
    SDL2_LIBS += -lSDL2main
endif
ifeq ($(PLATFORM),win32)
    CC      = x86_64-w64-mingw32-gcc
    CFLAGS  = -Wall -O2 -Wno-unknown-pragmas -Wno-stringop-truncation -Wno-format-truncation $(VERSION_CFLAGS)
    LDFLAGS = -lws2_32 -lmingw32
    TARGET  = lineclash.exe
    SDL2_CFLAGS := -I/usr/x86_64-w64-mingw32/include/SDL2
    SDL2_LIBS   := -L/usr/x86_64-w64-mingw32/lib -lSDL2main -lSDL2
endif

SHARED = ../../shared/src
INC    = -I../../shared/include -Isrc $(SDL2_CFLAGS)
# buildnum.h is in src/ and auto-generated each compile

# Client source files (may be overridden by platform-specific targets above)
ifeq ($(CLIENT_SRCS),)
CLIENT_SRCS = src/lineclash.c \
              src/client.c    \
              src/render_sdl.c \
              src/sound.c     \
              src/mod.c
endif

# Shared source files — compiled into build/ so they never collide
# with the server's Linux objects in ../../shared/src/
SHARED_SRCS = $(SHARED)/field.c    \
              $(SHARED)/piece.c    \
              $(SHARED)/net.c      \
              $(SHARED)/protocol.c

# Object files: client srcs go to src/*.o, shared srcs go to build/*.o
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)
SHARED_OBJS = build/field.o build/piece.o build/net.o build/protocol.o

OBJS = $(CLIENT_OBJS) $(SHARED_OBJS)




all: gen-buildnum build $(TARGET)
	@echo "Client built: $(TARGET)"

build:
	mkdir -p build

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(SDL2_LIBS)

# Client sources → src/*.o
src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

# Shared sources → build/*.o  (isolated from server objects)
build/field.o:    $(SHARED)/field.c    | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

build/piece.o:    $(SHARED)/piece.c    | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

build/net.o:      $(SHARED)/net.c      | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

build/protocol.o: $(SHARED)/protocol.c | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

clean:
	rm -f $(CLIENT_OBJS) $(TARGET) lineclash lineclash.exe src/buildnum.h
	rm -rf build

run: all
	./$(TARGET)

sdl2-install-macos:
	brew install sdl2

sdl2-install-ubuntu:
	sudo apt-get install -y libsdl2-dev

sdl2-install-fedora:
	sudo dnf install -y SDL2-devel

help:
	@echo "LineClash client build targets:"
	@echo "  make                       - auto-detect platform (Linux/macOS/Windows)"
	@echo "  make PLATFORM=win32        - cross-compile for Windows (mingw64)"
	@echo ""
	@echo "Linux packaging (from macOS via Docker, or native on Linux):"
	@echo "  ../package-linux.sh              - auto (Docker on macOS, native on Linux)"
	@echo "  ../package-linux.sh --docker     - force Docker build"
	@echo "  ../package-linux.sh --arch arm64 - arm64 target"
	@echo ""
	@echo ""
	@echo ""
	@echo "  - ahi.device v4+ for sound (AHI package)"
