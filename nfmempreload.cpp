#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <dlfcn.h>
#include <map>

static pthread_mutex_t sMutex;
typedef void * (*OriginalMalloc)(size_t);
OriginalMalloc sOriginalMalloc = reinterpret_cast<OriginalMalloc>(dlsym(RTLD_NEXT, "malloc"));
typedef void * (*OriginalRealloc)(void *, size_t);
OriginalRealloc sOriginalRealloc = reinterpret_cast<OriginalRealloc>(dlsym(RTLD_NEXT, "realloc"));
typedef void (*OriginalFree)(void *);
OriginalFree sOriginalFree = reinterpret_cast<OriginalFree>(dlsym(RTLD_NEXT, "free"));

extern "C" {
void *malloc(size_t size)
{
    void *ret = sOriginalMalloc(size);
    if (size >= 2 * 1024 * 1024) {
        printf("BIG ALLOC in malloc %d\n", size);
    }
    return ret;
}

void *realloc(void *ptr, size_t size)
{
    ptr = sOriginalRealloc(ptr, size);
    if (size >= 2 * 1024 * 1024) {
        printf("BIG ALLOC in realloc %d\n", size);
    }
    return ptr;
}

void free(void *ptr)
{
    if (ptr) {
        sOriginalFree(ptr);
    }
}
}
