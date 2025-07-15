# MDOS Filesystem Library and Tools Manual

## Table of Contents

1. [Overview](#overview)
2. [Library Architecture](#library-architecture)
3. [Building and Installation](#building-and-installation)
4. [Library API Reference](#library-api-reference)
5. [mdostool Command Reference](#mdostool-command-reference)
6. [mdosextract Tool](#mdosextract-tool)
7. [File Format Support](#file-format-support)
8. [Examples](#examples)
9. [Error Handling](#error-handling)
10. [Troubleshooting](#troubleshooting)

---

## Overview

The MDOS Filesystem Library provides comprehensive support for working with MDOS (Motorola Disk Operating System) disk images used on Motorola EXORciser systems. The library includes:

- **Core filesystem operations** (mount, read, write, directory listing)
- **Image format conversion** (IMD ↔ DSK)
- **High-level utility functions** (import/export, file manipulation)
- **Command-line tools** for disk management
- **File extraction and analysis tools**

### Supported Formats

- **DSK**: Raw disk image format (sequential sectors)
- **IMD**: ImageDisk format with compression support
- **MDOS**: Native MDOS filesystem with 128-byte sectors

### Key Features

- ✅ **Modular architecture** - 6 focused modules
- ✅ **POSIX-like API** - Familiar file operations
- ✅ **Format conversion** - Seamless IMD/DSK conversion
- ✅ **Error handling** - Comprehensive error codes
- ✅ **Memory safe** - No stack overflows or leaks
- ✅ **Production ready** - Clean, warning-free builds

---

## Library Architecture

The MDOS library is organized into 6 modules:

```
libmdos.a
├── mdos_diskio.c    - Low-level disk I/O operations
├── mdos_utils.c     - Utilities and filename handling
├── mdos_file.c      - File operations (open, read, write, seek)
├── mdos_dir.c       - Directory operations (ls, stat, unlink)
├── mdos_tools.c     - High-level utility functions
└── mdos_cvt.c       - Image format conversion (IMD/DSK)
```

### Headers

- **`mdos_fs.h`** - Public API (include this in your programs)
- **`mdos_internal.h`** - Internal functions (library use only)

---

## Building and Installation

### Prerequisites

- GCC compiler with C99 support
- Standard C library
- POSIX-compatible system (Linux, macOS, Unix)

### Build Commands

```bash
# Build library and tools
make

# Build only the library
make libmdos.a

# Build only mdostool
make mdostool

# Clean build artifacts
make clean

# Install to /usr/local (optional)
sudo make install

# Uninstall
sudo make uninstall
```

### Build Output

- **`libmdos.a`** - Static library
- **`mdostool`** - Command-line utility
- **Object files** - `*.o` for each module

---

## Library API Reference

### Core Filesystem Functions

#### Mount/Unmount Operations

```c
mdos_fs_t* mdos_mount(const char *disk_path, int read_only);
int mdos_unmount(mdos_fs_t *fs);
int mdos_sync(mdos_fs_t *fs);
```

**Example:**
```c
#include "mdos_fs.h"

// Mount for read-only access
mdos_fs_t *fs = mdos_mount("disk.dsk", 1);
if (!fs) {
    fprintf(stderr, "Failed to mount disk\n");
    return -1;
}

// Do operations...

// Unmount
mdos_unmount(fs);
```

#### File Operations

```c
int mdos_open(mdos_fs_t *fs, const char *filename, int flags, int type);
int mdos_close(mdos_fs_t *fs, int fd);
ssize_t mdos_read(mdos_fs_t *fs, int fd, void *buf, size_t count);
ssize_t mdos_read_raw(mdos_fs_t *fs, int fd, void *buf, size_t count);
ssize_t mdos_write(mdos_fs_t *fs, int fd, const void *buf, size_t count);
off_t mdos_lseek(mdos_fs_t *fs, int fd, off_t offset, int whence);
```

**File Flags:**
- `MDOS_O_RDONLY` - Read-only access
- `MDOS_O_WRONLY` - Write-only access  
- `MDOS_O_RDWR` - Read-write access
- `MDOS_O_CREAT` - Create file if it doesn't exist

**Seek Whence Values:**
- `MDOS_SEEK_SET` - Seek from beginning
- `MDOS_SEEK_CUR` - Seek from current position
- `MDOS_SEEK_END` - Seek from end

#### Directory Operations

```c
int mdos_readdir(mdos_fs_t *fs, mdos_file_info_t **files, int *count);
int mdos_stat(mdos_fs_t *fs, const char *filename, mdos_file_info_t *info);
int mdos_unlink(mdos_fs_t *fs, const char *filename);
```

**File Information Structure:**
```c
typedef struct {
    char name[MDOS_MAX_FILENAME];  // Filename
    int type;                      // File type (0-7)
    int size;                      // Size in bytes
    int sectors;                   // Size in sectors
    uint16_t load_addr;           // Load address
    uint16_t start_addr;          // Start address
    uint8_t attributes;           // File attributes
    int rib_sector;               // RIB sector location
} mdos_file_info_t;
```

#### File Creation

```c
int mdos_create_file(mdos_fs_t *fs, const char *filename, int file_type, 
                     const void *data, size_t size);
```

**File Types:**
- `MDOS_TYPE_USER_DEFINED` (0) - User defined
- `MDOS_TYPE_IMAGE` (2) - Binary/executable
- `MDOS_TYPE_OBJECT` (3) - Object file
- `MDOS_TYPE_ASCII` (5) - ASCII text file

#### Filesystem Creation

```c
int mdos_mkfs(const char *disk_path, int sides);
```

**Parameters:**
- `sides`: 1 = single-sided, 2 = double-sided

### High-Level Tool Functions

```c
int mdos_list_files(mdos_fs_t *fs, FILE *output);
int mdos_cat_file(mdos_fs_t *fs, const char *filename, FILE *output, int raw_mode);
int mdos_export_file(mdos_fs_t *fs, const char *mdos_name, const char *local_name);
int mdos_import_file(mdos_fs_t *fs, const char *local_name, const char *mdos_name_arg);
int mdos_test_seek(mdos_fs_t *fs, const char *filename, FILE *output);
int mdos_file_info(mdos_fs_t *fs, const char *filename, FILE *output);
```

### Image Conversion Functions

```c
int mdos_convert_imd_to_dsk(const char *imd_filename, const char *dsk_filename);
int mdos_convert_dsk_to_imd(const char *dsk_filename, const char *imd_filename);
```

### Utility Functions

```c
int mdos_free_space(mdos_fs_t *fs);
const char* mdos_strerror(int error);
int mdos_validate_filename(const char *filename);
int mdos_extract_filename(const char *local_path, char *mdos_name);
```

### Error Codes

```c
#define MDOS_EOK        0   // Success
#define MDOS_ENOENT    -1   // File not found
#define MDOS_ENOSPC    -2   // No space left
#define MDOS_EMFILE    -3   // Too many open files
#define MDOS_EBADF     -4   // Bad file descriptor
#define MDOS_EINVAL    -5   // Invalid argument
#define MDOS_EIO       -6   // I/O error
#define MDOS_EEXIST    -7   // File exists
#define MDOS_EPERM     -8   // Operation not permitted
```

---

## mdostool Command Reference

`mdostool` is a comprehensive command-line utility for MDOS disk management.

### Synopsis

```bash
mdostool <disk-image> [command] [args...]
mdostool - <conversion-command> [args...]
```

### Filesystem Commands

#### List Directory Contents
```bash
mdostool disk.dsk ls
```
Shows all files with size, type, and attributes.

#### Display File Contents
```bash
# With ASCII conversion
mdostool disk.dsk cat filename.txt

# Raw binary output
mdostool disk.dsk rawcat filename.bin
```

#### File Import/Export
```bash
# Import local file to MDOS disk
mdostool disk.dsk put localfile.txt [mdosname.txt]

# Export MDOS file to local filesystem
mdostool disk.dsk get mdosfile.txt [localname.txt]
```

#### File Information
```bash
# Detailed file information
mdostool disk.dsk info filename.obj

# Free space information
mdostool disk.dsk free
```

#### File Management
```bash
# Delete file
mdostool disk.dsk rm filename.txt

# Test seek operations (debugging)
mdostool disk.dsk seek filename.bin
```

#### Filesystem Creation
```bash
# Create new MDOS filesystem
mdostool newdisk.dsk mkfs 2    # 2 = double-sided
mdostool newdisk.dsk mkfs 1    # 1 = single-sided
```

### Image Conversion Commands

#### IMD to DSK Conversion
```bash
mdostool - imd2dsk input.imd output.dsk
```

#### DSK to IMD Conversion
```bash
mdostool - dsk2imd input.dsk output.imd
```

### Usage Examples

```bash
# Create a new disk
mdostool mydisk.dsk mkfs 2

# Mount and list contents
mdostool mydisk.dsk ls

# Add files
mdostool mydisk.dsk put readme.txt
mdostool mydisk.dsk put program.obj

# View file contents
mdostool mydisk.dsk cat readme.txt

# Get file information
mdostool mydisk.dsk info program.obj

# Extract files
mdostool mydisk.dsk get program.obj exported_program.obj

# Convert formats
mdostool - imd2dsk archive.imd archive.dsk
```

---

## mdosextract Tool

`mdosextract` is a specialized tool for extracting all files from MDOS IMD images and creating reconstruction packlists.

### Synopsis

```bash
mdosextract <IMD_FILE>
```

### Features

- **Complete extraction** - Extracts all files from MDOS IMD images
- **Text file decoding** - Automatically decodes MDOS text files with space compression
- **Packlist generation** - Creates detailed reconstruction information
- **RIB analysis** - Analyzes and corrects corrupt RIB information
- **Path handling** - Creates output directory relative to IMD file location

### Output Structure

```
input.imd
input_extracted/
├── file1.obj           # Binary files (raw)
├── file2.sa            # Text files (raw MDOS format)
├── file2.sa.txt        # Text files (decoded)
├── file3.bin
└── input.packlist      # Reconstruction information
```

### Packlist Format

The `.packlist` file contains all information needed to reconstruct the MDOS disk:

```
# MDOS Packlist generated by mdosextract.c
# Source IMD: input.imd
# Generated: 2025-01-15 14:30:25

filename load_addr start_addr attr file_size last_bytes rib_sector
input_extracted/program.obj load=2000 start=2000 attr=0002 size=0008 last=80 rib=0019
input_extracted/data.bin load=3000 start=0000 attr=0002 size=0004 last=40 rib=001A
```

### Text File Decoding

MDOS text files use space compression where high-bit-set bytes (0x80-0xFF) represent compressed spaces:

- **0x81** = 1 space
- **0x85** = 5 spaces  
- **0xFF** = 127 spaces

The extractor automatically:
- Detects text files by attributes (format 5) or extension (.sa, .al, .sb, .sc)
- Expands compressed spaces
- Converts CR (0x0D) to LF (0x0A) 
- Filters control characters
- Creates `.txt` decoded versions

### RIB Analysis and Correction

The extractor analyzes MDOS RIB (Record Information Block) structures:

- **SDW chain analysis** - Follows sector descriptor words
- **Size validation** - Detects and corrects corrupt size fields
- **End marker detection** - Finds actual file boundaries
- **Post-extraction verification** - Validates extracted file sizes

### Usage Examples

```bash
# Extract MDOS disk
./mdosextract system.imd

# Creates:
# system_extracted/
# ├── command.obj
# ├── editor.sa
# ├── editor.sa.txt      # Decoded text version
# ├── basic.obj
# └── system.packlist

# From different directory
cd /path/to/disks
./mdosextract ../archives/games.imd
# Creates: ../archives/games_extracted/

# Analyze packlist
cat system_extracted/system.packlist
```

### Error Handling

The extractor handles various error conditions:

- **Missing sectors** - Fills with zeros, reports warnings
- **Corrupt RIB data** - Analyzes SDW chain, corrects size fields
- **Invalid file entries** - Skips deleted/corrupted directory entries
- **Text decoding errors** - Gracefully handles malformed text files

---

## File Format Support

### MDOS Filesystem Structure

MDOS uses a cluster-based filesystem with 128-byte sectors:

- **Sector 0**: Disk ID and metadata
- **Sector 1**: Cluster Allocation Table (CAT)
- **Sector 2**: Logical CAT (bad block bitmap)  
- **Sectors 3-22**: Directory (20 sectors)
- **Sectors 23+**: Data clusters (4 sectors each)

### File Types and Attributes

**File Type Field (bits 0-2 of attributes):**
- 0: User defined
- 1: Unknown format 1
- 2: Image/executable file
- 3: Object file  
- 4: Unknown format 4
- 5: ASCII text file
- 6: Unknown format 6
- 7: ASCII converted file

**Attribute Flags:**
- `0x80`: Write protected
- `0x40`: Delete protected
- `0x20`: System file
- `0x10`: Continuous allocation
- `0x08`: Compressed

### Filename Conventions

MDOS uses 8.2 filename format:
- **Name**: 1-8 characters (letters/numbers)
- **Extension**: 0-2 characters (optional)
- **Case**: Stored uppercase, displayed lowercase

Common extensions:
- `.SA` - Assembly source
- `.AL` - Assembly listing  
- `.OB` - Object file
- `.CM` - Command file
- `.TT` - Text file

---

## Examples

### Basic Library Usage

```c
#include <stdio.h>
#include "mdos_fs.h"

int main() {
    // Mount disk
    mdos_fs_t *fs = mdos_mount("system.dsk", 0); // Read-write
    if (!fs) {
        printf("Failed to mount disk\n");
        return 1;
    }
    
    // List files
    mdos_list_files(fs, stdout);
    
    // Read a file
    int fd = mdos_open(fs, "readme.txt", MDOS_O_RDONLY, 0);
    if (fd >= 0) {
        char buffer[1024];
        ssize_t bytes = mdos_read(fs, fd, buffer, sizeof(buffer));
        if (bytes > 0) {
            fwrite(buffer, bytes, 1, stdout);
        }
        mdos_close(fs, fd);
    }
    
    // Import a file
    mdos_import_file(fs, "local.txt", "mdos.txt");
    
    // Unmount
    mdos_unmount(fs);
    return 0;
}
```

### Compile and Link

```bash
gcc -o myprogram myprogram.c -L. -lmdos
```

### Complete Workflow Example

```bash
# 1. Convert IMD to DSK
mdostool - imd2dsk archive.imd working.dsk

# 2. Mount and explore
mdostool working.dsk ls
mdostool working.dsk info game.obj

# 3. Extract file
mdostool working.dsk get game.obj game_backup.obj

# 4. Add new file  
mdostool working.dsk put newfile.txt

# 5. Convert back to IMD
mdostool - dsk2imd working.dsk archive_updated.imd

# 6. Extract everything for analysis
mdosextract archive_updated.imd
```

---

## Error Handling

### Library Error Handling

All library functions return error codes that can be checked:

```c
int result = mdos_open(fs, "file.txt", MDOS_O_RDONLY, 0);
if (result < 0) {
    printf("Error: %s\n", mdos_strerror(result));
    // Handle error...
}
```

### Common Error Scenarios

- **MDOS_ENOENT**: File not found
- **MDOS_ENOSPC**: Disk full, cannot create file
- **MDOS_EMFILE**: Too many files open (max 16)
- **MDOS_EBADF**: Invalid file descriptor
- **MDOS_EINVAL**: Invalid filename or parameters
- **MDOS_EIO**: Disk I/O error
- **MDOS_EPERM**: Read-only filesystem, cannot write

### Tool Error Handling

Command-line tools provide descriptive error messages:

```bash
$ mdostool badfile.dsk ls
Failed to mount MDOS disk: badfile.dsk
Make sure the file exists and is a valid MDOS disk image.

$ mdostool disk.dsk get nonexistent.txt
Error in get: File not found
```

---

## Troubleshooting

### Common Issues

**"Failed to mount MDOS disk"**
- Check file exists and has correct permissions
- Verify file is a valid MDOS disk image
- Try converting from IMD format first

**"No space left on device"**
- Check free space: `mdostool disk.dsk free`
- MDOS has limited directory entries
- Consider using larger disk image

**"Invalid argument" errors**
- Check filename format (8.2 convention)
- Ensure filenames contain only letters/numbers
- Verify file exists before operations

**Text files display garbled**
- Use `mdosextract` for proper text decoding
- Try `rawcat` instead of `cat` for binary inspection
- Check file type attributes

### Debug Mode

Enable verbose output by examining intermediate files:

```bash
# Check disk structure
mdostool disk.dsk info system.obj

# Test seek operations  
mdostool disk.dsk seek file.bin

# Extract everything for analysis
mdosextract disk.imd
```

### Performance Considerations

- **Mount overhead**: Mounting is fast, no caching needed
- **Large files**: Reading is efficient, seeks are supported
- **Many files**: Directory operations scale well
- **Conversions**: IMD/DSK conversion preserves all data

---

## Version Information

- **Library Version**: 1.1
- **mdostool Version**: 1.1  
- **mdosextract Version**: 1.2

### Compatibility

- **Compilers**: GCC 4.9+, Clang 6.0+
- **Systems**: Linux, macOS, BSD, other POSIX systems
- **Standards**: C99 compliance required

---

## License and Credits

This MDOS Filesystem Library was developed for working with Motorola EXORciser MDOS disk images. The library implements the MDOS filesystem specification with modern, safe C code.

For support or contributions, refer to the project documentation.

---

*Last updated: January 2025*
