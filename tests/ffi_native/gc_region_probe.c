/* Exercise the actual legacy scanner without depending on native-stack roots.
 * Bytes outside the logical region remain valid storage, so a bad scan is
 * detected by unwanted marks rather than by a crash or allocator behavior. */
#include "../../libs/runtime/src/kn_runtime.c"

static int check_list_slots(void)
{
    KnList *list = __kn_list_new();
    uint64_t tag, payload;
    for (uint64_t i = 1; i <= 4; i++) __kn_list_add(list, KN_ANY_TAG_I64, i);
    __kn_list_remove_at(list, 1, &tag, &payload);
    if (tag != KN_ANY_TAG_I64 || payload != 2 || list->payloads[1] != 3 ||
        list->payloads[3] || list->tags[3]) return 3;
    __kn_list_pop(list, &tag, &payload);
    if (tag != KN_ANY_TAG_I64 || payload != 4 || list->payloads[2] || list->tags[2]) return 4;
    __kn_list_clear(list);
    for (uint64_t slot = 0; slot < list->cap; slot++)
        if (list->payloads[slot] || list->tags[slot]) return 5;
    return list->count != 0;
}

int main(void)
{
    unsigned char objects[3] = {0};
    KnGcBlock blocks[3] = {0};
    KnGcBlock *index[3];
    KnGcBlock *stack[3] = {0};
    _Alignas(uintptr_t) unsigned char storage[3 * sizeof(uintptr_t) + 1] = {0};
    for (int i = 0; i < 3; ++i)
    {
        blocks[i].ptr = &objects[i];
        blocks[i].size = 1;
        index[i] = &blocks[i];
    }

    for (size_t alignment = 0; alignment < 2; ++alignment)
    {
        unsigned char *region = storage + alignment;
        for (int i = 0; i < 3; ++i)
        {
            uintptr_t value = (uintptr_t)&objects[i];
            rt_memcpy(region + i * sizeof(value), &value, sizeof(value));
        }
        for (size_t size = 0; size <= 3 * sizeof(uintptr_t); ++size)
        {
            int sp = 0;
            for (int i = 0; i < 3; ++i) blocks[i].marked = 0;
            gc_scan_region(region, size, index, 3, stack, &sp, 3);
            int expected = (int)(size / sizeof(uintptr_t));
            if (sp != expected)
            {
                fprintf(stderr, "region size=%zu offset=%zu: marked=%d expected=%d\n",
                        size, alignment, sp, expected);
                return 1;
            }
            for (int i = 0; i < 3; ++i)
                if (blocks[i].marked != (i < expected)) return 2;
        }
    }
    int sp = 0;
    gc_scan_region(0, sizeof(uintptr_t), index, 3, stack, &sp, 3);
    return sp ? 1 : check_list_slots();
}
