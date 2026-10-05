#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
static int frames, pushes, roots, globals, allocations, strings, collections;
void *__kn_gc_alloc(uint64_t size) { allocations++; return calloc(1, (size_t)(size ? size : 1)); }
void *__kn_gc_push_frame(void) { frames++; pushes++; return (void*)(uintptr_t)frames; }
void __kn_gc_add_root(void *frame, void *slot, uint64_t size) { if(frame && slot && size) roots++; }
void __kn_gc_add_global_root(void *slot, uint64_t size) { if(slot && size) globals++; }
void __kn_gc_pop_frame(void *frame) { if((uintptr_t)frame!=(uintptr_t)frames)abort(); frames--; }
void __kn_gc_init(void) { }
void __kn_gc_collect(void) { collections++; }
char *__kn_str_concat(const char *a, const char *b) { size_t x=a?strlen(a):0,y=b?strlen(b):0; char *p=__kn_gc_alloc(x+y+1); strings++; if(x)memcpy(p,a,x); if(y)memcpy(p+x,b,y); return p; }
char *kn_test_any_to_string(uint64_t tag, uint64_t payload) { char *p=__kn_gc_alloc(64); strings++; if(tag==1)snprintf(p,64,"%lld",(long long)payload); else abort(); return p; }
static void *current_exception, *last_exception;
void *__kn_exc_get(void) { return current_exception; }
bool __kn_exc_has(void) { return current_exception != NULL; }
void *__kn_exc_last(void) { return last_exception; }
void __kn_exc_set(void *e) { current_exception = last_exception = e; }
void __kn_exc_clear(void) { current_exception = NULL; }
void __kn_exc_push(const char *trace) { (void)trace; }
char *__kn_exc_trace(void) { return ""; }
typedef struct { uint64_t tag, payload, count; } TestList;
void *__kn_list_new(void) { return __kn_gc_alloc(sizeof(TestList)); }
void __kn_list_add(void *p, uint64_t tag, uint64_t payload) { TestList *list=p; list->tag=tag; list->payload=payload; list->count++; }
uint64_t __kn_list_count(void *p) { return ((TestList*)p)->count; }
bool __kn_list_contains(void *p, uint64_t tag, uint64_t payload) { TestList *list=p; return list->count && list->tag==tag && list->payload==payload; }
extern void __kn_entry(int64_t *);
int main(void) { int64_t result=0; __kn_entry(&result); if(result!=42) {fprintf(stderr,"result=%lld\n",(long long)result);return 1;} return frames==0 && pushes>0 && roots>0 && globals>0 && allocations>0 && strings>=3 && collections==1 ? 0:2; }
