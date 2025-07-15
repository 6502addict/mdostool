/*
 * MDOS Filesystem Utility Tool
 * Copyright (C) 2025
 * 
 * Command-line utility for working with MDOS disk images
 * Uses the modular MDOS filesystem library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mdos_fs.h"

void print_usage(const char *program_name) {
    fprintf(stderr, "MDOS Filesystem Utility v1.1\n");
    fprintf(stderr, "Usage: %s <mdos-disk-image> [command] [args...]\n", program_name);
    fprintf(stderr, "\nCommands:\n");
    fprintf(stderr, "  ls                    - List directory contents\n");
    fprintf(stderr, "  cat <filename>        - Display file contents (with ASCII conversion)\n");
    fprintf(stderr, "  rawcat <filename>     - Display raw file contents (no conversion)\n");
    fprintf(stderr, "  get <filename> [out]  - Export file from MDOS to local filesystem\n");
    fprintf(stderr, "  put <local> [mdos]    - Import file from local to MDOS filesystem\n");
    fprintf(stderr, "  mkfs <sides>          - Create new MDOS filesystem (1=single, 2=double sided)\n");
    fprintf(stderr, "  seek <filename>       - Test seek operations on file\n");
    fprintf(stderr, "  info <filename>       - Show detailed file information\n");
    fprintf(stderr, "  free                  - Show free space information\n");
    fprintf(stderr, "  rm <filename>         - Delete file from MDOS filesystem\n");
    fprintf(stderr, "\nImage Conversion Commands:\n");
    fprintf(stderr, "  imd2dsk <input.imd> <output.dsk> - Convert IMD to DSK format\n");
    fprintf(stderr, "  dsk2imd <input.dsk> <output.imd> - Convert DSK to IMD format\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  %s disk.dsk ls\n", program_name);
    fprintf(stderr, "  %s disk.dsk cat readme.txt\n", program_name);
    fprintf(stderr, "  %s disk.dsk put myfile.txt\n", program_name);
    fprintf(stderr, "  %s disk.dsk get data.bin exported.bin\n", program_name);
    fprintf(stderr, "  %s newdisk.dsk mkfs 2\n", program_name);
    fprintf(stderr, "  %s - imd2dsk disk.imd disk.dsk\n", program_name);
    fprintf(stderr, "  %s - dsk2imd disk.dsk disk.imd\n", program_name);
}

void print_error(const char *operation, int error) {
    fprintf(stderr, "Error in %s: %s\n", operation, mdos_strerror(error));
}

int handle_mkfs(const char *disk_path, int sides) {
    printf("Creating MDOS filesystem on %s (%s sided)...\n", 
           disk_path, (sides == 1) ? "single" : "double");
    
    int result = mdos_mkfs(disk_path, sides);
    if (result != MDOS_EOK) {
        print_error("mkfs", result);
        return 1;
    }
    
    printf("Filesystem created successfully!\n");
    return 0;
}

int handle_ls(mdos_fs_t *fs) {
    printf("Directory listing:\n");
    printf("==================\n");
    
    int result = mdos_list_files(fs, stdout);
    if (result != MDOS_EOK) {
        print_error("ls", result);
        return 1;
    }
    
    return 0;
}

int handle_cat(mdos_fs_t *fs, const char *filename, int raw_mode) {
    printf("%s contents of '%s':\n", raw_mode ? "Raw" : "Formatted", filename);
    printf("========================================\n");
    
    int result = mdos_cat_file(fs, filename, stdout, raw_mode);
    if (result != MDOS_EOK) {
        print_error("cat", result);
        return 1;
    }
    
    printf("\n========================================\n");
    return 0;
}

int handle_get(mdos_fs_t *fs, const char *mdos_name, const char *local_name) {
    printf("Exporting '%s' to '%s'...\n", mdos_name, local_name);
    
    int result = mdos_export_file(fs, mdos_name, local_name);
    if (result < 0) {
        print_error("get", result);
        return 1;
    }
    
    printf("Successfully exported %d bytes\n", result);
    return 0;
}

int handle_put(mdos_fs_t *fs, const char *local_name, const char *mdos_name) {
    if (mdos_name) {
        printf("Importing '%s' as '%s'...\n", local_name, mdos_name);
    } else {
        printf("Importing '%s' (auto-naming)...\n", local_name);
    }
    
    int result = mdos_import_file(fs, local_name, mdos_name);
    if (result < 0) {
        print_error("put", result);
        return 1;
    }
    
    printf("Successfully imported %d bytes\n", result);
    return 0;
}

int handle_info(mdos_fs_t *fs, const char *filename) {
    printf("File Information:\n");
    printf("=================\n");
    
    int result = mdos_file_info(fs, filename, stdout);
    if (result != MDOS_EOK) {
        print_error("info", result);
        return 1;
    }
    
    return 0;
}

int handle_seek(mdos_fs_t *fs, const char *filename) {
    printf("Seek Test Results:\n");
    printf("==================\n");
    
    int result = mdos_test_seek(fs, filename, stdout);
    if (result != MDOS_EOK) {
        print_error("seek", result);
        return 1;
    }
    
    return 0;
}

int handle_free(mdos_fs_t *fs) {
    int free_bytes = mdos_free_space(fs);
    if (free_bytes < 0) {
        print_error("free", free_bytes);
        return 1;
    }
    
    printf("Free Space Information:\n");
    printf("=======================\n");
    printf("Free space: %d bytes\n", free_bytes);
    printf("Free space: %d KB\n", free_bytes / 1024);
    printf("Free clusters: %d\n", free_bytes / (4 * MDOS_SECTOR_SIZE));
    printf("Free sectors: %d\n", free_bytes / MDOS_SECTOR_SIZE);
    
    return 0;
}

int handle_imd_to_dsk(const char *imd_filename, const char *dsk_filename) {
    printf("Converting IMD to DSK format...\n");
    printf("Input:  %s\n", imd_filename);
    printf("Output: %s\n", dsk_filename);
    
    int result = mdos_convert_imd_to_dsk(imd_filename, dsk_filename);
    if (result != MDOS_EOK) {
        print_error("imd2dsk", result);
        return 1;
    }
    
    printf("IMD to DSK conversion completed successfully!\n");
    return 0;
}

int handle_dsk_to_imd(const char *dsk_filename, const char *imd_filename) {
    printf("Converting DSK to IMD format...\n");
    printf("Input:  %s\n", dsk_filename);
    printf("Output: %s\n", imd_filename);
    
    int result = mdos_convert_dsk_to_imd(dsk_filename, imd_filename);
    if (result != MDOS_EOK) {
        print_error("dsk2imd", result);
        return 1;
    }
    
    printf("DSK to IMD conversion completed successfully!\n");
    return 0;
}

int handle_rm(mdos_fs_t *fs, const char *filename) {
    printf("Deleting '%s'...\n", filename);
    
    /* Check if file exists first */
    mdos_file_info_t info;
    int stat_result = mdos_stat(fs, filename, &info);
    if (stat_result != MDOS_EOK) {
        print_error("stat", stat_result);
        return 1;
    }
    
    printf("File found: %d bytes, type %d\n", info.size, info.type);
    
    int result = mdos_unlink(fs, filename);
    if (result != MDOS_EOK) {
        print_error("rm", result);
        return 1;
    }
    
    printf("File '%s' deleted successfully\n", filename);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *disk_path = argv[1];
    const char *command = (argc > 2) ? argv[2] : "ls";
    
    /* Handle conversion commands specially (don't need MDOS disk) */
    if (strcmp(command, "imd2dsk") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: imd2dsk requires input and output filenames\n");
            fprintf(stderr, "Usage: %s - imd2dsk <input.imd> <output.dsk>\n", argv[0]);
            return 1;
        }
        return handle_imd_to_dsk(argv[3], argv[4] ? argv[4] : "output.dsk");
    }
    
    if (strcmp(command, "dsk2imd") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: dsk2imd requires input and output filenames\n");
            fprintf(stderr, "Usage: %s - dsk2imd <input.dsk> <output.imd>\n", argv[0]);
            return 1;
        }
        return handle_dsk_to_imd(argv[3], argv[4] ? argv[4] : "output.imd");
    }
    
    /* Handle mkfs command specially (doesn't need mounting) */
    if (strcmp(command, "mkfs") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: mkfs requires sides parameter (1 or 2)\n");
            print_usage(argv[0]);
            return 1;
        }
        
        int sides = atoi(argv[3]);
        if (sides != 1 && sides != 2) {
            fprintf(stderr, "Error: sides must be 1 (single) or 2 (double)\n");
            return 1;
        }
        
        return handle_mkfs(disk_path, sides);
    }    
    /* Determine if we need write access */
    int need_write = (strcmp(command, "put") == 0 || strcmp(command, "rm") == 0);
    
    /* Mount the MDOS filesystem */
    printf("Mounting MDOS disk: %s (%s mode)\n", 
           disk_path, need_write ? "read-write" : "read-only");
    
    mdos_fs_t *fs = mdos_mount(disk_path, !need_write);
    if (!fs) {
        fprintf(stderr, "Failed to mount MDOS disk: %s\n", disk_path);
        fprintf(stderr, "Make sure the file exists and is a valid MDOS disk image.\n");
        return 1;
    }
    
    int result = 0;
    
    /* Dispatch commands */
    if (strcmp(command, "ls") == 0) {
        result = handle_ls(fs);
    }
    else if (strcmp(command, "cat") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: cat command requires filename\n");
            result = 1;
        } else {
            result = handle_cat(fs, argv[3], 0); /* Normal mode */
        }
    }
    else if (strcmp(command, "rawcat") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: rawcat command requires filename\n");
            result = 1;
        } else {
            result = handle_cat(fs, argv[3], 1); /* Raw mode */
        }
    }
    else if (strcmp(command, "get") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: get command requires MDOS filename\n");
            result = 1;
        } else {
            const char *local_name = (argc > 4) ? argv[4] : argv[3];
            result = handle_get(fs, argv[3], local_name);
        }
    }
    else if (strcmp(command, "put") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: put command requires local filename\n");
            result = 1;
        } else {
            const char *mdos_name = (argc > 4) ? argv[4] : NULL;
            result = handle_put(fs, argv[3], mdos_name);
        }
    }
    else if (strcmp(command, "info") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: info command requires filename\n");
            result = 1;
        } else {
            result = handle_info(fs, argv[3]);
        }
    }
    else if (strcmp(command, "seek") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: seek command requires filename\n");
            result = 1;
        } else {
            result = handle_seek(fs, argv[3]);
        }
    }
    else if (strcmp(command, "free") == 0) {
        result = handle_free(fs);
    }
    else if (strcmp(command, "rm") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: rm command requires filename\n");
            result = 1;
        } else {
            result = handle_rm(fs, argv[3]);
        }
    }
    else {
        fprintf(stderr, "Error: Unknown command '%s'\n", command);
        print_usage(argv[0]);
        result = 1;
    }
    
    /* Clean up */
    int unmount_result = mdos_unmount(fs);
    if (unmount_result != MDOS_EOK) {
        print_error("unmount", unmount_result);
        if (result == 0) result = 1; /* Don't override existing error */
    }
    
    if (result == 0) {
        printf("\nOperation completed successfully.\n");
    }
    
    return result;
}
