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
	@cp -f $(BUILD_DIR_DEBUG)/cmd/quad/quad dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/cmd/quadc/quadc dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/cmd/quadfmt/quadfmt dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/cmd/quadlint/quadlint dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/cmd/quadlsp/quadlsp dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/cmd/quadpm/quadpm dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/cmd/quaduses/quaduses dist/bin/
	@if [ -f $(BUILD_DIR_DEBUG)/cmd/quadrepl/quadrepl ]; then cp -f $(BUILD_DIR_DEBUG)/cmd/quadrepl/quadrepl dist/bin/; else echo "Note: quadrepl not built (readline not found)"; fi
	@echo "Creating static libraries..."
	@rm -f dist/lib/libqdrt.a && cd $(BUILD_DIR_DEBUG)/lib/qdrt && ar rcs ../../../../dist/lib/libqdrt.a $$(ar -t libqdrt_static.a) && echo "  libqdrt.a"
	@rm -f dist/lib/libqd.a && cd $(BUILD_DIR_DEBUG)/lib/qd && ar rcs ../../../../dist/lib/libqd.a $$(ar -t libqd_static.a) && echo "  libqd.a"
	# qdbits is pure Quadrate (no C library)
	@rm -f dist/lib/libqdfmt.a && cd $(BUILD_DIR_DEBUG)/lib/qdfmt && ar rcs ../../../../dist/lib/libqdfmt.a $$(ar -t libqdfmt.a) && echo "  libqdfmt.a"
	@rm -f dist/lib/libqdio.a && cd $(BUILD_DIR_DEBUG)/lib/qdio && ar rcs ../../../../dist/lib/libqdio.a $$(ar -t libqdio.a) && echo "  libqdio.a"
	@rm -f dist/lib/libqdmath.a && cd $(BUILD_DIR_DEBUG)/lib/qdmath && ar rcs ../../../../dist/lib/libqdmath.a $$(ar -t libqdmath.a) && echo "  libqdmath.a"
	@rm -f dist/lib/libqdmem.a && cd $(BUILD_DIR_DEBUG)/lib/qdmem && ar rcs ../../../../dist/lib/libqdmem.a $$(ar -t libqdmem.a) && echo "  libqdmem.a"
	@rm -f dist/lib/libqdnet.a && cd $(BUILD_DIR_DEBUG)/lib/qdnet && ar rcs ../../../../dist/lib/libqdnet.a $$(ar -t libqdnet.a) && echo "  libqdnet.a"
	@rm -f dist/lib/libqdos.a && cd $(BUILD_DIR_DEBUG)/lib/qdos && ar rcs ../../../../dist/lib/libqdos.a $$(ar -t libqdos.a) && echo "  libqdos.a"
	@rm -f dist/lib/libqdstr.a && cd $(BUILD_DIR_DEBUG)/lib/qdstr && ar rcs ../../../../dist/lib/libqdstr.a $$(ar -t libqdstr.a) && echo "  libqdstr.a"
	@rm -f dist/lib/libqdstrconv.a && cd $(BUILD_DIR_DEBUG)/lib/qdstrconv && ar rcs ../../../../dist/lib/libqdstrconv.a $$(ar -t libqdstrconv.a) && echo "  libqdstrconv.a"
	@rm -f dist/lib/libqdtime.a && cd $(BUILD_DIR_DEBUG)/lib/qdtime && ar rcs ../../../../dist/lib/libqdtime.a $$(ar -t libqdtime.a) && echo "  libqdtime.a"
	@rm -f dist/lib/libqdtesting.a && cd $(BUILD_DIR_DEBUG)/lib/qdtesting && ar rcs ../../../../dist/lib/libqdtesting.a $$(ar -t libqdtesting.a) && echo "  libqdtesting.a"
	@cp -rf lib/qdrt/include/qdrt dist/include/
	@cp -rf lib/qd/include/qd dist/include/
	# qdbits has no C headers (pure Quadrate module)
	@cp -rf lib/qdfmt/include/qdfmt dist/include/
	@cp -rf lib/qdio/include/qdio dist/include/
	@cp -rf lib/qdmath/include/qdmath dist/include/
	@cp -rf lib/qdmem/include/qdmem dist/include/
	@cp -rf lib/qdnet/include/qdnet dist/include/
	@cp -rf lib/qdos/include/qdos dist/include/
	@cp -rf lib/qdstr/include/qdstr dist/include/
	@cp -rf lib/qdstrconv/include/qdstrconv dist/include/
	@cp -rf lib/qdtime/include/qdtime dist/include/
	@cp -rf lib/qdtesting/include/qdtesting dist/include/
	@mkdir -p dist/share/quadrate
	@cp -r lib/qdbase64/qd/base64 dist/share/quadrate/
	@cp -r lib/qdbits/qd/bits dist/share/quadrate/
	@cp -r lib/qdflag/qd/flag dist/share/quadrate/
	@cp -r lib/qdfmt/qd/fmt dist/share/quadrate/
	@cp -r lib/qdio/qd/io dist/share/quadrate/
	@cp -r lib/qdjson/qd/json dist/share/quadrate/
	@cp -r lib/qdmath/qd/math dist/share/quadrate/
	@cp -r lib/qdmem/qd/mem dist/share/quadrate/
	@cp -r lib/qdnet/qd/net dist/share/quadrate/
	@cp -r lib/qdos/qd/os dist/share/quadrate/
	@cp -r lib/qdsb/qd/sb dist/share/quadrate/
	@cp -r lib/qdstr/qd/str dist/share/quadrate/
	@cp -r lib/qdstrconv/qd/strconv dist/share/quadrate/
	@cp -r lib/qdtime/qd/time dist/share/quadrate/
	@cp -r lib/qdunicode/qd/unicode dist/share/quadrate/
	@cp -r lib/qduri/qd/uri dist/share/quadrate/
	@cp -r lib/qdhex/qd/hex dist/share/quadrate/
	@cp -r lib/qdbytes/qd/bytes dist/share/quadrate/
	@cp -r lib/qdcrc32/qd/crc32 dist/share/quadrate/
	@cp -r lib/qdsha256/qd/sha256 dist/share/quadrate/
	@cp -r lib/qdregex/qd/regex dist/share/quadrate/
	@cp -r lib/qdpath/qd/path dist/share/quadrate/
	@cp -r lib/qdsort/qd/sort dist/share/quadrate/
	@cp -r lib/qdrand/qd/rand dist/share/quadrate/
	@cp -r lib/qduuid/qd/uuid dist/share/quadrate/
	@cp -r lib/qdtesting/qd/testing dist/share/quadrate/
	@echo "Debug build complete - static libraries ready"

release:
	meson setup $(BUILD_DIR_RELEASE) --buildtype=release $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_RELEASE)
	@mkdir -p dist/bin dist/lib dist/include
	@cp -f $(BUILD_DIR_RELEASE)/cmd/quad/quad dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/cmd/quadc/quadc dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/cmd/quadfmt/quadfmt dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/cmd/quadlint/quadlint dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/cmd/quadlsp/quadlsp dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/cmd/quadpm/quadpm dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/cmd/quaduses/quaduses dist/bin/
	@if [ -f $(BUILD_DIR_RELEASE)/cmd/quadrepl/quadrepl ]; then cp -f $(BUILD_DIR_RELEASE)/cmd/quadrepl/quadrepl dist/bin/; else echo "Note: quadrepl not built (readline not found)"; fi
	@echo "Creating static libraries (release)..."
	@rm -f dist/lib/libqdrt.a && cd $(BUILD_DIR_RELEASE)/lib/qdrt && ar rcs ../../../../dist/lib/libqdrt.a $$(ar -t libqdrt_static.a) && echo "  libqdrt.a"
	@rm -f dist/lib/libqd.a && cd $(BUILD_DIR_RELEASE)/lib/qd && ar rcs ../../../../dist/lib/libqd.a $$(ar -t libqd_static.a) && echo "  libqd.a"
	# qdbits is pure Quadrate (no C library)
	@rm -f dist/lib/libqdfmt.a && cd $(BUILD_DIR_RELEASE)/lib/qdfmt && ar rcs ../../../../dist/lib/libqdfmt.a $$(ar -t libqdfmt.a) && echo "  libqdfmt.a"
	@rm -f dist/lib/libqdio.a && cd $(BUILD_DIR_RELEASE)/lib/qdio && ar rcs ../../../../dist/lib/libqdio.a $$(ar -t libqdio.a) && echo "  libqdio.a"
	@rm -f dist/lib/libqdmath.a && cd $(BUILD_DIR_RELEASE)/lib/qdmath && ar rcs ../../../../dist/lib/libqdmath.a $$(ar -t libqdmath.a) && echo "  libqdmath.a"
	@rm -f dist/lib/libqdmem.a && cd $(BUILD_DIR_RELEASE)/lib/qdmem && ar rcs ../../../../dist/lib/libqdmem.a $$(ar -t libqdmem.a) && echo "  libqdmem.a"
	@rm -f dist/lib/libqdnet.a && cd $(BUILD_DIR_RELEASE)/lib/qdnet && ar rcs ../../../../dist/lib/libqdnet.a $$(ar -t libqdnet.a) && echo "  libqdnet.a"
	@rm -f dist/lib/libqdos.a && cd $(BUILD_DIR_RELEASE)/lib/qdos && ar rcs ../../../../dist/lib/libqdos.a $$(ar -t libqdos.a) && echo "  libqdos.a"
	@rm -f dist/lib/libqdstr.a && cd $(BUILD_DIR_RELEASE)/lib/qdstr && ar rcs ../../../../dist/lib/libqdstr.a $$(ar -t libqdstr.a) && echo "  libqdstr.a"
	@rm -f dist/lib/libqdstrconv.a && cd $(BUILD_DIR_RELEASE)/lib/qdstrconv && ar rcs ../../../../dist/lib/libqdstrconv.a $$(ar -t libqdstrconv.a) && echo "  libqdstrconv.a"
	@rm -f dist/lib/libqdtime.a && cd $(BUILD_DIR_RELEASE)/lib/qdtime && ar rcs ../../../../dist/lib/libqdtime.a $$(ar -t libqdtime.a) && echo "  libqdtime.a"
	@rm -f dist/lib/libqdtesting.a && cd $(BUILD_DIR_RELEASE)/lib/qdtesting && ar rcs ../../../../dist/lib/libqdtesting.a $$(ar -t libqdtesting.a) && echo "  libqdtesting.a"
	@cp -rf lib/qdrt/include/qdrt dist/include/
	@cp -rf lib/qd/include/qd dist/include/
	# qdbits has no C headers (pure Quadrate module)
	@cp -rf lib/qdfmt/include/qdfmt dist/include/
	@cp -rf lib/qdio/include/qdio dist/include/
	@cp -rf lib/qdmath/include/qdmath dist/include/
	@cp -rf lib/qdmem/include/qdmem dist/include/
	@cp -rf lib/qdnet/include/qdnet dist/include/
	@cp -rf lib/qdos/include/qdos dist/include/
	@cp -rf lib/qdstr/include/qdstr dist/include/
	@cp -rf lib/qdstrconv/include/qdstrconv dist/include/
	@cp -rf lib/qdtime/include/qdtime dist/include/
	@cp -rf lib/qdtesting/include/qdtesting dist/include/
	@mkdir -p dist/share/quadrate
	@cp -r lib/qdbase64/qd/base64 dist/share/quadrate/
	@cp -r lib/qdbits/qd/bits dist/share/quadrate/
	@cp -r lib/qdflag/qd/flag dist/share/quadrate/
	@cp -r lib/qdfmt/qd/fmt dist/share/quadrate/
	@cp -r lib/qdio/qd/io dist/share/quadrate/
	@cp -r lib/qdjson/qd/json dist/share/quadrate/
	@cp -r lib/qdmath/qd/math dist/share/quadrate/
	@cp -r lib/qdmem/qd/mem dist/share/quadrate/
	@cp -r lib/qdnet/qd/net dist/share/quadrate/
	@cp -r lib/qdos/qd/os dist/share/quadrate/
	@cp -r lib/qdsb/qd/sb dist/share/quadrate/
	@cp -r lib/qdstr/qd/str dist/share/quadrate/
	@cp -r lib/qdstrconv/qd/strconv dist/share/quadrate/
	@cp -r lib/qdtime/qd/time dist/share/quadrate/
	@cp -r lib/qdunicode/qd/unicode dist/share/quadrate/
	@cp -r lib/qduri/qd/uri dist/share/quadrate/
	@cp -r lib/qdhex/qd/hex dist/share/quadrate/
	@cp -r lib/qdbytes/qd/bytes dist/share/quadrate/
	@cp -r lib/qdcrc32/qd/crc32 dist/share/quadrate/
	@cp -r lib/qdsha256/qd/sha256 dist/share/quadrate/
	@cp -r lib/qdregex/qd/regex dist/share/quadrate/
	@cp -r lib/qdpath/qd/path dist/share/quadrate/
	@cp -r lib/qdsort/qd/sort dist/share/quadrate/
	@cp -r lib/qdrand/qd/rand dist/share/quadrate/
	@cp -r lib/qduuid/qd/uuid dist/share/quadrate/
	@cp -r lib/qdtesting/qd/testing dist/share/quadrate/
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
	meson compile -C $(BUILD_DIR_DEBUG) examples/embed/embed examples/embed/embed_copy examples/embed/multi-module-test examples/embed/multi-module-test_copy examples/embed/native-functions-test examples/embed/native-functions-test_copy examples/embed/incremental-test examples/embed/incremental-test_copy examples/hello-world/hello-world examples/hello-world-c/hello-world-c examples/bmi/bmi examples/dc/dc examples/defer/defer examples/donut/donut examples/errors/errors examples/fibonacci/fibonacci examples/modules/modules examples/sha256sum/sha256sum examples/sierpinski/sierpinski examples/stars/stars examples/structs/structs examples/threading/threading examples/web-server/web-server
	@echo "Copying shared libraries for embed examples..."
	@cp -f $(BUILD_DIR_DEBUG)/lib/qd/libqd.so dist/lib/
	@cp -f $(BUILD_DIR_DEBUG)/lib/qdrt/libqdrt.so dist/lib/

format:
	find cmd lib examples -type f \( -name '*.cc' -o -name '*.h' \) -not -name 'utf8.h' -not -path '*/utf8/*' -exec clang-format -i {} +

install: release
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include
	install -m 755 dist/bin/quad $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadc $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadfmt $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadlint $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadlsp $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadpm $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quaduses $(DESTDIR)$(PREFIX)/bin/
	@if [ -f dist/bin/quadrepl ]; then install -m 755 dist/bin/quadrepl $(DESTDIR)$(PREFIX)/bin/; fi
	install -m 644 dist/lib/libqdrt.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqd.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdfmt.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdio.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdmath.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdmem.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdnet.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdos.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdstr.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdstrconv.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdtime.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdtesting.a $(DESTDIR)$(PREFIX)/lib/
	cp -r dist/include/qdrt $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qd $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qdfmt $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qdio $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qdmath $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qdmem $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qdnet $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qdos $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qdstr $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qdstrconv $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qdtime $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qdtesting $(DESTDIR)$(PREFIX)/include/
	@echo "Installing Quadrate standard library modules to $(DESTDIR)$(PREFIX)/share/quadrate/"
	install -d $(DESTDIR)$(PREFIX)/share/quadrate
	@cp -r lib/qdbase64/qd/base64 $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdbits/qd/bits $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdflag/qd/flag $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdfmt/qd/fmt $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdio/qd/io $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdjson/qd/json $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdmath/qd/math $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdmem/qd/mem $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdnet/qd/net $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdos/qd/os $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdsb/qd/sb $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdstr/qd/str $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdstrconv/qd/strconv $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdtime/qd/time $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdunicode/qd/unicode $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qduri/qd/uri $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdhex/qd/hex $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdbytes/qd/bytes $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdcrc32/qd/crc32 $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdsha256/qd/sha256 $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdregex/qd/regex $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdpath/qd/path $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdsort/qd/sort $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdrand/qd/rand $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qduuid/qd/uuid $(DESTDIR)$(PREFIX)/share/quadrate/
	@cp -r lib/qdtesting/qd/testing $(DESTDIR)$(PREFIX)/share/quadrate/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/quad
	rm -f $(DESTDIR)$(PREFIX)/bin/quadc
	rm -f $(DESTDIR)$(PREFIX)/bin/quadfmt
	rm -f $(DESTDIR)$(PREFIX)/bin/quadlint
	rm -f $(DESTDIR)$(PREFIX)/bin/quadlsp
	rm -f $(DESTDIR)$(PREFIX)/bin/quadpm
	rm -f $(DESTDIR)$(PREFIX)/bin/quaduses
	-rm -f $(DESTDIR)$(PREFIX)/bin/quadrepl
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdrt.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqd.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdfmt.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdio.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdmath.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdmem.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdnet.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdos.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdstr.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdstrconv.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdtime.a
	rm -rf $(DESTDIR)$(PREFIX)/include/qdrt
	rm -rf $(DESTDIR)$(PREFIX)/include/qd
	rm -rf $(DESTDIR)$(PREFIX)/include/qdfmt
	rm -rf $(DESTDIR)$(PREFIX)/include/qdio
	rm -rf $(DESTDIR)$(PREFIX)/include/qdmath
	rm -rf $(DESTDIR)$(PREFIX)/include/qdmem
	rm -rf $(DESTDIR)$(PREFIX)/include/qdnet
	rm -rf $(DESTDIR)$(PREFIX)/include/qdos
	rm -rf $(DESTDIR)$(PREFIX)/include/qdstr
	rm -rf $(DESTDIR)$(PREFIX)/include/qdstrconv
	rm -rf $(DESTDIR)$(PREFIX)/include/qdtime
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
