#ifndef KN_FREESTANDING_H
#define KN_FREESTANDING_H

#include "kn/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Core helpers emitted or expected by freestanding builds. */
KN_API void __kn_panic(const char *message);
KN_API uint64_t __kn_strlen(const char *text);
KN_API uint8_t __kn_str_eq(const char *lhs, const char *rhs);
KN_API int32_t __kn_mem_compare(const uint8_t *lhs, const uint8_t *rhs, uint64_t count);

/* Optional allocator hooks for --runtime alloc and --runtime gc. */
KN_API void * __kn_gc_alloc(uint64_t size);
KN_API char * __kn_str_concat(const char *lhs, const char *rhs);

/* Optional GC hooks for --runtime gc. */
KN_API void __kn_gc_collect(void);
KN_API void * __kn_gc_push_frame(void);
KN_API void __kn_gc_add_root(void *frame, void *slot, uint64_t size);
KN_API void __kn_gc_pop_frame(void *frame);
KN_API void __kn_gc_init(void);

#ifdef __cplusplus
}
#endif

#endif
