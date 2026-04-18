# Build the SDL2 FFI shim into libdoomsdl.a, placed under dist/lib so quadc
# can find it at link time. Invoked by the top-level Makefile in examples/doom.

QUAD_ROOT ?= ../../..
INCLUDE    = -I$(QUAD_ROOT)/dist/include
SDL_CFLAGS = $(shell sdl2-config --cflags)

CFLAGS  += -O2 -g -Wall $(SDL_CFLAGS) $(INCLUDE)

OUT = $(QUAD_ROOT)/dist/lib/libdoomsdl.a
DEPS = $(QUAD_ROOT)/dist/lib/libdoomsdl.deps

all: $(OUT) $(DEPS)

$(OUT): sdl_shim.o
	ar rcs $@ $<

sdl_shim.o: sdl_shim.c
	$(CC) $(CFLAGS) -c $< -o $@

# `.deps` telling quadc's linker that libdoomsdl.a needs these libraries on
# the final link line. Used by lib/llvmgen/src/generator.cc:processLibraryDeps.
$(DEPS):
	echo "$(shell sdl2-config --libs)" > $@

clean:
	rm -f sdl_shim.o $(OUT) $(DEPS)

.PHONY: all clean
