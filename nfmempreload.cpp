#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <dlfcn.h>

static pthread_mutex_t sMutex;
typedef void * (*OriginalMalloc)(size_t);
OriginalMalloc originalMalloc;
typedef void * (*OriginalRealloc)(void *, size_t);
OriginalRealloc originalRealloc;
typedef void (*OriginalFree)(void *);
OriginalFree originalFree;

struct Node {
    Node() : pointer(0), size(0), next(-1), prev(-1) {}
    void clear() { pointer = 0; size = 0; next = prev = -1; }

    void *pointer;
    size_t size;
    int next, prev;
};

static Node sNodes[1024 * 1024];
static size_t sSize = 0;
static size_t sCount = 0;
static int sFirst = -1;

static void add(void *ptr, size_t size)
{
    sSize += size;
    ++sCount;
    if (sFirst == -1) {
        sFirst = 0;
        sNodes[0].pointer = ptr;
        sNodes[0].size = size;
    } else {
        int idx = sFirst;
        int prev = -1;
        while (true) {
            Node &node = sNodes[idx];
            assert(node.pointer);
            if (node.next != -1) {
                prev = idx;
                idx = node.next;
            } else {
                for (size_t i=0; i<sizeof(sNodes) / sizeof(sNodes[0]); ++i) {
                    if (!sNodes[i].pointer) {
                        sNodes[i].pointer = ptr;
                        sNodes[i].size = size;
                        sNodes[i].prev = prev;
                        node.next = i;
                        break;
                    }
                }
                break;
            }
        }
    }
}

static void remove(void *ptr)
{
    int idx = sFirst;
    int prev = -1;
    --sCount;
    while (true) {
        Node &node = sNodes[idx];
        assert(node.pointer);
        if (node.pointer == ptr) {
            sSize -= node.size;
            if (prev == -1) {
                sFirst = node.next;
            } else {
                Node &prevNode = sNodes[prev];
                prevNode.next = node.next;
            }
            if (node.next != -1) {
                Node &next = sNodes[node.next];
                next.prev = node.prev;
            }
            node.clear();
            break;
        }
    }
}

class Scope
{
public:
    Scope()
    {
        static pthread_mutex_t sControlMutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&sControlMutex);
        if (!originalMalloc) {
            originalMalloc = reinterpret_cast<OriginalMalloc>(dlsym(RTLD_NEXT, "malloc"));
            originalRealloc = reinterpret_cast<OriginalRealloc>(dlsym(RTLD_NEXT, "realloc"));
            originalFree = reinterpret_cast<OriginalFree>(dlsym(RTLD_NEXT, "free"));
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_init(&sMutex, &attr);
            pthread_mutexattr_destroy(&attr);
        }
        pthread_mutex_unlock(&sControlMutex);
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
    void *ret = originalMalloc(size);
    if (ret)
        add(ret, size);
    return ret;
}

void *realloc(void *ptr, size_t size)
{
    Scope scope;
    if (ptr)
        remove(ptr);
    ptr = originalRealloc(ptr, size);
    add(ptr, size);
    return ptr;
}

void free(void *ptr)
{
    Scope scope;
    if (ptr)
        remove(ptr);
    originalFree(ptr);
}

void dumpAllocations()
{
    Scope scope;
    printf("MALLOC: %u in %u allocations\n", sSize, sCount);
    int idx = sFirst;
    int i = 0;
    size_t total = 0;
    while (idx != -1) {
        const Node &node = sNodes[idx];
        printf("  %d/%u: %p %d bytes\n", ++i, sCount, node.pointer, node.size);
        total += node.size;
        idx = node.next;
    }
    assert(total == sSize);
}
}
