#include "kn/source.h"

void kn_source_init(KnSource *src, const char *path, const char *text, size_t len)
{
    if (!src) return;
    src->path = path ? path : "";
    src->text = text ? text : "";
    src->len = len;

    int count = 1;
    for (size_t i = 0; i < len; i++)
    {
        if (text[i] == '\n')
            count++;
    }
    src->line_offsets = (size_t *)kn_malloc(sizeof(size_t) * (size_t)count);
    if (!src->line_offsets)
    {
        src->line_count = 0;
        return;
    }
    int idx = 0;
    src->line_offsets[idx++] = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (text[i] == '\n' && idx < count)
            src->line_offsets[idx++] = i + 1;
    }
    src->line_count = count;
}

void kn_source_free(KnSource *src)
{
    if (!src) return;
    if (src->line_offsets)
        kn_free(src->line_offsets);
    src->line_offsets = 0;
    src->line_count = 0;
}

const char *kn_source_get_line(const KnSource *src, int line, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!src || !src->text || !src->line_offsets || line <= 0 || line > src->line_count)
        return "";
    size_t start = src->line_offsets[line - 1];
    size_t end = src->len;
    if (line < src->line_count)
        end = src->line_offsets[line];
    if (line == 1 && end >= start + 3 &&
        (unsigned char)src->text[start] == 0xEF &&
        (unsigned char)src->text[start + 1] == 0xBB &&
        (unsigned char)src->text[start + 2] == 0xBF)
    {
        start += 3;
    }
    if (end > start && src->text[end - 1] == '\n')
        end--;
    if (end > start && src->text[end - 1] == '\r')
        end--;
    if (out_len) *out_len = end - start;
    return src->text + start;
}


