BUILD_DIR_DEBUG   := build/debug
BUILD_DIR_RELEASE := build/release

MESON_FLAGS := -Dbuild_tests=true

PREFIX ?= /usr

.PHONY: all debug release tests valgrind examples format install uninstall clean

all: debug

debug:
	meson setup $(BUILD_DIR_DEBUG) --buildtype=debug $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_DEBUG)
	@mkdir -p dist/bin dist/lib dist/include
	@cp -f $(BUILD_DIR_DEBUG)/bin/quadc/quadc dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/bin/quadc-llvm/quadc-llvm dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/bin/quadfmt/quadfmt dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/bin/quadlsp/quadlsp dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/lib/qdrt/libqdrt.so dist/lib/
	@cp -f $(BUILD_DIR_DEBUG)/lib/qdrt/libqdrt_static.a dist/lib/
	@cp -f $(BUILD_DIR_DEBUG)/lib/qd/libqd.so dist/lib/
	@cp -f $(BUILD_DIR_DEBUG)/lib/qd/libqd_static.a dist/lib/
	@cp -f $(BUILD_DIR_DEBUG)/lib/stdqd/libstdqd.so dist/lib/
	@cp -f $(BUILD_DIR_DEBUG)/lib/stdqd/libstdqd_static.a dist/lib/
	@cp -rf lib/qdrt/include/qdrt dist/include/
	@cp -rf lib/qd/include/qd dist/include/
	@cp -rf lib/stdqd/include/stdqd dist/include/

release:
	meson setup $(BUILD_DIR_RELEASE) --buildtype=release $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_RELEASE)
	@mkdir -p dist/bin dist/lib dist/include
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadc/quadc dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadc-llvm/quadc-llvm dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadfmt/quadfmt dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadlsp/quadlsp dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/lib/qdrt/libqdrt.so dist/lib/
	@cp -f $(BUILD_DIR_RELEASE)/lib/qdrt/libqdrt_static.a dist/lib/
	@cp -f $(BUILD_DIR_RELEASE)/lib/qd/libqd.so dist/lib/
	@cp -f $(BUILD_DIR_RELEASE)/lib/qd/libqd_static.a dist/lib/
	@cp -f $(BUILD_DIR_RELEASE)/lib/stdqd/libstdqd.so dist/lib/
	@cp -f $(BUILD_DIR_RELEASE)/lib/stdqd/libstdqd_static.a dist/lib/
	@cp -rf lib/qdrt/include/qdrt dist/include/
	@cp -rf lib/qd/include/qd dist/include/
	@cp -rf lib/stdqd/include/stdqd dist/include/

tests: debug
	@echo "=========================================="
	@echo "  Quadrate Test Suite - All Backends"
	@echo "=========================================="
	@echo ""
	@echo "=== Running C/C++ unit tests ==="
	meson test -C $(BUILD_DIR_DEBUG) test_runtime test_ast test_semantic_validator stdqd --print-errorlogs
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
	@echo "=========================================="
	@echo "  Backend: quadc (C Transpiler)"
	@echo "=========================================="
	@echo ""
	@echo "=== Running Quadrate language tests (quadc) ==="
	QUADC=$(BUILD_DIR_DEBUG)/bin/quadc/quadc bash tests/run_qd_tests_parallel.sh || true
	@echo ""
	@echo "=========================================="
	@echo "  Backend: quadc-llvm (LLVM)"
	@echo "=========================================="
	@echo ""
	@echo "=== Running Quadrate language tests (quadc-llvm) ==="
	@cd tests/qd && QUADRATE_ROOT=../../lib/stdqd/qd QUADC_LLVM=../../$(BUILD_DIR_DEBUG)/bin/quadc-llvm/quadc-llvm bash ../../tests/run_qd_tests_llvm.sh || true
	@echo ""
	@echo "=========================================="
	@echo "  Other Tests"
	@echo "=========================================="
	@echo ""
	@echo "=== Running formatter tests ==="
	bash tests/run_formatter_tests.sh
	@echo ""
	@echo "=========================================="
	@echo "  Test Suite Complete"
	@echo "=========================================="

valgrind: debug
	@echo "=== Running C/C++ unit tests with valgrind ==="
	meson test -C $(BUILD_DIR_DEBUG) test_runtime test_ast test_semantic_validator stdqd --setup=valgrind --print-errorlogs
	@echo ""
	@echo "=== Running Quadrate language tests with valgrind ==="
	QUADC=$(BUILD_DIR_DEBUG)/bin/quadc/quadc bash tests/run_qd_tests_valgrind_parallel.sh
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
	install -m 755 dist/bin/quadc-llvm $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadfmt $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadlsp $(DESTDIR)$(PREFIX)/bin/
	install -m 644 dist/lib/libqdrt.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqdrt_static.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqd.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libqd_static.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdqd.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libstdqd_static.a $(DESTDIR)$(PREFIX)/lib/
	cp -r dist/include/qdrt $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/qd $(DESTDIR)$(PREFIX)/include/
	cp -r dist/include/stdqd $(DESTDIR)$(PREFIX)/include/
	@echo "Installing Quadrate standard library modules to $(DESTDIR)$(PREFIX)/share/quadrate/"
	install -d $(DESTDIR)$(PREFIX)/share/quadrate
	@cd lib/stdqd/qd && find . -maxdepth 1 -type d -not -name "." -exec cp -r {} $(DESTDIR)$(PREFIX)/share/quadrate/ \;

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/quadc
	rm -f $(DESTDIR)$(PREFIX)/bin/quadc-llvm
	rm -f $(DESTDIR)$(PREFIX)/bin/quadfmt
	rm -f $(DESTDIR)$(PREFIX)/bin/quadlsp
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdrt.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libqdrt_static.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libqd.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libqd_static.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdqd.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libstdqd_static.a
	rm -rf $(DESTDIR)$(PREFIX)/include/qdrt
	rm -rf $(DESTDIR)$(PREFIX)/include/qd
	rm -rf $(DESTDIR)$(PREFIX)/include/stdqd
	@echo "Removing Quadrate standard library modules from $(DESTDIR)$(PREFIX)/share/quadrate/"
	rm -rf $(DESTDIR)$(PREFIX)/share/quadrate

clean:
	rm -rf build
	rm -rf dist
