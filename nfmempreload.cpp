#include <assert.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

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

static void init()
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&sMutex, &attr);
    pthread_mutexattr_destroy(&attr);
    if (!sOriginalMalloc)
        sOriginalMalloc = reinterpret_cast<OriginalMalloc>(dlsym(RTLD_NEXT, "malloc"));
    if (!sOriginalRealloc)
        sOriginalRealloc = reinterpret_cast<OriginalRealloc>(dlsym(RTLD_NEXT, "realloc"));
    if (!sOriginalFree)
        sOriginalFree = reinterpret_cast<OriginalFree>(dlsym(RTLD_NEXT, "free"));
}

static pthread_once_t sOnce = PTHREAD_ONCE_INIT;

class Scope
{
public:
    Scope()
    {
        pthread_once(&sOnce, init);
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
    const time_t t = time - (time % interval);
    if (t != sLastFileOpen) {
        sLastFileOpen = t;
        if (sFile)
            fclose(sFile);
        static const char *prefix = getenv("NFLD_PREFIX");
        char buf[32];
        static const int pid = getpid();
        snprintf(buf, sizeof(buf), "%s%d_%ld.malloc", prefix ? prefix : "", pid, t);
        sFile = fopen(buf, "w");
    }
    assert(sFile);
    return sFile;
}

#define DUMP_STACK()                                                    \
    do {                                                                \
        static int stackSize = -1;                                      \
        if (stackSize == -1)                                            \
            if (const char *env = getenv("NFLD_STACKSIZE"))             \
                stackSize = atoi(env);                                  \
        if (stackSize == -1)                                            \
            stackSize = 30;                                             \
        if (stackSize) {                                                \
            void *stack[stackSize];                                     \
            const int count = backtrace(stack, stackSize);              \
            fprintf(f, "[%p", stack[1]);                                \
            for (int i=2; i<count; ++i)                                 \
                fprintf(f, " %p", stack[i]);                            \
            fprintf(f, "]\n");                                          \
            static const bool symbolsEnabled = getenv("NFLD_SYMBOLS");  \
            if (symbolsEnabled) {                                       \
                char **symbols = backtrace_symbols(stack, count);       \
                for (int i=1; i<count; ++i)                             \
                    fprintf(f, "  %d/%d %s\n", i, count, symbols[i]);   \
                free(symbols);                                          \
            }                                                           \
        }                                                               \
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
        fprintf(f, "%ld malloc %p %d ", time, ret, static_cast<int>(size));
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
        if (ptr) {
            fprintf(f, "%ld realloc %p => %p %d ", time, ptr, ret, static_cast<int>(size));
        } else {
            fprintf(f, "%ld realloc 0x0 => %p %d ", time, ret, static_cast<int>(size));
        }
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
