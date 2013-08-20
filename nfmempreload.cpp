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

// struct Node {
//     Node() : pointer(0), size(0), next(-1), prev(-1) {}
//     void clear() { pointer = 0; size = 0; next = prev = -1; }

//     void *pointer;
//     size_t size;
//     int next, prev;
// };

// enum { NodeCount = 1024 * 1024 };
// static Node *sNodes = 0;
static size_t sSize = 0;
// static size_t sCount = 0;
// static int sFirst = -1;
struct Entry {
    Entry() : size(0), time(0) {}
    size_t size;
    enum { Stacksize = 32 };
    void *stack[Stacksize];
    time_t time;
};
static bool sEnable = true;
static std::map<void*, Entry> *sMap = 0;
static void add(void *ptr, size_t size)
{
    if (!sEnable)
        return;
    sEnable = false;
    if (!sMap)
        sMap = new std::map<void*, Entry>();
    sSize += size;
    Entry &entry = (*sMap)[ptr];
    entry.size = size;
    ret->mSize = ::backtrace(ret->mStack, MAX_STACK_SIZE);

    entry.add
    sEnable = true;
}

static void remove(void *ptr)
{
    if (!sEnable)
        return;
    sEnable = false;
    if (!sMap)
        sMap = new std::map<void*, size_t>();
    std::map<void*, size_t>::iterator it = sMap->find(ptr);
    if (it != sMap->end()) {
        // assert(it != sMap->end());
        --sSize -= it->second;
        sMap->erase(it);
    }
    sEnable = true;
}

class Scope
{
public:
    Scope()
    {
        static pthread_mutex_t sControlMutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&sControlMutex);
        if (!sMap) {
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_init(&sMutex, &attr);
            pthread_mutexattr_destroy(&attr);
        }
        pthread_mutex_unlock(&sControlMutex);

        if (!sOriginalMalloc)
            sOriginalMalloc = reinterpret_cast<OriginalMalloc>(dlsym(RTLD_NEXT, "malloc"));
        if (!sOriginalRealloc)
            sOriginalRealloc = reinterpret_cast<OriginalRealloc>(dlsym(RTLD_NEXT, "realloc"));
        if (!sOriginalFree)
            sOriginalFree = reinterpret_cast<OriginalFree>(dlsym(RTLD_NEXT, "free"));
        pthread_mutex_lock(&sMutex);
    }

    ~Scope()
    {
        pthread_mutex_unlock(&sMutex);
    }
};

extern "C" {
void *malloc(size_t size)
{
    Scope scope;
    void *ret = sOriginalMalloc(size);
    if (ret)
        add(ret, size);
    return ret;
}

void *realloc(void *ptr, size_t size)
{
    Scope scope;
    if (ptr)
        remove(ptr);
    ptr = sOriginalRealloc(ptr, size);
    add(ptr, size);
    return ptr;
}

void free(void *ptr)
{
    if (ptr) {
        Scope scope;
        if (ptr)
            remove(ptr);
        sOriginalFree(ptr);
    }
}

void dumpAllocations(const char *file)
{
    FILE *f = fopen(file, "w");
    Scope scope;
    sEnable = false;
    int i = 0;
    size_t total = 0;
    for (std::map<void*, size_t>::const_iterator it = sMap->begin(); it != sMap->end(); ++it) {
        printf("  %d/%u: %p %d bytes\n", ++i, sMap->size(), it->first, it->second);
        total += it->second;
    }
    printf("MALLOC: %u in %u allocations\n", sSize, sMap->size());
    // if (total != sSize) {
    //     printf("%d vs %d\n", total, sSize);
    // }
    // assert(total == sSize);
    sEnable = true;
}

size_t totalAllocations()
{
    Scope scope;
    return sSize;
}
}
