#include <map>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
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

    for (std::vector<char*>::const_iterator it = files.begin(); it != files.end(); ++it) {
        if (start || end) {
            int time;
            if (sscanf(*it, "%d.malloc", &time) != 1) {
                fprintf(stderr, "I don't understand this filename %s\n", *it);
                continue;
            }
            if (end && time > end)
                continue;
            if (time > end || time + interval < start)
                continue;
        }
        FILE *f = fopen(*it, "r");
        if (!f) {
            fprintf(stderr, "I can't open %s for read\n", *it);
            continue;
        }
        char *buf;
        while (!feof(f)) {

            // if (vfsscanf(line, "%d.malloc", &time) != 1) {

            // }
            // int64_t time = mktime(&t);
            // if (time != -1) {
            //     mCurrent->data.push_back(time);
            //     mStart = std::min(time, mStart);
            //     mEnd = std::max(time, mEnd);

            //     // int ret = sscanf("%d.malloc, const char *format, ...);
            // }

        }
    }

    return 0;
}
