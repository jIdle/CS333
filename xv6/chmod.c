#ifdef CS333_P5
#include "types.h"
#include "user.h"
int
main(int argc, char *argv[])
{
  if(argc != 3) {
    printf(2, "\n%s:%d: Error: Incorrect number of arguments.\n", __FILE__, __LINE__);
    exit();
  }
  int mode = atoo(argv[1]);
  char * path = argv[2];
  if(chmod(path, mode)) {
    printf(2, "\n%s:%d: Error: System call 'chmod' return failure.\n", __FILE__, __LINE__);
    exit();
  }
  printf(1, "\nSuccesfully changed file mode.\n");
  exit();
}
#endif
