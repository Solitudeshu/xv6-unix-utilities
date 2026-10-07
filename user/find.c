#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
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
run_command(char *path, char **command, int command_argc)
{
  char *args[MAXARG];
  int i;
  int pid;

  for (i = 0; i < command_argc; i++)
    args[i] = command[i];

  args[command_argc] = path;
  args[command_argc + 1] = 0;

  pid = fork();
  if (pid < 0) {
    fprintf(2, "find: fork failed\n");
    return;
  }

  if (pid == 0) {
    for (i = 3; i < NOFILE; i++)
      close(i);

    exec(args[0], args);
    fprintf(2, "find: exec %s failed\n", args[0]);
    exit(1);
  }

  wait(0);
}

void
find(char *path, char *target, char **command, int command_argc)
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

  if (strcmp(basename(path), target) == 0) {
    if (command == 0)
      printf("%s\n", path);
    else
      run_command(path, command, command_argc);
  }

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

    find(buf, target, command, command_argc);
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  char **command = 0;
  int command_argc = 0;

  if (argc == 3) {
    command = 0;
  } else if (argc >= 5 && strcmp(argv[3], "-exec") == 0) {
    command = &argv[4];
    command_argc = argc - 4;

    if (command_argc + 2 > MAXARG) {
      fprintf(2, "find: too many arguments\n");
      exit(1);
    }
  } else {
    fprintf(2, "Usage: find path name [-exec command [args...]]\n");
    exit(1);
  }

  find(argv[1], argv[2], command, command_argc);
  exit(0);
}
