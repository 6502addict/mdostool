#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

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

// Function to write IMD comment header
int write_imd_comment(FILE *fp, const char *dsk_filename) {
    char comment[512];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    snprintf(comment, sizeof(comment), 
             "IMD file created from DSK: %s\r\n"
             "Created by dsktoimd.c on %04d-%02d-%02d %02d:%02d:%02d\r\n"
             "MDOS format: 128-byte sectors, up to 26 sectors per track\r\n",
             dsk_filename,
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    // Write comment
    if (fputs(comment, fp) == EOF) {
        return -1;
    }
    
    // Write 0x1A marker to end comment
    if (fputc(0x1A, fp) == EOF) {
        return -1;
    }
    
    return 0;
}

// Function to check if a sector is empty (all zeros)
bool is_sector_empty(const uint8_t *sector_data) {
    for (int i = 0; i < SECTOR_SIZE; i++) {
        if (sector_data[i] != 0) {
            return false;
        }
    }
    return true;
}

// Function to check if a sector is filled with the same byte
bool is_sector_compressed(const uint8_t *sector_data, uint8_t *fill_byte) {
    *fill_byte = sector_data[0];
    for (int i = 1; i < SECTOR_SIZE; i++) {
        if (sector_data[i] != *fill_byte) {
            return false;
        }
    }
    return true;
}

// Function to determine how many tracks contain data
int find_last_track_with_data(FILE *dsk_fp) {
    uint8_t sector_data[SECTOR_SIZE];
    int last_track = -1;
    
    // Save current position
    long current_pos = ftell(dsk_fp);
    
    // Check each track
    for (int track = 0; track < MAX_TRACKS; track++) {
        bool track_has_data = false;
        
        for (int sector = 0; sector < MAX_SECTORS_PER_TRACK; sector++) {
            long sector_pos = (long)track * MAX_SECTORS_PER_TRACK * SECTOR_SIZE + 
                             (long)sector * SECTOR_SIZE;
            
            if (fseek(dsk_fp, sector_pos, SEEK_SET) != 0) {
                break;
            }
            
            if (fread(sector_data, SECTOR_SIZE, 1, dsk_fp) != 1) {
                break;
            }
            
            if (!is_sector_empty(sector_data)) {
                track_has_data = true;
                break;
            }
        }
        
        if (track_has_data) {
            last_track = track;
        }
    }
    
    // Restore position
    fseek(dsk_fp, current_pos, SEEK_SET);
    
    return last_track;
}

int convert_dsk_to_imd(const char *dsk_filename, const char *imd_filename) {
    FILE *dsk_fp, *imd_fp;
    uint8_t sector_data[SECTOR_SIZE];
    
    // Open input DSK file
    dsk_fp = fopen(dsk_filename, "rb");
    if (!dsk_fp) {
        perror("Error opening DSK file");
        return -1;
    }
    
    // Find the last track with data
    int last_track = find_last_track_with_data(dsk_fp);
    if (last_track < 0) {
        fprintf(stderr, "No data found in DSK file\n");
        fclose(dsk_fp);
        return -1;
    }
    
    printf("Found data up to track %d\n", last_track);
    
    // Open output IMD file
    imd_fp = fopen(imd_filename, "wb");
    if (!imd_fp) {
        perror("Error creating IMD file");
        fclose(dsk_fp);
        return -1;
    }
    
    // Write IMD comment header
    if (write_imd_comment(imd_fp, dsk_filename) != 0) {
        fprintf(stderr, "Error writing IMD comment\n");
        fclose(dsk_fp);
        fclose(imd_fp);
        return -1;
    }
    
    printf("Converting DSK to IMD...\n");
    
    int tracks_written = 0;
    int total_sectors = 0;
    int compressed_sectors = 0;
    
    // Process each track up to the last one with data
    for (int track = 0; track <= last_track; track++) {
        // Count non-empty sectors in this track
        int valid_sectors = 0;
        for (int sector = 0; sector < MAX_SECTORS_PER_TRACK; sector++) {
            long sector_pos = (long)track * MAX_SECTORS_PER_TRACK * SECTOR_SIZE + 
                             (long)sector * SECTOR_SIZE;
            
            if (fseek(dsk_fp, sector_pos, SEEK_SET) != 0) {
                break;
            }
            
            if (fread(sector_data, SECTOR_SIZE, 1, dsk_fp) != 1) {
                break;
            }
            
            if (!is_sector_empty(sector_data)) {
                valid_sectors = MAX_SECTORS_PER_TRACK; // Include all sectors if any has data
                break;
            }
        }
        
        if (valid_sectors == 0) {
            printf("Track %d: empty, skipping\n", track);
            continue;
        }
        
        printf("Track %d: writing %d sectors\n", track, MAX_SECTORS_PER_TRACK);
        
        // Write IMD track header
        imd_track_header_t header;
        header.mode = 0x00;        // FM mode (can be adjusted)
        header.cylinder = track;   // Track number
        header.head = 0x00;        // Head 0, no optional maps
        header.sector_count = MAX_SECTORS_PER_TRACK;
        header.sector_size = 0x00; // 128 bytes (sector size code 0)
        
        if (fwrite(&header, sizeof(header), 1, imd_fp) != 1) {
            fprintf(stderr, "Error writing track header\n");
            fclose(dsk_fp);
            fclose(imd_fp);
            return -1;
        }
        
        // Write sector number map (1-based numbering for MDOS)
        uint8_t sector_map[MAX_SECTORS_PER_TRACK];
        for (int i = 0; i < MAX_SECTORS_PER_TRACK; i++) {
            sector_map[i] = i + 1; // 1-based sector numbering
        }
        
        if (fwrite(sector_map, MAX_SECTORS_PER_TRACK, 1, imd_fp) != 1) {
            fprintf(stderr, "Error writing sector map\n");
            fclose(dsk_fp);
            fclose(imd_fp);
            return -1;
        }
        
        // Write sector data with compression
        for (int sector = 0; sector < MAX_SECTORS_PER_TRACK; sector++) {
            long sector_pos = (long)track * MAX_SECTORS_PER_TRACK * SECTOR_SIZE + 
                             (long)sector * SECTOR_SIZE;
            
            if (fseek(dsk_fp, sector_pos, SEEK_SET) != 0) {
                fprintf(stderr, "Error seeking to sector position\n");
                fclose(dsk_fp);
                fclose(imd_fp);
                return -1;
            }
            
            if (fread(sector_data, SECTOR_SIZE, 1, dsk_fp) != 1) {
                fprintf(stderr, "Error reading sector data\n");
                fclose(dsk_fp);
                fclose(imd_fp);
                return -1;
            }
            
            uint8_t fill_byte;
            
            // Check if sector can be compressed
            if (is_sector_compressed(sector_data, &fill_byte)) {
                // Write compressed sector
                if (fputc(2, imd_fp) == EOF) { // Type 2: compressed
                    fprintf(stderr, "Error writing sector type\n");
                    fclose(dsk_fp);
                    fclose(imd_fp);
                    return -1;
                }
                
                if (fputc(fill_byte, imd_fp) == EOF) {
                    fprintf(stderr, "Error writing fill byte\n");
                    fclose(dsk_fp);
                    fclose(imd_fp);
                    return -1;
                }
                
                compressed_sectors++;
            } else {
                // Write normal sector
                if (fputc(1, imd_fp) == EOF) { // Type 1: normal data
                    fprintf(stderr, "Error writing sector type\n");
                    fclose(dsk_fp);
                    fclose(imd_fp);
                    return -1;
                }
                
                if (fwrite(sector_data, SECTOR_SIZE, 1, imd_fp) != 1) {
                    fprintf(stderr, "Error writing sector data\n");
                    fclose(dsk_fp);
                    fclose(imd_fp);
                    return -1;
                }
            }
            
            total_sectors++;
        }
        
        tracks_written++;
    }
    
    fclose(dsk_fp);
    fclose(imd_fp);
    
    printf("Conversion completed successfully!\n");
    printf("Written %d tracks, %d sectors total\n", tracks_written, total_sectors);
    printf("Compressed %d sectors\n", compressed_sectors);
    
    return 0;
}

void print_usage(const char *program_name) {
    printf("Usage: %s <input.dsk> <output.imd>\n", program_name);
    printf("Convert DSK file to ImageDisk (IMD) format\n");
    printf("Optimized for MDOS disk images with 128-byte sectors\n");
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *dsk_filename = argv[1];
    const char *imd_filename = argv[2];
    
    printf("DSK to IMD Converter v1.0 (MDOS optimized)\n");
    printf("Input file: %s\n", dsk_filename);
    printf("Output file: %s\n", imd_filename);
    
    if (convert_dsk_to_imd(dsk_filename, imd_filename) == 0) {
        printf("Conversion successful!\n");
        return 0;
    } else {
        printf("Conversion failed!\n");
        return 1;
    }
}
