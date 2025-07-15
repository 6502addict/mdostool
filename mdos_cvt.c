/*
 * MDOS Filesystem Library - Image Conversion Module
 * Copyright (C) 2025
 * 
 * Functions for converting between IMD and DSK disk image formats
 */

#include "mdos_fs.h"
#include "mdos_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_TRACKS 77
#define MAX_SECTORS_PER_TRACK 26
#define MDOS_SECTOR_SIZE 128

// IMD track header structure
typedef struct {
    uint8_t mode;          // Recording mode
    uint8_t cylinder;      // Track number
    uint8_t head;          // Head number + flags
    uint8_t sector_count;  // Sectors in this track
    uint8_t sector_size;   // Sector size code
} imd_track_header_t;

/* Read IMD comment section */
static int read_imd_comment(FILE *fp, char *comment, size_t max_len) {
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
        return -1;
    }
    
    return (int)len;
}

/* Write IMD comment header */
static int write_imd_comment(FILE *fp, const char *dsk_filename) {
    char comment[512];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    snprintf(comment, sizeof(comment), 
             "IMD file created from DSK: %s\r\n"
             "Created by MDOS library on %04d-%02d-%02d %02d:%02d:%02d\r\n"
             "MDOS format: 128-byte sectors, up to 26 sectors per track\r\n",
             dsk_filename,
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    // Write comment
    if (fputs(comment, fp) == EOF) {
        return MDOS_EIO;
    }
    
    // Write 0x1A marker to end comment
    if (fputc(0x1A, fp) == EOF) {
        return MDOS_EIO;
    }
    
    return MDOS_EOK;
}

/* Check if a sector is empty (all zeros) */
static bool is_sector_empty(const uint8_t *sector_data) {
    for (int i = 0; i < MDOS_SECTOR_SIZE; i++) {
        if (sector_data[i] != 0) {
            return false;
        }
    }
    return true;
}

/* Check if a sector is filled with the same byte */
static bool is_sector_compressed(const uint8_t *sector_data, uint8_t *fill_byte) {
    *fill_byte = sector_data[0];
    for (int i = 1; i < MDOS_SECTOR_SIZE; i++) {
        if (sector_data[i] != *fill_byte) {
            return false;
        }
    }
    return true;
}

/* Find the last track with data in DSK file */
static int find_last_track_with_data(FILE *dsk_fp) {
    uint8_t sector_data[MDOS_SECTOR_SIZE];
    int last_track = -1;
    
    // Save current position
    long current_pos = ftell(dsk_fp);
    
    // Check each track
    for (int track = 0; track < MAX_TRACKS; track++) {
        bool track_has_data = false;
        
        for (int sector = 0; sector < MAX_SECTORS_PER_TRACK; sector++) {
            long sector_pos = (long)track * MAX_SECTORS_PER_TRACK * MDOS_SECTOR_SIZE + 
                             (long)sector * MDOS_SECTOR_SIZE;
            
            if (fseek(dsk_fp, sector_pos, SEEK_SET) != 0) {
                break;
            }
            
            if (fread(sector_data, MDOS_SECTOR_SIZE, 1, dsk_fp) != 1) {
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

/* Convert IMD file to DSK format */
int mdos_convert_imd_to_dsk(const char *imd_filename, const char *dsk_filename) {
    FILE *imd_fp, *dsk_fp;
    char comment[1024];
    imd_track_header_t header;
    uint8_t sector_data[MDOS_SECTOR_SIZE];
    
    // Track and sector storage
    uint8_t disk_sectors[MAX_TRACKS][MAX_SECTORS_PER_TRACK][MDOS_SECTOR_SIZE];
    bool sector_valid[MAX_TRACKS][MAX_SECTORS_PER_TRACK];
    
    // Initialize
    memset(sector_valid, false, sizeof(sector_valid));
    memset(disk_sectors, 0, sizeof(disk_sectors));
    
    // Open input IMD file
    imd_fp = fopen(imd_filename, "rb");
    if (!imd_fp) {
        return MDOS_EIO;
    }
    
    // Read IMD comment
    read_imd_comment(imd_fp, comment, sizeof(comment));
    
    int tracks_parsed = 0;
    int total_sectors = 0;
    int valid_sectors = 0;
    
    // Parse tracks
    while (tracks_parsed < 200) { // Safety limit
        if (fread(&header, sizeof(header), 1, imd_fp) != 1) {
            break; // End of file
        }
        
        int track_num = header.cylinder;
        
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
        if (fread(sector_map, header.sector_count, 1, imd_fp) != 1) {
            fclose(imd_fp);
            return MDOS_EIO;
        }
        
        // Read optional cylinder map if present
        if (header.head & 0x80) {
            uint8_t cylinder_map[header.sector_count];
            if (fread(cylinder_map, header.sector_count, 1, imd_fp) != 1) {
                fclose(imd_fp);
                return MDOS_EIO;
            }
        }
        
        // Read optional head map if present
        if (header.head & 0x40) {
            uint8_t head_map[header.sector_count];
            if (fread(head_map, header.sector_count, 1, imd_fp) != 1) {
                fclose(imd_fp);
                return MDOS_EIO;
            }
        }
        
        // Read sector data
        for (int s = 0; s < header.sector_count; s++) {
            int sector_type_int = fgetc(imd_fp);
            if (sector_type_int == EOF) {
                fclose(imd_fp);
                return MDOS_EIO;
            }
            uint8_t sector_type = (uint8_t)sector_type_int;
            
            int sector_num = sector_map[s];
            
            // Handle sector types
            switch (sector_type) {
                case 0: // Data unavailable
                    memset(sector_data, 0, MDOS_SECTOR_SIZE);
                    break;
                    
                case 1: // Normal data
                    if (fread(sector_data, MDOS_SECTOR_SIZE, 1, imd_fp) != 1) {
                        fclose(imd_fp);
                        return MDOS_EIO;
                    }
                    break;
                    
                case 2: // Compressed - all same byte
                    {
                        int fill_byte = fgetc(imd_fp);
                        if (fill_byte == EOF) {
                            fclose(imd_fp);
                            return MDOS_EIO;
                        }
                        memset(sector_data, fill_byte, MDOS_SECTOR_SIZE);
                    }
                    break;
                    
                default:
                    // Handle other types as normal data
                    if (fread(sector_data, MDOS_SECTOR_SIZE, 1, imd_fp) != 1) {
                        fclose(imd_fp);
                        return MDOS_EIO;
                    }
                    break;
            }
            
            // Convert from 1-based to 0-based sector numbering
            int mdos_sector = sector_num - 1;  // Convert 1-26 to 0-25
            
            if (mdos_sector >= 0 && mdos_sector < MAX_SECTORS_PER_TRACK) {
                memcpy(disk_sectors[track_num][mdos_sector], sector_data, MDOS_SECTOR_SIZE);
                sector_valid[track_num][mdos_sector] = true;
                valid_sectors++;
            }
            total_sectors++;
        }
        
        tracks_parsed++;
    }
    
    fclose(imd_fp);
    
    // Open output DSK file
    dsk_fp = fopen(dsk_filename, "wb");
    if (!dsk_fp) {
        return MDOS_EIO;
    }
    
    // Write sectors in sequential order
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
            for (int sector = 0; sector < MAX_SECTORS_PER_TRACK; sector++) {
                if (sector_valid[track][sector]) {
                    if (fwrite(disk_sectors[track][sector], MDOS_SECTOR_SIZE, 1, dsk_fp) != 1) {
                        fclose(dsk_fp);
                        return MDOS_EIO;
                    }
                    written_sectors++;
                } else {
                    // Write empty sector
                    uint8_t empty_sector[MDOS_SECTOR_SIZE];
                    memset(empty_sector, 0, MDOS_SECTOR_SIZE);
                    if (fwrite(empty_sector, MDOS_SECTOR_SIZE, 1, dsk_fp) != 1) {
                        fclose(dsk_fp);
                        return MDOS_EIO;
                    }
                }
            }
        }
    }
    
    fclose(dsk_fp);
    return MDOS_EOK;
}

/* Convert DSK file to IMD format */
int mdos_convert_dsk_to_imd(const char *dsk_filename, const char *imd_filename) {
    FILE *dsk_fp, *imd_fp;
    uint8_t sector_data[MDOS_SECTOR_SIZE];
    
    // Open input DSK file
    dsk_fp = fopen(dsk_filename, "rb");
    if (!dsk_fp) {
        return MDOS_EIO;
    }
    
    // Find the last track with data
    int last_track = find_last_track_with_data(dsk_fp);
    if (last_track < 0) {
        fclose(dsk_fp);
        return MDOS_ENOENT;
    }
    
    // Open output IMD file
    imd_fp = fopen(imd_filename, "wb");
    if (!imd_fp) {
        fclose(dsk_fp);
        return MDOS_EIO;
    }
    
    // Write IMD comment header
    if (write_imd_comment(imd_fp, dsk_filename) != MDOS_EOK) {
        fclose(dsk_fp);
        fclose(imd_fp);
        return MDOS_EIO;
    }
    
    int tracks_written = 0;
    int total_sectors = 0;
    
    // Process each track up to the last one with data
    for (int track = 0; track <= last_track; track++) {
        // Count non-empty sectors in this track
        int valid_sectors = 0;
        for (int sector = 0; sector < MAX_SECTORS_PER_TRACK; sector++) {
            long sector_pos = (long)track * MAX_SECTORS_PER_TRACK * MDOS_SECTOR_SIZE + 
                             (long)sector * MDOS_SECTOR_SIZE;
            
            if (fseek(dsk_fp, sector_pos, SEEK_SET) != 0) {
                break;
            }
            
            if (fread(sector_data, MDOS_SECTOR_SIZE, 1, dsk_fp) != 1) {
                break;
            }
            
            if (!is_sector_empty(sector_data)) {
                valid_sectors = MAX_SECTORS_PER_TRACK; // Include all sectors if any has data
                break;
            }
        }
        
        if (valid_sectors == 0) {
            continue;
        }
        
        // Write IMD track header
        imd_track_header_t header;
        header.mode = 0x00;        // FM mode
        header.cylinder = track;   // Track number
        header.head = 0x00;        // Head 0, no optional maps
        header.sector_count = MAX_SECTORS_PER_TRACK;
        header.sector_size = 0x00; // 128 bytes (sector size code 0)
        
        if (fwrite(&header, sizeof(header), 1, imd_fp) != 1) {
            fclose(dsk_fp);
            fclose(imd_fp);
            return MDOS_EIO;
        }
        
        // Write sector number map (1-based numbering for MDOS)
        uint8_t sector_map[MAX_SECTORS_PER_TRACK];
        for (int i = 0; i < MAX_SECTORS_PER_TRACK; i++) {
            sector_map[i] = i + 1; // 1-based sector numbering
        }
        
        if (fwrite(sector_map, MAX_SECTORS_PER_TRACK, 1, imd_fp) != 1) {
            fclose(dsk_fp);
            fclose(imd_fp);
            return MDOS_EIO;
        }
        
        // Write sector data with compression
        for (int sector = 0; sector < MAX_SECTORS_PER_TRACK; sector++) {
            long sector_pos = (long)track * MAX_SECTORS_PER_TRACK * MDOS_SECTOR_SIZE + 
                             (long)sector * MDOS_SECTOR_SIZE;
            
            if (fseek(dsk_fp, sector_pos, SEEK_SET) != 0) {
                fclose(dsk_fp);
                fclose(imd_fp);
                return MDOS_EIO;
            }
            
            if (fread(sector_data, MDOS_SECTOR_SIZE, 1, dsk_fp) != 1) {
                fclose(dsk_fp);
                fclose(imd_fp);
                return MDOS_EIO;
            }
            
            uint8_t fill_byte;
            
            // Check if sector can be compressed
            if (is_sector_compressed(sector_data, &fill_byte)) {
                // Write compressed sector
                if (fputc(2, imd_fp) == EOF) { // Type 2: compressed
                    fclose(dsk_fp);
                    fclose(imd_fp);
                    return MDOS_EIO;
                }
                
                if (fputc(fill_byte, imd_fp) == EOF) {
                    fclose(dsk_fp);
                    fclose(imd_fp);
                    return MDOS_EIO;
                }
            } else {
                // Write normal sector
                if (fputc(1, imd_fp) == EOF) { // Type 1: normal data
                    fclose(dsk_fp);
                    fclose(imd_fp);
                    return MDOS_EIO;
                }
                
                if (fwrite(sector_data, MDOS_SECTOR_SIZE, 1, imd_fp) != 1) {
                    fclose(dsk_fp);
                    fclose(imd_fp);
                    return MDOS_EIO;
                }
            }
            
            total_sectors++;
        }
        
        tracks_written++;
    }
    
    fclose(dsk_fp);
    fclose(imd_fp);
    
    return MDOS_EOK;
}
