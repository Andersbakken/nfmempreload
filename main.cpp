#include <dlfcn.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    void *foo = malloc(20);
    foo = realloc(foo, 30);
    typedef void (*DumpAllocations)();
    DumpAllocations dumpAllocations = reinterpret_cast<DumpAllocations>(dlsym(RTLD_NEXT, "dumpAllocations"));
    for (int i=0; i<100; ++i) {
        void *foo = 0;
        foo = realloc(foo, i + 1);
    }
    if (dumpAllocations)
        dumpAllocations();

    free(foo);
    if (dumpAllocations)
        dumpAllocations();
    return 0;
}