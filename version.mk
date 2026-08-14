# version.mk — Pure variable file, no $(shell) calls.
# VERSION, VERSION_NUM, BUILD_DATE set by the Makefile before including this.
# Hard-set for this release — overrides get-version.sh result.
VERSION      := 0.9
VERSION_NUM  := 0.9
BUILD_DATE   ?= unknown
# BUILD_DATE comes from src/buildnum.h, generated each compile
VERSION_CFLAGS = -DTETRINETZ_VERSION=\"$(VERSION)\"
