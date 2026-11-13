# Single release version; command-line VERSION overrides it.
VERSION ?= 0.1
VERSION_CFLAGS = -DTETRINETZ_VERSION=\"$(VERSION)\" -DLINECLASH_VERSION=\"$(VERSION)\"
