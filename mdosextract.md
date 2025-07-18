# mdosextract(1)

## NAME

**mdosextract** - extract files from MDOS IMD disk images

## SYNOPSIS

```
mdosextract IMD_FILE [OPTIONS] [WILDCARDS]
mdosextract IMD_FILE -o OUTPUT_DIR [OPTIONS] [WILDCARDS]
```

## DESCRIPTION

**mdosextract** extracts files from MDOS (Motorola Disk Operating System) IMD (ImageDisk) disk images. The program can extract files in multiple formats: original binary format, decoded text format for ASCII files, and Motorola S19 format with load/start addresses from the RIB (Record Information Block).

The tool analyzes the MDOS file system structure, reads the directory entries, follows the SDW (Sector Descriptor Word) chains, and extracts files according to the specified options and wildcards. It also generates a packlist file containing detailed information about each extracted file.

## OPTIONS

### Output Control

- **`-o OUTPUT_DIR`**  
  Specify custom output directory. If not provided, creates a directory named `IMD_FILENAME_extracted` in the same location as the IMD file.

- **`-h, --help`**  
  Display help message and exit.

### Format Selection

- **`--all`** *(default)*  
  Extract all formats: original binary, text (.txt), and S19 (.s19). This is the default behavior when no format options are specified.

- **`--original`**  
  Extract only the original binary format. Files are extracted exactly as stored on the MDOS disk.

- **`--text`**  
  Extract only text format (.txt). Text files are decoded from MDOS space-compressed format to standard ASCII with proper line endings.

- **`--s19`**  
  Extract only Motorola S19 format (.s19). Creates S-record files with load and start addresses from the file's RIB.

**Note:** Multiple format options can be combined. For example: `--original --s19` will extract both original and S19 formats.

## WILDCARDS

Multiple wildcards can be specified to filter which files are extracted. Wildcards are **case-insensitive** and support the following patterns:

| Pattern | Description | Example |
|---------|-------------|---------|
| `*.EXTENSION` | Files ending with extension | `*.cm` extracts only .cm files |
| `PREFIX*.*` | Files starting with prefix | `game*.*` extracts files starting with "game" |
| `PREFIX*.EXTENSION` | Files with prefix and extension | `editor*.sa` extracts editor*.sa files |
| `*.*` | All files | Default when no wildcards specified |

If no wildcards are provided, all files are extracted.

## OUTPUT FORMATS

**mdosextract** can generate files in three different formats:

### 1. Original Binary Format
The file as stored on the MDOS disk, including any MDOS-specific formatting or compression.

### 2. Text Format (.txt)
For files identified as text (either by MDOS file attributes or file extension), the original file is decoded:

- Space compression is expanded (high-bit-set bytes become multiple spaces)
- MDOS carriage returns (0x0D) are converted to Unix line feeds (0x0A)
- Control characters and padding bytes are filtered out
- The decoded filename preserves the original extension: `file.sa` becomes `file.sa.txt`

### 3. Motorola S19 Format (.s19)
Industry-standard S-record format containing:

- S0 header record
- S1 data records with 16-bit addresses (16 bytes per record)
- S9 termination record with start address from RIB
- Proper checksums for data integrity

## FILES

- **`FILENAME_extracted/`**  
  Default output directory created for extracted files.

- **`FILENAME_extracted/FILENAME.packlist`**  
  Generated packlist file containing detailed information about all extracted files, including load addresses, start addresses, file attributes, and extraction status.

## EXAMPLES

### Basic Usage

Extract all files in all formats:
```bash
mdosextract disk.imd
```

Extract to custom directory:
```bash
mdosextract disk.imd -o /tmp/extracted
```

### Format-Specific Extraction

Extract only original format for .cm files:
```bash
mdosextract disk.imd --original *.cm
```

Extract only text format for assembly source files:
```bash
mdosextract disk.imd --text *.sa
```

Extract only S19 format for files starting with "game":
```bash
mdosextract disk.imd --s19 game*.*
```

### Advanced Usage

Extract multiple file types in all formats:
```bash
mdosextract disk.imd *.cm *.sa *.ob
```

Extract editor files in original and S19 formats:
```bash
mdosextract disk.imd --original --s19 editor*.*
```

Combine custom directory with specific formats:
```bash
mdosextract orig.imd -o cm_files --original *.cm
```

## DIAGNOSTICS

**mdosextract** provides detailed progress information including:

- IMD file parsing results
- MDOS file system verification  
- File extraction progress with RIB information
- Text file detection and decoding statistics
- S19 conversion details
- Summary of extracted files and formats

### Error Reporting

Error conditions are reported with descriptive messages for:

- Invalid or corrupted IMD files
- Missing or inaccessible RIB sectors
- File I/O errors
- Invalid command line arguments

## TECHNICAL DETAILS

**mdosextract** implements the complete MDOS file system specification:

### Disk Structure

- **Sector 0:** Disk ID and system information
- **Sector 1:** Cluster Allocation Table (CAT)
- **Sectors 3-22:** Directory entries (16 bytes each)
- **Remaining sectors:** File data organized in 4-sector clusters

### RIB (Record Information Block)

Each file has a RIB containing:

- SDW chain defining file's cluster allocation
- Load address for program execution
- Start address for program entry point
- File size in sectors and bytes in last sector
- File attributes including format type

### SDW Chain Analysis

Sector Descriptor Words are parsed to:

- Locate all clusters belonging to a file
- Determine actual file size from end markers
- Handle fragmented files across multiple clusters
- Detect and correct corrupted size information

## COMPATIBILITY

**mdosextract** is designed for cross-platform compatibility:

- Linux/Unix systems (using POSIX functions)
- Windows systems (using Windows-specific APIs)
- Handles both forward slash and backslash path separators
- Works with IMD files created by various disk imaging tools

## EXIT STATUS

| Code | Description |
|------|-------------|
| 0 | Successful extraction of all requested files |
| 1 | Error in command line arguments, file access, or IMD parsing |

## SEE ALSO

- `file(1)` - determine file type
- `hexdump(1)` - display file contents in hexadecimal
- `od(1)` - dump files in octal and other formats

**External References:**
- IMD format specification and ImageDisk utility by Dave Dunfield
- MDOS documentation and technical references for Motorola development systems

## AUTHOR

Written for MDOS disk image analysis and file extraction.

## BUGS

Report bugs and suggestions to the maintainer.

**Known Issues:**
- Large files (>1000 sectors) may trigger corruption warnings due to RIB field limitations, but are handled correctly through SDW chain analysis.

## VERSION HISTORY

- **1.0** - Initial release with full MDOS support, multiple output formats, and wildcard filtering

---

*Last updated: January 18, 2025*