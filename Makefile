BUILD_DIR_DEBUG   := build/debug
BUILD_DIR_RELEASE := build/release

MESON_FLAGS := -Dbuild_tests=true

PREFIX ?= /usr

# Use clang by default for better LLVM integration
export CC  := clang
export CXX := clang++

.PHONY: all debug release tests valgrind examples format install uninstall clean docs

all: debug

debug:
	meson setup $(BUILD_DIR_DEBUG) --buildtype=debug $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_DEBUG)
	@mkdir -p dist/bin dist/lib dist/include
	@cp -f $(BUILD_DIR_DEBUG)/bin/quadc/quadc dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/bin/quadfmt/quadfmt dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/bin/quadlsp/quadlsp dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/bin/quaduses/quaduses dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/lib/qdrt/libqdrt.so dist/lib/
	@echo "Creating full archive for libqdrt_static.a..."
	@rm -f dist/lib/libqdrt_static.a && cd $(BUILD_DIR_DEBUG)/lib/qdrt && echo "Files in thin archive:" && ar -t libqdrt_static.a | head -3 && ar rcs $(CURDIR)/dist/lib/libqdrt_static.a $$(ar -t libqdrt_static.a) && echo "Archive created successfully"
	@cp -f $(BUILD_DIR_DEBUG)/lib/qd/libqd.so dist/lib/
	@rm -f dist/lib/libqd_static.a && cd $(BUILD_DIR_DEBUG)/lib/qd && ar rcs $(CURDIR)/dist/lib/libqd_static.a $$(ar -t libqd_static.a)
	@cp -f $(BUILD_DIR_DEBUG)/lib/stdbitsqd/libstdbitsqd.so dist/lib/
	@echo "Creating full archive for libstdbitsqd_static.a..."
	@rm -f dist/lib/libstdbitsqd_static.a && cd $(BUILD_DIR_DEBUG)/lib/stdbitsqd && ar rcs $(CURDIR)/dist/lib/libstdbitsqd_static.a $$(ar -t libstdbitsqd_static.a) || (echo "ERROR: Failed to create libstdbitsqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_DEBUG)/lib/stdfmtqd/libstdfmtqd.so dist/lib/
	@echo "Creating full archive for libstdfmtqd_static.a..."
	@rm -f dist/lib/libstdfmtqd_static.a && cd $(BUILD_DIR_DEBUG)/lib/stdfmtqd && ar rcs $(CURDIR)/dist/lib/libstdfmtqd_static.a $$(ar -t libstdfmtqd_static.a) || (echo "ERROR: Failed to create libstdfmtqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_DEBUG)/lib/stdmathqd/libstdmathqd.so dist/lib/
	@echo "Creating full archive for libstdmathqd_static.a..."
	@rm -f dist/lib/libstdmathqd_static.a && cd $(BUILD_DIR_DEBUG)/lib/stdmathqd && ar rcs $(CURDIR)/dist/lib/libstdmathqd_static.a $$(ar -t libstdmathqd_static.a) || (echo "ERROR: Failed to create libstdmathqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_DEBUG)/lib/stdnetqd/libstdnetqd.so dist/lib/
	@echo "Creating full archive for libstdnetqd_static.a..."
	@rm -f dist/lib/libstdnetqd_static.a && cd $(BUILD_DIR_DEBUG)/lib/stdnetqd && ar rcs $(CURDIR)/dist/lib/libstdnetqd_static.a $$(ar -t libstdnetqd_static.a) || (echo "ERROR: Failed to create libstdnetqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_DEBUG)/lib/stdosqd/libstdosqd.so dist/lib/
	@echo "Creating full archive for libstdosqd_static.a..."
	@rm -f dist/lib/libstdosqd_static.a && cd $(BUILD_DIR_DEBUG)/lib/stdosqd && ar rcs $(CURDIR)/dist/lib/libstdosqd_static.a $$(ar -t libstdosqd_static.a) || (echo "ERROR: Failed to create libstdosqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_DEBUG)/lib/stdstrqd/libstdstrqd.so dist/lib/
	@echo "Creating full archive for libstdstrqd_static.a..."
	@rm -f dist/lib/libstdstrqd_static.a && cd $(BUILD_DIR_DEBUG)/lib/stdstrqd && ar rcs $(CURDIR)/dist/lib/libstdstrqd_static.a $$(ar -t libstdstrqd_static.a) || (echo "ERROR: Failed to create libstdstrqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_DEBUG)/lib/stdtimeqd/libstdtimeqd.so dist/lib/
	@echo "Creating full archive for libstdtimeqd_static.a..."
	@rm -f dist/lib/libstdtimeqd_static.a && cd $(BUILD_DIR_DEBUG)/lib/stdtimeqd && ar rcs $(CURDIR)/dist/lib/libstdtimeqd_static.a $$(ar -t libstdtimeqd_static.a) || (echo "ERROR: Failed to create libstdtimeqd_static.a" && exit 1)
	@cp -rf lib/qdrt/include/qdrt dist/include/
	@cp -rf lib/qd/include/qd dist/include/
	@cp -rf lib/stdbitsqd/include/stdbitsqd dist/include/
	@cp -rf lib/stdfmtqd/include/stdfmtqd dist/include/
	@cp -rf lib/stdmathqd/include/stdmathqd dist/include/
	@cp -rf lib/stdnetqd/include/stdnetqd dist/include/
	@cp -rf lib/stdosqd/include/stdosqd dist/include/
	@cp -rf lib/stdstrqd/include/stdstrqd dist/include/
	@cp -rf lib/stdtimeqd/include/stdtimeqd dist/include/
	@mkdir -p dist/share/quadrate
	@cp -r lib/stdbitsqd/qd/bits dist/share/quadrate/
	@cp -r lib/stdfmtqd/qd/fmt dist/share/quadrate/
	@cp -r lib/stdmathqd/qd/math dist/share/quadrate/
	@cp -r lib/stdnetqd/qd/net dist/share/quadrate/
	@cp -r lib/stdosqd/qd/os dist/share/quadrate/
	@cp -r lib/stdstrqd/qd/str dist/share/quadrate/
	@cp -r lib/stdtimeqd/qd/time dist/share/quadrate/
	@echo "Verifying static archives..."
	@file dist/lib/libqdrt_static.a dist/lib/libstdosqd_static.a | head -2
	@echo "Debug build complete - static libraries ready"

release:
	meson setup $(BUILD_DIR_RELEASE) --buildtype=release $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_RELEASE)
	@mkdir -p dist/bin dist/lib dist/include
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadc/quadc dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadfmt/quadfmt dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadlsp/quadlsp dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/bin/quaduses/quaduses dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/lib/qdrt/libqdrt.so dist/lib/
	@echo "Creating full archive for libqdrt_static.a (release)..."
	@rm -f dist/lib/libqdrt_static.a && cd $(BUILD_DIR_RELEASE)/lib/qdrt && ar rcs $(CURDIR)/dist/lib/libqdrt_static.a $$(ar -t libqdrt_static.a) && echo "Archive created successfully"
	@cp -f $(BUILD_DIR_RELEASE)/lib/qd/libqd.so dist/lib/
	@echo "Creating full archive for libqd_static.a (release)..."
	@rm -f dist/lib/libqd_static.a && cd $(BUILD_DIR_RELEASE)/lib/qd && ar rcs $(CURDIR)/dist/lib/libqd_static.a $$(ar -t libqd_static.a) || (echo "ERROR: Failed to create libqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_RELEASE)/lib/stdbitsqd/libstdbitsqd.so dist/lib/
	@echo "Creating full archive for libstdbitsqd_static.a (release)..."
	@rm -f dist/lib/libstdbitsqd_static.a && cd $(BUILD_DIR_RELEASE)/lib/stdbitsqd && ar rcs $(CURDIR)/dist/lib/libstdbitsqd_static.a $$(ar -t libstdbitsqd_static.a) || (echo "ERROR: Failed to create libstdbitsqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_RELEASE)/lib/stdfmtqd/libstdfmtqd.so dist/lib/
	@echo "Creating full archive for libstdfmtqd_static.a (release)..."
	@rm -f dist/lib/libstdfmtqd_static.a && cd $(BUILD_DIR_RELEASE)/lib/stdfmtqd && ar rcs $(CURDIR)/dist/lib/libstdfmtqd_static.a $$(ar -t libstdfmtqd_static.a) || (echo "ERROR: Failed to create libstdfmtqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_RELEASE)/lib/stdmathqd/libstdmathqd.so dist/lib/
	@echo "Creating full archive for libstdmathqd_static.a (release)..."
	@rm -f dist/lib/libstdmathqd_static.a && cd $(BUILD_DIR_RELEASE)/lib/stdmathqd && ar rcs $(CURDIR)/dist/lib/libstdmathqd_static.a $$(ar -t libstdmathqd_static.a) || (echo "ERROR: Failed to create libstdmathqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_RELEASE)/lib/stdnetqd/libstdnetqd.so dist/lib/
	@echo "Creating full archive for libstdnetqd_static.a (release)..."
	@rm -f dist/lib/libstdnetqd_static.a && cd $(BUILD_DIR_RELEASE)/lib/stdnetqd && ar rcs $(CURDIR)/dist/lib/libstdnetqd_static.a $$(ar -t libstdnetqd_static.a) || (echo "ERROR: Failed to create libstdnetqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_RELEASE)/lib/stdosqd/libstdosqd.so dist/lib/
	@echo "Creating full archive for libstdosqd_static.a (release)..."
	@rm -f dist/lib/libstdosqd_static.a && cd $(BUILD_DIR_RELEASE)/lib/stdosqd && ar rcs $(CURDIR)/dist/lib/libstdosqd_static.a $$(ar -t libstdosqd_static.a) || (echo "ERROR: Failed to create libstdosqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_RELEASE)/lib/stdstrqd/libstdstrqd.so dist/lib/
	@echo "Creating full archive for libstdstrqd_static.a (release)..."
	@rm -f dist/lib/libstdstrqd_static.a && cd $(BUILD_DIR_RELEASE)/lib/stdstrqd && ar rcs $(CURDIR)/dist/lib/libstdstrqd_static.a $$(ar -t libstdstrqd_static.a) || (echo "ERROR: Failed to create libstdstrqd_static.a" && exit 1)
	@cp -f $(BUILD_DIR_RELEASE)/lib/stdtimeqd/libstdtimeqd.so dist/lib/
	@echo "Creating full archive for libstdtimeqd_static.a (release)..."
	@rm -f dist/lib/libstdtimeqd_static.a && cd $(BUILD_DIR_RELEASE)/lib/stdtimeqd && ar rcs $(CURDIR)/dist/lib/libstdtimeqd_static.a $$(ar -t libstdtimeqd_static.a) || (echo "ERROR: Failed to create libstdtimeqd_static.a" && exit 1)
	@cp -rf lib/qdrt/include/qdrt dist/include/
	@cp -rf lib/qd/include/qd dist/include/
	@cp -rf lib/stdbitsqd/include/stdbitsqd dist/include/
	@cp -rf lib/stdfmtqd/include/stdfmtqd dist/include/
	@cp -rf lib/stdmathqd/include/stdmathqd dist/include/
	@cp -rf lib/stdnetqd/include/stdnetqd dist/include/
	@cp -rf lib/stdosqd/include/stdosqd dist/include/
	@cp -rf lib/stdstrqd/include/stdstrqd dist/include/
	@cp -rf lib/stdtimeqd/include/stdtimeqd dist/include/
	@mkdir -p dist/share/quadrate
	@cp -r lib/stdbitsqd/qd/bits dist/share/quadrate/
	@cp -r lib/stdfmtqd/qd/fmt dist/share/quadrate/
	@cp -r lib/stdmathqd/qd/math dist/share/quadrate/
	@cp -r lib/stdnetqd/qd/net dist/share/quadrate/
	@cp -r lib/stdosqd/qd/os dist/share/quadrate/
	@cp -r lib/stdstrqd/qd/str dist/share/quadrate/
	@cp -r lib/stdtimeqd/qd/time dist/share/quadrate/
	@echo "Verifying static archives (release)..."
	@file dist/lib/libqdrt_static.a dist/lib/libstdosqd_static.a | head -2
	@echo "Release build complete - static libraries ready"

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
	@echo "=== Running tree-sitter grammar tests ==="
	@if command -v tree-sitter >/dev/null 2>&1; then \
		meson test -C $(BUILD_DIR_DEBUG) tree-sitter-grammar --print-errorlogs; \
	else \
		echo "⚠️  Skipped (tree-sitter not installed)"; \
	fi
	@echo ""
	@echo "=== Running Quadrate language tests ==="
	QUADC=$(BUILD_DIR_DEBUG)/bin/quadc/quadc bash tests/run_tests.sh qd || true
	@echo ""
	@echo "=== Running formatter tests ==="
	bash tests/run_tests.sh formatter
	@echo ""
	@echo "=== Running quaduses tests ==="
	bash tests/run_tests.sh quaduses
	@echo ""
	@echo "=========================================="
	@echo "  Test Suite Complete"
	@echo "=========================================="

valgrind: debug
	@echo "=== Running C/C++ unit tests with valgrind ==="
	meson test -C $(BUILD_DIR_DEBUG) test_runtime test_ast test_semantic_validator --setup=valgrind --print-errorlogs
	@echo ""
	@echo "=== Running Quadrate language tests with valgrind ==="
	QUADC=$(BUILD_DIR_DEBUG)/bin/quadc/quadc bash tests/run_tests.sh valgrind
	@echo ""
	@echo "=== Running LSP tests with valgrind ==="
	@if command -v valgrind >/dev/null 2>&1; then \
		meson test -C $(BUILD_DIR_DEBUG) test_lsp test_lsp_extended --setup=valgrind --print-errorlogs; \
	else \
		echo "⚠️  Valgrind not installed, skipping"; \
	fi

examples:
	@mkdir -p dist/examples
	meson setup $(BUILD_DIR_DEBUG) --buildtype=debug --reconfigure -Dbuild_examples=true $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_DEBUG) examples/embed/embed examples/hello-world/hello-world examples/hello-world-c/hello-world-c examples/bmi/bmi examples/web-server/web-server

format:
	find bin lib examples -type f \( -name '*.cc' -o -name '*.h' \) -not -name 'utf8.h' -not -path '*/utf8/*' -exec clang-format -i {} +

install: release
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include
	install -m 755 dist/bin/quadc $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadfmt $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadlsp $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quaduses $(DESTDIR)$(PREFIX)/bin/
	install -m 644 dist/lib/libqdrt.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdrt_static.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqd.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqd_static.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdbitsqd.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdbitsqd_static.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdfmtqd.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdfmtqd_static.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdmathqd.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdmathqd_static.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdnetqd.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdnetqd_static.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdosqd.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdosqd_static.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdstrqd.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdstrqd_static.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdtimeqd.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdtimeqd_static.a $(DESTDIR)$(PREFIX)/lib/
	cp -r dist/include/qdrt $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qd $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/stdbitsqd $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/stdfmtqd $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/stdmathqd $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/stdnetqd $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/stdosqd $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/stdstrqd $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/stdtimeqd $(DESTDIR)$(PREFIX)/include/
	@echo "Installing Quadrate standard library modules to $(DESTDIR)$(PREFIX)/share/quadrate/"
	install -d $(DESTDIR)$(PREFIX)/share/quadrate
	@cp -r lib/stdbitsqd/qd/bits $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/stdfmtqd/qd/fmt $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/stdmathqd/qd/math $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/stdnetqd/qd/net $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/stdosqd/qd/os $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/stdstrqd/qd/str $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/stdtimeqd/qd/time $(DESTDIR)$(PREFIX)/share/quadrate/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/quadc
	rm -f $(DESTDIR)$(PREFIX)/bin/quadfmt
	rm -f $(DESTDIR)$(PREFIX)/bin/quadlsp
	rm -f $(DESTDIR)$(PREFIX)/bin/quaduses
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdrt.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdrt_static.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqd.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libqd_static.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdbitsqd.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdbitsqd_static.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdfmtqd.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdfmtqd_static.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdmathqd.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdmathqd_static.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdnetqd.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdnetqd_static.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdosqd.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdosqd_static.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdstrqd.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdstrqd_static.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdtimeqd.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdtimeqd_static.a
	rm -rf $(DESTDIR)$(PREFIX)/include/qdrt
	rm -rf $(DESTDIR)$(PREFIX)/include/qd
	rm -rf $(DESTDIR)$(PREFIX)/include/stdbitsqd
	rm -rf $(DESTDIR)$(PREFIX)/include/stdfmtqd
	rm -rf $(DESTDIR)$(PREFIX)/include/stdmathqd
	rm -rf $(DESTDIR)$(PREFIX)/include/stdnetqd
	rm -rf $(DESTDIR)$(PREFIX)/include/stdosqd
	rm -rf $(DESTDIR)$(PREFIX)/include/stdstrqd
	rm -rf $(DESTDIR)$(PREFIX)/include/stdtimeqd
	@echo "Removing Quadrate standard library modules from $(DESTDIR)$(PREFIX)/share/quadrate/"
	rm -rf $(DESTDIR)$(PREFIX)/share/quadrate

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
