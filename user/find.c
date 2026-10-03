#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char *
basename(char *path)
{
  char *p;

  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  return p + 1;
}

void
find(char *path, char *target)
{
  int fd;
  struct stat st;
  char buf[512], *p;
  struct dirent de;
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }


  if (strcmp(basename(path), target) == 0)
    printf("%s\n", path);

  if (st.type != T_DIR) {
    close(fd);
    return;
  }

  if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
    fprintf(2, "find: path too long\n");
    close(fd);
    return;
  }

  strcpy(buf, path);
  p = buf + strlen(buf);

  if (p > buf && *(p - 1) != '/')
    *p++ = '/';

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;

    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = '\0';

    if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
      continue;

    find(buf, target);
  }

  close(fd);  close(fd);
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

