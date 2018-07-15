#ifdef CS333_P2
#include "types.h"
#include "user.h"
int
main(void)
{
    printf(1, "\nStarting time command...\n");

    if(fork() == 0){
        char * argv[3];
        argv[0] = "echo";
        argv[1] = "hello";
        argv[2] = 0;
        exec("echo", argv);
        exit();
    }
    wait();
    if(fork() == 0){
        char * argv[3];
        argv[0] = "mkdir";
        argv[1] = "booty";
        argv[2] = 0;
        exec("mkdir", argv);
        exit();
    }
    printf(1, "\nFinished.\n");

    exit();
}

#endif
