#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

void
find(char *path, char *target)
{
  // TODO: implement recursive traversal
}

int
main(int argc, char *argv[])
{
  if (argc != 3) {
    fprintf(2, "Usage: find path name\n");
    exit(1);
  }

  find(argv[1], argv[2]);
  exit(0);
}

