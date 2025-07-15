/*
 * MDOS Filesystem Library - Utility Functions
 * Copyright (C) 2025
 * 
 * RIB operations, filename handling, and directory utilities
 */

#include "mdos_fs.h"
#include "mdos_internal.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>  /* For strcasecmp */
#include <ctype.h>

/* Calculate file size in sectors from RIB */
int mdos_calc_sects(mdos_rib_t *rib) {
    int total = 0;
    for (size_t x = 0; x < sizeof(rib->sdw); x += 2) {
        int sdw = (rib->sdw[x] << 8) + rib->sdw[x + 1];
        if (sdw & 0x8000) {
            return (sdw & 0x7FFF) + 1;
        }
        total += ((sdw >> 10) & 0x1F) + 1;
    }
    return total * 4;
}

/* Convert logical sector number to physical sector number */
int mdos_lsn_to_psn(mdos_rib_t *rib, int lsn) {
    for (size_t x = 0; x < sizeof(rib->sdw); x += 2) {
        int sdw = (rib->sdw[x] << 8) + rib->sdw[x + 1];
        if (sdw & 0x8000) {
            return -1; /* End of file */
        }
        int clust = (sdw & 0x3FF);
        int sect = clust * 4;
        int siz = ((sdw >> 10) & 0x1F) + 1;
        int len = siz * 4;
        if (lsn < len) {
            return sect + lsn;
        }
        lsn -= len;
    }
    return -1;
}

/* Hash function for filenames - currently unused but kept for completeness */
int mdos_hash_filename(const char *name) {
    uint8_t filename[10];
    size_t x;
    unsigned b, a;
    
    /* Convert to MDOS format */
    memset(filename, ' ', 10);
    for (x = 0; x < 8 && name[x] && name[x] != '.'; x++) {
        filename[x] = (uint8_t)toupper((unsigned char)name[x]);
    }
    if (name[x] == '.') {
        x++;
        for (size_t i = 0; i < 2 && name[x]; i++, x++) {
            filename[8 + i] = (uint8_t)toupper((unsigned char)name[x]);
        }
    }
    
    /* MDOS hash algorithm */
    b = 0;
    for (x = 0; x < 10; x++) {
        a = filename[x];
        if (a >= 0x25) a -= 0x25;
        else a = 0;
        b = (b & 0xFF) + a + (b >> 8);
        b = (((b << 1) + (b >> 8)) & 0x1FF);
    }
    b = (((b >> 1) + (b << 8)) & 0x1FF);
    a = b;
    a = (((a >> 1) + (a << 8)) & 0x1FF);
    a = (((a >> 1) + (a << 8)) & 0x1FF);
    a = (((a >> 1) + (a << 8)) & 0x1FF);
    a = (((a >> 1) + (a << 8)) & 0x1FF);
    a = (a & 0xFF) + (b & 0xFF);
    b = a;
    b &= 0x1f;
    if (b >= 20) {
        b -= 20;
        if (b < 10) {
            b = (b << 1) + (a & 1);
        }
    }
    return (int)b;
}

/* Normalize filename to MDOS format */
int mdos_normalize_filename(const char *input, char *output) {
    size_t x = 0, ext_pos = 0;
    
    /* Find extension */
    for (size_t i = 0; input[i]; i++) {
        if (input[i] == '.') ext_pos = i;
    }
    
    /* Copy name part (max 8 chars) */
    for (size_t i = 0; i < 8 && input[i] && (ext_pos == 0 || i < ext_pos); i++) {
        if (!isalnum((unsigned char)input[i])) return MDOS_EINVAL;
        output[x++] = (char)toupper((unsigned char)input[i]);
    }
    
    if (x == 0) return MDOS_EINVAL;
    
    /* Add extension if present */
    if (ext_pos > 0) {
        output[x++] = '.';
        for (size_t i = ext_pos + 1, count = 0; input[i] && count < 2; i++, count++) {
            if (!isalnum((unsigned char)input[i])) return MDOS_EINVAL;
            output[x++] = (char)toupper((unsigned char)input[i]);
        }
    }
    
    output[x] = '\0';
    return MDOS_EOK;
}

/* Validate filename format */
int mdos_validate_filename(const char *filename) {
    if (!filename) return MDOS_EINVAL;
    
    char normalized[MDOS_MAX_FILENAME];
    return mdos_normalize_filename(filename, normalized);
}

/* Extract MDOS filename from local path */
int mdos_extract_filename(const char *local_path, char *mdos_name) {
    const char *start = local_path;
    const char *last_slash = NULL;
    
    /* Find last path separator */
    for (const char *p = local_path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            last_slash = p;
        }
    }
    
    /* Use filename part only */
    if (last_slash) {
        start = last_slash + 1;
    }
    
    /* Validate and convert filename */
    size_t name_len = 0;
    size_t ext_len = 0;
    const char *dot = NULL;
    
    /* Find extension */
    for (const char *p = start; *p; p++) {
        if (*p == '.') dot = p;
    }
    
    if (dot) {
        name_len = dot - start;
        ext_len = strlen(dot + 1);
    } else {
        name_len = strlen(start);
        ext_len = 0;
    }
    
    /* Validate lengths */
    if (name_len == 0 || name_len > 8) {
        return MDOS_EINVAL; /* Name must be 1-8 characters */
    }
    
    if (ext_len > 2) {
        return MDOS_EINVAL; /* Extension must be 0-2 characters */
    }
    
    /* Build MDOS name */
    size_t pos = 0;
    for (size_t i = 0; i < name_len; i++) {
        char c = start[i];
        if (!isalnum((unsigned char)c)) {
            return MDOS_EINVAL; /* Must contain only letters and numbers */
        }
        mdos_name[pos++] = (char)tolower((unsigned char)c);
    }
    
    if (ext_len > 0) {
        mdos_name[pos++] = '.';
        for (size_t i = 0; i < ext_len; i++) {
            char c = dot[1 + i];
            if (!isalnum((unsigned char)c)) {
                return MDOS_EINVAL; /* Extension must contain only letters and numbers */
            }
            mdos_name[pos++] = (char)tolower((unsigned char)c);
        }
    } else {
        /* Default extension if none provided */
        strcpy(mdos_name + pos, ".sa");
        pos += 3;
    }
    
    mdos_name[pos] = '\0';
    return MDOS_EOK;
}

/* Error string lookup */
const char* mdos_strerror(int error) {
    switch (error) {
        case MDOS_EOK: return "Success";
        case MDOS_ENOENT: return "File not found";
        case MDOS_ENOSPC: return "No space left on device";
        case MDOS_EMFILE: return "Too many open files";
        case MDOS_EBADF: return "Bad file descriptor";
        case MDOS_EINVAL: return "Invalid argument";
        case MDOS_EIO: return "I/O error";
        case MDOS_EEXIST: return "File exists";
        case MDOS_EPERM: return "Operation not permitted";
        default: return "Unknown error";
    }
}

/* Find file in directory */
int mdos_find_file(mdos_fs_t *fs, const char *filename, int *type, int delete_entry) {
    uint8_t buf[MDOS_SECTOR_SIZE];
    char normalized[MDOS_MAX_FILENAME];
    
    if (mdos_normalize_filename(filename, normalized) != MDOS_EOK) {
        return -1;
    }
    
    for (int sect = MDOS_SECTOR_DIR; sect < MDOS_SECTOR_DIR + MDOS_SECTOR_DIR_SIZE; sect++) {
        mdos_getsect(fs, buf, sect);
        for (int y = 0; y < MDOS_SECTOR_SIZE; y += sizeof(mdos_dirent_t)) {
            mdos_dirent_t *d = (mdos_dirent_t *)(buf + y);
            if (d->name[0] && d->name[0] != 0xFF) {
                char name[MDOS_MAX_FILENAME];
                int p = 0;
                
                /* Build filename from directory entry */
                for (int i = 0; i < 8; i++) {
                    if (d->name[i] != ' ') {
                        name[p++] = (char)tolower((unsigned char)d->name[i]);
                    }
                }
                name[p++] = '.';
                for (int i = 0; i < 2; i++) {
                    if (d->suffix[i] != ' ') {
                        name[p++] = (char)tolower((unsigned char)d->suffix[i]);
                    }
                }
                name[p] = '\0';
                
                if (strcasecmp(name, filename) == 0) {
                    if (type) *type = (d->attr_high & 7);
                    if (delete_entry) {
                        d->name[0] = 0xFF;
                        d->name[1] = 0xFF;
                        mdos_putsect(fs, buf, sect);
                    }
                    return (d->sector_high << 8) + d->sector_low;
                }
            }
        }
    }
    return -1;
}

/* Write directory entry */
int mdos_write_directory_entry(mdos_fs_t *fs, const char *filename, int rib_sector, int file_type) {
    char name_part[9];
    char ext_part[3];
    
    /* Parse filename */
    const char *dot = strchr(filename, '.');
    if (dot) {
        size_t name_len = dot - filename;
        if (name_len > 8) name_len = 8;
        strncpy(name_part, filename, name_len);
        name_part[name_len] = '\0';
        
        size_t ext_len = strlen(dot + 1);
        if (ext_len > 2) ext_len = 2;
        /* Fix the strncpy warning by ensuring null termination */
        memcpy(ext_part, dot + 1, ext_len);
        ext_part[ext_len] = '\0';
    } else {
        strncpy(name_part, filename, 8);
        name_part[8] = '\0';
        strcpy(ext_part, "");
    }
    
    /* Create directory entry */
    mdos_dirent_t entry;
    memset(&entry, 0, sizeof(entry));
    
    /* Fill name (space-padded) */
    memset(entry.name, ' ', 8);
    for (int i = 0; i < 8 && name_part[i]; i++) {
        entry.name[i] = (uint8_t)toupper((unsigned char)name_part[i]);
    }
    
    /* Fill extension (space-padded) */
    memset(entry.suffix, ' ', 2);
    for (int i = 0; i < 2 && ext_part[i]; i++) {
        entry.suffix[i] = (uint8_t)toupper((unsigned char)ext_part[i]);
    }
    
    entry.sector_high = (rib_sector >> 8);
    entry.sector_low = rib_sector & 0xFF;
    entry.attr_high = file_type;
    entry.attr_low = 0;
    
    /* Find empty directory slot */
    uint8_t dir_buf[MDOS_SECTOR_SIZE];
    for (int sect = MDOS_SECTOR_DIR; sect < MDOS_SECTOR_DIR + MDOS_SECTOR_DIR_SIZE; sect++) {
        mdos_getsect(fs, dir_buf, sect);
        for (int y = 0; y < MDOS_SECTOR_SIZE; y += sizeof(mdos_dirent_t)) {
            mdos_dirent_t *d = (mdos_dirent_t *)(dir_buf + y);
            if (d->name[0] == 0x00 || d->name[0] == 0xFF) {
                /* Found empty slot */
                memcpy(d, &entry, sizeof(mdos_dirent_t));
                mdos_putsect(fs, dir_buf, sect);
                return MDOS_EOK;
            }
        }
    }
    
    return MDOS_ENOSPC; /* Directory full */
}
