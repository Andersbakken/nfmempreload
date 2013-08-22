#include <map>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

struct Node {
    int size;
    std::string stack;
};
int main(int argc, char **argv)
{
    std::map<void *, Node> data;
    time_t start = 0;
    time_t end = 0;
    int interval = 60;
    std::vector<char*> files;
    for (int i=1; i<argc; ++i) {
        if (!strcmp("--start", argv[i])) {
            start = atoi(argv[++i]);
        } else if (!strcmp("--end", argv[i])) {
            end = atoi(argv[++i]);
        } else if (!strcmp("--interval", argv[i])) {
            interval = atoi(argv[++i]);
        } else {
            files.push_back(argv[i]);
        }
    }

    int idx = 0;
    const int total = files.size();
    for (std::vector<char*>::const_iterator it = files.begin(); it != files.end(); ++it) {
        if (start || end) {
            int time;
            if (sscanf(*it, "%d.malloc", &time) != 1) {
                fprintf(stderr, "I don't understand this filename %s\n", *it);
                continue;
            }
            if (end && time > end)
                continue;
            if (time + interval < start)
                continue;
        }
        FILE *f = fopen(*it, "r");
        if (!f) {
            fprintf(stderr, "I can't open %s for read\n", *it);
            continue;
        }
        ++idx;
        printf("Reading %s (%d/%d) %02f%%\n", *it, idx, total, (static_cast<double>(idx) / static_cast<double>(total)) * 100.);
        char buf[1024];
        memset(buf, 0, sizeof(buf));
        int line = 0;
        while (fgets(buf, sizeof(buf), f)) {
            ++line;
            char *ch;
            const time_t t = strtoull(buf, &ch, 10);
            if (*ch++ != ' ') {
                fprintf(stderr, "Can't parse line %d of %s: '%s' err %d\n", line, *it, buf, __LINE__);
                continue;
            }
            if (t < start)
                continue;
            if (end && t > end)
                continue;

            enum Mode {
                Free,
                Malloc,
                Realloc
            } mode;
            switch (*ch) {
            case 'f':
                mode = Free;
                ch += 5;
                break;
            case 'm':
                mode = Malloc;
                ch += 7;
                break;
            case 'r':
                mode = Realloc;
                ch += 8;
                break;
            default:
                fprintf(stderr, "Can't parse line %d of %s: '%s' err %d\n", line, *it, buf, __LINE__);
                continue;
            }
            ch += 2; // skip past 0x
            char *end;
            void *ptr = reinterpret_cast<void *>(strtoull(ch, &end, 16));
            if (*end != ' ') {
                fprintf(stderr, "Can't parse line %d of %s: '%s' err %d\n", line, *it, buf, __LINE__);
                continue;
            }
            ch = end;

            int size = -1;
            switch (mode) {
            case Free:
                if (ptr)
                    data.erase(ptr);
                continue;
            case Malloc:
                size = strtoul(++ch, &end, 10);
                if (*end != ' ') {
                    fprintf(stderr, "Can't parse line %d of %s: '%s' err %d\n", line, *it, buf, __LINE__);
                    continue;
                }
                ch = end;
                break;
            case Realloc:
                if (ptr)
                    data.erase(ptr);
                if (strncmp(++ch, "=> ", 3)) {
                    fprintf(stderr, "Can't parse line %d of %s: '%s' err %d\n", line, *it, buf, __LINE__);
                    continue;
                }
                ptr = reinterpret_cast<void *>(strtoull(ch + 3, &end, 16));
                if (*end != ' ') {
                    fprintf(stderr, "Can't parse line %d of %s: '%s' err %d\n", line, *it, buf, __LINE__);
                    continue;
                }
                ch = end + 1;
                size = strtoul(ch, &end, 10);
                if (*end != ' ') {
                    fprintf(stderr, "Can't parse line %d of %s: '%s' err %d\n", line, *it, buf, __LINE__);
                    continue;
                }
                ch = end;
                break;
            }

            Node &node = data[ptr];
            if (node.size) {
                fprintf(stderr, "Data is busted. I already had something for %p\n", ptr);
                return 1;
            }
            node.size = size;

            if (*++ch != '[') {
                fprintf(stderr, "Can't parse line %d of %s: '%s' err %d\n", line, *it, buf, __LINE__);
                continue;
            }
            end = strchr(++ch, ']');
            if (!end) {
                fprintf(stderr, "Can't parse line %d of %s: '%s' err %d\n", line, *it, buf, __LINE__);
                continue;
            }
            ++ch;
            node.stack.assign(ch, end - ch - 1);
        }
    }
    unsigned long long tot = 0;
    for (std::map<void *, Node>::const_iterator it = data.begin(); it != data.end(); ++it) {
        printf("%p %d [%s]\n", it->first, it->second.size, it->second.stack.c_str());
        tot += it->second.size;
    }
    printf("Total: %lld\n", tot);


    return 0;
}
