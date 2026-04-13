/* linker.ld — i686 multiboot kernel link layout.
 *
 * GRUB / qemu -kernel loads the kernel at 1 MiB (0x100000), which is the
 * conventional load address for multiboot1 kernels (below 1 MiB is full
 * of legacy BIOS data). We place the multiboot header first so the
 * loader can find it within the 8 KiB scan window.
 */

ENTRY(_kernel_entry)

SECTIONS {
	. = 1M;

	.boot ALIGN(4K) : {
		KEEP(*(.multiboot))
	}

	.text ALIGN(4K) : {
		*(.text)
		*(.text.*)
	}

	.rodata ALIGN(4K) : {
		*(.rodata)
		*(.rodata.*)
	}

	.data ALIGN(4K) : {
		*(.data)
		*(.data.*)
	}

	.bss ALIGN(4K) : {
		*(COMMON)
		*(.bss)
		*(.bss.*)
	}

	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
