// Built only by the regression script in its isolated test copy.
#include "kernel/types.h"
#include "user/user.h"

// Exercise the actual implementation, with fault injection at its I/O and
// allocation boundary. Production find and the course grader are unchanged.
#define main   find_program_main
#define malloc checked_malloc
#define free   checked_free
#define read   checked_read
#include "../user/find.c"
#undef main
#undef malloc
#undef free
#undef read

int allocations_left = -1;
int reads_left = -1;
int short_read = 0;
int live_allocations = 0;

void *
checked_malloc(uint size)
{
  if (allocations_left == 0)
    return 0;
  if (allocations_left > 0)
    allocations_left--;
  void *p = malloc(size);
  if (p)
    live_allocations++;
  return p;
}

void
checked_free(void *p)
{
  if (p) {
    live_allocations--;
    free(p);
  }
}

int
checked_read(int fd, void *buf, int count)
{
  if (reads_left == 0)
    return -1;
  if (reads_left > 0)
    reads_left--;
  if (short_read) {
    short_read = 0;
    return read(fd, buf, 1);
  }
  return read(fd, buf, count);
}

int
available_descriptors(void)
{
  int fds[NOFILE];
  int count = 0;
  while (count < NOFILE) {
    int fd = open(".", O_RDONLY);
    if (fd < 0)
      break;
    fds[count++] = fd;
  }
  for (int i = 0; i < count; i++)
    close(fds[i]);
  return count;
}

int
fault_case(char *label, int allocation_limit, int read_limit, int short_result,
           int repetitions, int expect_failure)
{
  int pid = fork();
  int status = -1;
  if (pid < 0)
    return 1;
  if (pid == 0) {
    allocations_left = allocation_limit;
    reads_left = read_limit;
    short_read = short_result;
    int available = available_descriptors();
    for (int i = 0; i < repetitions; i++) {
      int result = find("rf", "missing", 0, 0);
      if ((result < 0) != expect_failure || live_allocations != 0 ||
          available_descriptors() != available)
        exit(1);
    }
    exit(0);
  }
  if (wait(&status) < 0 || status != 0) {
    printf("FAIL %s\n", label);
    return 1;
  }
  printf("PASS %s\n", label);
  return 0;
}

int
main_case(char *label, int mode, int expected_status)
{
  int pid = fork();
  int status = -1;
  if (pid < 0)
    return 1;
  if (pid == 0) {
    char *args[MAXARG + 5];
    int argc = 3;
    args[0] = "find";
    args[1] = "rf/a/f";
    args[2] = "f";
    if (mode == 0) {
      allocations_left = 0;
    } else {
      args[3] = "-exec";
      args[4] = mode == 3 ? "missing-command" : "echo";
      argc = mode == 3 ? 5 : mode == 1 ? MAXARG + 4 : MAXARG + 2;
      for (int i = 5; i < argc; i++)
        args[i] = "x";
    }
    args[argc] = 0;
    find_program_main(argc, args);
    exit(99);
  }
  if (wait(&status) < 0 || status != expected_status) {
    printf("FAIL %s\n", label);
    return 1;
  }
  printf("PASS %s\n", label);
  return 0;
}

int
main(void)
{
  int failed = 0;
  failed += fault_case("allocation-first", 0, -1, 0, 1, 1);
  failed += fault_case("allocation-after-first", 1, -1, 0, 1, 1);
  failed += fault_case("read-first", -1, 0, 0, 1, 1);
  failed += fault_case("read-after-entries", -1, 4, 0, 1, 1);
  failed += fault_case("short-directory-read", -1, -1, 1, 1, 1);
  failed += fault_case("repeat-no-leaks", -1, -1, 0, 100, 0);
  failed += main_case("main-allocation-failure", 0, 1);
  failed += main_case("MAXARG-rejected", 1, 1);
  failed += main_case("MAXARG-boundary", 2, 0);
  failed += main_case("exec-failure-status", 3, 1);
  exit(failed != 0);
}
