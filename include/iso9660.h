#ifndef FPSU_ISO9660_H
#define FPSU_ISO9660_H

#include <stddef.h>

int iso9660_read_file(const char *iso_path, const char *inner_path,
    unsigned char **out_data, size_t *out_size);

#endif
