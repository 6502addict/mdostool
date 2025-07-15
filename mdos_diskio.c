/*
 * MDOS Filesystem Library - Disk I/O Module
 * Copyright (C) 2025
 * 
 * Low-level disk image access and filesystem management
 */

#include "mdos_fs.h"
#include "mdos_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Read a sector from disk */
void mdos_getsect(mdos_fs_t *fs, uint8_t *buf, int sect) {
    fseek(fs->disk, sect * MDOS_SECTOR_SIZE, SEEK_SET);
    fread(buf, MDOS_SECTOR_SIZE, 1, fs->disk);
}

/* Write a sector to disk */
void mdos_putsect(mdos_fs_t *fs, uint8_t *buf, int sect) {
    if (fs->read_only) return;
    fseek(fs->disk, sect * MDOS_SECTOR_SIZE, SEEK_SET);
    fwrite(buf, MDOS_SECTOR_SIZE, 1, fs->disk);
}

/* Allocate space and build RIB */
int mdos_alloc_space(mdos_fs_t *fs __attribute__((unused)), uint8_t *cat, mdos_rib_t *rib, int sects) {
    int req = (sects + 3) / 4; /* Number of clusters to allocate */
    int seg = 0; /* Segment pointer in RIB */
    
    memset(rib, 0, sizeof(mdos_rib_t));
    
    while (req > 0) {
        int start = -1;
        int found = 0;
        
        if (seg >= 114) {
            return MDOS_ENOSPC; /* Too many segments */
        }
        
        /* Find contiguous free clusters */
        for (int x = 0; x < MDOS_SECTOR_SIZE * 8; ++x) {
            if (!(cat[x >> 3] & (1 << (7 - (x & 7))))) {
                /* Found free cluster */
                if (start == -1) start = x;
                found++;
                if (found >= req || found >= 32) break; /* Max 32 clusters per segment */
            } else {
                /* Hit allocated cluster, reset */
                start = -1;
                found = 0;
            }
        }
        
        if (found == 0) {
            return MDOS_ENOSPC; /* No space left */
        }
        
        /* Mark clusters as allocated */
        for (int i = 0; i < found; i++) {
            cat[(start + i) >> 3] |= (1 << (7 - ((start + i) & 7)));
        }
        
        /* Add segment to RIB */
        int sdw = start + ((found - 1) << 10);
        rib->sdw[seg] = (sdw >> 8);
        rib->sdw[seg + 1] = sdw;
        seg += 2;
        
        req -= found;
    }
    
    /* Add end mark */
    int sdw = 0x8000 + sects - 1; /* Last logical sector */
    rib->sdw[seg] = (sdw >> 8);
    rib->sdw[seg + 1] = sdw;
    
    return MDOS_EOK;
}

/* Mount MDOS filesystem */
mdos_fs_t* mdos_mount(const char *disk_path, int read_only) {
    mdos_fs_t *fs = calloc(1, sizeof(mdos_fs_t));
    if (!fs) return NULL;
    
    fs->disk = fopen(disk_path, read_only ? "rb" : "r+b");
    if (!fs->disk) {
        free(fs);
        return NULL;
    }
    
    /* Use malloc + strcpy instead of strdup for portability */
    size_t path_len = strlen(disk_path);
    fs->disk_path = malloc(path_len + 1);
    if (!fs->disk_path) {
        fclose(fs->disk);
        free(fs);
        return NULL;
    }
    strcpy(fs->disk_path, disk_path);
    fs->read_only = read_only;
    
    return fs;
}

/* Unmount filesystem */
int mdos_unmount(mdos_fs_t *fs) {
    if (!fs) return MDOS_EINVAL;
    
    /* Close all open files */
    for (int i = 0; i < MDOS_MAX_OPEN_FILES; i++) {
        if (fs->open_files[i]) {
            mdos_close(fs, i);
        }
    }
    
    if (fs->disk) fclose(fs->disk);
    free(fs->disk_path);
    free(fs);
    return MDOS_EOK;
}

/* Sync filesystem */
int mdos_sync(mdos_fs_t *fs) {
    if (!fs || !fs->disk) return MDOS_EINVAL;
    return fflush(fs->disk) == 0 ? MDOS_EOK : MDOS_EIO;
}

/* Get free space */
int mdos_free_space(mdos_fs_t *fs) {
    if (!fs) return MDOS_EINVAL;
    
    uint8_t cat[MDOS_SECTOR_SIZE];
    mdos_getsect(fs, cat, MDOS_SECTOR_CAT);
    
    int total = 0;
    for (int x = 0; x < MDOS_SECTOR_SIZE * 8; x++) {
        if (!(cat[x >> 3] & (1 << (7 - (x & 7))))) {
            total++;
        }
    }
    return total * 4 * MDOS_SECTOR_SIZE; /* Return bytes */
}

/* Create a new MDOS filesystem */
int mdos_mkfs(const char *disk_path, int sides) {
    if (!disk_path || (sides != 1 && sides != 2)) {
        return MDOS_EINVAL;
    }
    
    /* MDOS disk parameters */
    const int sectors_per_track = 32;
    const int tracks_per_side = 77;
    const int total_sectors = sectors_per_track * tracks_per_side * sides;
    const int total_clusters = total_sectors / 4; /* 4 sectors per cluster */
    
    printf("Creating MDOS filesystem:\n");
    printf("  Disk: %s\n", disk_path);
    printf("  Sides: %d\n", sides);
    printf("  Tracks per side: %d\n", tracks_per_side);
    printf("  Sectors per track: %d\n", sectors_per_track);
    printf("  Total sectors: %d\n", total_sectors);
    printf("  Total clusters: %d\n", total_clusters);
    printf("  Disk size: %d bytes\n", total_sectors * MDOS_SECTOR_SIZE);
    
    /* Create the disk file */
    FILE *disk = fopen(disk_path, "wb");
    if (!disk) {
        return MDOS_EIO;
    }
    
    /* Initialize all sectors to zero */
    uint8_t zero_sector[MDOS_SECTOR_SIZE];
    memset(zero_sector, 0, MDOS_SECTOR_SIZE);
    
    for (int i = 0; i < total_sectors; i++) {
        if (fwrite(zero_sector, MDOS_SECTOR_SIZE, 1, disk) != 1) {
            fclose(disk);
            return MDOS_EIO;
        }
    }
    
    /* Sector 0: Disk ID sector */
    uint8_t id_sector[MDOS_SECTOR_SIZE];
    memset(id_sector, 0, MDOS_SECTOR_SIZE);
    
    /* MDOS disk identifier */
    strcpy((char*)id_sector, "MDOS");
    id_sector[4] = sides; /* Number of sides */
    id_sector[5] = tracks_per_side; /* Tracks per side */
    id_sector[6] = sectors_per_track; /* Sectors per track */
    
    fseek(disk, MDOS_SECTOR_ID * MDOS_SECTOR_SIZE, SEEK_SET);
    fwrite(id_sector, MDOS_SECTOR_SIZE, 1, disk);
    
    /* Sector 1: Cluster Allocation Table (CAT) */
    uint8_t cat_sector[MDOS_SECTOR_SIZE];
    memset(cat_sector, 0, MDOS_SECTOR_SIZE);
    
    /* Mark system clusters as allocated */
    /* Clusters 0-5 are reserved for system use */
    for (int i = 0; i < 6; i++) {
        cat_sector[i >> 3] |= (1 << (7 - (i & 7)));
    }
    
    fseek(disk, MDOS_SECTOR_CAT * MDOS_SECTOR_SIZE, SEEK_SET);
    fwrite(cat_sector, MDOS_SECTOR_SIZE, 1, disk);
    
    /* Sector 2: Logical CAT (LCAT) - Bad block bitmap */
    uint8_t lcat_sector[MDOS_SECTOR_SIZE];
    memset(lcat_sector, 0, MDOS_SECTOR_SIZE);
    /* All sectors good by default */
    
    fseek(disk, MDOS_SECTOR_LCAT * MDOS_SECTOR_SIZE, SEEK_SET);
    fwrite(lcat_sector, MDOS_SECTOR_SIZE, 1, disk);
    
    /* Sectors 3-22: Directory sectors */
    uint8_t dir_sector[MDOS_SECTOR_SIZE];
    memset(dir_sector, 0, MDOS_SECTOR_SIZE);
    
    for (int i = 0; i < MDOS_SECTOR_DIR_SIZE; i++) {
        fseek(disk, (MDOS_SECTOR_DIR + i) * MDOS_SECTOR_SIZE, SEEK_SET);
        fwrite(dir_sector, MDOS_SECTOR_SIZE, 1, disk);
    }
    
    /* Sectors 23-24: Boot sectors */
    uint8_t boot_sector[MDOS_SECTOR_SIZE];
    memset(boot_sector, 0, MDOS_SECTOR_SIZE);
    
    /* Simple boot message */
    strcpy((char*)boot_sector, "MDOS Boot Loader\r\nInsert system disk and press any key\r\n");
    
    fseek(disk, 23 * MDOS_SECTOR_SIZE, SEEK_SET);
    fwrite(boot_sector, MDOS_SECTOR_SIZE, 1, disk);
    
    memset(boot_sector, 0, MDOS_SECTOR_SIZE);
    fseek(disk, 24 * MDOS_SECTOR_SIZE, SEEK_SET);
    fwrite(boot_sector, MDOS_SECTOR_SIZE, 1, disk);
    
    fclose(disk);
    
    /* Calculate free space */
    int free_clusters = total_clusters - 6; /* Minus system clusters */
    int free_sectors = free_clusters * 4;
    int free_bytes = free_sectors * MDOS_SECTOR_SIZE;
    
    printf("\nFilesystem created successfully!\n");
    printf("Free space: %d clusters, %d sectors, %d bytes\n", 
           free_clusters, free_sectors, free_bytes);
    
    return MDOS_EOK;
}
