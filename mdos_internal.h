/*
 * MDOS Filesystem Library - Internal Header
 * Copyright (C) 2025
 * 
 * Internal functions and structures shared between MDOS modules
 */

#ifndef MDOS_INTERNAL_H
#define MDOS_INTERNAL_H

#include "mdos_fs.h"

/* Internal disk I/O functions (mdos_diskio.c) */
void mdos_getsect(mdos_fs_t *fs, uint8_t *buf, int sect);
void mdos_putsect(mdos_fs_t *fs, uint8_t *buf, int sect);
int mdos_alloc_space(mdos_fs_t *fs, uint8_t *cat, mdos_rib_t *rib, int sects);

/* Internal utility functions (mdos_utils.c) */
int mdos_calc_sects(mdos_rib_t *rib);
int mdos_lsn_to_psn(mdos_rib_t *rib, int lsn);
int mdos_normalize_filename(const char *input, char *output);
int mdos_hash_filename(const char *name);
int mdos_find_file(mdos_fs_t *fs, const char *filename, int *type, int delete_entry);
int mdos_write_directory_entry(mdos_fs_t *fs, const char *filename, int rib_sector, int file_type);

/* Internal file functions (mdos_file.c) */
mdos_file_t* mdos_get_file_handle(mdos_fs_t *fs, int fd);
int mdos_allocate_fd(mdos_fs_t *fs);
void mdos_free_fd(mdos_fs_t *fs, int fd);

#endif /* MDOS_INTERNAL_H */
