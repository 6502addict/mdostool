#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

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

// Global storage for disk sectors
uint8_t disk_sectors[MAX_TRACKS][MAX_SECTORS_PER_TRACK][SECTOR_SIZE];
bool sector_valid[MAX_TRACKS][MAX_SECTORS_PER_TRACK];
int total_sectors = 0;
int valid_sectors = 0;

// Function prototypes
bool parse_imd_file(const char *filename);
void verify_mdos_structure();
void scan_directory();
void extract_filename(struct dirent *d, char *output);
void extract_file(struct dirent *d);
void read_file(int rib_sector, const char *filename);
void get_sector(unsigned char *buf, int sect);
uint16_t read_little_endian_16(uint8_t *data, int offset);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <IMD_FILE>\n", argv[0]);
        return 1;
    }

    printf("MDOS IMD File Extractor - Corrected Version\n");
    printf("===========================================\n\n");

    // Initialize sector validity array
    memset(sector_valid, false, sizeof(sector_valid));

    printf("INFO: Analyzing file: %s\n", argv[1]);
    
    if (!parse_imd_file(argv[1])) {
        printf("ERROR: Failed to parse IMD file\n");
        return 1;
    }

    printf("INFO: Successfully parsed %d valid sectors\n", valid_sectors);
    
    verify_mdos_structure();
    scan_directory();

    return 0;
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
            uint8_t sector_type = fgetc(file);
            if (sector_type == EOF) {
                fclose(file);
                return false;
            }

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
            char filename[12];
            extract_filename(d, filename);

            // Get RIB sector (big-endian)
            uint16_t rib_sector = (d->sector_high << 8) | d->sector_low;
            uint16_t attributes = (d->attr_high << 8) | d->attr_low;

            printf("File %d: %s (RIB: %d, Attr: 0x%04X)\n", 
                   file_count, filename, rib_sector, attributes);

            // Verify RIB address is in valid range
            if (rib_sector >= MAX_SECTORS) {
                printf("  ERROR: RIB sector %d is out of range\n", rib_sector);
                continue;
            }

            // Extract the file using correct MDOS algorithm
            read_file(rib_sector, filename);
            extracted_count++;
        }
    }

    printf("\nSummary: Found %d files, extracted %d files\n", file_count, extracted_count);
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
        snprintf(output, 12, "%s.%s", name, suffix);
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

// Fixed read_file function for mdosextract.c
// This version uses SDW chain and end marker instead of corrupted file size

void read_file(int rib_sector, const char *filename) {
    unsigned char rib_buf[SECTOR_SIZE];
    struct rib *r = (struct rib *)rib_buf;
    
    // Get the RIB
    get_sector(rib_buf, rib_sector);
    
    // Extract file metadata
    int last_size = r->last_size;
    int file_size_from_rib = (r->size_high << 8) | r->size_low;  // Might be corrupted
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
    
    // Open output file
    FILE *f = fopen(filename, "wb");
    if (!f) {
        printf("  ERROR: Cannot create %s\n", filename);
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
           filename, total_bytes, logical_sector);
    
    // Verify extraction
    if (logical_sector != actual_file_size) {
        printf("  Warning: Expected %d sectors, extracted %d sectors\n", 
               actual_file_size, logical_sector);
    }
}

// Helper function to read 16-bit little-endian values
uint16_t read_little_endian_16(uint8_t *data, int offset) {
    return data[offset] | (data[offset + 1] << 8);
}
