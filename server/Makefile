# lineclashd — Server Makefile
UNAME := $(shell uname -s 2>/dev/null || echo Windows)
ifneq ($(filter MINGW% MSYS% CYGWIN%,$(UNAME)),)
UNAME := Windows
endif
.DEFAULT_GOAL := all

include ../version.mk


.PHONY: all clean run gen-buildnum

gen-buildnum: src/buildnum.h

.PHONY: FORCE
FORCE:
src/buildnum.h: FORCE ../get-version.sh ../version.mk
	@VERSION="$(VERSION)" sh ../get-version.sh header $@



ifeq ($(UNAME),Darwin)
    CC          = cc
    CFLAGS      = -Wall -Wextra -O2 -DPLATFORM_POSIX -Wno-format-truncation -Wno-stringop-truncation -Wno-unknown-warning-option $(VERSION_CFLAGS)
    LDFLAGS     =
    SQLITE_LIBS = -lsqlite3
    SERVER      = lineclashd
    ADMIN       = lineclashd-tool
endif
ifeq ($(UNAME),Linux)
    CC          = gcc
    CFLAGS      = -Wall -Wextra -O2 -DPLATFORM_POSIX -Wno-format-truncation -Wno-stringop-truncation -Wno-unknown-warning-option $(VERSION_CFLAGS)
    LDFLAGS     = -lm
    SQLITE_LIBS := $(shell $(CC) -lsqlite3 -shared -o /dev/null 2>/dev/null \
                      && echo "-lsqlite3" \
                      || find /usr/lib* -name "libsqlite3.so*" 2>/dev/null | head -1)
    SERVER      = lineclashd
    ADMIN       = lineclashd-tool
endif
ifeq ($(UNAME),Windows)
    CC          = gcc
    CFLAGS      = -Wall -O2 -DPLATFORM_WINDOWS $(VERSION_CFLAGS)
    LDFLAGS     = -lws2_32 -lpsapi
    SQLITE_LIBS = -lsqlite3
    SERVER      = lineclashd.exe
    ADMIN       = lineclashd-tool.exe
endif
ifeq ($(PLATFORM),win32)
    CC          = x86_64-w64-mingw32-gcc
    CFLAGS      = -Wall -O2 -DPLATFORM_WINDOWS $(VERSION_CFLAGS)
    LDFLAGS     = -lws2_32 -lpsapi
    SQLITE_LIBS = -lsqlite3
    SERVER      = lineclashd.exe
    ADMIN       = lineclashd-tool.exe
endif
ifeq ($(PLATFORM),amiga)
    CC          = m68k-amigaos-gcc
    CFLAGS      = -Wall -O2 -DPLATFORM_AMIGA -noixemul -m68020 $(VERSION_CFLAGS)
    LDFLAGS     = -lnet
    SQLITE_LIBS =
    SERVER      = lineclashd
    ADMIN       = lineclashd-tool
endif

SHARED = ../shared/src
INC    = -I../shared/include -Isrc

# Server sources (own files)
SERVER_OWN_SRCS = src/lineclashd.c \
                  src/ai.c \
                  src/server.c      \
                  src/server_cmd.c  \
                  src/server_api.c  \
                  src/auth.c        \
                  src/config.c

# Shared sources compiled into build/ so they don't collide with client objects
SHARED_SRCS = $(SHARED)/field.c    \
              $(SHARED)/piece.c    \
              $(SHARED)/net.c      \
              $(SHARED)/protocol.c

SERVER_OWN_OBJS   = $(SERVER_OWN_SRCS:.c=.o)
SHARED_OBJS       = build/field.o build/piece.o build/net.o build/protocol.o
SERVER_OBJS       = $(SERVER_OWN_OBJS) $(SHARED_OBJS)

# Admin tool — own files
ADMIN_OWN_SRCS = src/lineclashd-tool.c \
                 src/auth.c              \
                 src/config.c
SHARED_AO      = build/field.ao build/piece.ao build/net.ao build/protocol.ao
ADMIN_OWN_OBJS = $(patsubst %.c,%.ao,$(ADMIN_OWN_SRCS))
ADMIN_OBJS     = $(ADMIN_OWN_OBJS) $(SHARED_AO)



all: gen-buildnum build $(SERVER) $(ADMIN)
	@rm -f lineclash-admin lineclash-admin.exe src/lineclash-admin.ao
	@echo "Server built:     $(SERVER)"
	@echo "Tool built: lineclashd-tool"
	@echo "Run server: ./$(SERVER) [port]"
	@echo "Admin: ./lineclashd-tool [host] [cmd...]"

build:
	mkdir -p build

$(SERVER): $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o $@ $(LDFLAGS) $(SQLITE_LIBS)

$(ADMIN): $(ADMIN_OBJS)
	$(CC) $(ADMIN_OBJS) -o $@ $(LDFLAGS) $(SQLITE_LIBS)

# Server own sources → src/*.o
src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

# Admin own sources → src/*.ao
src/%.ao: src/%.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

# Shared sources for server → build/*.o
build/field.o:    $(SHARED)/field.c    | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
build/piece.o:    $(SHARED)/piece.c    | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
build/net.o:      $(SHARED)/net.c      | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
build/protocol.o: $(SHARED)/protocol.c | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

# Shared sources for admin → build/*.ao
build/field.ao:    $(SHARED)/field.c    | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
build/piece.ao:    $(SHARED)/piece.c    | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
build/net.ao:      $(SHARED)/net.c      | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
build/protocol.ao: $(SHARED)/protocol.c | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

clean:
	rm -f src/buildnum.h
	rm -f $(SERVER_OWN_OBJS) $(ADMIN_OWN_OBJS)
	rm -f $(SERVER) lineclashd-tool lineclashd.exe lineclashd-tool.exe
	rm -f lineclash-admin lineclash-admin.exe src/lineclash-admin.ao src/lineclash-admin.c.old
	rm -f src/*.d
	rm -rf build

run: all
	./$(SERVER)

# Track headers and serialize metadata generation before parallel compilation.
CFLAGS += -MMD -MP
$(SERVER_OWN_OBJS) $(ADMIN_OWN_OBJS) $(SHARED_OBJS) $(SHARED_AO): src/buildnum.h
-include $(SERVER_OWN_OBJS:.o=.d) $(ADMIN_OWN_OBJS:.ao=.d) $(SHARED_OBJS:.o=.d) $(SHARED_AO:.ao=.d)
