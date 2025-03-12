BINARY_NAME := quadc
INSTALL_BIN := /usr/bin
INSTALL_LIB := /usr/lib/quadrate
STD_LIB := stdlib

.PHONY: build
build:
	go build -o $(BINARY_NAME)

.PHONY: install
install: build
	@echo "Installing binary to $(INSTALL_BIN)..."
	install -Dm755 $(BINARY_NAME) $(INSTALL_BIN)/$(BINARY_NAME)
	@echo "Installing folders to $(INSTALL_LIB)..."
	mkdir -p $(INSTALL_LIB)
	for folder in $(STD_LIB); do \
		echo "Copying $$folder to $(INSTALL_LIB)..."; \
		cp -r $$folder/* $(INSTALL_LIB)/; \
	done
	@echo "Installation complete."

# Uninstall the binary and folders
.PHONY: uninstall
uninstall:
	@echo "Removing binary from $(INSTALL_BIN)..."
	rm -f $(INSTALL_BIN)/$(BINARY_NAME)
	@echo "Removing folders from $(INSTALL_LIB)..."
	rm -rf $(INSTALL_LIB)
	@echo "Uninstallation complete."

# Clean up build artifacts
.PHONY: clean
clean:
	@echo "Cleaning up build artifacts..."
	rm -f $(BINARY_NAME)
