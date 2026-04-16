OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
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
		/* CPU stack last — grows downward from stack_top, must not overlap runtime data */
		*(.bss.stack)
	}

	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
