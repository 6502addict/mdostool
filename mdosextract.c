#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <libgen.h>
#endif

#define MAX_TRACKS 77
#define MAX_SECTORS_PER_TRACK 26
#define SECTOR_SIZE 128
#define MAX_SECTORS (MAX_TRACKS * MAX_SECTORS_PER_TRACK)
#define CLUSTER_SIZE (SECTOR_SIZE * 4)

// IMD track header structure
typedef struct {
    uint8_t mode;          // Recording mode
    uint8_t cylinder;      // Track number
    uint8_t head;          // Head number + flags
    uint8_t sector_count;  // Sectors in this track
    uint8_t sector_size;   // Sector size code
} imd_track_header_t;

// MDOS RIB structure (from official code)
struct rib {
    unsigned char sdw[114];      // 114 bytes of SDWs
    unsigned char blank[3];      // 3 blank bytes
    unsigned char last_size;     // byte 117: bytes in last sector
    unsigned char size_high;     // byte 118: file size high byte
    unsigned char size_low;      // byte 119: file size low byte  
    unsigned char addr_high;     // byte 120: load address high
    unsigned char addr_low;      // byte 121: load address low
    unsigned char pc_high;       // byte 122: start address high
    unsigned char pc_low;        // byte 123: start address low
    unsigned char blank_1[4];    // 4 more blank bytes
};

// MDOS directory entry structure
struct dirent {
    unsigned char name[8];
    unsigned char suffix[2];
    unsigned char sector_high;   // RIB sector (big-endian)
    unsigned char sector_low;
    unsigned char attr_high;
    unsigned char attr_low;
    unsigned char blank[2];
};

// MDOS disk ID structure
typedef struct {
    char disk_id[8];        // "MDOS304" etc.
    uint16_t version;       // Version number
    uint16_t revision;      // Revision number
    char date[6];           // Date (MMDDYY)
    char user_name[20];     // User name
} mdos_disk_id_t;

// Structure to hold file information for packlist
typedef struct {
    char filename[13];      // 8.3 format
    char filepath[256];     // Full path to extracted file
    uint16_t load_addr;
    uint16_t start_addr;
    uint16_t attributes;
    uint16_t file_size_sectors;
    uint8_t last_sector_bytes;
    int rib_sector;
    bool extracted_ok;
} file_info_t;

// Global storage for disk sectors
uint8_t disk_sectors[MAX_TRACKS][MAX_SECTORS_PER_TRACK][SECTOR_SIZE];
bool sector_valid[MAX_TRACKS][MAX_SECTORS_PER_TRACK];
int total_sectors = 0;
int valid_sectors = 0;

// File information array for packlist
file_info_t file_info[320]; // Max 320 files
int file_info_count = 0;

// Function prototypes
bool parse_imd_file(const char *filename);
void verify_mdos_structure();
void scan_directory();
void extract_filename(struct dirent *d, char *output);
void extract_file(struct dirent *d);
void read_file(int rib_sector, const char *filename);
void get_sector(unsigned char *buf, int sect);
uint16_t read_little_endian_16(uint8_t *data, int offset);
void create_packlist(const char *imd_filename);
void create_output_directory(const char *imd_filename);
int analyze_sdw_chain(struct rib *rib);
void fix_rib_after_extraction(file_info_t *info, const char *extracted_file);
void create_s19_file(const char *binary_path, const char *filename, uint16_t load_addr, uint16_t start_addr);
uint8_t calculate_s19_checksum(uint8_t *data, int length);

// Global output directory and base paths
char output_dir[512];
char base_dir[512];
char base_name[256];

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <IMD_FILE>\n", argv[0]);
        printf("Extracts MDOS files and creates a .packlist with RIB information\n");
        printf("Also creates Motorola S19 files for each extracted file\n");
        return 1;
    }

    printf("MDOS IMD File Extractor with Packlist and S19 Generator\n");
    printf("======================================================\n\n");

    // Initialize sector validity array
    memset(sector_valid, false, sizeof(sector_valid));
    file_info_count = 0;

    printf("INFO: Analyzing file: %s\n", argv[1]);
    
    // Create output directory
    create_output_directory(argv[1]);
    
    if (!parse_imd_file(argv[1])) {
        printf("ERROR: Failed to parse IMD file\n");
        return 1;
    }

    printf("INFO: Successfully parsed %d valid sectors\n", valid_sectors);
    
    verify_mdos_structure();
    scan_directory();
    
    // Create packlist with RIB information
    create_packlist(argv[1]);

    return 0;
}

void create_output_directory(const char *imd_filename) {
#ifdef _WIN32
    // Windows version - use backslash separators and different path handling
    char path_copy1[512];
    char path_copy2[512];
    strcpy(path_copy1, imd_filename);
    strcpy(path_copy2, imd_filename);
    
    // Get directory containing the IMD file (Windows style)
    char *last_slash = strrchr(path_copy1, '\\');
    if (!last_slash) last_slash = strrchr(path_copy1, '/');
    
    char dir_path[512];
    if (last_slash) {
        *last_slash = '\0';
        strcpy(dir_path, path_copy1);
    } else {
        strcpy(dir_path, ".");
    }
    
    // Get filename without extension
    char *base = strrchr(path_copy2, '\\');
    if (!base) base = strrchr(path_copy2, '/');
    if (base) {
        base++;
    } else {
        base = path_copy2;
    }
    
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    
    // Store the base name and directory for later use
    strncpy(base_name, base, sizeof(base_name) - 1);
    base_name[sizeof(base_name) - 1] = '\0';
    
    strncpy(base_dir, dir_path, sizeof(base_dir) - 1);
    base_dir[sizeof(base_dir) - 1] = '\0';
    
    // Create output directory path relative to the IMD file location
    if (strcmp(dir_path, ".") == 0) {
        // IMD file is in current directory
        snprintf(output_dir, sizeof(output_dir), "%s_extracted", base_name);
    } else {
        // IMD file is in another directory
        snprintf(output_dir, sizeof(output_dir), "%s\\%s_extracted", dir_path, base_name);
    }
    
    // Create directory if it doesn't exist (Windows version)
    struct _stat st = {0};
    if (_stat(output_dir, &st) == -1) {
        if (_mkdir(output_dir) != 0) {
            printf("ERROR: Cannot create output directory %s\n", output_dir);
            perror("mkdir");
            exit(1);
        }
    }
    
#else
    // Unix/Linux version - use forward slashes and libgen functions
    // Make copies for dirname/basename since they may modify the string
    char *path_copy1 = strdup(imd_filename);
    char *path_copy2 = strdup(imd_filename);
    
    // Get directory containing the IMD file
    char *dir_path = dirname(path_copy1);
    
    // Get filename without extension
    char *base = basename(path_copy2);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    
    // Store the base name and directory for later use
    strncpy(base_name, base, sizeof(base_name) - 1);
    base_name[sizeof(base_name) - 1] = '\0';
    
    strncpy(base_dir, dir_path, sizeof(base_dir) - 1);
    base_dir[sizeof(base_dir) - 1] = '\0';
    
    // Create output directory path relative to the IMD file location
    if (strcmp(dir_path, ".") == 0) {
        // IMD file is in current directory
        snprintf(output_dir, sizeof(output_dir), "%s_extracted", base_name);
    } else {
        // IMD file is in another directory
        snprintf(output_dir, sizeof(output_dir), "%s/%s_extracted", dir_path, base_name);
    }
    
    // Create directory if it doesn't exist (Unix version)
    struct stat st = {0};
    if (stat(output_dir, &st) == -1) {
        if (mkdir(output_dir, 0755) != 0) {
            printf("ERROR: Cannot create output directory %s\n", output_dir);
            perror("mkdir");
            exit(1);
        }
    }
    
    free(path_copy1);
    free(path_copy2);
#endif
    
    printf("INFO: Output directory: %s/\n", output_dir);
    printf("INFO: Base directory: %s\n", base_dir);
    printf("INFO: Base name: %s\n", base_name);
}

// Calculate checksum for S19 record
uint8_t calculate_s19_checksum(uint8_t *data, int length) {
    uint8_t sum = 0;
    for (int i = 0; i < length; i++) {
        sum += data[i];
    }
    return ~sum;  // One's complement
}

// Create Motorola S19 file from binary data
void create_s19_file(const char *binary_path, const char *filename, uint16_t load_addr, uint16_t start_addr) {
    FILE *bin_file = fopen(binary_path, "rb");
    if (!bin_file) {
        printf("    ERROR: Cannot open %s for S19 conversion\n", binary_path);
        return;
    }
    
    // Create S19 filename
    char s19_filename[512];
    snprintf(s19_filename, sizeof(s19_filename), "%s/%s.s19", output_dir, filename);
    
    FILE *s19_file = fopen(s19_filename, "w");
    if (!s19_file) {
        printf("    ERROR: Cannot create %s\n", s19_filename);
        fclose(bin_file);
        return;
    }
    
    printf("    Creating S19 file: %s\n", s19_filename);
    
    // Write S0 header record
    fprintf(s19_file, "S00F000068656C6C6F202020202000003C\n");
    
    // Read binary data and create S1 records
    uint8_t buffer[16];  // 16 bytes per S1 record (good balance)
    uint16_t address = load_addr;
    size_t bytes_read;
    int total_records = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), bin_file)) > 0) {
        // Prepare S1 record data
        uint8_t record_data[32];  // Max size for address + data + checksum calculation
        int record_length = 3 + bytes_read;  // 3 bytes (length + address) + data bytes
        
        record_data[0] = record_length;           // Length byte
        record_data[1] = (address >> 8) & 0xFF;  // Address high byte
        record_data[2] = address & 0xFF;         // Address low byte
        
        // Copy data bytes
        for (size_t i = 0; i < bytes_read; i++) {
            record_data[3 + i] = buffer[i];
        }
        
        // Calculate checksum
        uint8_t checksum = calculate_s19_checksum(record_data, record_length);
        
        // Write S1 record
        fprintf(s19_file, "S1%02X%04X", record_length, address);
        for (size_t i = 0; i < bytes_read; i++) {
            fprintf(s19_file, "%02X", buffer[i]);
        }
        fprintf(s19_file, "%02X\n", checksum);
        
        address += bytes_read;
        total_records++;
    }
    
    // Write S9 termination record with start address
    uint8_t term_data[3] = {0x03, (start_addr >> 8) & 0xFF, start_addr & 0xFF};
    uint8_t term_checksum = calculate_s19_checksum(term_data, 3);
    fprintf(s19_file, "S903%04X%02X\n", start_addr, term_checksum);
    
    fclose(bin_file);
    fclose(s19_file);
    
    printf("    S19 conversion complete: %d data records, load=0x%04X, start=0x%04X\n", 
           total_records, load_addr, start_addr);
}

// Add this function to decode MDOS text files with space compression
void decode_text_file(const char *input_path, const char *output_path) {
    FILE *input = fopen(input_path, "rb");
    if (!input) {
        printf("    ERROR: Cannot open %s for text decoding\n", input_path);
        return;
    }
    
    FILE *output = fopen(output_path, "wb");
    if (!output) {
        printf("    ERROR: Cannot create %s\n", output_path);
        fclose(input);
        return;
    }
    
    int c;
    int decoded_bytes = 0;
    int original_bytes = 0;
    int space_expansions = 0;
    int line_conversions = 0;
    int null_bytes_skipped = 0;
    int control_chars_found = 0;
    
    while ((c = fgetc(input)) != EOF) {
        original_bytes++;
        
        if (c == 0x00) {
            // Skip null bytes (EOF markers or padding)
            null_bytes_skipped++;
            continue;
        } else if (c == 0x1A) {
            // Skip SUB character (often used as EOF in CP/M-style systems)
            null_bytes_skipped++;
            continue;
        } else if (c & 0x80) {
            // High bit set - this is compressed spaces
            int space_count = c & 0x7F;  // Get bits 0-6
            for (int i = 0; i < space_count; i++) {
                fputc(' ', output);
                decoded_bytes++;
            }
            space_expansions++;
        } else if (c == 0x0D) {
            // Convert MDOS carriage return to Unix line feed
            fputc(0x0A, output);
            decoded_bytes++;
            line_conversions++;
        } else if (c == 0x7F) {
            // DEL character - skip it (this is probably what shows as ^?)
            printf("    DEBUG: Found DEL character (0x7F) at position %d - skipping\n", original_bytes);
            control_chars_found++;
            continue;
        } else if (c < 0x20 && c != 0x09 && c != 0x0A) {
            // Other control characters (except TAB and LF) - log and skip
            printf("    DEBUG: Found control character 0x%02X at position %d - skipping\n", c, original_bytes);
            control_chars_found++;
            continue;
        } else {
            // Normal character - copy as-is
            fputc(c, output);
            decoded_bytes++;
        }
    }
    
    fclose(input);
    fclose(output);
    
    printf("    Text decoded: %s -> %s\n", input_path, output_path);
    printf("    Stats: %d bytes -> %d bytes, %d space expansions, %d line endings converted, %d null/EOF bytes removed, %d control chars filtered\n", 
           original_bytes, decoded_bytes, space_expansions, line_conversions, null_bytes_skipped, control_chars_found);
}

// Add this function to check if a file is a text file based on attributes
bool is_text_file(uint16_t attributes) {
    // Check file format field (bits 0-2)
    int format = attributes & 0x07;
    printf("    DEBUG: Checking attributes 0x%04X, format = %d\n", attributes, format);
    return (format == 5); // Format 5 = ASCII record file
}

bool parse_imd_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("ERROR: Cannot open file %s\n", filename);
        return false;
    }

    // Get file size
    struct stat st;
    stat(filename, &st);
    printf("INFO: File size: %ld bytes\n", st.st_size);

    // Read comment block until 0x1A
    char comment[1024];
    int pos = 0;
    while (pos < sizeof(comment) - 1) {
        int c = fgetc(file);
        if (c == 0x1A || c == EOF) break;
        comment[pos++] = c;
    }
    comment[pos] = '\0';
    printf("INFO: IMD Comment: %s\n", comment);

    // Parse tracks
    int tracks_parsed = 0;
    while (tracks_parsed < 200) { // Safety limit
        imd_track_header_t header;
        if (fread(&header, sizeof(header), 1, file) != 1) {
            break; // End of file
        }

        int track_num = header.cylinder; // Use actual cylinder number

        if (header.sector_count == 0) {
            tracks_parsed++;
            continue;
        }

        // Safety check
        if (track_num >= MAX_TRACKS) {
            tracks_parsed++;
            continue;
        }

        // Read sector number map
        uint8_t sector_map[header.sector_count];
        if (fread(sector_map, header.sector_count, 1, file) != 1) {
            break;
        }

        // Read optional cylinder map if present
        if (header.head & 0x80) {
            uint8_t cylinder_map[header.sector_count];
            if (fread(cylinder_map, header.sector_count, 1, file) != 1) {
                break;
            }
        }

        // Read optional head map if present
        if (header.head & 0x40) {
            uint8_t head_map[header.sector_count];
            if (fread(head_map, header.sector_count, 1, file) != 1) {
                break;
            }
        }

        // Read sector data
        for (int s = 0; s < header.sector_count; s++) {
            int sector_type_int = fgetc(file);
            if (sector_type_int == EOF) {
                fclose(file);
                return false;
            }
            uint8_t sector_type = (uint8_t)sector_type_int;

            uint8_t sector_data[SECTOR_SIZE];
            int sector_num = sector_map[s];

            switch (sector_type) {
                case 0: // Data unavailable
                    memset(sector_data, 0, SECTOR_SIZE);
                    break;

                case 1: // Normal data
                    if (fread(sector_data, SECTOR_SIZE, 1, file) != 1) {
                        fclose(file);
                        return false;
                    }
                    break;

                case 2: // Compressed - all same byte
                    {
                        int fill_byte = fgetc(file);
                        if (fill_byte == EOF) {
                            fclose(file);
                            return false;
                        }
                        memset(sector_data, fill_byte, SECTOR_SIZE);
                    }
                    break;

                default:
                    if (fread(sector_data, SECTOR_SIZE, 1, file) != 1) {
                        fclose(file);
                        return false;
                    }
                    break;
            }

            // Convert from 1-based to 0-based sector numbering
            int mdos_sector = sector_num - 1;  // Convert 1-26 to 0-25
            
            if (mdos_sector >= 0 && mdos_sector < MAX_SECTORS_PER_TRACK) {
                memcpy(disk_sectors[track_num][mdos_sector], sector_data, SECTOR_SIZE);
                sector_valid[track_num][mdos_sector] = true;
                valid_sectors++;
            }
            total_sectors++;
        }
        
        tracks_parsed++;
    }

    fclose(file);
    return true;
}

void verify_mdos_structure() {
    printf("INFO: Verifying MDOS file system structure...\n");

    if (!sector_valid[0][0]) {
        printf("ERROR: Disk ID sector (0,0) not available\n");
        return;
    }

    mdos_disk_id_t *disk_id = (mdos_disk_id_t *)disk_sectors[0][0];
    
    printf("INFO: Disk ID: %.8s\n", disk_id->disk_id);
    printf("INFO: Date: %.6s\n", disk_id->date);
    printf("INFO: User: %.20s\n", disk_id->user_name);

    // Verify Cluster Allocation Table (sector 1)
    if (sector_valid[0][1]) {
        uint8_t *cat = disk_sectors[0][1];
        int allocated = 0;
        for (int i = 0; i < 128; i++) {
            for (int bit = 0; bit < 8; bit++) {
                if (cat[i] & (1 << (7 - bit))) {
                    allocated++;
                }
            }
        }
        printf("INFO: Allocated clusters: %d/1024 (%.1f%%)\n", 
               allocated, (allocated * 100.0) / 1024);
    }
}

void scan_directory() {
    printf("INFO: Scanning directory and extracting files...\n");
    
    int file_count = 0;
    int extracted_count = 0;

    // Directory is in sectors 3-22 (20 sectors)
    for (int dir_sector = 3; dir_sector <= 22; dir_sector++) {
        if (!sector_valid[0][dir_sector]) continue;

        uint8_t *sector_data = disk_sectors[0][dir_sector];

        // Each sector contains 8 directory entries (128 / 16 = 8)
        for (int entry = 0; entry < 8; entry++) {
            struct dirent *d = (struct dirent *)(sector_data + (entry * 16));

            // Check if entry is empty or deleted
            if (d->name[0] == 0 || d->name[0] == 0xFF) {
                continue;
            }

            file_count++;

            // Extract filename
            char filename[13];
            extract_filename(d, filename);

            // Get RIB sector (big-endian)
            uint16_t rib_sector = (d->sector_high << 8) | d->sector_low;
            uint16_t attributes = (d->attr_high << 8) | d->attr_low;

            printf("File %d: %s (RIB: %d, Attr: 0x%04X)\n", 
                   file_count, filename, rib_sector, attributes);

            // Store file information for packlist
            if (file_info_count < 320) {
                file_info_t *info = &file_info[file_info_count];
                strcpy(info->filename, filename);
                snprintf(info->filepath, sizeof(info->filepath), "%s/%s", output_dir, filename);
                info->rib_sector = rib_sector;
                info->attributes = attributes;
                info->extracted_ok = false;
                
                // Get RIB information if sector is valid
                if (rib_sector < MAX_SECTORS) {
                    int track = rib_sector / MAX_SECTORS_PER_TRACK;
                    int sector = rib_sector % MAX_SECTORS_PER_TRACK;
                    
                    if (track < MAX_TRACKS && sector < MAX_SECTORS_PER_TRACK && 
                        sector_valid[track][sector]) {
                        
                        struct rib *rib = (struct rib *)disk_sectors[track][sector];
                        info->load_addr = (rib->addr_high << 8) | rib->addr_low;
                        info->start_addr = (rib->pc_high << 8) | rib->pc_low;
                        info->file_size_sectors = (rib->size_high << 8) | rib->size_low;
                        info->last_sector_bytes = rib->last_size;
                        
                        // Analyze SDW chain to get actual file size
                        int actual_sectors = analyze_sdw_chain(rib);
                        
                        // Check for corrupted RIB size information
                        bool size_corrupted = false;
                        if (info->file_size_sectors == 0 || info->file_size_sectors > 1000) {
                            printf("  WARNING: RIB size field corrupted (%d sectors), using SDW analysis\n", 
                                   info->file_size_sectors);
                            info->file_size_sectors = actual_sectors;
                            size_corrupted = true;
                        }
                        
                        if (info->last_sector_bytes == 0 || info->last_sector_bytes > SECTOR_SIZE) {
                            printf("  WARNING: RIB last_size field corrupted (%d bytes), using default\n", 
                                   info->last_sector_bytes);
                            info->last_sector_bytes = SECTOR_SIZE;
                            size_corrupted = true;
                        }
                        
                        if (size_corrupted) {
                            printf("  CORRECTED: Using %d sectors, %d bytes in last sector\n",
                                   info->file_size_sectors, info->last_sector_bytes);
                        }
                        
                        printf("  RIB Info: Load=0x%04X, Start=0x%04X, Size=%d sectors, Last=%d bytes%s\n",
                               info->load_addr, info->start_addr, info->file_size_sectors, 
                               info->last_sector_bytes, size_corrupted ? " [CORRECTED]" : "");
                    }
                }
                
                file_info_count++;
            }

            // Verify RIB address is in valid range
            if (rib_sector >= MAX_SECTORS) {
                printf("  ERROR: RIB sector %d is out of range\n", rib_sector);
                continue;
            }

            // Extract the file using correct MDOS algorithm
            read_file(rib_sector, filename);

           // NEW CODE: Check if this is a text file and decode it
            printf("  DEBUG: Checking if %s is a text file...\n", filename);
            
            // Check attributes for text file format (format 5 = ASCII record)
            bool is_text_by_attr = is_text_file(attributes);
            
            // Also check file extension as backup (MDOS extensions are only 2 chars)
            char *ext = strrchr(filename, '.');
            bool is_text_by_ext = false;
            
            printf("  DEBUG: Extension found: %s\n", ext ? ext : "none");
            
            if (ext) {
                char upper_ext[4] = {0};
                for (int i = 0; i < 3 && ext[i+1]; i++) {
                    upper_ext[i] = toupper(ext[i+1]);
                }
                
                printf("  DEBUG: Uppercase extension: '%s'\n", upper_ext);
                
                if (strcmp(upper_ext, "SA") == 0 ||   // Assembly source
                    strcmp(upper_ext, "AL") == 0 ||   // Assembly listing
                    strcmp(upper_ext, "SB") == 0 ||   // Another text format
                    strcmp(upper_ext, "SC") == 0) {   // Another text format
                    is_text_by_ext = true;
                }
            }
            
            printf("  DEBUG: is_text_by_attr = %d, is_text_by_ext = %d\n", 
                   is_text_by_attr, is_text_by_ext);
            
            if (is_text_by_attr || is_text_by_ext) {
                printf("  Detected text file (%s), creating decoded version...\n", 
                       is_text_by_attr ? "by attribute" : "by extension");
                
                // Create decoded filename by appending ".txt" to the full filename
                // This preserves the original extension: xtrek.sa -> xtrek.sa.txt
                char decoded_filename[256];
                snprintf(decoded_filename, sizeof(decoded_filename), "%s.txt", filename);
                
                char decoded_path[512];
                snprintf(decoded_path, sizeof(decoded_path), "%s/%s", output_dir, decoded_filename);
                
                // Create full path for the original file
                char original_path[512];
                snprintf(original_path, sizeof(original_path), "%s/%s", output_dir, filename);
                
                printf("  DEBUG: Original path: %s\n", original_path);
                printf("  DEBUG: Decoded path: %s\n", decoded_path);
                
                decode_text_file(original_path, decoded_path);
            } else {
                printf("  Not a text file, skipping decode\n");
            }
	    
            // NEW: Create S19 file for every extracted file
            if (file_info_count > 0) {
                file_info_t *info = &file_info[file_info_count - 1];
                char original_path[512];
                snprintf(original_path, sizeof(original_path), "%s/%s", output_dir, filename);
                
                printf("  Creating S19 file for %s...\n", filename);
                create_s19_file(original_path, filename, info->load_addr, info->start_addr);
            }
	    
            // Mark as successfully extracted and fix RIB information using actual extracted file
            if (file_info_count > 0) {
                file_info[file_info_count - 1].extracted_ok = true;
                fix_rib_after_extraction(&file_info[file_info_count - 1], filename);
            }
            
            extracted_count++;
        }
    }

    printf("\nSummary: Found %d files, extracted %d files\n", file_count, extracted_count);
    printf("Each file was extracted in 3 formats: original, .txt (if text), and .s19\n");
}

void extract_filename(struct dirent *d, char *output) {
    char name[9] = {0};
    char suffix[3] = {0};

    // Extract name part (convert to lowercase)
    int name_len = 0;
    for (int i = 0; i < 8 && d->name[i] != ' ' && d->name[i] != 0; i++) {
        if (isprint(d->name[i])) {
            name[name_len++] = tolower(d->name[i]);
        }
    }

    // Extract suffix part (convert to lowercase)
    int suffix_len = 0;
    for (int i = 0; i < 2 && d->suffix[i] != ' ' && d->suffix[i] != 0; i++) {
        if (isprint(d->suffix[i])) {
            suffix[suffix_len++] = tolower(d->suffix[i]);
        }
    }

    // Combine name and suffix
    if (suffix_len > 0) {
        snprintf(output, 13, "%s.%s", name, suffix);
    } else {
        strcpy(output, name);
    }
}

// Get sector data (helper function)
void get_sector(unsigned char *buf, int sect) {
    int track = sect / MAX_SECTORS_PER_TRACK;
    int sector = sect % MAX_SECTORS_PER_TRACK;
    
    if (track < MAX_TRACKS && sector < MAX_SECTORS_PER_TRACK && 
        sector_valid[track][sector]) {
        memcpy(buf, disk_sectors[track][sector], SECTOR_SIZE);
    } else {
        memset(buf, 0, SECTOR_SIZE);
        printf("    Warning: Missing sector %d\n", sect);
    }
}

void read_file(int rib_sector, const char *filename) {
    unsigned char rib_buf[SECTOR_SIZE];
    struct rib *r = (struct rib *)rib_buf;
    
    // Get the RIB
    get_sector(rib_buf, rib_sector);
    
    // Extract file metadata
    int last_size = r->last_size;
    int file_size_from_rib = (r->size_high << 8) | r->size_low;
    int load_addr = (r->addr_high << 8) | r->addr_low;
    int start_addr = (r->pc_high << 8) | r->pc_low;
    
    printf("  RIB metadata: Size: %d sectors, Last: %d bytes, Load: 0x%04X, Start: 0x%04X\n",
           file_size_from_rib, last_size, load_addr, start_addr);
    
    // Scan SDWs to find actual file size and end marker
    int actual_file_size = 0;
    int actual_last_size = last_size;
    bool found_end_marker = false;
    
    // First pass: find the end marker to get actual file size
    for (int x = 0; x < 114; x += 2) {
        int sdw = (r->sdw[x] << 8) | r->sdw[x + 1];
        
        if (sdw & 0x8000) {
            // End marker found
            actual_file_size = (sdw & 0x7FFF) + 1;  // Convert to 1-based count
            found_end_marker = true;
            printf("  End marker found: actual file size = %d sectors\n", actual_file_size);
            break;
        }
    }
    
    // If no end marker or corrupted size, fall back to original method
    if (!found_end_marker || actual_file_size <= 0) {
        printf("  Warning: No valid end marker found, using RIB size field\n");
        actual_file_size = file_size_from_rib;
    }
    
    // Fix last_size if it's corrupted (0 or > 128)
    if (actual_last_size <= 0 || actual_last_size > SECTOR_SIZE) {
        actual_last_size = SECTOR_SIZE;  // Assume full last sector
        printf("  Warning: Invalid last_size (%d), assuming full sector\n", last_size);
    }
    
    printf("  Using: %d sectors, last sector: %d bytes\n", actual_file_size, actual_last_size);
    
    // Create full path for output file
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", output_dir, filename);
    
    // Open output file
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        printf("  ERROR: Cannot create %s\n", filepath);
        return;
    }
    
    int logical_sector = 0;
    int total_bytes = 0;
    
    // Parse SDWs using official algorithm
    for (int x = 0; x < 114; x += 2) {
        // Read SDW as big-endian (official format)
        int sdw = (r->sdw[x] << 8) | r->sdw[x + 1];
        
        if (sdw & 0x8000) {
            // End marker found
            int last_logical = sdw & 0x7FFF;
            printf("  End marker: last logical sector = %d\n", last_logical);
            break;
        } else if (sdw != 0) {
            // Data segment
            int cluster = sdw & 0x3FF;           // bits 0-9: cluster number
            int cluster_count = ((sdw >> 10) & 0x1F) + 1;  // bits 10-14: cluster count
            int start_sector = cluster * 4;      // Convert cluster to sector
            int sector_count = cluster_count * 4; // Convert to sectors
            
            printf("  Segment: cluster %d, count %d (sectors %d-%d)\n",
                   cluster, cluster_count, start_sector, start_sector + sector_count - 1);
            
            // Read all sectors in this segment
            for (int i = 0; i < sector_count; i++) {
                int physical_sector = start_sector + i;
                
                // Skip the RIB sector itself
                if (physical_sector != rib_sector) {
                    unsigned char buf[SECTOR_SIZE];
                    get_sector(buf, physical_sector);
                    
                    // Check if we've reached the actual file size
                    if (logical_sector >= actual_file_size) {
                        printf("    Reached file size limit, stopping\n");
                        goto done;
                    }
                    
                    // Write sector to file
                    if (logical_sector + 1 == actual_file_size && actual_last_size < SECTOR_SIZE) {
                        // Last sector - only write specified number of bytes
                        fwrite(buf, actual_last_size, 1, f);
                        total_bytes += actual_last_size;
                        printf("    Sector %d -> %d bytes (last)\n", physical_sector, actual_last_size);
                    } else {
                        // Full sector
                        fwrite(buf, SECTOR_SIZE, 1, f);
                        total_bytes += SECTOR_SIZE;
                        printf("    Sector %d -> %d bytes\n", physical_sector, SECTOR_SIZE);
                    }
                    logical_sector++;
                }
            }
        }
    }
    
done:
    fclose(f);
    printf("  Extracted %s (%d bytes total, %d logical sectors)\n", 
           filepath, total_bytes, logical_sector);
    
    // Verify extraction
    if (logical_sector != actual_file_size) {
        printf("  Warning: Expected %d sectors, extracted %d sectors\n", 
               actual_file_size, logical_sector);
    }
}

void create_packlist(const char *imd_filename) {
    // Create packlist filename in the output directory
    char packlist_path[512];
    snprintf(packlist_path, sizeof(packlist_path), "%s/%s.packlist", output_dir, base_name);
    
    FILE *fp = fopen(packlist_path, "w");
    if (!fp) {
        printf("ERROR: Cannot create packlist file %s\n", packlist_path);
        return;
    }
    
    printf("\nINFO: Creating packlist: %s\n", packlist_path);
    
    // Write header
    fprintf(fp, "# MDOS Packlist generated by mdosextract.c\n");
    fprintf(fp, "# Source IMD: %s\n", imd_filename);
    fprintf(fp, "# Extracted to: %s/\n", output_dir);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    fprintf(fp, "# Generated: %04d-%02d-%02d %02d:%02d:%02d\n",
            tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    fprintf(fp, "#\n");
    fprintf(fp, "# Format: filename load_addr start_addr attr file_size last_bytes rib_sector\n");
    fprintf(fp, "# All addresses and values in hexadecimal\n");
    fprintf(fp, "# Note: Each file is extracted in multiple formats:\n");
    fprintf(fp, "#   - Original binary format\n");
    fprintf(fp, "#   - .txt format (for text files with space decompression)\n");
    fprintf(fp, "#   - .s19 format (Motorola S-record with load/start addresses)\n");
    fprintf(fp, "#\n\n");
    
    int successful_count = 0;
    int failed_count = 0;
    
    // Write file entries
    for (int i = 0; i < file_info_count; i++) {
        file_info_t *info = &file_info[i];
        
        if (info->extracted_ok) {
            fprintf(fp, "%s load=%04X start=%04X attr=%04X size=%04X last=%02X rib=%04X\n",
                    info->filepath,
                    info->load_addr,
                    info->start_addr, 
                    info->attributes,
                    info->file_size_sectors,
                    info->last_sector_bytes,
                    info->rib_sector);
            successful_count++;
        } else {
            fprintf(fp, "# FAILED: %s (RIB sector %d not accessible)\n", 
                    info->filename, info->rib_sector);
            failed_count++;
        }
    }
    
    fprintf(fp, "\n# Summary: %d files extracted, %d failed\n", successful_count, failed_count);
    fprintf(fp, "# Total files created: %d original + %d text + %d S19 = %d files\n", 
            successful_count, successful_count, successful_count, successful_count * 3);
    
    fclose(fp);
    
    printf("INFO: Packlist created with %d entries (%d successful, %d failed)\n", 
           file_info_count, successful_count, failed_count);
    printf("INFO: Each extracted file created in 3 formats (original, .txt if applicable, .s19)\n");
    printf("INFO: Total files created: %d\n", successful_count * 3);
}

// Analyze SDW chain to determine actual file size in sectors
int analyze_sdw_chain(struct rib *rib) {
    int total_sectors = 0;
    
    for (int x = 0; x < 114; x += 2) {
        int sdw = (rib->sdw[x] << 8) | rib->sdw[x + 1];
        
        if (sdw & 0x8000) {
            // End marker found - return the sector count indicated
            return (sdw & 0x7FFF) + 1;  // Convert to 1-based count
        } else if (sdw != 0) {
            // Data segment
            int cluster = sdw & 0x3FF;           // bits 0-9: cluster number
            int cluster_count = ((sdw >> 10) & 0x1F) + 1;  // bits 10-14: cluster count
            total_sectors += cluster_count * 4;  // Each cluster = 4 sectors
        }
    }
    
    // No end marker found, return what we counted
    return total_sectors;
}

// Fix RIB information after extraction using actual extracted file size
void fix_rib_after_extraction(file_info_t *info, const char *filename) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", output_dir, filename);
    
    // Get actual extracted file size
    struct stat st;
    if (stat(filepath, &st) == 0) {
        long actual_file_size = st.st_size;
        int actual_sectors_needed = (actual_file_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
        int actual_last_bytes = actual_file_size % SECTOR_SIZE;
        if (actual_last_bytes == 0) actual_last_bytes = SECTOR_SIZE;
        
        // Check if RIB size information was wrong
        long expected_size = (info->file_size_sectors - 1) * SECTOR_SIZE + info->last_sector_bytes;
        
        if (abs(actual_file_size - expected_size) > SECTOR_SIZE || 
            info->file_size_sectors == 0 || info->file_size_sectors > 1000) {
            
            printf("  FIXING: RIB claimed %ld bytes (%d sectors), actual file is %ld bytes (%d sectors)\n",
                   expected_size, info->file_size_sectors, actual_file_size, actual_sectors_needed);
            
            // Update with correct values
            info->file_size_sectors = actual_sectors_needed;
            info->last_sector_bytes = actual_last_bytes;
            
            printf("  CORRECTED: Now using %d sectors, %d bytes in last sector\n",
                   info->file_size_sectors, info->last_sector_bytes);
        }
    }
}

// Helper function to read 16-bit little-endian values
uint16_t read_little_endian_16(uint8_t *data, int offset) {
    return data[offset] | (data[offset + 1] << 8);
}