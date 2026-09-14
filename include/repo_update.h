#ifndef FPSU_REPO_UPDATE_H
#define FPSU_REPO_UPDATE_H

#include <stddef.h>

int repo_update_databases(char *message, size_t message_size);
int repo_update_patches(char *message, size_t message_size);

#endif
