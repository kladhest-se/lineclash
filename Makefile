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

# Local release preparation. These targets never publish or push tags.
PYTHON ?= python3
.PHONY: verify release-plan release-build release-check release-prepare release-tools-test
verify:
	bash scripts/verify-platform.sh
release-plan:
	$(PYTHON) scripts/release.py plan
release-build:
	$(PYTHON) scripts/release.py build
release-check:
	$(PYTHON) scripts/release.py check
release-prepare:
	$(PYTHON) scripts/release.py prepare
release-tools-test:
	$(PYTHON) -B -m unittest discover -s tests/release_tools -v

.PHONY: directory-test web-test
directory-test:
	$(PYTHON) tests/test_directory.py
web-test: server
	$(PYTHON) tests/test_web_mariadb.py
	$(PYTHON) tests/test_directory_e2e.py
