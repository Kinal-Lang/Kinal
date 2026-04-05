#pragma once
#include "kn/platform.h"

typedef struct
{
    int argc;
    char **argv;
} KnArgs;

size_t kn_strlen(const char *s);
int kn_strncmp(const char *a, const char *b, size_t n);
int kn_strcmp(const char *a, const char *b);
void kn_memcpy(void *dst, const void *src, size_t n);
void kn_memset(void *dst, int c, size_t n);
void *kn_malloc(size_t n);
void kn_free(void *p);
void *kn_realloc(void *p, size_t n);
void kn_append(char *buf, size_t cap, size_t *off, const char *s);

// ----------------------
// Temporary arena allocation (for tools like the LSP server)
//
// When enabled, kn_malloc allocates from a bump arena, and kn_free becomes a no-op.
// Use begin/end to scope allocations and avoid unbounded growth in long-running processes.
// ----------------------
void kn_temp_arena_begin(void);
void kn_temp_arena_end(void);

void kn_write_str(const char *s);
void kn_write_out_str(const char *s);
void kn_write_out_buf(const char *s, size_t len);
void kn_write_u32(uint32_t v);
void kn_die(const char *msg);

char *kn_read_file(const char *path, size_t *out_size);
char *kn_read_stdin(size_t *out_size);
KnArgs kn_parse_cmdline(void);


