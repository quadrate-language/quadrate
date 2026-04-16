TARGET   := x86_64-unknown-none-elf
CFLAGS   := --target=$(TARGET) -ffreestanding -fno-builtin -nostdlib \
            -fno-stack-protector -mno-red-zone -O2 -Wall -Wextra
ASFLAGS  := --target=$(TARGET) -ffreestanding -nostdlib

REPO_ROOT       := ../..
QUADC           := $(REPO_ROOT)/build/release/cmd/quadc/quadc
FREESTANDING_RT := $(REPO_ROOT)/lib/rt/src/freestanding.c
MEM_SRC         := $(REPO_ROOT)/lib/mem/src/mem.c
RT_INC          := $(REPO_ROOT)/lib/rt/include
MEM_INC         := $(REPO_ROOT)/lib/mem/include

OBJS := boot.o interrupts.o kernel.o freestanding.o mem.o

.PHONY: all iso run run-iso clean

all: kernel.elf

kernel.o: kernel.qd
	QUADRATE_ROOT=$(REPO_ROOT)/dist/share/quadrate \
	  $(QUADC) --freestanding --target $(TARGET) -O2 kernel.qd -o kernel.o

boot.o: boot.S
	clang $(ASFLAGS) -c $< -o $@

interrupts.o: interrupts.S
	clang $(ASFLAGS) -c $< -o $@

freestanding.o: $(FREESTANDING_RT)
	clang $(CFLAGS) -I$(RT_INC) -c $< -o $@

mem.o: $(MEM_SRC)
	clang $(CFLAGS) -I$(RT_INC) -I$(MEM_INC) -c $< -o $@

kernel64.elf: $(OBJS) linker.ld
	ld -nostdlib -T linker.ld -o $@ $(OBJS)

# QEMU's multiboot1 loader requires ELF32. Convert the 64-bit ELF to 32-bit.
# The binary content is identical — only the ELF container changes.
kernel.elf: kernel64.elf
	objcopy -O elf32-i386 $< $@
	@echo "Built $@ ($$(wc -c < $@) bytes)"

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

run: kernel.elf
	qemu-system-x86_64 -kernel kernel.elf -display curses

run-iso: kernel.iso
	qemu-system-x86_64 -cdrom kernel.iso -display curses

clean:
	rm -f *.o kernel.elf kernel64.elf kernel.iso
	rm -rf isodir
