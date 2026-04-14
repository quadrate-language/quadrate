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
