/*
 * MDOS Filesystem Library
 * Copyright (C) 2025
 * 
 * This library provides POSIX-like file operations for MDOS filesystems
 * used on Motorola EXORciser systems.
 */

#ifndef MDOS_FS_H
#define MDOS_FS_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

/* MDOS filesystem constants */
#define MDOS_SECTOR_SIZE 128
#define MDOS_CLUSTER_SIZE (MDOS_SECTOR_SIZE * 4)
#define MDOS_MAX_OPEN_FILES 16
#define MDOS_MAX_FILENAME 13  /* 8.2 format + null terminator */

/* Special sectors */
#define MDOS_SECTOR_ID 0
#define MDOS_SECTOR_CAT 1      /* Allocation bitmap */
#define MDOS_SECTOR_LCAT 2     /* Bad block bitmap */
#define MDOS_SECTOR_DIR 3
#define MDOS_SECTOR_DIR_SIZE 20

/* File types */
#define MDOS_TYPE_USER_DEFINED 0
#define MDOS_TYPE_UNKNOWN_1 1
#define MDOS_TYPE_IMAGE 2
#define MDOS_TYPE_OBJECT 3
#define MDOS_TYPE_UNKNOWN_4 4
#define MDOS_TYPE_ASCII 5
#define MDOS_TYPE_UNKNOWN_6 6
#define MDOS_TYPE_ASCII_CONVERTED 7

/* File attributes */
#define MDOS_ATTR_WRITE_PROTECT 0x80
#define MDOS_ATTR_DELETE_PROTECT 0x40
#define MDOS_ATTR_SYSTEM 0x20
#define MDOS_ATTR_CONT 0x10
#define MDOS_ATTR_COMPR 0x08

/* Open flags */
#define MDOS_O_RDONLY 0x01
#define MDOS_O_WRONLY 0x02
#define MDOS_O_RDWR   0x03
#define MDOS_O_CREAT  0x04
#define MDOS_O_TRUNC  0x08

/* Seek whence values */
#define MDOS_SEEK_SET 0
#define MDOS_SEEK_CUR 1
#define MDOS_SEEK_END 2

/* Error codes */
#define MDOS_EOK        0
#define MDOS_ENOENT    -1   /* File not found */
#define MDOS_ENOSPC    -2   /* No space left */
#define MDOS_EMFILE    -3   /* Too many open files */
#define MDOS_EBADF     -4   /* Bad file descriptor */
#define MDOS_EINVAL    -5   /* Invalid argument */
#define MDOS_EIO       -6   /* I/O error */
#define MDOS_EEXIST    -7   /* File exists */
#define MDOS_EPERM     -8   /* Operation not permitted */

/* Forward declarations */
typedef struct mdos_fs mdos_fs_t;
typedef struct mdos_file mdos_file_t;

/* MDOS RIB (Record Information Block) structure */
typedef struct {
    uint8_t sdw[114];       /* Segment descriptor words */
    uint8_t blank[3];
    uint8_t last_size;      /* Size of last sector */
    uint8_t size_high;      /* File size high byte */
    uint8_t size_low;       /* File size low byte */
    uint8_t addr_high;      /* Load address high byte */
    uint8_t addr_low;       /* Load address low byte */
    uint8_t pc_high;        /* Program counter high byte */
    uint8_t pc_low;         /* Program counter low byte */
    uint8_t blank_1[4];
} mdos_rib_t;

/* MDOS directory entry structure */
typedef struct {
    uint8_t name[8];        /* Filename (space-padded) */
    uint8_t suffix[2];      /* File extension */
    uint8_t sector_high;    /* RIB sector high byte */
    uint8_t sector_low;     /* RIB sector low byte */
    uint8_t attr_high;      /* Attributes high byte */
    uint8_t attr_low;       /* Attributes low byte */
    uint8_t blank[2];
} mdos_dirent_t;

/* File information structure */
typedef struct {
    char name[MDOS_MAX_FILENAME];
    int type;
    int size;               /* Size in bytes */
    int sectors;            /* Size in sectors */
    uint16_t load_addr;     /* Load address (for executable files) */
    uint16_t start_addr;    /* Start address (for executable files) */
    uint8_t attributes;     /* File attributes */
    int rib_sector;         /* Sector containing RIB */
} mdos_file_info_t;

/* Filesystem handle */
struct mdos_fs {
    FILE *disk;
    char *disk_path;
    int read_only;
    mdos_file_t *open_files[MDOS_MAX_OPEN_FILES];
    int next_fd;
};

/* File handle */
struct mdos_file {
    mdos_fs_t *fs;
    int fd;
    char name[MDOS_MAX_FILENAME];
    int flags;
    int type;
    int rib_sector;
    int file_size;          /* File size in bytes */
    int sectors;            /* File size in sectors */
    int position;           /* Current file position */
    uint8_t last_size;      /* Size of last sector */
    mdos_rib_t rib;         /* Cached RIB */
    int dirty;              /* File has been modified */
};

/* Core filesystem functions */
mdos_fs_t* mdos_mount(const char *disk_path, int read_only);
int mdos_unmount(mdos_fs_t *fs);
int mdos_sync(mdos_fs_t *fs);

/* File operations */
int mdos_open(mdos_fs_t *fs, const char *filename, int flags, int type);
int mdos_close(mdos_fs_t *fs, int fd);
ssize_t mdos_read(mdos_fs_t *fs, int fd, void *buf, size_t count);
ssize_t mdos_read_raw(mdos_fs_t *fs, int fd, void *buf, size_t count);
ssize_t mdos_write(mdos_fs_t *fs, int fd, const void *buf, size_t count);
off_t mdos_lseek(mdos_fs_t *fs, int fd, off_t offset, int whence);

/* Filesystem creation */
int mdos_mkfs(const char *disk_path, int sides);

/* Directory operations */
int mdos_readdir(mdos_fs_t *fs, mdos_file_info_t **files, int *count);
int mdos_stat(mdos_fs_t *fs, const char *filename, mdos_file_info_t *info);
int mdos_unlink(mdos_fs_t *fs, const char *filename);

/* File creation */
int mdos_create_file(mdos_fs_t *fs, const char *filename, int file_type, const void *data, size_t size);

/* Utility functions */
int mdos_free_space(mdos_fs_t *fs);
const char* mdos_strerror(int error);
int mdos_validate_filename(const char *filename);
int mdos_extract_filename(const char *local_path, char *mdos_name);

/* High-level tool functions (mdos_tools.c) */
int mdos_list_files(mdos_fs_t *fs, FILE *output);
int mdos_cat_file(mdos_fs_t *fs, const char *filename, FILE *output, int raw_mode);
int mdos_export_file(mdos_fs_t *fs, const char *mdos_name, const char *local_name);
int mdos_import_file(mdos_fs_t *fs, const char *local_name, const char *mdos_name_arg);
int mdos_test_seek(mdos_fs_t *fs, const char *filename, FILE *output);
int mdos_file_info(mdos_fs_t *fs, const char *filename, FILE *output);

/* Image conversion functions (mdos_cvt.c) */
int mdos_convert_imd_to_dsk(const char *imd_filename, const char *dsk_filename);
int mdos_convert_dsk_to_imd(const char *dsk_filename, const char *imd_filename);

#endif /* MDOS_FS_H */
