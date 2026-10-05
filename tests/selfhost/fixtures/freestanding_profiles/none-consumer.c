#include <stdint.h>
extern void __kn_entry(int64_t *);
int main(void) { int64_t result=0; __kn_entry(&result); if(result!=42)return 1; __kn_entry(&result); return result==43 ? 0 : 2; }
