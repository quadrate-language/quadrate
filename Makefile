PREFIX                ?= /usr
INSTALL_BIN_DIR       := $(PREFIX)/bin
INSTALL_LIB_DIR       := $(PREFIX)/lib
INSTALL_INCLUDE_DIR   := $(PREFIX)/include
INSTALL_PKGCONFIG_DIR := $(INSTALL_LIB_DIR)/pkgconfig

CMDS                  := quadc quadfmt
BIN_DIR               := dist/bin
LIB_DIR               := dist/lib
INCLUDE_DIR           := dist/include
PKGCONFIG_DIR         := dist/pkgconfig

# ====== Phony ======
.PHONY: all libquadrate cmds install uninstall clean

# ====== Default: build everything ======
all: cmds libquadrate

cmds: $(CMDS:%=$(BIN_DIR)/%)

$(BIN_DIR)/%: cmd/%/main.go
	@mkdir -p $(BIN_DIR)
	$(GOENV) go build -o $@ ./cmd/$*

# Build the shared library + header + .pc by delegating to sub-Makefile
libquadrate:
	$(MAKE) -C libquadrate all
	mkdir -p dist/lib dist/include/quadrate dist/pkgconfig
	cp libquadrate/lib/libquadrate.so dist/lib/libquadrate.so
	cp libquadrate/include/quadrate/qd.h dist/include/quadrate/qd.h
	cp libquadrate/pkgconfig/libquadrate.pc dist/pkgconfig/libquadrate.pc

install: all
	@echo "==> Installing CLIs to $(INSTALL_BIN_DIR)"
	install -d "$(INSTALL_BIN_DIR)"
	install -m 0755 "$(BIN_DIR)/quadc"   "$(INSTALL_BIN_DIR)/quadc"
	install -m 0755 "$(BIN_DIR)/quadfmt" "$(INSTALL_BIN_DIR)/quadfmt"

	@echo "==> Installing libquadrate (.so, header, pc)"
	install -d "$(INSTALL_LIB_DIR)" "$(INSTALL_INCLUDE_DIR)/quadrate" "$(INSTALL_PKGCONFIG_DIR)"
	install -m 0644 "$(LIB_DIR)/libquadrate.so"       "$(INSTALL_LIB_DIR)/libquadrate.so"
	install -m 0644 "$(INCLUDE_DIR)/quadrate/qd.h"    "$(INSTALL_INCLUDE_DIR)/quadrate/qd.h"
	install -m 0644 "$(PKGCONFIG_DIR)/libquadrate.pc" "$(INSTALL_PKGCONFIG_DIR)/libquadrate.pc"

uninstall:
	@echo "==> Removing CLIs from $(INSTALL_BIN_DIR)"
	-rm -f "$(INSTALL_BIN_DIR)/quadc" "$(INSTALL_BIN_DIR)/quadfmt"

	@echo "==> Removing libquadrate (.so, header, pc)"
	-rm -f "$(INSTALL_LIB_DIR)/libquadrate.so"
	-rm -rf "$(INSTALL_INCLUDE_DIR)/quadrate"
	-rm -f "$(INSTALL_PKGCONFIG_DIR)/libquadrate.pc"

clean:
	-rm -rf dist
	$(MAKE) -C libquadrate clean

