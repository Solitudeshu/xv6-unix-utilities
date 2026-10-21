#include "kernel/types.h"
#include "kernel/riscv.h"
#include "user/user.h"

#define NPAGES  64
#define MSG     "Here it is: "
#define MSG_LEN (sizeof(MSG) - 1)

static int
alnum(char c)
{
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z');
}

int
main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  char *base = sbrk((NPAGES + 1) * PGSIZE);
  if (base == SBRK_ERROR)
    exit(1);

  // Keep only complete pages inside the memory returned by sbrk.
  uint64 begin = PGROUNDUP((uint64)base);
  uint64 end = (uint64)base + (NPAGES + 1) * PGSIZE;

  for (uint64 page = begin; page + PGSIZE <= end; page += PGSIZE) {
    char *p = (char *)page;

    // secret.c places MSG at page base + 32 + the first character's ASCII.
    for (int ch = '0'; ch <= 'z'; ch++) {
      if (!alnum((char)ch))
        continue;

      int offset = 32 + ch;
      if (memcmp(p + offset, MSG, MSG_LEN) != 0)
        continue;

      char *secret = p + offset + MSG_LEN;
      if (secret[0] != ch)
        continue;

      int n = 0;
      while ((uint64)(secret + n) < page + PGSIZE && alnum(secret[n]))
        n++;

      if (n > 0 && (uint64)(secret + n) < page + PGSIZE && secret[n] == '\0') {
        write(1, secret, n);
        write(1, "\n", 1);
        exit(0);
      }
    }
  }

  exit(1);
}
