/* Exercise the actual legacy scanner without depending on native-stack roots.
 * Bytes outside the logical region remain valid storage, so a bad scan is
 * detected by unwanted marks rather than by a crash or allocator behavior. */
#include "../../libs/runtime/src/kn_runtime.c"

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
    return sp != 0;
}
