#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <stdlib.h>

static pthread_mutex_t sMutex;
typedef void * (*OriginalMalloc)(size_t);
static OriginalMalloc sOriginalMalloc = reinterpret_cast<OriginalMalloc>(dlsym(RTLD_NEXT, "malloc"));
typedef void * (*OriginalRealloc)(void *, size_t);
static OriginalRealloc sOriginalRealloc = reinterpret_cast<OriginalRealloc>(dlsym(RTLD_NEXT, "realloc"));
typedef void (*OriginalFree)(void *);
static OriginalFree sOriginalFree = reinterpret_cast<OriginalFree>(dlsym(RTLD_NEXT, "free"));
static FILE *sFile = 0;
static time_t sLastFileOpen = 0;
static bool sEnable = true;
static bool sStarted = false;

class Scope
{
public:
    Scope()
    {
        static pthread_mutex_t sControlMutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&sControlMutex);
        if (!sStarted) {
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_init(&sMutex, &attr);
            pthread_mutexattr_destroy(&attr);
            sStarted = true;
            if (!sOriginalMalloc)
                sOriginalMalloc = reinterpret_cast<OriginalMalloc>(dlsym(RTLD_NEXT, "malloc"));
            if (!sOriginalRealloc)
                sOriginalRealloc = reinterpret_cast<OriginalRealloc>(dlsym(RTLD_NEXT, "realloc"));
            if (!sOriginalFree)
                sOriginalFree = reinterpret_cast<OriginalFree>(dlsym(RTLD_NEXT, "free"));
        }
        pthread_mutex_unlock(&sControlMutex);
        pthread_mutex_lock(&sMutex);
    }

    ~Scope()
    {
        pthread_mutex_unlock(&sMutex);
    }
};


FILE *file(time_t &time)
{
    static int interval = 0;
    if (!interval) {
        if (const char *env = getenv("NFLD_INTERVAL"))
            interval = atoi(env);
        if (!interval)
            interval = 60;
    }
    time = ::time(0);
    const time_t t = ::time(0) / interval;
    if (t != sLastFileOpen) {
        sLastFileOpen = t;
        if (sFile)
            fclose(sFile);
        static const char *prefix = getenv("NFLD_PREFIX");
        char buf[32];
        snprintf(buf, sizeof(buf), "%s%ld.malloc", prefix ? prefix : "", t);
        sFile = fopen(buf, "w");
    }
    assert(sFile);
    return sFile;
}

enum { STACK_SIZE = 64 };
#define DUMP_STACK()                                    \
    do {                                                \
        void *stack[STACK_SIZE];                        \
        const int count = backtrace(stack, STACK_SIZE); \
        fprintf(f, "[%p", stack[1]);                    \
        for (int i=2; i<count; ++i)                     \
            fprintf(f, " %p", stack[i]);                \
        fprintf(f, "]\n");                              \
    } while (false)


extern "C" {
void *malloc(size_t size)
{
    Scope scope;
    void *ret = sOriginalMalloc(size);
    if (sEnable) {
        sEnable = false;
        time_t time;
        FILE *f = file(time);
        fprintf(f, "%ld malloc %p %d ", time, ret, size);
        DUMP_STACK();
        sEnable = true;
    }
    return ret;
}

void *realloc(void *ptr, size_t size)
{
    Scope scope;
    void *ret = sOriginalRealloc(ptr, size);
    if (sEnable) {
        sEnable = false;
        time_t time;
        FILE *f = file(time);
        fprintf(f, "%ld realloc 0x%x => %p %d ", time, reinterpret_cast<unsigned int>(ptr), ret, size);
        DUMP_STACK();
        sEnable = true;
    }
    return ret;
}

void free(void *ptr)
{
    Scope scope;
    if (ptr) {
        sOriginalFree(ptr);
        if (sEnable) {
            sEnable = false;
            time_t time;
            FILE *f = file(time);
            fprintf(f, "%ld free %p ", time, ptr);
            DUMP_STACK();
            sEnable = true;
        }
    }
}
}
