/*
 * MDOS Filesystem Library - File Operations
 * Copyright (C) 2025
 * 
 * File I/O operations: open, close, read, write, seek
 */

#include "mdos_fs.h"
#include "mdos_internal.h"
#include <stdlib.h>
#include <string.h>

/* Get file handle from file descriptor */
mdos_file_t* mdos_get_file_handle(mdos_fs_t *fs, int fd) {
    if (fd < 0 || fd >= MDOS_MAX_OPEN_FILES) return NULL;
    return fs->open_files[fd];
}

/* Allocate a new file descriptor */
int mdos_allocate_fd(mdos_fs_t *fs) {
    for (int i = 0; i < MDOS_MAX_OPEN_FILES; i++) {
        if (fs->open_files[i] == NULL) {
            return i;
        }
    }
    return -1;
}

/* Free a file descriptor */
void mdos_free_fd(mdos_fs_t *fs, int fd) {
    if (fd >= 0 && fd < MDOS_MAX_OPEN_FILES && fs->open_files[fd]) {
        free(fs->open_files[fd]);
        fs->open_files[fd] = NULL;
    }
}

/* Open file */
int mdos_open(mdos_fs_t *fs, const char *filename, int flags, int type __attribute__((unused))) {
    if (!fs || !filename) return MDOS_EINVAL;
    
    int fd = mdos_allocate_fd(fs);
    if (fd == -1) return MDOS_EMFILE;
    
    mdos_file_t *file = calloc(1, sizeof(mdos_file_t));
    if (!file) return MDOS_EMFILE;
    
    int file_type;
    int rib_sector = mdos_find_file(fs, filename, &file_type, 0);
    
    if (rib_sector == -1) {
        if (!(flags & MDOS_O_CREAT)) {
            free(file);
            return MDOS_ENOENT;
        }
        /* TODO: Create new file */
        free(file);
        return MDOS_ENOSPC; /* Not implemented yet */
    }
    
    /* Load RIB */
    uint8_t rib_buf[MDOS_SECTOR_SIZE];
    mdos_getsect(fs, rib_buf, rib_sector);
    memcpy(&file->rib, rib_buf, sizeof(mdos_rib_t));
    
    file->fs = fs;
    file->fd = fd;
    strncpy(file->name, filename, MDOS_MAX_FILENAME - 1);
    file->name[MDOS_MAX_FILENAME - 1] = '\0';
    file->flags = flags;
    file->type = file_type;
    file->rib_sector = rib_sector;
    file->sectors = mdos_calc_sects(&file->rib);
    file->last_size = file->rib.last_size;
    
    if (file->type == MDOS_TYPE_IMAGE) {
        file->file_size = (file->rib.size_high << 8) | file->rib.size_low;
        file->file_size = (file->file_size - 1) * MDOS_SECTOR_SIZE + file->last_size;
    } else {
        file->file_size = file->sectors * MDOS_SECTOR_SIZE;
    }
    
    file->position = 0;
    
    fs->open_files[fd] = file;
    return fd;
}

/* Close file */
int mdos_close(mdos_fs_t *fs, int fd) {
    if (!fs) return MDOS_EINVAL;
    
    mdos_file_t *file = mdos_get_file_handle(fs, fd);
    if (!file) return MDOS_EBADF;
    
    /* TODO: Flush any pending writes */
    
    mdos_free_fd(fs, fd);
    return MDOS_EOK;
}

/* Read from file */
ssize_t mdos_read(mdos_fs_t *fs, int fd, void *buf, size_t count) {
    if (!fs || !buf) return MDOS_EINVAL;
    
    mdos_file_t *file = mdos_get_file_handle(fs, fd);
    if (!file) return MDOS_EBADF;
    
    if (!(file->flags & MDOS_O_RDONLY) && !(file->flags & MDOS_O_RDWR)) {
        return MDOS_EPERM;
    }
    
    if (file->position >= file->file_size) {
        return 0; /* EOF */
    }
    
    size_t bytes_read = 0;
    uint8_t *output = (uint8_t *)buf;
    
    while (bytes_read < count && file->position < file->file_size) {
        int sector_offset = file->position / MDOS_SECTOR_SIZE;
        int byte_offset = file->position % MDOS_SECTOR_SIZE;
        
        /* Skip RIB sector (logical sector 0) */
        int physical_sector = mdos_lsn_to_psn(&file->rib, sector_offset + 1);
        if (physical_sector == -1) break;
        
        uint8_t sector_buf[MDOS_SECTOR_SIZE];
        mdos_getsect(fs, sector_buf, physical_sector);
        
        size_t bytes_in_sector = MDOS_SECTOR_SIZE - byte_offset;
        size_t bytes_remaining = count - bytes_read;
        size_t bytes_to_copy = (bytes_in_sector < bytes_remaining) ? bytes_in_sector : bytes_remaining;
        
        /* Don't read past end of file */
        if (file->position + (int)bytes_to_copy > file->file_size) {
            bytes_to_copy = (size_t)(file->file_size - file->position);
        }
        
        /* Handle ASCII file conversion for type 5 files */
        if (file->type == MDOS_TYPE_ASCII) {
            /* Convert MDOS ASCII format */
            for (size_t i = 0; i < bytes_to_copy && bytes_read < count; i++) {
                uint8_t c = sector_buf[byte_offset + i];
                
                if (c & 0x80) {
                    /* Compressed spaces - expand them */
                    int space_count = c & 0x7F;
                    for (int j = 0; j < space_count && bytes_read < count; j++) {
                        output[bytes_read++] = ' ';
                    }
                } else if (c == 13) {
                    /* CR - convert to Unix LF */
                    output[bytes_read++] = '\n';
                } else if (c == 10) {
                    /* LF - skip it (MDOS uses CR for line endings) */
                    /* Don't increment bytes_read */
                } else if (c == 0) {
                    /* NUL - skip it */
                    /* Don't increment bytes_read */
                } else {
                    /* Regular character */
                    output[bytes_read++] = c;
                }
                
                file->position++;
                if (file->position >= file->file_size) break;
            }
        } else {
            /* Binary file - copy as-is */
            memcpy(output + bytes_read, sector_buf + byte_offset, bytes_to_copy);
            bytes_read += bytes_to_copy;
            file->position += (int)bytes_to_copy;
        }
        
        if (file->position >= file->file_size) break;
    }
    
    return (ssize_t)bytes_read;
}

/* Read from file without any conversion (raw binary data) */
ssize_t mdos_read_raw(mdos_fs_t *fs, int fd, void *buf, size_t count) {
    if (!fs || !buf) return MDOS_EINVAL;
    
    mdos_file_t *file = mdos_get_file_handle(fs, fd);
    if (!file) return MDOS_EBADF;
    
    if (!(file->flags & MDOS_O_RDONLY) && !(file->flags & MDOS_O_RDWR)) {
        return MDOS_EPERM;
    }
    
    if (file->position >= file->file_size) {
        return 0; /* EOF */
    }
    
    size_t bytes_read = 0;
    uint8_t *output = (uint8_t *)buf;
    
    while (bytes_read < count && file->position < file->file_size) {
        int sector_offset = file->position / MDOS_SECTOR_SIZE;
        int byte_offset = file->position % MDOS_SECTOR_SIZE;
        
        /* Skip RIB sector (logical sector 0) */
        int physical_sector = mdos_lsn_to_psn(&file->rib, sector_offset + 1);
        if (physical_sector == -1) break;
        
        uint8_t sector_buf[MDOS_SECTOR_SIZE];
        mdos_getsect(fs, sector_buf, physical_sector);
        
        size_t bytes_in_sector = MDOS_SECTOR_SIZE - byte_offset;
        size_t bytes_remaining = count - bytes_read;
        size_t bytes_to_copy = (bytes_in_sector < bytes_remaining) ? bytes_in_sector : bytes_remaining;
        
        /* Don't read past end of file */
        if (file->position + (int)bytes_to_copy > file->file_size) {
            bytes_to_copy = (size_t)(file->file_size - file->position);
        }
        
        /* Always copy raw data without conversion */
        memcpy(output + bytes_read, sector_buf + byte_offset, bytes_to_copy);
        bytes_read += bytes_to_copy;
        file->position += (int)bytes_to_copy;
    }
    
    return (ssize_t)bytes_read;
}

/* Write to file */
ssize_t mdos_write(mdos_fs_t *fs, int fd, const void *buf __attribute__((unused)), size_t count __attribute__((unused))) {
    if (!fs) return MDOS_EINVAL;
    
    mdos_file_t *file = mdos_get_file_handle(fs, fd);
    if (!file) return MDOS_EBADF;
    
    if (fs->read_only) return MDOS_EPERM;
    
    if (!(file->flags & MDOS_O_WRONLY) && !(file->flags & MDOS_O_RDWR)) {
        return MDOS_EPERM;
    }
    
    /* TODO: Implement write functionality */
    /* This requires more complex logic for extending files, allocating new sectors, etc. */
    return MDOS_ENOSPC; /* Not implemented yet */
}

/* Seek in file */
off_t mdos_lseek(mdos_fs_t *fs, int fd, off_t offset, int whence) {
    if (!fs) return MDOS_EINVAL;
    
    mdos_file_t *file = mdos_get_file_handle(fs, fd);
    if (!file) return MDOS_EBADF;
    
    off_t new_pos;
    
    switch (whence) {
        case MDOS_SEEK_SET:
            new_pos = offset;
            break;
        case MDOS_SEEK_CUR:
            new_pos = file->position + offset;
            break;
        case MDOS_SEEK_END:
            new_pos = file->file_size + offset;
            break;
        default:
            return MDOS_EINVAL;
    }
    
    if (new_pos < 0) return MDOS_EINVAL;
    
    file->position = new_pos;
    return new_pos;
}

/* Create a new file */
int mdos_create_file(mdos_fs_t *fs, const char *filename, int file_type, const void *data, size_t size) {
    if (!fs || !filename || !data) return MDOS_EINVAL;
    if (fs->read_only) return MDOS_EPERM;
    
    /* Check if file already exists */
    int existing_type;
    if (mdos_find_file(fs, filename, &existing_type, 0) != -1) {
        return MDOS_EEXIST;
    }
    
    /* Calculate required sectors */
    int data_sectors = (size + MDOS_SECTOR_SIZE - 1) / MDOS_SECTOR_SIZE;
    int total_sectors = data_sectors + 1; /* +1 for RIB */
    
    /* Load cluster allocation table */
    uint8_t cat[MDOS_SECTOR_SIZE];
    mdos_getsect(fs, cat, MDOS_SECTOR_CAT);
    
    /* Allocate space */
    mdos_rib_t rib;
    int result = mdos_alloc_space(fs, cat, &rib, total_sectors);
    if (result != MDOS_EOK) {
        return result;
    }
    
    /* Set RIB metadata */
    rib.last_size = (size % MDOS_SECTOR_SIZE) ? (size % MDOS_SECTOR_SIZE) : MDOS_SECTOR_SIZE;
    rib.size_high = (data_sectors >> 8);
    rib.size_low = data_sectors & 0xFF;
    rib.addr_high = 0;
    rib.addr_low = 0;
    rib.pc_high = 0;
    rib.pc_low = 0;
    
    /* Find RIB sector (logical sector 0) */
    int rib_sector = mdos_lsn_to_psn(&rib, 0);
    if (rib_sector == -1) {
        return MDOS_EIO;
    }
    
    /* Write RIB */
    mdos_putsect(fs, (uint8_t*)&rib, rib_sector);
    
    /* Write data sectors */
    const uint8_t *data_ptr = (const uint8_t*)data;
    for (int i = 0; i < data_sectors; i++) {
        int phys_sector = mdos_lsn_to_psn(&rib, i + 1);
        if (phys_sector == -1) {
            return MDOS_EIO;
        }
        
        uint8_t sector_buf[MDOS_SECTOR_SIZE];
        memset(sector_buf, 0, MDOS_SECTOR_SIZE);
        
        size_t bytes_to_copy = MDOS_SECTOR_SIZE;
        if (i == data_sectors - 1) {
            /* Last sector */
            bytes_to_copy = size - (i * MDOS_SECTOR_SIZE);
        }
        
        memcpy(sector_buf, data_ptr + (i * MDOS_SECTOR_SIZE), bytes_to_copy);
        mdos_putsect(fs, sector_buf, phys_sector);
    }
    
    /* Write directory entry */
    result = mdos_write_directory_entry(fs, filename, rib_sector, file_type);
    if (result != MDOS_EOK) {
        return result;
    }
    
    /* Update cluster allocation table */
    mdos_putsect(fs, cat, MDOS_SECTOR_CAT);
    
    return MDOS_EOK;
}
