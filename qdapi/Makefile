LIB_DIR         := lib
INCLUDE_DIR     := include/quadrate
INSTALL_LIB_DIR := /usr/lib
INSTALL_INC_DIR := /usr/include

LIB_NAME        := libqd.so
ORG_HEADER_NAME := libqd.h
HEADER_NAME     := qd.h

GO_SRC := qd.go

all: $(LIB_DIR)/$(LIB_NAME) $(INCLUDE_DIR)/$(HEADER_NAME)

.PHONY: all
$(LIB_DIR)/$(LIB_NAME) $(INCLUDE_DIR)/$(HEADER_NAME): $(GO_SRC)
	go build -buildmode=c-shared -o $(LIB_NAME) $(GO_SRC)
	@mkdir -p $(LIB_DIR) $(INCLUDE_DIR)
	@mv $(LIB_NAME) $(LIB_DIR)/
	@mv $(ORG_HEADER_NAME) $(INCLUDE_DIR)/$(HEADER_NAME)

install: $(LIB_DIR)/$(LIB_NAME) $(INCLUDE_DIR)/$(HEADER_NAME)
	@echo "Installing library to $(INSTALL_LIB_DIR)/$(LIB_NAME)"
	@sudo cp $(LIB_DIR)/$(LIB_NAME) $(INSTALL_LIB_DIR)/$(LIB_NAME)
	@echo "Installing header to $(INSTALL_INC_DIR)/$(HEADER_NAME)"
	@sudo cp $(INCLUDE_DIR)/$(HEADER_NAME) $(INSTALL_INC_DIR)/$(HEADER_NAME)

uninstall:
	@echo "Removing library from $(INSTALL_LIB_DIR)/$(LIB_NAME)"
	@sudo rm -f $(INSTALL_LIB_DIR)/$(LIB_NAME)
	@echo "Removing header from $(INSTALL_INC_DIR)/$(HEADER_NAME)"
	@sudo rm -f $(INSTALL_INC_DIR)/$(HEADER_NAME)

clean:
	@rm -rf $(LIB_DIR) $(INCLUDE_DIR)
	@rm -f $(LIB_NAME) $(HEADER_NAME)

.PHONY: clean install uninstall
