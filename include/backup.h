#ifndef FPSU_BACKUP_H
#define FPSU_BACKUP_H

#include <stddef.h>

int backup_copy_file(const char *title_id, const char *source_path, char *backup_path, size_t backup_path_size);
int backup_restore_file(const char *backup_path, const char *target_path);
int backup_restore_all_for_title(const char *title_id, int *restored_count);
int backup_find_latest(char *title_id, size_t title_id_size, char *target_path, size_t target_path_size, char *backup_path, size_t backup_path_size);
int backup_find_latest_for_title(const char *title_id, char *target_path, size_t target_path_size, char *backup_path, size_t backup_path_size);
int backup_path_is_safe_target(const char *path);

#endif
