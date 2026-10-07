#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data, int len);

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    printf("Example 1:\n");
    int a[2] = {61810, 2026};
    memdump("ii", (char *)a, sizeof(a));

    printf("Example 2:\n");
    memdump("S", "a string", sizeof("a string"));

    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *)&s, sizeof(s));

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;

    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");

    printf("Example 4:\n");
    memdump("pihcS", (char *)&example, sizeof(example));

    printf("Example 5:\n");
    memdump("sccccc", (char *)&example, sizeof(example));
  } else if (argc == 2) {
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while (n < sizeof(data)) {
      int nn = read(0, data + n, sizeof(data) - n);
      if (nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data, n);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void
memdump(char *fmt, char *data, int len)
{
  for (; *fmt != '\0'; fmt++) {
    int size;

    switch (*fmt) {
    case 'i':
      size = 4;
      break;
    case 'p':
    case 's':
      size = 8;
      break;
    case 'h':
      size = 2;
      break;
    case 'c':
    case 'S':
      size = 1;
      break;
    default:
      printf("memdump: unknown format '%c'\n", *fmt);
      return;
    }

    if (len < size) {
      printf("memdump: not enough data for '%c'\n", *fmt);
      return;
    }

    switch (*fmt) {
    case 'i': {
      int value;
      memmove(&value, data, sizeof(value));
      printf("%d\n", value);
      break;
    }
    case 'p': {
      uint64 value;
      memmove(&value, data, sizeof(value));
      printf("%lx\n", value);
      break;
    }
    case 'h': {
      short value;
      memmove(&value, data, sizeof(value));
      printf("%d\n", (int)value);
      break;
    }
    case 'c':
      printf("%c\n", *data);
      break;
    case 's': {
      char *value;
      memmove(&value, data, sizeof(value));
      printf("%s\n", value);
      break;
    }
    case 'S':
      for (int i = 0; i < len && data[i] != '\0'; i++)
        printf("%c", data[i]);
      printf("\n");
      return;
    }

    data += size;
    len -= size;
  }
}
