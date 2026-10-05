#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
static int frames, pushes, roots, globals, allocations, strings;
void *__kn_gc_alloc(uint64_t size) { allocations++; return calloc(1, (size_t)(size ? size : 1)); }
void *__kn_gc_push_frame(void) { frames++; pushes++; return (void*)(uintptr_t)frames; }
void __kn_gc_add_root(void *frame, void *slot, uint64_t size) { if(frame && slot && size) roots++; }
void __kn_gc_add_global_root(void *slot, uint64_t size) { if(slot && size) globals++; }
void __kn_gc_pop_frame(void *frame) { if((uintptr_t)frame!=(uintptr_t)frames)abort(); frames--; }
void __kn_gc_init(void) { }
void __kn_gc_collect(void) { }
char *__kn_str_concat(const char *a, const char *b) { size_t x=a?strlen(a):0,y=b?strlen(b):0; char *p=__kn_gc_alloc(x+y+1); strings++; if(x)memcpy(p,a,x); if(y)memcpy(p+x,b,y); return p; }
char *kn_test_any_to_string(uint64_t tag, uint64_t payload) { char *p=__kn_gc_alloc(64); strings++; if(tag==1)snprintf(p,64,"%lld",(long long)payload); else abort(); return p; }
void *__kn_exc_get(void) { return NULL; }
bool __kn_exc_has(void) { return false; }
void *__kn_exc_last(void) { return NULL; }
void __kn_exc_set(void *e) { (void)e; abort(); }
void __kn_exc_clear(void) { }
void __kn_exc_push(const char *trace) { (void)trace; }
char *__kn_exc_trace(void) { return ""; }
extern void __kn_entry(int64_t *);
int main(void) { int64_t result=0; __kn_entry(&result); if(result!=42) {fprintf(stderr,"result=%lld\n",(long long)result);return 1;} return frames==0 && pushes>0 && roots>0 && globals>0 && allocations>0 && strings>=3 ? 0:2; }
