#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_TRACKS 77
#define MAX_SECTORS_PER_TRACK 26
#define SECTOR_SIZE 128  // MDOS uses 128-byte sectors

// IMD track header structure (matching your working code)
typedef struct {
    uint8_t mode;          // Recording mode
    uint8_t cylinder;      // Track number
    uint8_t head;          // Head number + flags
    uint8_t sector_count;  // Sectors in this track
    uint8_t sector_size;   // Sector size code
} imd_track_header_t;

// Function to read IMD comment
int read_imd_comment(FILE *fp, char *comment, size_t max_len) {
    size_t len = 0;
    int c;
    
    // Read characters until we find 0x1A marker
    while (len < max_len - 1) {
        c = fgetc(fp);
        if (c == 0x1A || c == EOF) break;
        comment[len++] = c;
    }
    comment[len] = '\0';
    
    if (c != 0x1A) {
        fprintf(stderr, "Warning: 0x1A marker not found in comment\n");
        return -1;
    }
    
    return len;
}

int convert_imd_to_dsk(const char *imd_filename, const char *dsk_filename) {
    FILE *imd_fp, *dsk_fp;
    char comment[1024];
    imd_track_header_t header;
    uint8_t sector_data[SECTOR_SIZE];
    
    // Track and sector storage (like your working code)
    uint8_t disk_sectors[MAX_TRACKS][MAX_SECTORS_PER_TRACK][SECTOR_SIZE];
    bool sector_valid[MAX_TRACKS][MAX_SECTORS_PER_TRACK];
    
    // Initialize
    memset(sector_valid, false, sizeof(sector_valid));
    memset(disk_sectors, 0, sizeof(disk_sectors));
    
    // Open input IMD file
    imd_fp = fopen(imd_filename, "rb");
    if (!imd_fp) {
        perror("Error opening IMD file");
        return -1;
    }
    
    // Read IMD comment
    if (read_imd_comment(imd_fp, comment, sizeof(comment)) >= 0) {
        printf("IMD Comment: %s\n", comment);
    }
    
    printf("Converting IMD to DSK...\n");
    
    int tracks_parsed = 0;
    int total_sectors = 0;
    int valid_sectors = 0;
    
    // Parse tracks (using your working algorithm)
    while (tracks_parsed < 200) { // Safety limit
        if (fread(&header, sizeof(header), 1, imd_fp) != 1) {
            break; // End of file
        }
        
        int track_num = header.cylinder; // Use actual cylinder number
        
        printf("Track %d: %d sectors\n", track_num, header.sector_count);
        
        if (header.sector_count == 0) {
            tracks_parsed++;
            continue;
        }
        
        // Safety check
        if (track_num >= MAX_TRACKS) {
            printf("Warning: Track %d >= MAX_TRACKS, skipping\n", track_num);
            tracks_parsed++;
            continue;
        }
        
        // Read sector number map
        uint8_t sector_map[header.sector_count];
        if (fread(sector_map, header.sector_count, 1, imd_fp) != 1) {
            fprintf(stderr, "Error reading sector map\n");
            break;
        }
        
        // Read optional cylinder map if present
        if (header.head & 0x80) {
            uint8_t cylinder_map[header.sector_count];
            if (fread(cylinder_map, header.sector_count, 1, imd_fp) != 1) {
                fprintf(stderr, "Error reading cylinder map\n");
                break;
            }
        }
        
        // Read optional head map if present
        if (header.head & 0x40) {
            uint8_t head_map[header.sector_count];
            if (fread(head_map, header.sector_count, 1, imd_fp) != 1) {
                fprintf(stderr, "Error reading head map\n");
                break;
            }
        }
        
        // Read sector data (using your working algorithm)
        for (int s = 0; s < header.sector_count; s++) {
            uint8_t sector_type = fgetc(imd_fp);
            if (sector_type == EOF) {
                fprintf(stderr, "Unexpected end of file reading sector type\n");
                fclose(imd_fp);
                return -1;
            }
            
            int sector_num = sector_map[s];
            
            // Handle sector types (simplified like your code)
            switch (sector_type) {
                case 0: // Data unavailable
                    memset(sector_data, 0, SECTOR_SIZE);
                    break;
                    
                case 1: // Normal data
                    if (fread(sector_data, SECTOR_SIZE, 1, imd_fp) != 1) {
                        fprintf(stderr, "Error reading normal sector data\n");
                        fclose(imd_fp);
                        return -1;
                    }
                    break;
                    
                case 2: // Compressed - all same byte
                    {
                        int fill_byte = fgetc(imd_fp);
                        if (fill_byte == EOF) {
                            fprintf(stderr, "Error reading compressed sector fill byte\n");
                            fclose(imd_fp);
                            return -1;
                        }
                        memset(sector_data, fill_byte, SECTOR_SIZE);
                    }
                    break;
                    
                default:
                    // Handle other types as normal data (like your code)
                    printf("Warning: Unknown sector type %d, treating as normal data\n", sector_type);
                    if (fread(sector_data, SECTOR_SIZE, 1, imd_fp) != 1) {
                        fprintf(stderr, "Error reading unknown sector type data\n");
                        fclose(imd_fp);
                        return -1;
                    }
                    break;
            }
            
            // Convert from 1-based to 0-based sector numbering (like your code)
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
    
    fclose(imd_fp);
    
    printf("Parsed %d tracks, %d valid sectors out of %d total\n", 
           tracks_parsed, valid_sectors, total_sectors);
    
    // Open output DSK file
    dsk_fp = fopen(dsk_filename, "wb");
    if (!dsk_fp) {
        perror("Error creating DSK file");
        return -1;
    }
    
    // Write sectors in sequential order (track 0 sectors 0-25, track 1 sectors 0-25, etc.)
    int written_sectors = 0;
    for (int track = 0; track < MAX_TRACKS; track++) {
        bool track_has_data = false;
        
        // Check if track has any valid sectors
        for (int sector = 0; sector < MAX_SECTORS_PER_TRACK; sector++) {
            if (sector_valid[track][sector]) {
                track_has_data = true;
                break;
            }
        }
        
        if (track_has_data) {
            printf("Writing track %d\n", track);
            for (int sector = 0; sector < MAX_SECTORS_PER_TRACK; sector++) {
                if (sector_valid[track][sector]) {
                    if (fwrite(disk_sectors[track][sector], SECTOR_SIZE, 1, dsk_fp) != 1) {
                        fprintf(stderr, "Error writing sector data\n");
                        fclose(dsk_fp);
                        return -1;
                    }
                    written_sectors++;
                } else {
                    // Write empty sector
                    uint8_t empty_sector[SECTOR_SIZE];
                    memset(empty_sector, 0, SECTOR_SIZE);
                    if (fwrite(empty_sector, SECTOR_SIZE, 1, dsk_fp) != 1) {
                        fprintf(stderr, "Error writing empty sector\n");
                        fclose(dsk_fp);
                        return -1;
                    }
                }
            }
        }
    }
    
    fclose(dsk_fp);
    
    printf("Conversion completed successfully!\n");
    printf("Written %d sectors to DSK file\n", written_sectors);
    
    return 0;
}

void print_usage(const char *program_name) {
    printf("Usage: %s <input.imd> <output.dsk>\n", program_name);
    printf("Convert ImageDisk (IMD) file to DSK format\n");
    printf("Optimized for MDOS disk images with 128-byte sectors\n");
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *imd_filename = argv[1];
    const char *dsk_filename = argv[2];
    
    printf("IMD to DSK Converter v1.2 (MDOS optimized)\n");
    printf("Input file: %s\n", imd_filename);
    printf("Output file: %s\n", dsk_filename);
    
    if (convert_imd_to_dsk(imd_filename, dsk_filename) == 0) {
        printf("Conversion successful!\n");
        return 0;
    } else {
        printf("Conversion failed!\n");
        return 1;
    }
}
