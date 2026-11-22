#ifndef KN_FUNC_INDEX_H
#define KN_FUNC_INDEX_H

#include "kn/hir.h"
#include "kn/util.h"
#include <limits.h>

typedef struct
{
    const char *key;
    int first, last;
} FuncNameGroup;

typedef struct
{
    FuncNameGroup *groups;
    int *next;
    unsigned mask;
} FuncNameIndex;

// Groups keep declaration order, including specializations registered while checking a body.
static inline FuncNameGroup *funcindex_slot(const FuncNameIndex *index, const char *name)
{
    unsigned hash = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p)
        hash = (hash ^ *p) * 16777619u;
    unsigned slot = hash & index->mask;
    while (index->groups[slot].key && kn_strcmp(index->groups[slot].key, name) != 0)
        slot = (slot + 1) & index->mask;
    return &index->groups[slot];
}

static inline int funcindex_first(const FuncNameIndex *index, const char *name)
{
    if (!index->groups || !name) return -1;
    const FuncNameGroup *group = funcindex_slot(index, name);
    return group->key ? group->first : -1;
}

static inline void funcindex_add(FuncNameIndex *index, const char *name, int item)
{
    FuncNameGroup *group = funcindex_slot(index, name);
    if (group->key)
        index->next[group->last] = item;
    else
    {
        group->key = name;
        group->first = item;
    }
    group->last = item;
    index->next[item] = -1;
}

static inline void funcindex_free(FuncNameIndex *index)
{
    kn_free(index->groups);
    kn_free(index->next);
}

static inline void funcindex_resize(FuncNameIndex *index, int capacity)
{
    funcindex_free(index);
    size_t slots = 2 * (size_t)capacity;
    index->groups = (FuncNameGroup *)kn_malloc(slots * sizeof(FuncNameGroup));
    index->next = (int *)kn_malloc((size_t)capacity * sizeof(int));
    if (!index->groups || !index->next) kn_die("out of memory");
    index->mask = (unsigned)(slots - 1);
    kn_memset(index->groups, 0, slots * sizeof(FuncNameGroup));
}

static inline int funcindex_capacity(int required)
{
    int capacity = 16;
    while (capacity < required)
    {
        if (capacity > INT_MAX / 2) kn_die("function index is too large");
        capacity *= 2;
    }
    return capacity;
}

// Ordinals map directly into the final FuncList, including materialized generics.
void kn_hir_lower_calls_indexed(FuncList *funcs, ClassList *classes, InterfaceList *interfaces,
                                StmtList *globals, const FuncNameIndex *symbols);

#endif
