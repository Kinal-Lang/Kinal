#pragma once
#include "kn/util.h"

typedef struct
{
    const char *path;
    const char *text;
    size_t len;
    size_t *line_offsets;
    int line_count;
} KnSource;

void kn_source_init(KnSource *src, const char *path, const char *text, size_t len);
void kn_source_free(KnSource *src);
const char *kn_source_get_line(const KnSource *src, int line, size_t *out_len);


