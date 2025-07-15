# MDOS Tools Suite

A comprehensive collection of tools for working with MDOS (Motorola Disk Operating System) disk image files.

## Overview

The MDOS Tools Suite provides utilities to convert, analyze, and manipulate MDOS disk image files used on Motorola EXORciser systems. The suite includes tools for directory listing, file import/export, format conversion, and complete disk extraction.

## Tools Included

### 🔧 mdostool
Main utility for MDOS disk operations and format conversion.

### 📁 mdosextract  
Specialized tool for extracting all files from MDOS IMD images with text decoding support.

## Features

### mdosextract Features ✅
- **Complete disk extraction** from IMD MDOS image files
- **Dual file output**:
  - Binary files saved as-is
  - Text files saved twice:
    - Original format (e.g., `xtrek.sa`)
    - Decoded and PC-readable (e.g., `xtrek.sa.txt`)
- **Automatic text decoding** with MDOS space compression handling
- **RIB analysis** and corruption repair
- **Packlist generation** for disk reconstruction

### mdostool Features
- **Directory operations** (list, info, free space)
- **File operations** (import, export, view, delete)
- **Disk operations** (create filesystem)
- **Format conversion** (IMD ↔ DSK)
- **Text processing** (ASCII conversion, raw binary)

---

## mdostool Usage

**MDOS Filesystem Utility v1.1**

### Syntax
```bash
mdostool <mdos-disk-image> [command] [args...]
```

### Commands

#### Directory Operations
```bash
mdostool disk.dsk ls                    # List directory contents
mdostool disk.dsk free                  # Show free space information
mdostool disk.dsk info <filename>       # Show detailed file information
```

#### File Operations
```bash
mdostool disk.dsk cat <filename>        # Display file contents (with ASCII conversion)
mdostool disk.dsk rawcat <filename>     # Display raw file contents (no conversion)
mdostool disk.dsk get <filename> [out]  # Export file from MDOS to local filesystem
mdostool disk.dsk put <local> [mdos]    # Import file from local to MDOS filesystem
mdostool disk.dsk rm <filename>         # Delete file from MDOS filesystem
```

#### Disk Operations
```bash
mdostool newdisk.dsk mkfs <sides>       # Create new MDOS filesystem
                                        # sides: 1=single, 2=double sided
mdostool disk.dsk seek <filename>       # Test seek operations on file
```

#### Image Conversion Commands
```bash
mdostool - imd2dsk <input.imd> <output.dsk>  # Convert IMD to DSK format
mdostool - dsk2imd <input.dsk> <output.imd>  # Convert DSK to IMD format
```

### Examples

```bash
# List disk contents
mdostool disk.dsk ls

# View text file
mdostool disk.dsk cat readme.txt

# Import local file
mdostool disk.dsk put myfile.txt

# Export MDOS file
mdostool disk.dsk get data.bin exported.bin

# Create new double-sided disk
mdostool newdisk.dsk mkfs 2

# Convert between formats
mdostool - imd2dsk disk.imd disk.dsk
mdostool - dsk2imd disk.dsk disk.imd
```

---

## mdosextract Usage

### Syntax
```bash
mdosextract <IMD_FILE>
```

### Features
- Extracts all files from MDOS IMD images
- Creates output directory: `<filename>_extracted/`
- Generates reconstruction packlist: `<filename>.packlist`
- Automatic text file detection and decoding

### Example
```bash
./mdosextract system.imd
```

**Output:**
```
system_extracted/
├── command.obj           # Binary file (as-is)
├── editor.sa             # Text file (original MDOS format)
├── editor.sa.txt         # Text file (decoded for PC)
├── basic.obj             # Binary file
└── system.packlist       # Reconstruction information
```

---

## Status & Known Issues

### ✅ Working Features
- **Format conversion tools** - Fully functional
- **File extraction** (`mdosextract`) - Fully functional  
- **File viewing** (`cat`, `rawcat`) - Fully functional
- **Directory listing** (`ls`) - Fully functional
- **File export** (`get`) - Fully functional

### ⚠️ Work in Progress
> **Note:** This is work in progress. Some functions are not fully tested.

### 🐛 Known Bugs
- **`put` command**: Saving files with wrong type and attributes
- **`rm` command**: Not fully tested

### 🔧 Troubleshooting
For issues or questions, contact: **didier@aida.org**

---

## Installation & Building

### Prerequisites
- GCC compiler with C99 support
- Standard C library
- POSIX-compatible system

### Build
```bash
make                # Build library and tools
make clean          # Clean build artifacts
```

### Output Files
- `libmdos.a` - MDOS filesystem library
- `mdostool` - Main utility
- `mdosextract` - Extraction tool

---

## File Format Support

### Supported Formats
- **IMD** - ImageDisk format (with compression)
- **DSK** - Raw disk image format
- **MDOS** - Native MDOS filesystem (128-byte sectors)

### MDOS File Types
- **Binary files** - Executables, object files
- **Text files** - Assembly source, documentation
- **System files** - Boot sectors, directory

---

## Technical Details

### MDOS Filesystem
- **Sector size**: 128 bytes
- **Cluster size**: 4 sectors (512 bytes)
- **Directory**: Sectors 3-22 (20 sectors)
- **Filename format**: 8.2 (8-character name, 2-character extension)

### Text File Decoding
MDOS text files use space compression:
- High bit set (0x80-0xFF) indicates compressed spaces
- Carriage returns (0x0D) converted to line feeds (0x0A)
- Control characters filtered for PC compatibility

---

## License

This project is provided as-is for working with MDOS disk images on Motorola EXORciser systems.

---

*For more detailed documentation, see `mdos.md`*
