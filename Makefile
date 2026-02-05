BUILD_DIR_DEBUG   := build/debug
BUILD_DIR_RELEASE := build/release

MESON_FLAGS := -Dbuild_tests=true -Db_pie=false

# Detect Haiku and set appropriate paths
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Haiku)
    PREFIX ?= $(HOME)/config/non-packaged
    DATADIR = $(PREFIX)/data
    INCLUDEDIR = $(PREFIX)/develop/headers
else
    PREFIX ?= /usr
    DATADIR = $(PREFIX)/share
    INCLUDEDIR = $(PREFIX)/include
endif

# Use clang by default for better LLVM integration
# Use ccache if available for faster rebuilds
CCACHE := $(shell command -v ccache 2>/dev/null)
ifdef CCACHE
    export CC  := ccache clang
    export CXX := ccache clang++
else
    export CC  := clang
    export CXX := clang++
endif

# Commands to copy
CMDS := quad quadc quadfmt quadlint quadlsp quadpm quaduses

# Libraries with C components (need static archive creation)
# Note: qdsqlite moved to external module (https://github.com/quadrate-language/sqlite)
LIBS_WITH_C := qdrt qd qdfmt qdio qdmath qdmem qdos qdsignal qdstr qdstrconv qdtime qdthread qdtesting

# Libraries with headers to install
LIBS_WITH_HEADERS := qdrt qd qdfmt qdio qdmath qdmem qdos qdstr qdstrconv qdtime qdtesting

# Standard library modules (auto-discovered from lib/qd*/qd/*/)
# Note: Some modules moved to external repos: http, sqlite, json, regex, ct, crypto
STDLIB_MODULES := $(shell find lib/qd*/qd -maxdepth 1 -mindepth 1 -type d -exec basename {} \; 2>/dev/null | sort -u)

.PHONY: all debug release docker-x64 docker-arm64 docker-all tests tests-failed tests-clear valgrind asan fuzz examples format install uninstall clean docs quadmcp playground

all: debug

# Common build function - called by debug and release targets
# Usage: $(call do_build,BUILD_DIR,BUILD_TYPE,MSG)
define do_build
	meson setup $(1) --buildtype=$(2) $(MESON_FLAGS)
	meson compile -C $(1)
	@mkdir -p dist/bin dist/lib dist/include
	@cp -f $(1)/lib/qdrt/libqdrt.so dist/lib/
	@cp -f $(1)/lib/qd/libqd.so dist/lib/
	@for cmd in $(CMDS); do \
		cp -f $(1)/cmd/$$cmd/$$cmd dist/bin/; \
		if command -v patchelf >/dev/null 2>&1; then \
			patchelf --set-rpath '$$ORIGIN/../lib' dist/bin/$$cmd 2>/dev/null || true; \
		fi; \
	done
	@if [ -f $(1)/cmd/quadrepl/quadrepl ]; then \
		cp -f $(1)/cmd/quadrepl/quadrepl dist/bin/; \
		if command -v patchelf >/dev/null 2>&1; then \
			patchelf --set-rpath '$$ORIGIN/../lib' dist/bin/quadrepl 2>/dev/null || true; \
		fi; \
	else echo "Note: quadrepl not built (readline not found)"; fi
	@echo "Creating static libraries..."
	@(cd $(1)/lib/qdrt && ar rcs libqdrt.a $$(ar -t libqdrt_static.a) && cp libqdrt.a ../../../../dist/lib/) && echo "  libqdrt.a"
	@(cd $(1)/lib/qd && ar rcs libqd.a $$(ar -t libqd_static.a) && cp libqd.a ../../../../dist/lib/) && echo "  libqd.a"
	@for lib in qdfmt qdio qdmath qdmem qdos qdsignal qdstr qdstrconv qdtime qdthread qdtesting qdtty; do \
		(cd $(1)/lib/$$lib && ar rcs lib$${lib}_regular.a $$(ar -t lib$$lib.a) && cp lib$${lib}_regular.a ../../../../dist/lib/lib$$lib.a) && echo "  lib$$lib.a"; \
	done
	@for lib in qdthread; do \
		if [ -f lib/$$lib/lib$$lib.deps ]; then cp lib/$$lib/lib$$lib.deps dist/lib/; fi; \
	done
	@for lib in $(LIBS_WITH_HEADERS); do cp -rf lib/$$lib/include/$$lib dist/include/; done
	@mkdir -p dist/share/quadrate
	@for mod in $(STDLIB_MODULES); do cp -r lib/qd*/qd/$$mod dist/share/quadrate/ 2>/dev/null || true; done
	@mkdir -p dist/share/bash-completion/completions
	@cp -f completions/quad.bash dist/share/bash-completion/completions/quad
	@echo "$(3)"
endef

debug:
	$(call do_build,$(BUILD_DIR_DEBUG),debug,Debug build complete - static libraries ready)
	@echo "Building quadmcp..."
	@mkdir -p $(BUILD_DIR_DEBUG)/modules
	cd cmd/quadmcp && QUADRATE_PATH=$(CURDIR)/$(BUILD_DIR_DEBUG)/modules QUADRATE_ROOT=$(CURDIR) $(CURDIR)/dist/bin/quadpm install
	@mkdir -p $(BUILD_DIR_DEBUG)/cmd/quadmcp
	@cat cmd/quadmcp/core.qd cmd/quadmcp/tools.qd cmd/quadmcp/resources.qd cmd/quadmcp/server.qd > $(BUILD_DIR_DEBUG)/cmd/quadmcp/quadmcp.qd
	cd cmd/quadmcp && QUADRATE_PATH=$(CURDIR)/$(BUILD_DIR_DEBUG)/modules QUADRATE_ROOT=$(CURDIR) $(CURDIR)/dist/bin/quad build $(CURDIR)/$(BUILD_DIR_DEBUG)/cmd/quadmcp/quadmcp.qd -o $(CURDIR)/$(BUILD_DIR_DEBUG)/cmd/quadmcp/quadmcp
	cp $(BUILD_DIR_DEBUG)/cmd/quadmcp/quadmcp dist/bin/

release:
	$(call do_build,$(BUILD_DIR_RELEASE),release,Release build complete - static libraries ready)
	@echo "Building quadmcp..."
	@mkdir -p $(BUILD_DIR_RELEASE)/modules
	cd cmd/quadmcp && QUADRATE_PATH=$(CURDIR)/$(BUILD_DIR_RELEASE)/modules QUADRATE_ROOT=$(CURDIR) $(CURDIR)/dist/bin/quadpm install
	@mkdir -p $(BUILD_DIR_RELEASE)/cmd/quadmcp
	@cat cmd/quadmcp/core.qd cmd/quadmcp/tools.qd cmd/quadmcp/resources.qd cmd/quadmcp/server.qd > $(BUILD_DIR_RELEASE)/cmd/quadmcp/quadmcp.qd
	cd cmd/quadmcp && QUADRATE_PATH=$(CURDIR)/$(BUILD_DIR_RELEASE)/modules QUADRATE_ROOT=$(CURDIR) $(CURDIR)/dist/bin/quad build -O3 $(CURDIR)/$(BUILD_DIR_RELEASE)/cmd/quadmcp/quadmcp.qd -o $(CURDIR)/$(BUILD_DIR_RELEASE)/cmd/quadmcp/quadmcp
	cp $(BUILD_DIR_RELEASE)/cmd/quadmcp/quadmcp dist/bin/
	@echo "Stripping binaries..."
	@for cmd in $(CMDS) quadrepl quadmcp; do \
		if [ -f dist/bin/$$cmd ]; then strip dist/bin/$$cmd && echo "  $$cmd"; fi; \
	done
	@strip dist/lib/*.so 2>/dev/null || true
	@echo "Release binaries stripped"

# Docker-based builds (for ARM64 and reproducible builds)
# Requires: docker with buildx and QEMU for cross-platform builds
#
# Setup QEMU (once): docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
#
docker-x64:
	@echo "Building x86_64 binaries in Docker..."
	docker build --platform linux/amd64 -f docker/Dockerfile.build -t quadrate-build-x64 .
	@mkdir -p dist/bin
	docker run --rm --platform linux/amd64 -v $(PWD)/dist/bin:/out quadrate-build-x64
	@echo "x86_64 build complete: dist/bin/"

docker-arm64:
	@echo "Building ARM64 binaries in Docker (this may take a while with QEMU)..."
	docker build --platform linux/arm64 -f docker/Dockerfile.build -t quadrate-build-arm64 .
	@mkdir -p dist/bin-arm64
	docker run --rm --platform linux/arm64 -v $(PWD)/dist/bin-arm64:/out quadrate-build-arm64
	@echo "ARM64 build complete: dist/bin-arm64/"

docker-all: docker-x64 docker-arm64
	@echo "All Docker builds complete"
	@ls -lh dist/bin/ dist/bin-arm64/

tests: debug
	@$(MAKE) examples --no-print-directory
	@bash tests/run_all.sh $(if $(TEST),--test $(TEST),) $(if $(SUITE),--suite $(SUITE),)

tests-failed: debug
	@$(MAKE) examples --no-print-directory
	@bash tests/run_all.sh --failed

tests-clear:
	@bash tests/run_all.sh --clear

valgrind: debug
	@$(MAKE) examples --no-print-directory
	@bash tests/run_all.sh --valgrind $(if $(TEST),--test $(TEST),) $(if $(SUITE),--suite $(SUITE),)

# ASAN (AddressSanitizer) build for memory error detection
# Note: Uses static linking for sanitizer runtime to avoid shared library issues
asan:
	meson setup build/asan --buildtype=debug -Db_sanitize=address -Db_lundef=false $(MESON_FLAGS) --reconfigure || meson setup build/asan --buildtype=debug -Db_sanitize=address -Db_lundef=false $(MESON_FLAGS)
	meson compile -C build/asan
	@echo "ASAN build complete. Run tests with: meson test -C build/asan --print-errorlogs"

# Fuzzing with libFuzzer (requires clang)
# Usage: make fuzz [TIME=60] [LEN=5000]
TIME ?= 60
LEN ?= 5000
fuzz:
	@if ! which clang++ > /dev/null 2>&1; then echo "Error: clang++ required for fuzzing"; exit 1; fi
	@CC=clang CXX=clang++ meson setup build/fuzz --buildtype=debug -Dbuild_fuzz=true $(MESON_FLAGS) --reconfigure >/dev/null 2>&1 || CC=clang CXX=clang++ meson setup build/fuzz --buildtype=debug -Dbuild_fuzz=true $(MESON_FLAGS) >/dev/null 2>&1
	@meson compile -C build/fuzz tests/fuzz/fuzz_parser >/dev/null 2>&1
	@echo "Running fuzzer for $(TIME) seconds..."
	@./build/fuzz/tests/fuzz/fuzz_parser tests/fuzz/corpus/ -max_len=$(LEN) -max_total_time=$(TIME) >/dev/null 2>&1 && echo "Done. No crashes found." || echo "Fuzzer crashed - check for crash-* files"

examples: debug
	@mkdir -p dist/examples
	meson setup $(BUILD_DIR_DEBUG) --buildtype=debug --reconfigure -Dbuild_examples=true $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_DEBUG) examples/embed/embed examples/embed/embed_copy examples/embed/multi-module-test examples/embed/multi-module-test_copy examples/embed/native-functions-test examples/embed/native-functions-test_copy examples/embed/incremental-test examples/embed/incremental-test_copy examples/ffi/ffi examples/hello-world/hello-world examples/hello-world-c/hello-world-c examples/bmi/bmi examples/dc/dc examples/defer/defer examples/donut/donut examples/errors/errors examples/fibonacci/fibonacci examples/modules/modules examples/sha256sum/sha256sum examples/sierpinski/sierpinski examples/stars/stars examples/threading/threading
	@echo "Copying shared libraries for embed examples..."
	@cp -f $(BUILD_DIR_DEBUG)/lib/qd/libqd.so dist/lib/
	@cp -f $(BUILD_DIR_DEBUG)/lib/qdrt/libqdrt.so dist/lib/

format:
	find cmd lib examples -type f \( -name '*.cc' -o -name '*.h' \) -exec clang-format -i {} +

# Build quadmcp (MCP server for AI assistants)
quadmcp: debug
	@echo "Building quadmcp (MCP server)..."
	@mkdir -p $(BUILD_DIR_DEBUG)/modules
	cd cmd/quadmcp && QUADRATE_PATH=$(CURDIR)/$(BUILD_DIR_DEBUG)/modules QUADRATE_ROOT=$(CURDIR) $(CURDIR)/dist/bin/quadpm install
	@mkdir -p $(BUILD_DIR_DEBUG)/cmd/quadmcp
	@cat cmd/quadmcp/core.qd cmd/quadmcp/tools.qd cmd/quadmcp/resources.qd cmd/quadmcp/server.qd > $(BUILD_DIR_DEBUG)/cmd/quadmcp/quadmcp.qd
	cd cmd/quadmcp && QUADRATE_PATH=$(CURDIR)/$(BUILD_DIR_DEBUG)/modules QUADRATE_ROOT=$(CURDIR) $(CURDIR)/dist/bin/quad build -O3 $(CURDIR)/$(BUILD_DIR_DEBUG)/cmd/quadmcp/quadmcp.qd -o $(CURDIR)/$(BUILD_DIR_DEBUG)/cmd/quadmcp/quadmcp
	cp $(BUILD_DIR_DEBUG)/cmd/quadmcp/quadmcp dist/bin/
	@echo "  quadmcp built successfully"

# Build playground (web-based REPL) - requires Go and Docker
playground: release
	@if ! command -v go >/dev/null 2>&1; then echo "Error: Go required for playground"; exit 1; fi
	@if ! command -v docker >/dev/null 2>&1; then echo "Error: Docker required for playground"; exit 1; fi
	@echo "Building playground sandbox image..."
	@tools/playground/build-sandbox.sh
	@echo "Building playground server..."
	@cd tools/playground && go build -o ../../dist/bin/playground .
	@echo "Playground built: dist/bin/playground"

install:
	@if [ ! -f dist/bin/quadc ]; then echo "Error: Run 'make release' first"; exit 1; fi
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(INCLUDEDIR)
	@for cmd in $(CMDS); do install -m 755 dist/bin/$$cmd $(DESTDIR)$(PREFIX)/bin/; done
	@if [ -f dist/bin/quadrepl ]; then install -m 755 dist/bin/quadrepl $(DESTDIR)$(PREFIX)/bin/; fi
	@if [ -f dist/bin/quadmcp ]; then install -m 755 dist/bin/quadmcp $(DESTDIR)$(PREFIX)/bin/; fi
	@for lib in $(LIBS_WITH_C); do install -m 644 dist/lib/lib$$lib.a $(DESTDIR)$(PREFIX)/lib/; done
	@for deps in dist/lib/*.deps; do if [ -f "$$deps" ]; then install -m 644 "$$deps" $(DESTDIR)$(PREFIX)/lib/; fi; done
	install -m 755 dist/lib/libqdrt.so $(DESTDIR)$(PREFIX)/lib/
	install -m 755 dist/lib/libqd.so $(DESTDIR)$(PREFIX)/lib/
	@for lib in $(LIBS_WITH_HEADERS); do cp -r dist/include/$$lib $(DESTDIR)$(INCLUDEDIR)/; done
	@find $(DESTDIR)$(INCLUDEDIR) -type f -exec chmod 644 {} +
	@find $(DESTDIR)$(INCLUDEDIR) -type d -exec chmod 755 {} +
	@echo "Installing Quadrate standard library modules to $(DESTDIR)$(DATADIR)/quadrate/"
	install -d $(DESTDIR)$(DATADIR)/quadrate
	@for mod in $(STDLIB_MODULES); do cp -r lib/qd*/qd/$$mod $(DESTDIR)$(DATADIR)/quadrate/ 2>/dev/null || true; done
	@find $(DESTDIR)$(DATADIR)/quadrate -type f -exec chmod 644 {} +
	@find $(DESTDIR)$(DATADIR)/quadrate -type d -exec chmod 755 {} +
	@echo "Installing API documentation to $(DESTDIR)$(DATADIR)/quadrate/docs/api/"
	install -d $(DESTDIR)$(DATADIR)/quadrate/docs/api
	@for json in docs/api/*.json; do install -m 644 "$$json" $(DESTDIR)$(DATADIR)/quadrate/docs/api/; done
	@echo "Installing bash completions to $(DESTDIR)$(DATADIR)/bash-completion/completions/"
	install -d $(DESTDIR)$(DATADIR)/bash-completion/completions
	install -m 644 completions/quad.bash $(DESTDIR)$(DATADIR)/bash-completion/completions/quad
	@for cmd in quadc quadfmt quadlint quadlsp quadpm quadrepl quaduses; do ln -sf quad $(DESTDIR)$(DATADIR)/bash-completion/completions/$$cmd; done

uninstall:
	@for cmd in $(CMDS) quadrepl quadmcp; do rm -f $(DESTDIR)$(PREFIX)/bin/$$cmd; done
	@for lib in $(LIBS_WITH_C); do rm -f $(DESTDIR)$(PREFIX)/lib/lib$$lib.a; done
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdrt.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libqd.so
	@for lib in $(LIBS_WITH_HEADERS); do rm -rf $(DESTDIR)$(INCLUDEDIR)/$$lib; done
	@echo "Removing Quadrate standard library modules from $(DESTDIR)$(DATADIR)/quadrate/"
	rm -rf $(DESTDIR)$(DATADIR)/quadrate
	@echo "Removing bash completions from $(DESTDIR)$(DATADIR)/bash-completion/completions/"
	@for cmd in quad quadc quadfmt quadlint quadlsp quadpm quadrepl quaduses; do rm -f $(DESTDIR)$(DATADIR)/bash-completion/completions/$$cmd; done

docs:
	@echo "=========================================="
	@echo "  Building documentation"
	@echo "=========================================="
	@cd docs && mkdocs build
	@echo ""
	@echo "Documentation built successfully!"
	@echo "To serve locally: cd docs && mkdocs serve"

clean:
	rm -rf build
	rm -rf dist
