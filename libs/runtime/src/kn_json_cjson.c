#include "cJSON.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define KN_JSON_TLS __declspec(thread)
#else
#define KN_JSON_TLS __thread
#endif

typedef struct
{
    cJSON *root;
    cJSON **nodes;
    int count;
    int capacity;
} KnJsonDocument;

static KN_JSON_TLS char g_kn_json_error[512];

void *__kn_gc_alloc(size_t size);

static void kn_json_set_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_kn_json_error, sizeof(g_kn_json_error), fmt, ap);
    g_kn_json_error[sizeof(g_kn_json_error) - 1] = 0;
    va_end(ap);
}

static void kn_json_clear_error(void)
{
    g_kn_json_error[0] = 0;
}

static const char *kn_json_safe(const char *text)
{
    return text ? text : "";
}

static char *kn_json_strdup_gc(const char *text)
{
    size_t len;
    char *copy;
    if (!text)
        text = "";
    len = strlen(text);
    copy = (char *)__kn_gc_alloc(len + 1);
    if (!copy)
        return 0;
    memcpy(copy, text, len + 1);
    return copy;
}

static int kn_json_doc_add_node(KnJsonDocument *doc, cJSON *node)
{
    cJSON **next_nodes;
    int next_capacity;
    if (!doc || !node)
        return -1;
    if (doc->count >= doc->capacity)
    {
        next_capacity = doc->capacity ? doc->capacity * 2 : 32;
        next_nodes = (cJSON **)realloc(doc->nodes, sizeof(cJSON *) * (size_t)next_capacity);
        if (!next_nodes)
            return -1;
        doc->nodes = next_nodes;
        doc->capacity = next_capacity;
    }
    doc->nodes[doc->count] = node;
    doc->count++;
    return doc->count - 1;
}

static int kn_json_doc_index_tree(KnJsonDocument *doc, cJSON *node)
{
    cJSON *child;
    if (kn_json_doc_add_node(doc, node) < 0)
        return 0;
    child = node ? node->child : 0;
    while (child)
    {
        if (!kn_json_doc_index_tree(doc, child))
            return 0;
        child = child->next;
    }
    return 1;
}

static int kn_json_doc_find_id(KnJsonDocument *doc, cJSON *node)
{
    int index;
    if (!doc || !node)
        return -1;
    for (index = 0; index < doc->count; index++)
        if (doc->nodes[index] == node)
            return index;
    return -1;
}

static cJSON *kn_json_doc_get(KnJsonDocument *doc, int node_id)
{
    if (!doc || node_id < 0 || node_id >= doc->count)
        return 0;
    return doc->nodes[node_id];
}

static void kn_json_format_parse_error(const char *input, const char *error_ptr)
{
    ptrdiff_t offset = 0;
    char preview[96];
    size_t i = 0;
    if (error_ptr && input)
        offset = error_ptr - input;
    if (offset < 0)
        offset = 0;

    if (error_ptr)
    {
        while (*error_ptr && *error_ptr != '\r' && *error_ptr != '\n' && i + 1 < sizeof(preview))
            preview[i++] = *error_ptr++;
    }
    preview[i] = 0;

    if (preview[0])
        kn_json_set_error("JSON parse error at byte %lld near '%s'", (long long)offset, preview);
    else
        kn_json_set_error("JSON parse error at byte %lld", (long long)offset);
}

void *__kn_json_doc_parse(const char *text)
{
    const char *error_ptr = 0;
    cJSON *root;
    KnJsonDocument *doc;

    kn_json_clear_error();
    if (!text)
    {
        kn_json_set_error("JSON text cannot be null");
        return 0;
    }

    root = cJSON_ParseWithOpts(text, &error_ptr, 0);
    if (!root)
    {
        kn_json_format_parse_error(text, error_ptr);
        return 0;
    }

    doc = (KnJsonDocument *)calloc(1, sizeof(KnJsonDocument));
    if (!doc)
    {
        cJSON_Delete(root);
        kn_json_set_error("Out of memory while creating JSON document");
        return 0;
    }

    doc->root = root;
    if (!kn_json_doc_index_tree(doc, root))
    {
        cJSON_Delete(root);
        free(doc->nodes);
        free(doc);
        kn_json_set_error("Out of memory while indexing JSON document");
        return 0;
    }
    return doc;
}

void __kn_json_doc_free(void *handle)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    if (!doc)
        return;
    cJSON_Delete(doc->root);
    free(doc->nodes);
    free(doc);
}

const char *__kn_json_last_error(void)
{
    return g_kn_json_error;
}

int __kn_json_doc_root(void *handle)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    if (!doc || doc->count <= 0)
        return -1;
    return 0;
}

int __kn_json_node_kind(void *handle, int node_id)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    cJSON *node = kn_json_doc_get(doc, node_id);
    double number;
    if (!node)
        return -1;
    if (cJSON_IsNull(node))
        return 0;
    if (cJSON_IsBool(node))
        return 1;
    if (cJSON_IsNumber(node))
    {
        number = node->valuedouble;
        if (number >= (double)INT_MIN && number <= (double)INT_MAX && floor(number) == number)
            return 2;
        return 3;
    }
    if (cJSON_IsString(node))
        return 4;
    if (cJSON_IsObject(node))
        return 5;
    if (cJSON_IsArray(node))
        return 6;
    return -1;
}

int __kn_json_node_bool(void *handle, int node_id)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    cJSON *node = kn_json_doc_get(doc, node_id);
    if (!node || !cJSON_IsBool(node))
        return 0;
    return cJSON_IsTrue(node) ? 1 : 0;
}

int __kn_json_node_int(void *handle, int node_id)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    cJSON *node = kn_json_doc_get(doc, node_id);
    if (!node || !cJSON_IsNumber(node))
        return 0;
    return node->valueint;
}

double __kn_json_node_float(void *handle, int node_id)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    cJSON *node = kn_json_doc_get(doc, node_id);
    if (!node || !cJSON_IsNumber(node))
        return 0.0;
    return node->valuedouble;
}

const char *__kn_json_node_string(void *handle, int node_id)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    cJSON *node = kn_json_doc_get(doc, node_id);
    if (!node || !cJSON_IsString(node))
        return "";
    return kn_json_strdup_gc(kn_json_safe(node->valuestring));
}

int __kn_json_object_count(void *handle, int node_id)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    cJSON *node = kn_json_doc_get(doc, node_id);
    if (!node || !cJSON_IsObject(node))
        return 0;
    return cJSON_GetArraySize(node);
}

const char *__kn_json_object_key_at(void *handle, int node_id, int index)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    cJSON *node = kn_json_doc_get(doc, node_id);
    cJSON *child;
    if (!node || !cJSON_IsObject(node) || index < 0)
        return "";
    child = cJSON_GetArrayItem(node, index);
    if (!child || !child->string)
        return "";
    return kn_json_strdup_gc(child->string);
}

int __kn_json_object_value_at(void *handle, int node_id, int index)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    cJSON *node = kn_json_doc_get(doc, node_id);
    cJSON *child;
    if (!node || !cJSON_IsObject(node) || index < 0)
        return -1;
    child = cJSON_GetArrayItem(node, index);
    if (!child)
        return -1;
    return kn_json_doc_find_id(doc, child);
}

int __kn_json_array_count(void *handle, int node_id)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    cJSON *node = kn_json_doc_get(doc, node_id);
    if (!node || !cJSON_IsArray(node))
        return 0;
    return cJSON_GetArraySize(node);
}

int __kn_json_array_item_at(void *handle, int node_id, int index)
{
    KnJsonDocument *doc = (KnJsonDocument *)handle;
    cJSON *node = kn_json_doc_get(doc, node_id);
    cJSON *child;
    if (!node || !cJSON_IsArray(node) || index < 0)
        return -1;
    child = cJSON_GetArrayItem(node, index);
    if (!child)
        return -1;
    return kn_json_doc_find_id(doc, child);
}

const char *__kn_json_format_text(const char *text, int pretty)
{
    const char *error_ptr = 0;
    cJSON *root;
    char *printed;
    const char *result;

    kn_json_clear_error();
    if (!text)
    {
        kn_json_set_error("JSON text cannot be null");
        return 0;
    }

    root = cJSON_ParseWithOpts(text, &error_ptr, 0);
    if (!root)
    {
        kn_json_format_parse_error(text, error_ptr);
        return 0;
    }

    if (pretty)
        printed = cJSON_Print(root);
    else
        printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!printed)
    {
        kn_json_set_error("Failed to format JSON text");
        return 0;
    }

    result = kn_json_strdup_gc(printed);
    free(printed);
    if (!result)
    {
        kn_json_set_error("Out of memory while copying formatted JSON text");
        return 0;
    }
    return result;
}

const char *__kn_json_format_number(double value)
{
    char buffer[64];

    if (isnan(value) || isinf(value))
    {
        kn_json_set_error("JSON does not support NaN or Infinity");
        return 0;
    }

    buffer[0] = 0;
    snprintf(buffer, sizeof(buffer), "%.17g", value);
    buffer[sizeof(buffer) - 1] = 0;
    return kn_json_strdup_gc(buffer);
}
