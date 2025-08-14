LIB_DIR               := lib
INCLUDE_DIR           := include/quadrate
PKGCONFIG_DIR         := pkgconfig
PREFIX                := /usr
INSTALL_LIB_DIR       := $(PREFIX)/lib
INSTALL_INC_DIR       := $(PREFIX)/include/quadrate
INSTALL_PKGCONFIG_DIR := $(INSTALL_LIB_DIR)/pkgconfig

LIB_NAME              := libquadrate.so
ORG_HEADER_NAME       := libquadrate.h
HEADER_NAME           := qd.h
PKGCONFIG_NAME        := libquadrate.pc
PC_FILE               := pkgconfig/$(PKGCONFIG_NAME)

VERSION               := $(shell git describe --tags --always --dirty 2>/dev/null || echo 0.0.0)

GO_SRC := main.go qd.go

all: $(LIB_DIR)/$(LIB_NAME) $(INCLUDE_DIR)/$(HEADER_NAME) $(PC_FILE)

.PHONY: all
$(LIB_DIR)/$(LIB_NAME) $(INCLUDE_DIR)/$(HEADER_NAME): $(GO_SRC)
	go build -buildmode=c-shared -o $(LIB_NAME) $(GO_SRC)
	@mkdir -p $(LIB_DIR) $(INCLUDE_DIR)
	@mv $(LIB_NAME) $(LIB_DIR)/
	@mv $(ORG_HEADER_NAME) $(INCLUDE_DIR)/$(HEADER_NAME)

$(PC_FILE):
	@mkdir -p pkgconfig
	@echo "prefix=$(PREFIX)"                     >  $(PC_FILE)
	@echo "exec_prefix=\$${prefix}"              >> $(PC_FILE)
	@echo "libdir=$(INSTALL_LIB_DIR)"            >> $(PC_FILE)
	@echo "includedir=$(INSTALL_INC_DIR)"        >> $(PC_FILE)
	@echo ""                                     >> $(PC_FILE)
	@echo "Name: libquadrate"                    >> $(PC_FILE)
	@echo "Description: Quadrate shared library" >> $(PC_FILE)
	@echo "Version: $(VERSION)"                  >> $(PC_FILE)
	@echo "Libs: -L\$${libdir} -lquadrate"       >> $(PC_FILE)
	@echo "Cflags: -I\$${includedir}"            >> $(PC_FILE)

install: $(LIB_DIR)/$(LIB_NAME) $(INCLUDE_DIR)/$(HEADER_NAME)
	@echo "Installing library to $(INSTALL_LIB_DIR)/$(LIB_NAME)"
	@sudo install -D -m 0644 "$(LIB_DIR)/$(LIB_NAME)" "$(INSTALL_LIB_DIR)/$(LIB_NAME)"
	@echo "Installing header to $(INSTALL_INC_DIR)/$(HEADER_NAME)"
	@sudo install -D -m 0644 "$(INCLUDE_DIR)/$(HEADER_NAME)" "$(INSTALL_INC_DIR)/$(HEADER_NAME)"
	@echo "Installing pkgconfig to $(INSTALL_PKGCONFIG_DIR)/$(PKGCONFIG_NAME)"
	@sudo install -D -m 0644 "$(PC_FILE)" "$(INSTALL_PKGCONFIG_DIR)/$(PKGCONFIG_NAME)"

uninstall:
	@echo "Removing library from $(INSTALL_LIB_DIR)/$(LIB_NAME)"
	@sudo rm -f $(INSTALL_LIB_DIR)/$(LIB_NAME)
	@echo "Removing header from $(INSTALL_INC_DIR)/$(HEADER_NAME)"
	@sudo rm -f $(INSTALL_INC_DIR)/$(HEADER_NAME)

clean:
	@rm -rf $(LIB_DIR) $(INCLUDE_DIR) $(PKGCONFIG_DIR)
	@rm -f $(LIB_NAME) $(HEADER_NAME)

.PHONY: clean install uninstall
