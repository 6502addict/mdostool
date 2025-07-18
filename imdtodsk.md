# imdtodsk(1)

## NAME

**imdtodsk** - convert ImageDisk (IMD) files to DSK format

## SYNOPSIS

```
imdtodsk INPUT.IMD OUTPUT.DSK
```

## DESCRIPTION

**imdtodsk** converts ImageDisk (IMD) format disk images to DSK format. The program is optimized for MDOS (Motorola Disk Operating System) disk images that use 128-byte sectors and up to 26 sectors per track.

The utility reads the IMD file structure, including track headers, sector maps, and various sector data formats, then writes the sectors sequentially to create a DSK file. The conversion process preserves all data while handling IMD-specific features like compressed sectors and missing data.

## FILE FORMATS

### ImageDisk (IMD) Format

A flexible disk image format created by Dave Dunfield that stores:

- ASCII comment header terminated by 0x1A
- Track headers with cylinder, head, and sector information  
- Optional cylinder and head maps
- Sector data in various formats (normal, compressed, unavailable)
- Support for different sector sizes and recording modes

### DSK Format

A simple sequential disk image format where:

- Sectors are stored in linear order: track 0 sectors 0-25, track 1 sectors 0-25, etc.
- Each sector is exactly 128 bytes for MDOS compatibility
- Missing or invalid sectors are filled with zeros
- No metadata or compression is preserved

## CONVERSION PROCESS

The conversion follows these steps:

### 1. IMD Parsing

- Read and display the IMD comment header
- Parse track headers for each cylinder
- Read sector number maps (converts from 1-based to 0-based numbering)
- Process optional cylinder and head maps if present

### 2. Sector Data Handling

| Type | Description | Action |
|------|-------------|--------|
| 0 | Unavailable | Fill with zeros |
| 1 | Normal | Copy 128 bytes directly |
| 2 | Compressed | Expand single fill byte to 128 bytes |
| Other | Unknown | Treat as normal data with warning |

### 3. DSK Generation

- Write sectors in sequential track order
- Fill missing sectors with zeros to maintain geometry
- Only include tracks that contain valid data
- Maintain 26 sectors per track for MDOS compatibility

## ARGUMENTS

**imdtodsk** takes exactly two arguments and supports no options. The program operation is automatic based on the input file content.

- **`INPUT.IMD`** - Source ImageDisk format file
- **`OUTPUT.DSK`** - Target DSK format file (will be created/overwritten)

## DIAGNOSTICS

The program provides detailed progress information:

### Parsing Information

- IMD comment header content
- Track-by-track parsing progress  
- Sector count and validity statistics
- Warning messages for unusual sector types

### Conversion Results

- Number of tracks and sectors processed
- Count of valid vs. total sectors
- DSK file writing progress
- Final conversion statistics

## ERROR HANDLING

**imdtodsk** handles various error conditions gracefully:

### File Access Errors

- Missing or unreadable input files
- Permission issues with output directory
- Disk space problems during writing

### Format Errors

- Corrupted IMD headers or data
- Unexpected end-of-file conditions
- Invalid sector numbering or maps

### Data Integrity

- Out-of-range track or sector numbers
- Inconsistent sector counts
- Unknown sector data types

## LIMITATIONS

### MDOS Optimization

The converter is optimized for MDOS disk images with specific characteristics:

- **128-byte sectors only** - Other sector sizes not supported
- **Maximum 77 tracks** - Tracks 0-76 supported
- **26 sectors per track** - 1-26 in IMD, 0-25 in DSK
- **Single-sided** - Multi-head images may not convert correctly

### Data Loss

Some IMD features are not preserved in DSK format:

- Comment headers and metadata
- Recording mode information
- Sector compression (expanded to full size)
- Track timing or formatting details

## EXAMPLES

### Basic Usage

Convert a basic MDOS disk image:
```bash
imdtodsk system.imd system.dsk
```

### Batch Conversion

Convert multiple files using shell scripting:
```bash
for f in *.imd; do 
    imdtodsk "$f" "${f%.imd}.dsk"
done
```

### With Verification

Convert and verify the result:
```bash
imdtodsk original.imd converted.dsk
ls -la converted.dsk
hexdump -C converted.dsk | head
```

## SAMPLE OUTPUT

```
IMD to DSK Converter v1.2 (MDOS optimized)
Input file: system.imd
Output file: system.dsk
IMD Comment: MDOS System Disk v3.04
Converting IMD to DSK...
Track 0: 26 sectors
Track 1: 26 sectors
...
Track 15: 26 sectors
Parsed 16 tracks, 416 valid sectors out of 416 total
Writing track 0
Writing track 1
...
Writing track 15
Conversion completed successfully!
Written 416 sectors to DSK file
Conversion successful!
```

## FILES

- **`INPUT.IMD`** - Source ImageDisk format file containing the disk image to convert
- **`OUTPUT.DSK`** - Target DSK format file that will be created (overwrites existing files)

## EXIT STATUS

| Code | Description |
|------|-------------|
| 0 | Successful conversion completed |
| 1 | Error occurred during conversion (file access, format issues, or data corruption) |

## COMPATIBILITY

**imdtodsk** is designed for cross-platform compatibility:

- Standard C library functions for maximum portability
- Binary file I/O with proper byte ordering
- Works with IMD files from various disk imaging tools
- Compatible with DSK files used by MDOS emulators

## SEE ALSO

- **dsktoimd(1)** - Convert DSK files back to IMD format
- **mdosextract(1)** - Extract files from MDOS disk images
- **file(1)** - Determine file type
- **hexdump(1)** - Display file contents in hexadecimal

**External References:**
- ImageDisk utility and IMD format documentation by Dave Dunfield
- MDOS technical documentation for Motorola development systems

## AUTHOR

Written for MDOS disk image conversion and analysis.

## BUGS

Report bugs and suggestions to the maintainer.

**Known Issues:**
- The converter assumes standard MDOS disk geometry and may not work correctly with non-standard sector sizes or track layouts
- Multi-head disk images may not be handled correctly
- Very large disk images (>77 tracks) will be truncated

## VERSION HISTORY

- **1.2** - Current version with improved error handling and MDOS optimization
- **1.0** - Initial release with basic IMD to DSK conversion

---

*Last updated: January 18, 2025*