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
	@cp -f $(BUILD_DIR_DEBUG)/bin/quadfmt/quadfmt dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/bin/quadlsp/quadlsp dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/lib/quadrate/libquadrate.so dist/lib/
	@cp -f $(BUILD_DIR_DEBUG)/lib/quadrate/libquadrate_static.a dist/lib/
	@cp -rf lib/quadrate/include/quadrate dist/include/

release:
	meson setup $(BUILD_DIR_RELEASE) --buildtype=release $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_RELEASE)
	@mkdir -p dist/bin dist/lib dist/include
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadc/quadc dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadfmt/quadfmt dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadlsp/quadlsp dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/lib/quadrate/libquadrate.so dist/lib/
	@cp -f $(BUILD_DIR_RELEASE)/lib/quadrate/libquadrate_static.a dist/lib/
	@cp -rf lib/quadrate/include/quadrate dist/include/

tests: debug
	@echo "=== Running C/C++ unit tests ==="
	meson test -C $(BUILD_DIR_DEBUG) test_str test_runtime test_ast test_semantic_validator --print-errorlogs
	@echo ""
	@echo "=== Running LSP tests ==="
	meson test -C $(BUILD_DIR_DEBUG) test_lsp test_lsp_extended test_lsp_stress --print-errorlogs
	@echo ""
	@echo "=== Running Quadrate language tests ==="
	QUADC=$(BUILD_DIR_DEBUG)/bin/quadc/quadc bash tests/run_qd_tests.sh
	@echo ""
	@echo "=== Running formatter tests ==="
	bash tests/run_formatter_tests.sh

valgrind: debug
	@echo "=== Running C/C++ unit tests with valgrind ==="
	meson test -C $(BUILD_DIR_DEBUG) test_str test_runtime test_ast test_semantic_validator --setup=valgrind --print-errorlogs

examples:
	meson setup $(BUILD_DIR_DEBUG) --buildtype=debug --reconfigure -Dbuild_examples=true $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_DEBUG) examples/embed/embed examples/hello-world/hello-world examples/hello-world-c/hello-world-c examples/bmi/bmi

format:
	find bin lib examples -type f \( -name '*.cc' -o -name '*.h' \) -not -name 'utf8.h' -not -path '*/utf8/*' -exec clang-format -i {} +

install: release
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include
	install -m 755 dist/bin/quadc $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadfmt $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadlsp $(DESTDIR)$(PREFIX)/bin/
	install -m 644 dist/lib/libquadrate.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libquadrate_static.a $(DESTDIR)$(PREFIX)/lib/
	cp -r dist/include/quadrate $(DESTDIR)$(PREFIX)/include/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/quadc
	rm -f $(DESTDIR)$(PREFIX)/bin/quadfmt
	rm -f $(DESTDIR)$(PREFIX)/bin/quadlsp
	rm -f $(DESTDIR)$(PREFIX)/lib/libquadrate.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libquadrate_static.a
	rm -rf $(DESTDIR)$(PREFIX)/include/quadrate

clean:
	rm -rf build
	rm -rf dist
