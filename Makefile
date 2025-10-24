BUILD_DIR_DEBUG   := build/debug
BUILD_DIR_RELEASE := build/release

MESON_FLAGS := -Dbuild_tests=true

PREFIX ?= /usr/local

.PHONY: all debug release tests valgrind examples format install uninstall clean

all: debug

debug:
	meson setup $(BUILD_DIR_DEBUG) --buildtype=debug $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_DEBUG)
	@mkdir -p dist/bin dist/lib dist/include
	@cp -f $(BUILD_DIR_DEBUG)/bin/quadc/quadc dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/bin/quadfmt/quadfmt dist/bin/
	@cp -f $(BUILD_DIR_DEBUG)/lib/quadrate/libquadrate.so dist/lib/
	@cp -f $(BUILD_DIR_DEBUG)/lib/quadrate/libquadrate_static.a dist/lib/
	@cp -rf lib/quadrate/include/quadrate dist/include/

release:
	meson setup $(BUILD_DIR_RELEASE) --buildtype=release $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_RELEASE)
	@mkdir -p dist/bin dist/lib dist/include
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadc/quadc dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/bin/quadfmt/quadfmt dist/bin/
	@cp -f $(BUILD_DIR_RELEASE)/lib/quadrate/libquadrate.so dist/lib/
	@cp -f $(BUILD_DIR_RELEASE)/lib/quadrate/libquadrate_static.a dist/lib/
	@cp -rf lib/quadrate/include/quadrate dist/include/

tests: debug
	meson test -C $(BUILD_DIR_DEBUG) --print-errorlogs

valgrind: debug
	meson test -C $(BUILD_DIR_DEBUG) --setup=valgrind --print-errorlogs

examples:
	meson setup $(BUILD_DIR_DEBUG) --buildtype=debug -Dbuild_examples=true $(MESON_FLAGS)
	meson compile -C $(BUILD_DIR_DEBUG)

format:
	find bin lib examples -type f \( -name '*.cc' -o -name '*.h' \) -not -name 'utf8.h' -not -path '*/utf8/*' -exec clang-format -i {} +

install: release
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include
	install -m 755 dist/bin/quadc $(DESTDIR)$(PREFIX)/bin/
	install -m 755 dist/bin/quadfmt $(DESTDIR)$(PREFIX)/bin/
	install -m 644 dist/lib/libquadrate.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 dist/lib/libquadrate_static.a $(DESTDIR)$(PREFIX)/lib/
	cp -r dist/include/quadrate $(DESTDIR)$(PREFIX)/include/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/quadc
	rm -f $(DESTDIR)$(PREFIX)/bin/quadfmt
	rm -f $(DESTDIR)$(PREFIX)/lib/libquadrate.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libquadrate_static.a
	rm -rf $(DESTDIR)$(PREFIX)/include/quadrate

clean:
	rm -rf build
	rm -rf dist
