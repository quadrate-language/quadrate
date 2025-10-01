BUILD_DIR_DEBUG   := build/debug
BUILD_DIR_RELEASE := build/release

CMAKE_FLAGS := -S . -DBUILD_TESTS=ON

.PHONY: all debug release tests examples format clean

all: debug

debug:
	cmake $(CMAKE_FLAGS) -B $(BUILD_DIR_DEBUG) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR_DEBUG) --parallel

release:
	cmake $(CMAKE_FLAGS) -B $(BUILD_DIR_RELEASE) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR_RELEASE) --parallel

tests: debug
	ctest --test-dir $(BUILD_DIR_DEBUG) --output-on-failure

examples:
	cmake $(CMAKE_FLAGS) -B $(BUILD_DIR_DEBUG) -DCMAKE_BUILD_TYPE=Debug -DBUILD_EXAMPLES=ON
	cmake --build $(BUILD_DIR_DEBUG) --parallel

format:
	find bin lib examples -type f \( -name '*.cc' -o -name '*.h' \) -not -name 'utf8.h' -not -path '*/utf8/*' -exec clang-format -i {} +

clean:
	rm -rf build
	rm -rf dist
