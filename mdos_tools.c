/*
 * MDOS Filesystem Library - High-level Tools
 * Copyright (C) 2025
 * 
 * Higher-level utility functions for common operations
 */

#include "mdos_fs.h"
#include "mdos_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  /* For strcasecmp */
#include <ctype.h>

/* Print directory listing to FILE stream */
int mdos_list_files(mdos_fs_t *fs, FILE *output) {
    if (!fs || !output) return MDOS_EINVAL;
    
    mdos_file_info_t *files;
    int count;
    
    fprintf(output, "%-12s %8s %6s %s\n", "Name", "Size", "Type", "Attributes");
    fprintf(output, "----------------------------------------\n");
    
    int result = mdos_readdir(fs, &files, &count);
    if (result != MDOS_EOK) {
        return result;
    }
    
    for (int i = 0; i < count; i++) {
        char attrs[8] = "------";
        if (files[i].attributes & MDOS_ATTR_WRITE_PROTECT) attrs[0] = 'W';
        if (files[i].attributes & MDOS_ATTR_DELETE_PROTECT) attrs[1] = 'D';
        if (files[i].attributes & MDOS_ATTR_SYSTEM) attrs[2] = 'S';
        if (files[i].attributes & MDOS_ATTR_CONT) attrs[3] = 'C';
        if (files[i].attributes & MDOS_ATTR_COMPR) attrs[4] = 'Z';
        
        fprintf(output, "%-12s %8d %6d %s\n", 
               files[i].name, 
               files[i].size, 
               files[i].type,
               attrs);
    }
    
    free(files);
    
    int free_bytes = mdos_free_space(fs);
    fprintf(output, "\nFree space: %d bytes\n", free_bytes);
    
    return MDOS_EOK;
}

/* Output file contents to FILE stream with optional conversion */
int mdos_cat_file(mdos_fs_t *fs, const char *filename, FILE *output, int raw_mode) {
    if (!fs || !filename || !output) return MDOS_EINVAL;
    
    int fd = mdos_open(fs, filename, MDOS_O_RDONLY, 0);
    if (fd < 0) {
        return fd;
    }
    
    char buffer[1024];
    ssize_t bytes_read;
    
    while ((bytes_read = (raw_mode ? mdos_read_raw(fs, fd, buffer, sizeof(buffer)) 
                                  : mdos_read(fs, fd, buffer, sizeof(buffer)))) > 0) {
        if (fwrite(buffer, 1, (size_t)bytes_read, output) != (size_t)bytes_read) {
            mdos_close(fs, fd);
            return MDOS_EIO;
        }
    }
    
    if (bytes_read < 0) {
        mdos_close(fs, fd);
        return (int)bytes_read;
    }
    
    mdos_close(fs, fd);
    return MDOS_EOK;
}

/* Copy file from MDOS to local filesystem */
int mdos_export_file(mdos_fs_t *fs, const char *mdos_name, const char *local_name) {
    if (!fs || !mdos_name || !local_name) return MDOS_EINVAL;
    
    int fd = mdos_open(fs, mdos_name, MDOS_O_RDONLY, 0);
    if (fd < 0) {
        return fd;
    }
    
    FILE *local_file = fopen(local_name, "wb");
    if (!local_file) {
        mdos_close(fs, fd);
        return MDOS_EIO;
    }
    
    char buffer[1024];
    ssize_t bytes_read;
    size_t total_bytes = 0;
    
    while ((bytes_read = mdos_read(fs, fd, buffer, sizeof(buffer))) > 0) {
        if (fwrite(buffer, 1, (size_t)bytes_read, local_file) != (size_t)bytes_read) {
            fclose(local_file);
            mdos_close(fs, fd);
            return MDOS_EIO;
        }
        total_bytes += (size_t)bytes_read;
    }
    
    if (bytes_read < 0) {
        fclose(local_file);
        mdos_close(fs, fd);
        return (int)bytes_read;
    }
    
    fclose(local_file);
    mdos_close(fs, fd);
    
    return (int)total_bytes; /* Return number of bytes copied */
}

/* Determine file type based on extension */
static int determine_file_type(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return MDOS_TYPE_ASCII; /* Default to ASCII */
    
    if (strcasecmp(ext, ".bin") == 0 || strcasecmp(ext, ".obj") == 0) {
        return MDOS_TYPE_IMAGE;
    } else if (strcasecmp(ext, ".txt") == 0 || strcasecmp(ext, ".sa") == 0 || 
               strcasecmp(ext, ".cm") == 0 || strcasecmp(ext, ".asm") == 0) {
        return MDOS_TYPE_ASCII;
    }
    
    return MDOS_TYPE_ASCII; /* Default to ASCII */
}

/* Convert ASCII data for MDOS format */
static uint8_t* convert_ascii_data(const uint8_t *data, size_t size, size_t *converted_size) {
    uint8_t *converted_data = malloc(size * 2); /* Worst case allocation */
    if (!converted_data) return NULL;
    
    size_t out_pos = 0;
    for (size_t i = 0; i < size; i++) {
        uint8_t c = data[i];
        if (c == '\n') {
            /* Convert LF to CR */
            converted_data[out_pos++] = '\r';
        } else if (c == '\r') {
            /* Skip CR (will be handled by LF) */
            if (i + 1 < size && data[i + 1] == '\n') {
                /* CRLF sequence, skip the CR */
            } else {
                /* Standalone CR, keep it */
                converted_data[out_pos++] = '\r';
            }
        } else {
            converted_data[out_pos++] = c;
        }
    }
    
    *converted_size = out_pos;
    return converted_data;
}

/* Copy file from local filesystem to MDOS */
int mdos_import_file(mdos_fs_t *fs, const char *local_name, const char *mdos_name_arg) {
    if (!fs || !local_name) return MDOS_EINVAL;
    if (fs->read_only) return MDOS_EPERM;
    
    char mdos_name[MDOS_MAX_FILENAME];
    
    /* Determine MDOS filename */
    if (mdos_name_arg) {
        strncpy(mdos_name, mdos_name_arg, sizeof(mdos_name) - 1);
        mdos_name[sizeof(mdos_name) - 1] = '\0';
    } else {
        int result = mdos_extract_filename(local_name, mdos_name);
        if (result != MDOS_EOK) {
            return result;
        }
    }
    
    /* Open local file */
    FILE *local_file = fopen(local_name, "rb");
    if (!local_file) {
        return MDOS_EIO;
    }
    
    /* Get file size */
    if (fseek(local_file, 0, SEEK_END) != 0) {
        fclose(local_file);
        return MDOS_EIO;
    }
    
    long file_size = ftell(local_file);
    if (file_size < 0) {
        fclose(local_file);
        return MDOS_EIO;
    }
    
    if (fseek(local_file, 0, SEEK_SET) != 0) {
        fclose(local_file);
        return MDOS_EIO;
    }
    
    /* Read entire file into memory */
    uint8_t *file_data = malloc((size_t)file_size);
    if (!file_data) {
        fclose(local_file);
        return MDOS_ENOSPC;
    }
    
    size_t bytes_read = fread(file_data, 1, (size_t)file_size, local_file);
    fclose(local_file);
    
    if (bytes_read != (size_t)file_size) {
        free(file_data);
        return MDOS_EIO;
    }
    
    /* Check if file already exists and delete it */
    mdos_file_info_t existing_info;
    if (mdos_stat(fs, mdos_name, &existing_info) == MDOS_EOK) {
        int unlink_result = mdos_unlink(fs, mdos_name);
        if (unlink_result != MDOS_EOK) {
            free(file_data);
            return unlink_result;
        }
    }
    
    /* Determine file type */
    int file_type = determine_file_type(mdos_name);
    
    /* Convert ASCII files */
    uint8_t *converted_data = file_data;
    size_t converted_size = (size_t)file_size;
    
    if (file_type == MDOS_TYPE_ASCII) {
        converted_data = convert_ascii_data(file_data, (size_t)file_size, &converted_size);
        if (!converted_data) {
            free(file_data);
            return MDOS_ENOSPC;
        }
    }
    
    /* Write file to MDOS filesystem */
    int result = mdos_create_file(fs, mdos_name, file_type, converted_data, converted_size);
    
    if (converted_data != file_data) {
        free(converted_data);
    }
    free(file_data);
    
    if (result == MDOS_EOK) {
        return (int)converted_size; /* Return number of bytes written */
    }
    
    return result;
}

/* Test seek operations on a file (useful for debugging) */
int mdos_test_seek(mdos_fs_t *fs, const char *filename, FILE *output) {
    if (!fs || !filename || !output) return MDOS_EINVAL;
    
    fprintf(output, "Testing seek operations on %s...\n", filename);
    
    int fd = mdos_open(fs, filename, MDOS_O_RDONLY, 0);
    if (fd < 0) {
        return fd;
    }
    
    /* Get file size */
    off_t file_size = mdos_lseek(fs, fd, 0, MDOS_SEEK_END);
    if (file_size < 0) {
        mdos_close(fs, fd);
        return (int)file_size;
    }
    fprintf(output, "File size: %ld bytes\n", file_size);
    
    /* Seek to beginning */
    off_t pos = mdos_lseek(fs, fd, 0, MDOS_SEEK_SET);
    if (pos != 0) {
        mdos_close(fs, fd);
        return (int)pos;
    }
    
    /* Read first 10 bytes */
    char buffer[10];
    ssize_t bytes_read = mdos_read_raw(fs, fd, buffer, 10);
    if (bytes_read > 0) {
        fprintf(output, "First 10 bytes: ");
        for (int i = 0; i < bytes_read; i++) {
            fprintf(output, "%02X ", (unsigned char)buffer[i]);
        }
        fprintf(output, "\n");
    }
    
    /* Seek to middle */
    pos = mdos_lseek(fs, fd, file_size / 2, MDOS_SEEK_SET);
    fprintf(output, "Seeked to position: %ld\n", pos);
    
    /* Read 10 bytes from middle */
    bytes_read = mdos_read_raw(fs, fd, buffer, 10);
    if (bytes_read > 0) {
        fprintf(output, "10 bytes from middle: ");
        for (int i = 0; i < bytes_read; i++) {
            fprintf(output, "%02X ", (unsigned char)buffer[i]);
        }
        fprintf(output, "\n");
    }
    
    /* Seek to 10 bytes before end */
    pos = mdos_lseek(fs, fd, -10, MDOS_SEEK_END);
    if (pos >= 0) {
        fprintf(output, "Seeked to 10 bytes before end: %ld\n", pos);
        
        /* Read last bytes */
        bytes_read = mdos_read_raw(fs, fd, buffer, 10);
        if (bytes_read > 0) {
            fprintf(output, "Last bytes: ");
            for (int i = 0; i < bytes_read; i++) {
                fprintf(output, "%02X ", (unsigned char)buffer[i]);
            }
            fprintf(output, "\n");
        }
    }
    
    mdos_close(fs, fd);
    return MDOS_EOK;
}

/* Print detailed file information */
int mdos_file_info(mdos_fs_t *fs, const char *filename, FILE *output) {
    if (!fs || !filename || !output) return MDOS_EINVAL;
    
    mdos_file_info_t info;
    int result = mdos_stat(fs, filename, &info);
    if (result != MDOS_EOK) {
        return result;
    }
    
    fprintf(output, "File information for '%s':\n", filename);
    fprintf(output, "  Size: %d bytes (%d sectors)\n", info.size, info.sectors);
    fprintf(output, "  Type: %d\n", info.type);
    fprintf(output, "  Load address: $%04X\n", info.load_addr);
    fprintf(output, "  Start address: $%04X\n", info.start_addr);
    fprintf(output, "  RIB sector: %d\n", info.rib_sector);
    fprintf(output, "  Attributes: ");
    
    if (info.attributes & MDOS_ATTR_WRITE_PROTECT) fprintf(output, "Write-Protected ");
    if (info.attributes & MDOS_ATTR_DELETE_PROTECT) fprintf(output, "Delete-Protected ");
    if (info.attributes & MDOS_ATTR_SYSTEM) fprintf(output, "System ");
    if (info.attributes & MDOS_ATTR_CONT) fprintf(output, "Continuous ");
    if (info.attributes & MDOS_ATTR_COMPR) fprintf(output, "Compressed ");
    if (info.attributes == 0) fprintf(output, "None");
    fprintf(output, "\n");
    
    return MDOS_EOK;
}
