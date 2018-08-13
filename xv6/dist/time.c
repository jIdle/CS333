#ifdef CS333_P2
#include "types.h"
#include "user.h"
int
main(int argc, char *argv[])
{
    char * argv2[argc];
    argv2[argc - 1] = 0;
    for(int i = 0; i < argc - 1; ++i){
        argv2[i] = argv[i + 1];
    }
    uint start = uptime();
    if(fork() == 0){
        exec(argv2[0], argv2);
        exit();
    }
    wait();
    uint total = uptime() - start;
    printf(1, "\n%s ran in %d.%d seconds.\n", argv2[0], total/1000, total%1000);
    exit();
}
#endif
