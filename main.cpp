#include <dlfcn.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <unistd.h>

// static std::string fisk = "123123l12iuj3l k12j3lk1j 3lk12j 3l12k3j 123kjlfisk";

// static void *foo = malloc(123123);

void *test()
{
    return malloc(12);
}

void bar()
{
    malloc(13);
}

int main(int argc, char **argv)
{
    void *ret = test();
    bar();
    free(ret);
    // std::map<int, int> foobar;
    // for (int i=0; i<2; ++i) {
    //     foobar[i] = i;
    //     sleep(1);
    // }

    return 0;
}
