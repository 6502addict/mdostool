# dsktoimd(1)

## NAME

**dsktoimd** - convert DSK files to ImageDisk (IMD) format

## SYNOPSIS

```
dsktoimd INPUT.DSK OUTPUT.IMD
```

## DESCRIPTION

**dsktoimd** converts DSK format disk images to ImageDisk (IMD) format. The program is optimized for MDOS (Motorola Disk Operating System) disk images that use 128-byte sectors and up to 26 sectors per track.

The utility reads sectors sequentially from the DSK file, analyzes the data for compression opportunities, and creates an IMD file with proper headers, sector maps, and optimized storage. The conversion process includes automatic compression of repeated data patterns and intelligent track detection.

## FILE FORMATS

### DSK Format

A simple sequential disk image format where:

- Sectors are stored in linear order: track 0 sectors 0-25, track 1 sectors 0-25, etc.
- Each sector is exactly 128 bytes for MDOS compatibility
- No metadata, headers, or compression
- Fixed geometry assumed (26 sectors per track)

### ImageDisk (IMD) Format

A flexible disk image format created by Dave Dunfield that stores:

- ASCII comment header with creation information
- Track headers with cylinder, head, and sector information
- Sector number maps for flexible addressing
- Compressed sector data for efficiency
- Support for different sector sizes and recording modes

## CONVERSION PROCESS

The conversion follows these steps:

### 1. Data Analysis

- Scan entire DSK file to find last track with data
- Identify empty sectors (all zeros) vs. data sectors
- Analyze sectors for compression opportunities
- Determine optimal track layout

### 2. IMD Header Generation

- Create descriptive comment header with timestamp
- Include source DSK filename and conversion details
- Add MDOS-specific format information
- Terminate header with 0x1A marker

### 3. Track Processing

- Generate track headers for each cylinder with data
- Create sector number maps (1-based for MDOS)
- Apply compression to uniform data sectors
- Write normal data for complex sectors

## COMPRESSION ALGORITHM

**dsktoimd** implements intelligent compression to reduce file size:

### Sector Analysis

Each 128-byte sector is examined to determine the best storage method:

| Sector Type | Condition | Compression |
|-------------|-----------|-------------|
| **Empty** | All bytes are zero | Not compressed, but noted |
| **Uniform** | All bytes have the same value | Compressed to 2 bytes |
| **Normal** | Mixed data | Stored uncompressed (129 bytes) |

### Compression Types

- **Type 1 (Normal)**: 128 bytes of data stored directly
- **Type 2 (Compressed)**: Single fill byte represents 128 identical bytes

### Space Savings

- Uniform sectors: 128 bytes → 2 bytes (98.4% reduction)
- Typical system disks: 20-50% overall size reduction
- Boot tracks with padding: Up to 80% reduction

## ARGUMENTS

**dsktoimd** takes exactly two arguments and supports no options. The program operation is automatic based on the input file content.

- **`INPUT.DSK`** - Source DSK format file
- **`OUTPUT.IMD`** - Target ImageDisk format file (will be created/overwritten)

## DIAGNOSTICS

The program provides detailed progress information:

### Analysis Phase

- Detection of last track containing data
- Track-by-track processing status
- Sector compression analysis results
- Memory usage and file size estimates

### Conversion Results

- Number of tracks and sectors processed
- Compression statistics and space savings
- IMD file generation progress
- Final conversion summary

## ERROR HANDLING

**dsktoimd** handles various error conditions gracefully:

### File Access Errors

- Missing or unreadable input DSK files
- Permission issues with output directory
- Disk space problems during writing

### Data Validation

- Empty or truncated DSK files
- Inconsistent file sizes
- Sector boundary alignment issues

### Format Issues

- Oversized input files (too many tracks)
- Invalid sector data patterns
- IMD writing errors

## LIMITATIONS

### MDOS Assumptions

The converter assumes standard MDOS disk geometry:

- **128-byte sectors only** - Other sizes not supported
- **26 sectors per track** - Fixed geometry
- **Maximum 77 tracks** - Tracks 0-76 supported
- **Single-sided** - No multi-head support

### Format Constraints

- No preservation of original IMD metadata
- Fixed recording mode (FM) in output
- No support for variable sector sizes
- Limited to sequential track layout

## EXAMPLES

### Basic Usage

Convert a basic DSK file:
```bash
dsktoimd system.dsk system.imd
```

### With Analysis

Convert and analyze results:
```bash
dsktoimd source.dsk target.imd
ls -la target.imd
```

### Batch Conversion

Convert multiple files using shell scripting:
```bash
for f in *.dsk; do 
    dsktoimd "$f" "${f%.dsk}.imd"
done
```

### Round-trip Test

Verify conversion integrity:
```bash
dsktoimd original.dsk temp.imd
imdtodsk temp.imd restored.dsk
cmp original.dsk restored.dsk && echo "Perfect conversion!"
```

## SAMPLE OUTPUT

```
DSK to IMD Converter v1.0 (MDOS optimized)
Input file: system.dsk
Output file: system.imd
Found data up to track 15
Converting DSK to IMD...
Track 0: writing 26 sectors
Track 1: writing 26 sectors
...
Track 15: writing 26 sectors
Conversion completed successfully!
Written 16 tracks, 416 sectors total
Compressed 89 sectors
Conversion successful!
```

## GENERATED IMD HEADER

The program creates a descriptive header in each IMD file:

```
IMD file created from DSK: system.dsk
Created by dsktoimd.c on 2025-01-18 14:30:22
MDOS format: 128-byte sectors, up to 26 sectors per track
```

## FILES

- **`INPUT.DSK`** - Source DSK format file containing sequential sector data
- **`OUTPUT.IMD`** - Target ImageDisk format file (overwrites existing files)

## EXIT STATUS

| Code | Description |
|------|-------------|
| 0 | Successful conversion completed |
| 1 | Error occurred during conversion (file access, format issues, or data problems) |

## PERFORMANCE

**dsktoimd** is optimized for efficiency:

### Memory Usage

- Single-pass processing minimizes memory requirements
- Sector-by-sector analysis reduces peak usage
- No full disk image buffering required

### Compression Benefits

- Uniform sectors compressed to 2 bytes each
- Typical space savings of 20-50% for system disks
- Faster disk image transfer and storage

### Processing Speed

- Linear file access pattern
- Minimal computational overhead
- Efficient compression detection

## COMPATIBILITY

**dsktoimd** is designed for cross-platform compatibility:

- Standard C library functions for maximum portability
- Binary file I/O with proper byte ordering
- Creates IMD files compatible with ImageDisk and emulators
- Works with DSK files from various sources

## TECHNICAL DETAILS

### Track Detection Algorithm

1. **Forward Scan**: Read through entire DSK file
2. **Data Detection**: Check each sector for non-zero content
3. **Last Track**: Record highest track number with data
4. **Optimization**: Skip empty trailing tracks

### Sector Compression Logic

```c
bool is_sector_compressed(sector_data, &fill_byte) {
    fill_byte = sector_data[0];
    for (i = 1; i < 128; i++) {
        if (sector_data[i] != fill_byte) return false;
    }
    return true;
}
```