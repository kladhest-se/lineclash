export

.PHONY: all server client amiga clean help run-server run-client

all: server client

amiga:
	$(MAKE) -C clients/amiga

server:
	$(MAKE) -C server all

client:
	$(MAKE) -C clients/gui all

clean:
	$(MAKE) -C server clean
	$(MAKE) -C clients/gui clean
	$(MAKE) -C tests clean

run-server: server
	./server/lineclashd

run-client: client
	./clients/gui/lineclash

help:
	@echo "LineClash"
	@echo "  make             build everything"
	@echo "  make server      build lineclashd + lineclashd-tool"
	@echo "  make client      build SDL2 GUI client"
	@echo "  make clean       clean all build artifacts"
	@echo "  make run-server  build and start the server"
	@echo "  make run-client  build and launch the GUI client"

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

.PHONY: directory-test web-test web-js-test
directory-test:
	$(PYTHON) tests/test_directory.py
web-test: server
	$(PYTHON) tests/test_web_offline.py
	$(PYTHON) tests/test_web_mariadb.py
	$(PYTHON) tests/test_account_policy.py
	$(PYTHON) tests/test_web_accounts.py
	$(PYTHON) tests/test_directory_e2e.py

web-js-test:
	node tests/test_web_downloads.js
