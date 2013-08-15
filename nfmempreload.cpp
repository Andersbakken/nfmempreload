#include <stdio.h>
#include <map>
#include <string>
#include <assert.h>
#include <dlfcn.h>

static std::map<void*, size_t> sAllocations;
static size_t sCurrent = 0;
static pthread_mutex_t sMutex;
static int sLevel = 0;
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

static Node nodes[1024 * 1024];
static int first = -1;

static void add(void *ptr, size_t size)
{
    sCurrent += size;
    if (first == -1) {
        first = 0;
        nodes[0].pointer = ptr;
        nodes[0].size = size;
    } else {
        int idx = first;
        int prev = -1;
        while (true) {
            Node &node = nodes[idx];
            assert(node.pointer);
            if (node.next != -1) {
                prev = idx;
                idx = node.next;
            } else {
                for (size_t i=0; i<sizeof(nodes) / sizeof(nodes[0]); ++i) {
                    if (!nodes[i].pointer) {
                        nodes[i].pointer = ptr;
                        nodes[i].size = size;
                        nodes[i].prev = prev;
                        node.next = i;
                        break;
                    }
                }
                break;
            }
        }
    }
}

static size_t remove(void *ptr)
{
    int idx = first;
    int prev = -1;
    while (true) {
        Node &node = nodes[idx];
        assert(node.pointer);
        if (node.pointer == ptr) {
            if (prev == -1) {
                first = node.next;
            } else {
                Node &prevNode = nodes[prev];
                prevNode.next = node.next;
            }
            if (node.next != -1) {
                Node &next = nodes[node.next];
                next.prev = node.prev;
            }
            node.clear();
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
        ++sLevel;
    }

    ~Scope()
    {
        --sLevel;
        pthread_mutex_unlock(&sMutex);
    }

    bool isReentrant() const
    {
        assert(sLevel > 0);
        return sLevel > 1;
    }
};

extern "C" {
void *malloc(size_t size)
{
    Scope scope;
    if (scope.isReentrant()) {
        void *ret = originalMalloc(size);
        assert(ret);
        return ret;
    }
    void *ret = originalMalloc(size);
    if (ret) {
        // assert(sAllocations.find(ret) == sAllocations.end());
        // sAllocations[ret] = size;
        // sCurrent += size;
    }
    assert(ret);
    return ret;
}

void *realloc(void *ptr, size_t size)
{
    Scope scope;
    if (scope.isReentrant()) {
        void *ret = originalRealloc(ptr, size);
        assert(ret);
        return ret;
    }
    if (ptr) {
        // std::map<void*, size_t>::iterator it = sAllocations.find(ptr);
        // assert(it != sAllocations.end());
        // sCurrent -= it->second;
        // sAllocations.erase(it);
    }

    // assert(sAllocations.find(ptr) == sAllocations.end());
    void *ret = originalRealloc(ptr, size);
    if (ret) {
        // assert(sAllocations.find(ret) == sAllocations.end());
        // sAllocations[ret] = size;
        // sCurrent += size;
    }
    assert(ret);
    return ret;
}

void free(void *ptr)
{
    Scope scope;
    if (scope.isReentrant()) {
        originalFree(ptr);
        return;
    }
    // assert(ptr);
    if (ptr) {
        // std::map<void*, size_t>::iterator it = sAllocations.find(ptr);
        // assert(it != sAllocations.end());
        // sCurrent -= it->second;
        // sAllocations.erase(it);
    }
    originalFree(ptr);
}

void dumpAllocations()
{
    Scope scope;
    assert(!scope.isReentrant());
    // size_t current;
    // {
    //     Scope scope;
    //     // ++sLevel;
    //     allocations = sAllocations;
    //     current = sCurrent;
    //     // --sLevel;
    // }

    printf("MALLOC: %u in %u allocations\n", sCurrent, sAllocations.size());
    int idx = 0;
    size_t total = 0;
    for (std::map<void*, size_t>::const_iterator it = sAllocations.begin(); it != sAllocations.end(); ++it) {
        printf("  %d/%u: %p %d bytes\n", ++idx, sAllocations.size(), it->first, it->second);
        total += it->second;
    }
    assert(total == sCurrent);
}
}
