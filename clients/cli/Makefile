# LineClash CLI client Makefile
# Targets: macOS and Linux (no SDL2, no sound)

BINARY   := lineclash-cli
BUILDDIR := build
SRCDIR   := src
SHARED   := ../../shared/src

CC       ?= cc
CFLAGS   := -Wall -Wextra -O2 -DPLATFORM_POSIX -I../../shared/include -I$(SRCDIR)
LDFLAGS  :=

# Include TETRINETZ_VERSION if version.mk exists
-include ../version.mk
ifdef VERSION
    CFLAGS += -DTETRINETZ_VERSION=\"$(VERSION)\"
else
    CFLAGS += -DTETRINETZ_VERSION=\"dev\"
endif

SRCS := \
    $(SRCDIR)/lineclash-cli.c \
    $(SHARED)/net.c \
    $(SHARED)/field.c \
    $(SHARED)/piece.c \
    $(SHARED)/protocol.c

OBJS := $(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(SRCS)))

.PHONY: all clean

all: $(BINARY)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile each source — all go into build/ with their basename
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SHARED)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINARY): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	@echo "Built: $(BINARY)"

clean:
	rm -rf $(BUILDDIR) $(BINARY)
