#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "user/user.h"

struct path_entry {
  struct path_entry *next;
  char path[];
};

char *
basename(char *path)
{
  char *name = path;

  // main removes trailing slashes but preserves the root directory.
  if (strcmp(path, "/") == 0)
    return path;
  for (char *p = path; *p; p++) {
    if (*p == '/')
      name = p + 1;
  }
  return name;
}

int
run_command(char *path, char **command, int command_argc)
{
  char *args[MAXARG];
  int i;
  int pid;
  int status;

  for (i = 0; i < command_argc; i++)
    args[i] = command[i];

  args[command_argc] = path;
  args[command_argc + 1] = 0;

  pid = fork();
  if (pid < 0) {
    fprintf(2, "find: fork failed\n");
    return -1;
  }

  if (pid == 0) {
    for (i = 3; i < NOFILE; i++)
      close(i);

    exec(args[0], args);
    fprintf(2, "find: exec %s failed\n", args[0]);
    exit(1);
  }

  if (wait(&status) < 0) {
    fprintf(2, "find: wait failed\n");
    return -1;
  }
  return status == 0 ? 0 : -1;
}

// Inspect and close the path before entering the recursive traversal.
int
path_type(char *path)
{
  struct stat st;
  int fd = open(path, O_RDONLY);

  if (fd < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return -1;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return -1;
  }
  close(fd);
  return st.type;
}

// Snapshot child paths on the heap, preserving directory entry order.
// No parent directory descriptor remains open while visiting a child.
int
read_children(char *path, struct path_entry **children)
{
  struct dirent de;
  struct path_entry *tail = 0;
  char name[DIRSIZ + 1];
  int fd, n, failed = 0;
  int length = strlen(path);
  int slash = length > 0 && path[length - 1] != '/';

  *children = 0;
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return -1;
  }

  while ((n = read(fd, &de, sizeof(de))) == sizeof(de)) {
    if (de.inum == 0)
      continue;

    memmove(name, de.name, DIRSIZ);
    name[DIRSIZ] = '\0';
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    int size = length + slash + strlen(name) + 1;
    if (size > MAXPATH) {
      fprintf(2, "find: path too long under %s\n", path);
      failed = 1;
      continue;
    }

    struct path_entry *entry = malloc(sizeof(*entry) + size);
    if (entry == 0) {
      fprintf(2, "find: out of memory\n");
      failed = 1;
      break;
    }
    entry->next = 0;
    strcpy(entry->path, path);
    if (slash)
      entry->path[length] = '/';
    strcpy(entry->path + length + slash, name);

    if (tail)
      tail->next = entry;
    else
      *children = entry;
    tail = entry;
  }

  if (n != 0 && n != sizeof(de)) {
    fprintf(2, "find: cannot read %s\n", path);
    failed = 1;
  }
  close(fd);
  return failed ? -1 : 0;
}

int
find(char *path, char *target, char **command, int command_argc)
{
  struct path_entry *children = 0;
  int failed = 0;
  int type = path_type(path);

  if (type < 0)
    return -1;
  if (strcmp(basename(path), target) == 0) {
    if (command == 0)
      printf("%s\n", path);
    else if (run_command(path, command, command_argc) < 0)
      failed = 1;
  }

  if (type == T_DIR) {
    if (read_children(path, &children) < 0)
      failed = 1;
    while (children) {
      struct path_entry *entry = children;
      children = entry->next;
      if (find(entry->path, target, command, command_argc) < 0)
        failed = 1;
      free(entry);
    }
  }
  return failed ? -1 : 0;
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

  int length = strlen(argv[1]);
  while (length > 1 && argv[1][length - 1] == '/')
    length--;
  if (length >= MAXPATH) {
    fprintf(2, "find: path too long\n");
    exit(1);
  }

  char *path = malloc(length + 1);
  if (path == 0) {
    fprintf(2, "find: out of memory\n");
    exit(1);
  }
  memmove(path, argv[1], length);
  path[length] = '\0';

  int status = find(path, argv[2], command, command_argc);
  free(path);
  exit(status < 0 ? 1 : 0);
}
