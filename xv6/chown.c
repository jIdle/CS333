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
    int uid = atoi(argv[1]);
    char * path = argv[2];
    if(chown(path, uid)) {
        printf(2, "\nError: System call 'chown' return failure. %s at line %d\n", __FILE__, __LINE__);
        exit();
    }
    printf(1, "\nSuccesfully changed file owner.\n");
    exit();
}
#endif
