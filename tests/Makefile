CC      = cc
CFLAGS  = -Wall -Wextra -O2 -I../shared/include
SHARED  = ../shared/src/field.c ../shared/src/piece.c \
          ../shared/src/protocol.c ../shared/src/net.c

SERVER ?= ../server/lineclashd

.PHONY: all unit integration clean

all: unit integration

unit: test_field
	@echo "--- Unit tests ---"
	./test_field

integration: test_integration
	@echo "--- Integration tests ---"
	./test_integration $(SERVER)

test_field: test_field.c $(SHARED)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test_integration: test_integration.c $(SHARED)
	$(CC) $(CFLAGS) $^ -o $@ -lm

clean:
	rm -f test_field test_integration
