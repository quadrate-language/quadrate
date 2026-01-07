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
export CC  := clang
export CXX := clang++

# Commands to copy
CMDS := quad quadc quadfmt quadlint quadlsp quadpm quaduses

# Libraries with C components (need static archive creation)
# Note: qdsqlite moved to external module (https://github.com/quadrate-language/sqlite)
LIBS_WITH_C := qdrt qd qdfmt qdio qdmath qdmem qdos qdsignal qdstr qdstrconv qdtime qdthread qdtesting

# Libraries with headers to install
LIBS_WITH_HEADERS := qdrt qd qdfmt qdio qdmath qdmem qdos qdstr qdstrconv qdtime qdtesting

# Standard library modules (pure Quadrate or mixed)
# Note: http moved to external module (https://github.com/quadrate-language/http)
# Note: sqlite moved to external module (https://github.com/quadrate-language/sqlite)
# Note: json moved to external module (https://github.com/quadrate-language/json)
# Note: regex moved to external module (https://github.com/quadrate-language/regex)
# Note: ct moved to external module (https://github.com/quadrate-language/ct)
# Note: crypto (sha256, sha512, md5, crc32) moved to external module (https://github.com/quadrate-language/crypto)
STDLIB_MODULES := base64 bits flag fmt io limits math mem os sb signal str strconv term thread time unicode uri hex bytes path sort rand uuid testing

.PHONY: all debug release tests tests-failed tests-clear valgrind asan fuzz examples format install uninstall clean docs

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
	@rm -f dist/lib/libqdrt.a && cd $(1)/lib/qdrt && ar rcs ../../../../dist/lib/libqdrt.a $$(ar -t libqdrt_static.a) && echo "  libqdrt.a"
	@rm -f dist/lib/libqd.a && cd $(1)/lib/qd && ar rcs ../../../../dist/lib/libqd.a $$(ar -t libqd_static.a) && echo "  libqd.a"
	@for lib in qdfmt qdio qdmath qdmem qdos qdsignal qdstr qdstrconv qdtime qdthread qdtesting; do \
		rm -f dist/lib/lib$$lib.a && cd $(1)/lib/$$lib && ar rcs ../../../../dist/lib/lib$$lib.a $$(ar -t lib$$lib.a) && echo "  lib$$lib.a" && cd ->/dev/null; \
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

release:
	$(call do_build,$(BUILD_DIR_RELEASE),release,Release build complete - static libraries ready)

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
	CC=clang CXX=clang++ meson setup build/fuzz --buildtype=debug -Dbuild_fuzz=true $(MESON_FLAGS) --reconfigure || CC=clang CXX=clang++ meson setup build/fuzz --buildtype=debug -Dbuild_fuzz=true $(MESON_FLAGS)
	meson compile -C build/fuzz fuzz/fuzz_parser
	@echo "Running fuzzer for $(TIME) seconds..."
	./build/fuzz/fuzz/fuzz_parser fuzz/corpus/ -max_len=$(LEN) -max_total_time=$(TIME)

examples: debug
	@mkdir -p dist/examples
	meson setup $(BUILD_DIR_DEBUG) --buildtype=debug --reconfigure -Dbuild_examples=true $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_DEBUG) examples/embed/embed examples/embed/embed_copy examples/embed/multi-module-test examples/embed/multi-module-test_copy examples/embed/native-functions-test examples/embed/native-functions-test_copy examples/embed/incremental-test examples/embed/incremental-test_copy examples/ffi/ffi examples/hello-world/hello-world examples/hello-world-c/hello-world-c examples/bmi/bmi examples/dc/dc examples/defer/defer examples/donut/donut examples/errors/errors examples/fibonacci/fibonacci examples/modules/modules examples/sha256sum/sha256sum examples/sierpinski/sierpinski examples/stars/stars examples/threading/threading
	@echo "Copying shared libraries for embed examples..."
	@cp -f $(BUILD_DIR_DEBUG)/lib/qd/libqd.so dist/lib/
	@cp -f $(BUILD_DIR_DEBUG)/lib/qdrt/libqdrt.so dist/lib/

format:
	find cmd lib examples -type f \( -name '*.cc' -o -name '*.h' \) -not -name 'utf8.h' -not -path '*/utf8/*' -exec clang-format -i {} +

install: release
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(INCLUDEDIR)
	@for cmd in $(CMDS); do install -m 755 dist/bin/$$cmd $(DESTDIR)$(PREFIX)/bin/; done
	@if [ -f dist/bin/quadrepl ]; then install -m 755 dist/bin/quadrepl $(DESTDIR)$(PREFIX)/bin/; fi
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
	@echo "Installing bash completions to $(DESTDIR)$(DATADIR)/bash-completion/completions/"
	install -d $(DESTDIR)$(DATADIR)/bash-completion/completions
	install -m 644 completions/quad.bash $(DESTDIR)$(DATADIR)/bash-completion/completions/quad
	@for cmd in quadc quadfmt quadlint quadlsp quadpm quadrepl quaduses; do ln -sf quad $(DESTDIR)$(DATADIR)/bash-completion/completions/$$cmd; done

uninstall:
	@for cmd in $(CMDS) quadrepl; do rm -f $(DESTDIR)$(PREFIX)/bin/$$cmd; done
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
	@echo "  Generating API Documentation"
	@echo "=========================================="
	@if ! which doxygen > /dev/null 2>&1; then \
		echo "" && \
		echo "⚠️  Warning: doxygen not found - skipping documentation generation" && \
		echo "" && \
		echo "To generate documentation, install doxygen and graphviz (optional, for diagrams):" && \
		echo "  Arch Linux: sudo pacman -S doxygen graphviz" && \
		echo "  Debian:     sudo apt install doxygen graphviz" && \
		echo ""; \
	else \
		echo "Running doxygen..." && \
		doxygen Doxyfile && \
		echo "" && \
		echo "Documentation generated successfully!" && \
		echo "HTML docs: dist/docs/html/index.html" && \
		echo "" && \
		echo "To view documentation, run:" && \
		echo "  xdg-open dist/docs/html/index.html"; \
	fi

clean:
	rm -rf build
	rm -rf dist
