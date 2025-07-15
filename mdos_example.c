/*
 * Example usage of the MDOS Filesystem Library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include "mdos_fs.h"

void print_error(const char *operation, int error);
void list_files(mdos_fs_t *fs);
int copy_file_from_mdos(mdos_fs_t *fs, const char *mdos_name, const char *local_name);
int cat_file(mdos_fs_t *fs, const char *filename);
int rawcat_file(mdos_fs_t *fs, const char *filename);
int test_seek_operations(mdos_fs_t *fs, const char *filename);
int extract_mdos_name(const char *local_path, char *mdos_name);
int put_file(mdos_fs_t *fs, const char *local_name, const char *mdos_name_arg);

void print_error(const char *operation, int error) {
    fprintf(stderr, "Error in %s: %s\n", operation, mdos_strerror(error));
}

void list_files(mdos_fs_t *fs) {
    mdos_file_info_t *files;
    int count;
    
    printf("\nDirectory listing:\n");
    printf("%-12s %8s %6s %s\n", "Name", "Size", "Type", "Attributes");
    printf("----------------------------------------\n");
    
    int result = mdos_readdir(fs, &files, &count);
    if (result != MDOS_EOK) {
        print_error("readdir", result);
        return;
    }
    
    for (int i = 0; i < count; i++) {
        char attrs[8] = "------";
        if (files[i].attributes & MDOS_ATTR_WRITE_PROTECT) attrs[0] = 'W';
        if (files[i].attributes & MDOS_ATTR_DELETE_PROTECT) attrs[1] = 'D';
        if (files[i].attributes & MDOS_ATTR_SYSTEM) attrs[2] = 'S';
        if (files[i].attributes & MDOS_ATTR_CONT) attrs[3] = 'C';
        if (files[i].attributes & MDOS_ATTR_COMPR) attrs[4] = 'Z';
        
        printf("%-12s %8d %6d %s\n", 
               files[i].name, 
               files[i].size, 
               files[i].type,
               attrs);
    }
    
    free(files);
    
    int free_bytes = mdos_free_space(fs);
    printf("\nFree space: %d bytes\n", free_bytes);
}

int copy_file_from_mdos(mdos_fs_t *fs, const char *mdos_name, const char *local_name) {
    printf("Copying %s to %s...\n", mdos_name, local_name);
    
    int fd = mdos_open(fs, mdos_name, MDOS_O_RDONLY, 0);
    if (fd < 0) {
        print_error("mdos_open", fd);
        return -1;
    }
    
    FILE *local_file = fopen(local_name, "wb");
    if (!local_file) {
        perror("fopen");
        mdos_close(fs, fd);
        return -1;
    }
    
    char buffer[1024];
    ssize_t bytes_read;
    size_t total_bytes = 0;
    
    while ((bytes_read = mdos_read(fs, fd, buffer, sizeof(buffer))) > 0) {
        if (fwrite(buffer, 1, (size_t)bytes_read, local_file) != (size_t)bytes_read) {
            perror("fwrite");
            fclose(local_file);
            mdos_close(fs, fd);
            return -1;
        }
        total_bytes += (size_t)bytes_read;
    }
    
    if (bytes_read < 0) {
        print_error("mdos_read", bytes_read);
        fclose(local_file);
        mdos_close(fs, fd);
        return -1;
    }
    
    fclose(local_file);
    mdos_close(fs, fd);
    
    printf("Successfully copied %zu bytes\n", total_bytes);
    return 0;
}

/* Extract filename from path and validate for MDOS */
int extract_mdos_name(const char *local_path, char *mdos_name) {
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
        printf("MDOS filename must be 1-8 characters\n");
        return -1;
    }
    
    if (ext_len > 2) {
        printf("MDOS extension must be 0-2 characters\n");
        return -1;
    }
    
    /* Build MDOS name */
    size_t pos = 0;
    for (size_t i = 0; i < name_len; i++) {
        char c = start[i];
        if (!isalnum((unsigned char)c)) {
            printf("MDOS filename must contain only letters and numbers\n");
            return -1;
        }
        mdos_name[pos++] = (char)tolower((unsigned char)c);
    }
    
    if (ext_len > 0) {
        mdos_name[pos++] = '.';
        for (size_t i = 0; i < ext_len; i++) {
            char c = dot[1 + i];
            if (!isalnum((unsigned char)c)) {
                printf("MDOS extension must contain only letters and numbers\n");
                return -1;
            }
            mdos_name[pos++] = (char)tolower((unsigned char)c);
        }
    } else {
        /* Default extension if none provided */
        strcpy(mdos_name + pos, ".sa");
        pos += 3;
    }
    
    mdos_name[pos] = '\0';
    return 0;
}

/* Copy file from local filesystem to MDOS */
int put_file(mdos_fs_t *fs, const char *local_name, const char *mdos_name_arg) {
    char mdos_name[16];
    
    /* Determine MDOS filename */
    if (mdos_name_arg) {
        strncpy(mdos_name, mdos_name_arg, sizeof(mdos_name) - 1);
        mdos_name[sizeof(mdos_name) - 1] = '\0';
    } else {
        if (extract_mdos_name(local_name, mdos_name) != 0) {
            return -1;
        }
    }
    
    printf("Copying %s to %s...\n", local_name, mdos_name);
    
    /* Open local file */
    FILE *local_file = fopen(local_name, "rb");
    if (!local_file) {
        perror("Failed to open local file");
        return -1;
    }
    
    /* Get file size */
    if (fseek(local_file, 0, SEEK_END) != 0) {
        perror("Failed to seek to end");
        fclose(local_file);
        return -1;
    }
    
    long file_size = ftell(local_file);
    if (file_size < 0) {
        perror("Failed to get file size");
        fclose(local_file);
        return -1;
    }
    
    if (fseek(local_file, 0, SEEK_SET) != 0) {
        perror("Failed to seek to beginning");
        fclose(local_file);
        return -1;
    }
    
    printf("Local file size: %ld bytes\n", file_size);
    
    /* Read entire file into memory */
    uint8_t *file_data = malloc((size_t)file_size);
    if (!file_data) {
        printf("Out of memory\n");
        fclose(local_file);
        return -1;
    }
    
    size_t bytes_read = fread(file_data, 1, (size_t)file_size, local_file);
    fclose(local_file);
    
    if (bytes_read != (size_t)file_size) {
        printf("Failed to read entire file\n");
        free(file_data);
        return -1;
    }
    
    /* Check if file already exists and delete it */
    mdos_file_info_t existing_info;
    if (mdos_stat(fs, mdos_name, &existing_info) == MDOS_EOK) {
        printf("File %s already exists, deleting...\n", mdos_name);
        int unlink_result = mdos_unlink(fs, mdos_name);
        if (unlink_result != MDOS_EOK) {
            printf("Warning: Failed to delete existing file: %s\n", mdos_strerror(unlink_result));
        }
    }
    
    /* Determine file type based on extension */
    const char *ext = strrchr(mdos_name, '.');
    int file_type = MDOS_TYPE_ASCII; /* Default to ASCII */
    
    if (ext) {
        if (strcasecmp(ext, ".bin") == 0 || strcasecmp(ext, ".obj") == 0) {
            file_type = MDOS_TYPE_IMAGE;
        } else if (strcasecmp(ext, ".txt") == 0 || strcasecmp(ext, ".sa") == 0 || strcasecmp(ext, ".cm") == 0) {
            file_type = MDOS_TYPE_ASCII;
        }
    }
    
    printf("Writing as file type: %d\n", file_type);
    
    /* Convert ASCII files */
    uint8_t *converted_data = file_data;
    size_t converted_size = (size_t)file_size;
    
    if (file_type == MDOS_TYPE_ASCII) {
        /* Convert Unix/DOS line endings to MDOS format */
        converted_data = malloc((size_t)file_size * 2); /* Worst case */
        if (!converted_data) {
            printf("Out of memory for conversion\n");
            free(file_data);
            return -1;
        }
        
        size_t out_pos = 0;
        for (size_t i = 0; i < (size_t)file_size; i++) {
            uint8_t c = file_data[i];
            if (c == '\n') {
                /* Convert LF to CR */
                converted_data[out_pos++] = '\r';
            } else if (c == '\r') {
                /* Skip CR (will be handled by LF) */
                if (i + 1 < (size_t)file_size && file_data[i + 1] == '\n') {
                    /* CRLF sequence, skip the CR */
                } else {
                    /* Standalone CR, keep it */
                    converted_data[out_pos++] = '\r';
                }
            } else {
                converted_data[out_pos++] = c;
            }
        }
        converted_size = out_pos;
    }
    
    /* Write file to MDOS filesystem */
    int result = mdos_create_file(fs, mdos_name, file_type, converted_data, converted_size);
    
    if (converted_data != file_data) {
        free(converted_data);
    }
    free(file_data);
    
    if (result != MDOS_EOK) {
        printf("Failed to write file: %s\n", mdos_strerror(result));
        return -1;
    }
    
    printf("Successfully wrote %zu bytes to MDOS filesystem!\n", converted_size);
    return 0;
}

int cat_file(mdos_fs_t *fs, const char *filename) {
    printf("Contents of %s:\n", filename);
    printf("----------------------------------------\n");
    
    int fd = mdos_open(fs, filename, MDOS_O_RDONLY, 0);
    if (fd < 0) {
        print_error("mdos_open", fd);
        return -1;
    }
    
    char buffer[128];
    ssize_t bytes_read;
    
    while ((bytes_read = mdos_read(fs, fd, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, (size_t)bytes_read, stdout);
    }
    
    if (bytes_read < 0) {
        print_error("mdos_read", bytes_read);
        mdos_close(fs, fd);
        return -1;
    }
    
    mdos_close(fs, fd);
    printf("\n----------------------------------------\n");
    return 0;
}

int rawcat_file(mdos_fs_t *fs, const char *filename) {
    printf("Raw contents of %s:\n", filename);
    printf("----------------------------------------\n");
    
    int fd = mdos_open(fs, filename, MDOS_O_RDONLY, 0);
    if (fd < 0) {
        print_error("mdos_open", fd);
        return -1;
    }
    
    char buffer[128];
    ssize_t bytes_read;
    
    while ((bytes_read = mdos_read_raw(fs, fd, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, (size_t)bytes_read, stdout);
    }
    
    if (bytes_read < 0) {
        print_error("mdos_read_raw", bytes_read);
        mdos_close(fs, fd);
        return -1;
    }
    
    mdos_close(fs, fd);
    printf("\n----------------------------------------\n");
    return 0;
}


int test_seek_operations(mdos_fs_t *fs, const char *filename) {
    printf("Testing seek operations on %s...\n", filename);
    
    int fd = mdos_open(fs, filename, MDOS_O_RDONLY, 0);
    if (fd < 0) {
        print_error("mdos_open", fd);
        return -1;
    }
    
    /* Get file size */
    off_t file_size = mdos_lseek(fs, fd, 0, MDOS_SEEK_END);
    if (file_size < 0) {
        print_error("mdos_lseek", file_size);
        mdos_close(fs, fd);
        return -1;
    }
    printf("File size: %ld bytes\n", file_size);
    
    /* Seek to beginning */
    off_t pos = mdos_lseek(fs, fd, 0, MDOS_SEEK_SET);
    if (pos != 0) {
        print_error("mdos_lseek", pos);
        mdos_close(fs, fd);
        return -1;
    }
    
    /* Read first 10 bytes */
    char buffer[10];
    ssize_t bytes_read = mdos_read(fs, fd, buffer, 10);
    if (bytes_read > 0) {
        printf("First 10 bytes: ");
        for (int i = 0; i < bytes_read; i++) {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n");
    }
    
    /* Seek to middle */
    pos = mdos_lseek(fs, fd, file_size / 2, MDOS_SEEK_SET);
    printf("Seeked to position: %ld\n", pos);
    
    /* Read 10 bytes from middle */
    bytes_read = mdos_read(fs, fd, buffer, 10);
    if (bytes_read > 0) {
        printf("10 bytes from middle: ");
        for (int i = 0; i < bytes_read; i++) {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n");
    }
    
    /* Seek to 10 bytes before end */
    pos = mdos_lseek(fs, fd, -10, MDOS_SEEK_END);
    if (pos >= 0) {
        printf("Seeked to 10 bytes before end: %ld\n", pos);
        
        /* Read last bytes */
        bytes_read = mdos_read(fs, fd, buffer, 10);
        if (bytes_read > 0) {
            printf("Last bytes: ");
            for (int i = 0; i < bytes_read; i++) {
                printf("%02X ", (unsigned char)buffer[i]);
            }
            printf("\n");
        }
    }
    
    mdos_close(fs, fd);
    return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
        fprintf(stderr, "Usage: %s <mdos-disk-image> [command] [args...]\n", argv[0]);
        fprintf(stderr, "\nCommands:\n");
        fprintf(stderr, "  ls                    - List directory\n");
        fprintf(stderr, "  cat <filename>        - Display file contents\n");
        fprintf(stderr, "  rawcat <filename>     - Display raw file contents (no conversion)\n");
        fprintf(stderr, "  get <filename> [out]  - Copy file from MDOS to local filesystem\n");
        fprintf(stderr, "  put <local> [mdos]    - Copy file from local to MDOS filesystem\n");
        fprintf(stderr, "  mkfs <sides>          - Create new MDOS filesystem (1=single, 2=double sided)\n");
        fprintf(stderr, "  seek <filename>       - Test seek operations\n");
        fprintf(stderr, "  info <filename>       - Show file information\n");
        return 1;
    }
    const char *disk_path = argv[1];
    const char *command = (argc > 2) ? argv[2] : "ls";
    
    /* Handle mkfs command specially (doesn't need mounting) */
    if (strcmp(command, "mkfs") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s <new-disk.dsk> mkfs <sides>\n", argv[0]);
            fprintf(stderr, "  sides: 1 = single sided, 2 = double sided\n");
            return 1;
        }
        
        int sides = atoi(argv[3]);
        if (sides != 1 && sides != 2) {
            fprintf(stderr, "Error: sides must be 1 (single) or 2 (double)\n");
            return 1;
        }
        
        int result = mdos_mkfs(disk_path, sides);
        if (result != MDOS_EOK) {
            fprintf(stderr, "Failed to create filesystem: %s\n", mdos_strerror(result));
            return 1;
        }
        return 0;
    }
    
    /* Determine if we need write access */
    int need_write = (argc > 2) && (strcmp(command, "put") == 0);
    
    /* Mount the MDOS filesystem */
    mdos_fs_t *fs = mdos_mount(disk_path, !need_write); /* Read-write for put, read-only otherwise */
    if (!fs) {
        fprintf(stderr, "Failed to mount MDOS disk: %s\n", disk_path);
        return 1;
    }
    
    printf("Successfully mounted MDOS disk: %s (%s)\n", disk_path, need_write ? "read-write" : "read-only");
    
    int result = 0;
    
    if (strcmp(command, "ls") == 0) {
        list_files(fs);
    }
    else if (strcmp(command, "cat") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s cat <filename>\n", argv[0], disk_path);
            result = 1;
        } else {
            result = cat_file(fs, argv[3]);
        }
    }
    else if (strcmp(command, "rawcat") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s rawcat <filename>\n", argv[0], disk_path);
            result = 1;
        } else {
            result = rawcat_file(fs, argv[3]);
        }
    }
    else if (strcmp(command, "put") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s put <local-file> [mdos-name]\n", argv[0], disk_path);
            result = 1;
        } else {
            const char *mdos_name = (argc > 4) ? argv[4] : NULL;
            result = put_file(fs, argv[3], mdos_name);
        }
    }
    else if (strcmp(command, "get") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s get <filename> [output-file]\n", argv[0], disk_path);
            result = 1;
        } else {
            const char *output_file = (argc > 4) ? argv[4] : argv[3];
            result = copy_file_from_mdos(fs, argv[3], output_file);
        }
    }
    else if (strcmp(command, "mkfs") == 0) {
        /* This should never be reached due to early handling above */
        fprintf(stderr, "Internal error: mkfs command not handled properly\n");
        result = 1;
    }
    else if (strcmp(command, "seek") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s seek <filename>\n", argv[0], disk_path);
            result = 1;
        } else {
            result = test_seek_operations(fs, argv[3]);
        }
    }
    else if (strcmp(command, "info") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s info <filename>\n", argv[0], disk_path);
            result = 1;
        } else {
            mdos_file_info_t info;
            int stat_result = mdos_stat(fs, argv[3], &info);
            if (stat_result == MDOS_EOK) {
                printf("File information for '%s':\n", argv[3]);
                printf("  Size: %d bytes (%d sectors)\n", info.size, info.sectors);
                printf("  Type: %d\n", info.type);
                printf("  Load address: $%04X\n", info.load_addr);
                printf("  Start address: $%04X\n", info.start_addr);
                printf("  RIB sector: %d\n", info.rib_sector);
                printf("  Attributes: ");
                if (info.attributes & MDOS_ATTR_WRITE_PROTECT) printf("Write-Protected ");
                if (info.attributes & MDOS_ATTR_DELETE_PROTECT) printf("Delete-Protected ");
                if (info.attributes & MDOS_ATTR_SYSTEM) printf("System ");
                if (info.attributes & MDOS_ATTR_CONT) printf("Continuous ");
                if (info.attributes & MDOS_ATTR_COMPR) printf("Compressed ");
                printf("\n");
            } else {
                print_error("mdos_stat", stat_result);
                result = 1;
            }
        }
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
        result = 1;
    }
    
    /* Unmount the filesystem */
    mdos_unmount(fs);
    
    return result;
}
