# version.mk — Pure variable file, no $(shell) calls.
# VERSION, VERSION_NUM, BUILD_DATE set by the Makefile before including this.
# Hard-set for this release — overrides get-version.sh result.
VERSION      := 1.0.0
VERSION_NUM  := 1.0.0
BUILD_DATE   ?= unknown
# BUILD_DATE comes from src/buildnum.h, generated each compile
VERSION_CFLAGS = -DLINECLASH_VERSION=\"$(VERSION)\"
