# Makefile for MDOS Filesystem Library
# Builds the modular MDOS library and tools

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
AR = ar
ARFLAGS = rcs

# Source files
SOURCES = mdos_diskio.c mdos_utils.c mdos_file.c mdos_dir.c mdos_tools.c mdos_cvt.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = mdos_fs.h mdos_internal.h

# Library and tools
LIBRARY = libmdos.a
TOOLS = mdostool

# Default target
all: $(LIBRARY) $(TOOLS)

# Build the static library
$(LIBRARY): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^
	@echo "Library $(LIBRARY) created successfully"

# Build mdostool
mdostool: mdostool.c $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ mdostool.c -L. -lmdos
	@echo "Tool mdostool built successfully"

# Compile object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(LIBRARY) $(TOOLS)
	@echo "Clean completed"

# Install library and tools (optional)
install: $(LIBRARY) $(TOOLS)
	mkdir -p /usr/local/lib /usr/local/include /usr/local/bin
	cp $(LIBRARY) /usr/local/lib/
	cp mdos_fs.h /usr/local/include/
	cp mdostool /usr/local/bin/
	@echo "Library installed to /usr/local/lib"
	@echo "Header installed to /usr/local/include"
	@echo "Tool installed to /usr/local/bin"

# Uninstall library and tools (optional)
uninstall:
	rm -f /usr/local/lib/$(LIBRARY)
	rm -f /usr/local/include/mdos_fs.h
	rm -f /usr/local/bin/mdostool
	@echo "Library and tools uninstalled"

# Build example programs (for compatibility)
examples: $(LIBRARY) mdostool
	@echo "mdostool is the main example program"

# Show usage
help:
	@echo "MDOS Filesystem Library Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build library and tools (default)"
	@echo "  $(LIBRARY)    - Build only the library"
	@echo "  mdostool   - Build only the mdostool utility"
	@echo "  clean      - Remove build artifacts"
	@echo "  install    - Install library and tools to /usr/local"
	@echo "  uninstall  - Remove library and tools from /usr/local"
	@echo "  examples   - Build example programs (same as mdostool)"
	@echo "  help       - Show this help"
	@echo ""
	@echo "Usage examples:"
	@echo "  make                    # Build everything"
	@echo "  make mdostool          # Build just the tool"
	@echo "  ./mdostool disk.dsk ls # List files on MDOS disk"
	@echo ""
	@echo "Library usage in your programs:"
	@echo "  gcc -o myprogram myprogram.c -L. -lmdos"

.PHONY: all clean install uninstall examples help
