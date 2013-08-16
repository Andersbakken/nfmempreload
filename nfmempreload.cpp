#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <dlfcn.h>

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
static bool sEnable = true;
static bool sStart = true;

static std::map<void*, size_t> sMap;
static void add(void *ptr, size_t size)
{
    if (!sEnable)
        return;
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
    if (!sEnable)
        return;
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
        if (sStart) {
            sStart = false;
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
        if (!sNodes) {
            sNodes = reinterpret_cast<Node*>(0x1);
            sNodes = new Node[NodeCount];
        }
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
    Scope scope;
    if (ptr)
        remove(ptr);
    sOriginalFree(ptr);
}

void dumpAllocations()
{
    Scope scope;
    sEnable = false;
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
    sEnable = true;

}
}
