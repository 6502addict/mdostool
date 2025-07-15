/*
 * MDOS Filesystem Library - Directory Operations
 * Copyright (C) 2025
 * 
 * Directory listing, file status, and file deletion
 */

#include "mdos_fs.h"
#include "mdos_internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Read directory */
int mdos_readdir(mdos_fs_t *fs, mdos_file_info_t **files, int *count) {
    if (!fs || !files || !count) return MDOS_EINVAL;
    
    mdos_file_info_t *file_list = NULL;
    int file_count = 0;
    int capacity = 64; /* Initial capacity */
    
    file_list = malloc(capacity * sizeof(mdos_file_info_t));
    if (!file_list) return MDOS_ENOSPC;
    
    uint8_t buf[MDOS_SECTOR_SIZE];
    uint8_t rib_buf[MDOS_SECTOR_SIZE];
    
    for (int sect = MDOS_SECTOR_DIR; sect < MDOS_SECTOR_DIR + MDOS_SECTOR_DIR_SIZE; sect++) {
        mdos_getsect(fs, buf, sect);
        for (int y = 0; y < MDOS_SECTOR_SIZE; y += sizeof(mdos_dirent_t)) {
            mdos_dirent_t *d = (mdos_dirent_t *)(buf + y);
            if (d->name[0] && d->name[0] != 0xFF) {
                /* Expand array if needed */
                if (file_count >= capacity) {
                    capacity *= 2;
                    mdos_file_info_t *new_list = realloc(file_list, capacity * sizeof(mdos_file_info_t));
                    if (!new_list) {
                        free(file_list);
                        return MDOS_ENOSPC;
                    }
                    file_list = new_list;
                }
                
                mdos_file_info_t *info = &file_list[file_count];
                
                /* Build filename */
                int p = 0;
                for (int i = 0; i < 8 && d->name[i] != ' '; i++) {
                    info->name[p++] = (char)tolower((unsigned char)d->name[i]);
                }
                info->name[p++] = '.';
                for (int i = 0; i < 2 && d->suffix[i] != ' '; i++) {
                    info->name[p++] = (char)tolower((unsigned char)d->suffix[i]);
                }
                info->name[p] = '\0';
                
                info->type = (d->attr_high & 7);
                info->attributes = d->attr_high;
                info->rib_sector = (d->sector_high << 8) + d->sector_low;
                
                /* Load RIB to get size information */
                mdos_getsect(fs, rib_buf, info->rib_sector);
                mdos_rib_t *rib = (mdos_rib_t *)rib_buf;
                
                info->sectors = mdos_calc_sects(rib);
                info->load_addr = (rib->addr_high << 8) + rib->addr_low;
                info->start_addr = (rib->pc_high << 8) + rib->pc_low;
                
                if (info->type == MDOS_TYPE_IMAGE) {
                    int rib_size = (rib->size_high << 8) + rib->size_low;
                    info->size = (rib_size - 1) * MDOS_SECTOR_SIZE + rib->last_size;
                } else {
                    info->size = info->sectors * MDOS_SECTOR_SIZE;
                }
                
                file_count++;
            }
        }
    }
    
    *files = file_list;
    *count = file_count;
    return MDOS_EOK;
}

/* Get file status */
int mdos_stat(mdos_fs_t *fs, const char *filename, mdos_file_info_t *info) {
    if (!fs || !filename || !info) return MDOS_EINVAL;
    
    int type;
    int rib_sector = mdos_find_file(fs, filename, &type, 0);
    if (rib_sector == -1) return MDOS_ENOENT;
    
    /* Load directory entry to get attributes */
    uint8_t buf[MDOS_SECTOR_SIZE];
    for (int sect = MDOS_SECTOR_DIR; sect < MDOS_SECTOR_DIR + MDOS_SECTOR_DIR_SIZE; sect++) {
        mdos_getsect(fs, buf, sect);
        for (int y = 0; y < MDOS_SECTOR_SIZE; y += sizeof(mdos_dirent_t)) {
            mdos_dirent_t *d = (mdos_dirent_t *)(buf + y);
            if (d->name[0] && d->name[0] != 0xFF) {
                int entry_rib = (d->sector_high << 8) + d->sector_low;
                if (entry_rib == rib_sector) {
                    /* Found the entry */
                    strncpy(info->name, filename, MDOS_MAX_FILENAME - 1);
                    info->name[MDOS_MAX_FILENAME - 1] = '\0';
                    info->type = type;
                    info->attributes = d->attr_high;
                    info->rib_sector = rib_sector;
                    
                    /* Load RIB */
                    uint8_t rib_buf[MDOS_SECTOR_SIZE];
                    mdos_getsect(fs, rib_buf, rib_sector);
                    mdos_rib_t *rib = (mdos_rib_t *)rib_buf;
                    
                    info->sectors = mdos_calc_sects(rib);
                    info->load_addr = (rib->addr_high << 8) + rib->addr_low;
                    info->start_addr = (rib->pc_high << 8) + rib->pc_low;
                    
                    if (info->type == MDOS_TYPE_IMAGE) {
                        int rib_size = (rib->size_high << 8) + rib->size_low;
                        info->size = (rib_size - 1) * MDOS_SECTOR_SIZE + rib->last_size;
                    } else {
                        info->size = info->sectors * MDOS_SECTOR_SIZE;
                    }
                    
                    return MDOS_EOK;
                }
            }
        }
    }
    
    return MDOS_ENOENT;
}

/* Delete file */
int mdos_unlink(mdos_fs_t *fs, const char *filename) {
    if (!fs || !filename) return MDOS_EINVAL;
    if (fs->read_only) return MDOS_EPERM;
    
    int type;
    int rib_sector = mdos_find_file(fs, filename, &type, 1); /* Delete directory entry */
    if (rib_sector == -1) return MDOS_ENOENT;
    
    /* Free allocated clusters */
    uint8_t rib_buf[MDOS_SECTOR_SIZE];
    uint8_t cat[MDOS_SECTOR_SIZE];
    
    mdos_getsect(fs, rib_buf, rib_sector);
    mdos_getsect(fs, cat, MDOS_SECTOR_CAT);
    
    mdos_rib_t *rib = (mdos_rib_t *)rib_buf;
    
    for (size_t x = 0; x < sizeof(rib->sdw); x += 2) {
        int sdw = (rib->sdw[x] << 8) + rib->sdw[x + 1];
        if (sdw & 0x8000) {
            break;
        }
        int clust = (sdw & 0x3FF);
        int siz = ((sdw >> 10) & 0x1F) + 1;
        
        /* Mark clusters as free */
        for (int i = 0; i < siz; i++) {
            int cluster_idx = clust + i;
            cat[cluster_idx >> 3] &= (uint8_t)~(1 << (7 - (cluster_idx & 7)));
        }
    }
    
    mdos_putsect(fs, cat, MDOS_SECTOR_CAT);
    return MDOS_EOK;
}
