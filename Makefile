export

.PHONY: all server client cli clean help run-server run-client run-cli

# Default: build everything
all: server client cli

server:
	$(MAKE) -C server all

client:
	$(MAKE) -C clients/gui all

cli:
	$(MAKE) -C clients/cli all

clean:
	$(MAKE) -C server clean
	$(MAKE) -C clients/gui clean
	$(MAKE) -C clients/cli clean
	$(MAKE) -C tests clean

run-server: server
	./server/lineclashd

run-client: client
	./clients/gui/lineclash

run-cli: cli
	./clients/cli/lineclash-cli

help:
	@echo "LineClash"
	@echo "  make             build everything"
	@echo "  make server      build lineclashd + lineclashd-tool"
	@echo "  make client      build SDL2 GUI client"
	@echo "  make cli         build terminal client"
	@echo "  make clean       clean all build artifacts"
	@echo "  make run-server  build and start the server"
	@echo "  make run-client  build and launch the GUI client"
	@echo "  make run-cli     build and launch the CLI client"
