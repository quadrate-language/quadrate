BUILD_DIR_DEBUG   := build/debug
BUILD_DIR_RELEASE := build/release

MESON_FLAGS := -Dbuild_tests=true

.PHONY: all debug release tests valgrind examples format clean

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

clean:
	rm -rf build
	rm -rf dist
