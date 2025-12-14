BUILD_DIR_DEBUG   := build/debug
BUILD_DIR_RELEASE := build/release

MESON_FLAGS := -Dbuild_tests=true

PREFIX ?= /usr

# Use clang by default for better LLVM integration
export CC  := clang
export CXX := clang++

# Commands to copy
CMDS := quad quadc quadfmt quadlint quadlsp quadpm quaduses

# Libraries with C components (need static archive creation)
LIBS_WITH_C := qdrt qd qdfmt qdio qdmath qdmem qdnet qdos qdsignal qdstr qdstrconv qdtime qdtesting

# Libraries with headers to install
LIBS_WITH_HEADERS := qdrt qd qdfmt qdio qdmath qdmem qdnet qdos qdstr qdstrconv qdtime qdtesting

# Standard library modules (pure Quadrate or mixed)
STDLIB_MODULES := base64 bits flag fmt hof io json limits math mem net os sb signal str strconv time unicode uri hex bytes crc32 sha256 regex path sort rand uuid testing

.PHONY: all debug release tests valgrind examples format install uninstall clean docs

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
	@for lib in qdfmt qdio qdmath qdmem qdnet qdos qdsignal qdstr qdstrconv qdtime qdtesting; do \
		rm -f dist/lib/lib$$lib.a && cd $(1)/lib/$$lib && ar rcs ../../../../dist/lib/lib$$lib.a $$(ar -t lib$$lib.a) && echo "  lib$$lib.a" && cd ->/dev/null; \
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
	@echo "=========================================="
	@echo "  Quadrate Test Suite"
	@echo "=========================================="
	@echo ""
	@echo "=== Running C/C++ unit tests ==="
	meson test -C $(BUILD_DIR_DEBUG) test_runtime test_ast test_semantic_validator --print-errorlogs
	@echo ""
	@echo "=== Running LSP tests ==="
	meson test -C $(BUILD_DIR_DEBUG) test_lsp test_lsp_extended test_lsp_stress --print-errorlogs
	@echo ""
	@echo "=== Running Quadrate language tests ==="
	QUADC=$(BUILD_DIR_DEBUG)/cmd/quadc/quadc bash tests/run_tests.sh qd
	@echo ""
	@echo "=== Running formatter tests ==="
	bash tests/run_tests.sh formatter
	@echo ""
	@echo "=== Running linter tests ==="
	meson test -C $(BUILD_DIR_DEBUG) --suite linter --print-errorlogs
	@echo ""
	@echo "=== Running quaduses tests ==="
	bash tests/run_tests.sh quaduses
	@echo ""
	@echo "=== Running quadpm tests ==="
	meson test -C $(BUILD_DIR_DEBUG) --suite quadpm --print-errorlogs
	@echo ""
	@echo "=== Building and testing embed examples ==="
	@$(MAKE) examples
	bash tests/run_embed_tests.sh
	@echo ""
	@echo "=========================================="
	@echo "  Test Suite Complete"
	@echo "=========================================="

valgrind: debug
	@echo "=== Running C/C++ unit tests with valgrind ==="
	meson test -C $(BUILD_DIR_DEBUG) test_runtime test_ast test_semantic_validator --setup=valgrind --print-errorlogs
	@echo ""
	@echo "=== Running Quadrate language tests with valgrind ==="
	QUADC=$(BUILD_DIR_DEBUG)/cmd/quadc/quadc bash tests/run_tests.sh valgrind
	@echo ""
	@echo "=== Running LSP tests with valgrind ==="
	@if command -v valgrind >/dev/null 2>&1; then \
		meson test -C $(BUILD_DIR_DEBUG) test_lsp test_lsp_extended --setup=valgrind --print-errorlogs; \
	else \
		echo "⚠️  Valgrind not installed, skipping"; \
	fi
	@echo ""
	@echo "=== Running linter tests with valgrind ==="
	@if command -v valgrind >/dev/null 2>&1; then \
		meson test -C $(BUILD_DIR_DEBUG) --suite linter --setup=valgrind --print-errorlogs; \
	else \
		echo "⚠️  Valgrind not installed, skipping"; \
	fi
	@echo ""
	@echo "=== Building and testing embed examples with valgrind ==="
	@$(MAKE) examples
	@if command -v valgrind >/dev/null 2>&1; then \
		bash tests/run_embed_tests.sh valgrind; \
	else \
		echo "⚠️  Valgrind not installed, running without valgrind"; \
		bash tests/run_embed_tests.sh; \
	fi

examples: debug
	@mkdir -p dist/examples
	meson setup $(BUILD_DIR_DEBUG) --buildtype=debug --reconfigure -Dbuild_examples=true $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_DEBUG) examples/embed/embed examples/embed/embed_copy examples/embed/multi-module-test examples/embed/multi-module-test_copy examples/embed/native-functions-test examples/embed/native-functions-test_copy examples/embed/incremental-test examples/embed/incremental-test_copy examples/ffi/ffi examples/hello-world/hello-world examples/hello-world-c/hello-world-c examples/bmi/bmi examples/dc/dc examples/defer/defer examples/donut/donut examples/errors/errors examples/fibonacci/fibonacci examples/modules/modules examples/sha256sum/sha256sum examples/sierpinski/sierpinski examples/stars/stars examples/threading/threading examples/web-server/web-server
	@echo "Copying shared libraries for embed examples..."
	@cp -f $(BUILD_DIR_DEBUG)/lib/qd/libqd.so dist/lib/
	@cp -f $(BUILD_DIR_DEBUG)/lib/qdrt/libqdrt.so dist/lib/

format:
	find cmd lib examples -type f \( -name '*.cc' -o -name '*.h' \) -not -name 'utf8.h' -not -path '*/utf8/*' -exec clang-format -i {} +

install: release
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include
	@for cmd in $(CMDS); do install -m 755 dist/bin/$$cmd $(DESTDIR)$(PREFIX)/bin/; done
	@if [ -f dist/bin/quadrepl ]; then install -m 755 dist/bin/quadrepl $(DESTDIR)$(PREFIX)/bin/; fi
	@for lib in $(LIBS_WITH_C); do install -m 644 dist/lib/lib$$lib.a $(DESTDIR)$(PREFIX)/lib/; done
	install -m 755 dist/lib/libqdrt.so $(DESTDIR)$(PREFIX)/lib/
	install -m 755 dist/lib/libqd.so $(DESTDIR)$(PREFIX)/lib/
	@for lib in $(LIBS_WITH_HEADERS); do cp -r dist/include/$$lib $(DESTDIR)$(PREFIX)/include/; done
	@echo "Installing Quadrate standard library modules to $(DESTDIR)$(PREFIX)/share/quadrate/"
	install -d $(DESTDIR)$(PREFIX)/share/quadrate
	@for mod in $(STDLIB_MODULES); do cp -r lib/qd*/qd/$$mod $(DESTDIR)$(PREFIX)/share/quadrate/ 2>/dev/null || true; done
	@echo "Installing bash completions to $(DESTDIR)$(PREFIX)/share/bash-completion/completions/"
	install -d $(DESTDIR)$(PREFIX)/share/bash-completion/completions
	install -m 644 completions/quad.bash $(DESTDIR)$(PREFIX)/share/bash-completion/completions/quad
	@for cmd in quadc quadfmt quadlint quadlsp quadpm quadrepl quaduses; do ln -sf quad $(DESTDIR)$(PREFIX)/share/bash-completion/completions/$$cmd; done

uninstall:
	@for cmd in $(CMDS) quadrepl; do rm -f $(DESTDIR)$(PREFIX)/bin/$$cmd; done
	@for lib in $(LIBS_WITH_C); do rm -f $(DESTDIR)$(PREFIX)/lib/lib$$lib.a; done
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdrt.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libqd.so
	@for lib in $(LIBS_WITH_HEADERS); do rm -rf $(DESTDIR)$(PREFIX)/include/$$lib; done
	@echo "Removing Quadrate standard library modules from $(DESTDIR)$(PREFIX)/share/quadrate/"
	rm -rf $(DESTDIR)$(PREFIX)/share/quadrate
	@echo "Removing bash completions from $(DESTDIR)$(PREFIX)/share/bash-completion/completions/"
	@for cmd in quad quadc quadfmt quadlint quadlsp quadpm quadrepl quaduses; do rm -f $(DESTDIR)$(PREFIX)/share/bash-completion/completions/$$cmd; done

docs:
	@echo "=========================================="
	@echo "  Generating API Documentation"
	@echo "=========================================="
	@if ! which doxygen > /dev/null 2>&1; then \
		echo "" && \
		echo "⚠️  Warning: doxygen not found - skipping documentation generation" && \
		echo "" && \
		echo "To generate documentation, install doxygen and graphviz (optional, for diagrams):" && \
		echo "  Arch Linux:    sudo pacman -S doxygen graphviz" && \
		echo "  Ubuntu/Debian: sudo apt install doxygen graphviz" && \
		echo "  Fedora:        sudo dnf install doxygen graphviz" && \
		echo "  macOS:         brew install doxygen graphviz" && \
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
