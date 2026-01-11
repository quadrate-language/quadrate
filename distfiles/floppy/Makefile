# Floppy disk distribution Makefile
# Creates a 1.44 MB floppy ISO image containing Quadrate source code

REPO_ROOT := ../..
VERSION := $(shell git -C $(REPO_ROOT) describe --tags --always 2>/dev/null || echo "2.0.0")
ISO_NAME := quadrate-$(VERSION)-src.iso
STAGING := staging
MAX_SIZE := 1474560  # 1.44 MB in bytes

# Find available ISO creation tool
MKISOFS := $(shell which mkisofs genisoimage xorrisofs 2>/dev/null | head -1)

.PHONY: all clean check-size check-tools

all: check-tools $(ISO_NAME)
	@echo ""
	@echo "Floppy ISO created: $(ISO_NAME)"
	@echo "Size: $$(ls -lh $(ISO_NAME) | awk '{print $$5}')"
	@echo ""
	@echo "To write to floppy:"
	@echo "  dd if=$(ISO_NAME) of=/dev/fd0 bs=1440k"
	@echo ""
	@echo "Or to USB (find device with lsblk first!):"
	@echo "  dd if=$(ISO_NAME) of=/dev/sdX bs=1440k"

check-tools:
ifeq ($(MKISOFS),)
	$(error No ISO tool found. Install one of: cdrtools, genisoimage, or xorriso)
endif

$(ISO_NAME): $(STAGING)
	@echo "Creating ISO image with $(MKISOFS)..."
	$(MKISOFS) -o $@ \
		-V "QUADRATE" \
		-publisher "Quadrate Language Project" \
		-R -J \
		$(STAGING)
	@$(MAKE) check-size --no-print-directory

$(STAGING): clean
	@echo "Preparing floppy contents..."
	@mkdir -p $(STAGING)

	# Export source code
	@echo "  Exporting source..."
	@git -C $(REPO_ROOT) archive --format=tar.gz --prefix=quadrate/ HEAD > $(STAGING)/SOURCE.TGZ

	# Add README
	@echo "  Creating README..."
	@echo "QUADRATE $(VERSION)" > $(STAGING)/README.TXT
	@echo "================" >> $(STAGING)/README.TXT
	@echo "" >> $(STAGING)/README.TXT
	@echo "A stack-based programming language." >> $(STAGING)/README.TXT
	@echo "" >> $(STAGING)/README.TXT
	@echo "TO BUILD:" >> $(STAGING)/README.TXT
	@echo "  tar xzf SOURCE.TGZ" >> $(STAGING)/README.TXT
	@echo "  cd quadrate" >> $(STAGING)/README.TXT
	@echo "  make release" >> $(STAGING)/README.TXT
	@echo "  make install" >> $(STAGING)/README.TXT
	@echo "" >> $(STAGING)/README.TXT
	@echo "REQUIRES:" >> $(STAGING)/README.TXT
	@echo "  - C++20 compiler (GCC 10+ or Clang 10+)" >> $(STAGING)/README.TXT
	@echo "  - LLVM 14+" >> $(STAGING)/README.TXT
	@echo "  - Meson build system" >> $(STAGING)/README.TXT
	@echo "" >> $(STAGING)/README.TXT
	@echo "https://quadrate-lang.org" >> $(STAGING)/README.TXT

	# Add extras from this directory
	@if [ -f FILE_ID.DIZ ]; then cp FILE_ID.DIZ $(STAGING)/; fi
	@if [ -f QUADRATE.ANS ]; then cp QUADRATE.ANS $(STAGING)/; fi
	@if [ -f QUAD.NFO ]; then cp QUAD.NFO $(STAGING)/; fi
	@if [ -f HELLO.QD ]; then cp HELLO.QD $(STAGING)/; fi

	@echo "  Contents:"
	@ls -la $(STAGING)/

check-size:
	@SIZE=$$(stat -c%s $(ISO_NAME) 2>/dev/null || stat -f%z $(ISO_NAME)); \
	if [ $$SIZE -gt $(MAX_SIZE) ]; then \
		echo "ERROR: ISO too large for floppy! ($$SIZE > $(MAX_SIZE) bytes)"; \
		exit 1; \
	else \
		echo "Size OK: $$SIZE bytes (max $(MAX_SIZE))"; \
	fi

clean:
	rm -rf $(STAGING)
	rm -f $(ISO_NAME)
