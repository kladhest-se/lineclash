# Single release version; command-line VERSION overrides it.
VERSION ?= 1.0.0
VERSION_CFLAGS = -DTETRINETZ_VERSION=\"$(VERSION)\" -DLINECLASH_VERSION=\"$(VERSION)\"
