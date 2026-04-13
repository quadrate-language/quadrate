# Makefile — bare-metal i686 multiboot kernel example.
#
# Usage:
#   make            Build kernel.elf (multiboot-compliant ELF).
#   make iso        Build kernel.iso (GRUB-bootable; needs grub-mkrescue + xorriso).
#   make run        Boot kernel.elf in QEMU (needs qemu-system-i386).
#   make run-iso    Boot kernel.iso in QEMU (full GRUB boot path).
#   make clean      Remove build artifacts.
#
# Toolchain: clang (with built-in i686 cross-compilation), ld, optionally
# grub-mkrescue + xorriso for ISO builds, and qemu-system-i386 to run.

# Cross-target. clang ships with a built-in i686-elf cross compiler; no
# separate toolchain install needed.
TARGET   := i686-unknown-none-elf
CFLAGS   := --target=$(TARGET) -ffreestanding -fno-builtin -nostdlib \
            -fno-stack-protector -mno-red-zone -O2 -Wall -Wextra
ASFLAGS  := --target=$(TARGET) -ffreestanding -nostdlib

# Path to quadc and the freestanding runtime source. Adjust if your
# build directory differs.
REPO_ROOT       := ../..
QUADC           := $(REPO_ROOT)/build/release/cmd/quadc/quadc
FREESTANDING_RT := $(REPO_ROOT)/lib/rt/src/freestanding.c
MEM_SRC         := $(REPO_ROOT)/lib/mem/src/mem.c
RT_INC          := $(REPO_ROOT)/lib/rt/include
MEM_INC         := $(REPO_ROOT)/lib/mem/include

OBJS := boot.o kernel.o freestanding.o mem.o

.PHONY: all iso run run-iso clean

all: kernel.elf

# Quadrate kernel module → object file (no main, _start exported).
# QUADRATE_ROOT lets quadc find the mem module's .qd source for type
# checking. Linking is still done by us against the freestanding-safe
# libmem (mem.o below) — the stdlib's static .a (libmem.a) is libc-
# linked and not used here.
kernel.o: kernel.qd
	QUADRATE_ROOT=$(REPO_ROOT)/dist/share/quadrate \
	  $(QUADC) --freestanding --target $(TARGET) -O2 kernel.qd -o kernel.o

# Multiboot header + entry stub.
boot.o: boot.S
	clang $(ASFLAGS) -c $< -o $@

# Freestanding runtime (static stack, halt hook, push/pop).
freestanding.o: $(FREESTANDING_RT)
	clang $(CFLAGS) -I$(RT_INC) -c $< -o $@

# Freestanding-safe subset of the mem module — no malloc/realloc/free/
# to_string/from_string (those live in mem_heap.c which we don't link
# here). The single-source file compiles identically for hosted and
# freestanding builds.
mem.o: $(MEM_SRC)
	clang $(CFLAGS) -I$(RT_INC) -I$(MEM_INC) -c $< -o $@

# Final link. -nostdlib is critical — there's no libc in this binary.
kernel.elf: $(OBJS) linker.ld
	ld -m elf_i386 -nostdlib -T linker.ld -o $@ $(OBJS)
	@echo "Built $@ ($$(wc -c < $@) bytes)"

# GRUB ISO. Needs grub-mkrescue (provides the ISO9660 layout that boots).
isodir/boot/grub:
	mkdir -p $@

isodir/boot/kernel.elf: kernel.elf | isodir/boot/grub
	cp $< $@

isodir/boot/grub/grub.cfg: grub.cfg | isodir/boot/grub
	cp $< $@

iso: kernel.iso
kernel.iso: isodir/boot/kernel.elf isodir/boot/grub/grub.cfg
	grub-mkrescue -o $@ isodir 2>/dev/null
	@echo "Built $@ ($$(wc -c < $@) bytes)"

# Run via QEMU's built-in multiboot loader (faster turnaround than ISO).
run: kernel.elf
	qemu-system-i386 -kernel kernel.elf -display curses

run-iso: kernel.iso
	qemu-system-i386 -cdrom kernel.iso -display curses

clean:
	rm -f *.o kernel.elf kernel.iso
	rm -rf isodir
