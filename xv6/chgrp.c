#ifdef CS333_P5
#include "types.h"
#include "user.h"
int
main(int argc, char *argv[])
{
    if(argc != 3) {
        printf(2, "\nError: Incorrect number of arguments. %s at line %d\n", __FILE__, __LINE__);
        exit();
    }
    int gid = atoi(argv[1]);
    char * path = argv[2];
    if(chgrp(path, gid)) {
        printf(2, "\nError: System call 'chgrp' return failure. %s at line %d\n", __FILE__, __LINE__);
        exit();
    }
    printf(1, "\nSuccesfully changed file group.\n");
    exit();
}
#endif
