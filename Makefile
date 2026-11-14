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
	$(PYTHON) tests/test_smtp.py
	$(PYTHON) tests/test_password_recovery.py
	$(PYTHON) tests/test_pin_routes.py
	$(PYTHON) tests/test_web_pins.py
	$(PYTHON) tests/test_email_verification.py
	$(PYTHON) tests/test_account_profile.py
	$(PYTHON) tests/test_web_accounts.py
	$(PYTHON) tests/test_web_reset.py
	$(PYTHON) tests/test_web_users_reset.py
	$(PYTHON) tests/test_directory_e2e.py

web-js-test:
	node tests/test_profile_avatar.js
	node tests/test_web_downloads.js
	node tests/test_password_recovery.js
	node tests/test_email_verification.js
	node tests/test_email_change.js
	node tests/test_web_replay_privacy.js

.PHONY: tvos
tvos:
	bash scripts/build-tvos.sh simulator

.PHONY: tvos-focus-test server-update-test
tvos-focus-test:
	$(CC) -Wall -Wextra -Werror tests/test_tvos_focus.c clients/tvos/focus.c -o /tmp/lineclash-tvos-focus-test
	/tmp/lineclash-tvos-focus-test
server-update-test: server
	$(PYTHON) tests/test_server_update.py
	$(PYTHON) tests/test_server_stdin.py

.PHONY: tvos-ui-test
tvos-ui-test: client
	$(CC) -Wall -Wextra -Wno-unused-variable -O2 -DPLATFORM_POSIX $(VERSION_CFLAGS) $$(sdl2-config --cflags) -Iclients/tvos tests/test_tvos_ui.c clients/tvos/focus.c clients/gui/src/client.o clients/gui/src/directory.o clients/gui/src/accounts.o clients/gui/src/account_https.o clients/gui/src/sound.o clients/gui/src/music.o clients/gui/src/music_decoder.o clients/gui/src/mod.o clients/gui/build/*.o -o /tmp/lineclash-tvos-ui-test $$(sdl2-config --libs | sed 's/-lSDL2main//g') -lcurl -lm
	/tmp/lineclash-tvos-ui-test
