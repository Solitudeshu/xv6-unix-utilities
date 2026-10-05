#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

struct sixfive_parser {
  char *digits;
  int length;
  int capacity;
  int invalid;
  int remainder;
};

void
parser_init(struct sixfive_parser *p)
{
  p->digits = 0;
  p->length = 0;
  p->capacity = 0;
  p->invalid = 0;
  p->remainder = 0;
}

// kết thúc token khi gặp dấu phân cách hoặc EOF
void
parser_finish(struct sixfive_parser *p)
{
  if (!p->invalid && p->length > 0 &&
      (p->remainder % 5 == 0 || p->remainder % 6 == 0)) {
    int first = 0;

    // giữ lại một chữ số 0 nếu toàn bộ số đều là 0
    while (first + 1 < p->length && p->digits[first] == '0')
      first++;

    p->digits[p->length] = '\0';
    printf("%s\n", p->digits + first);
  }

  p->length = 0;
  p->invalid = 0;
  p->remainder = 0;
}

// nhận thêm một đoạn dữ liệu. Trả về -1 nếu không thể cấp phát bộ nhớ
int
parser_feed(struct sixfive_parser *p, const char *buf, int n)
{
  for (int i = 0; i < n; i++) {
    char c = buf[i];

    if (strchr(" -\r\t\n./,", c) != 0) {
      parser_finish(p);
      continue;
    }

    if (c < '0' || c > '9') {
      p->invalid = 1;
      continue;
    }

    if (p->invalid)
      continue;

    if (p->length + 1 >= p->capacity) {
      int new_capacity;
      char *new_digits;

      if (p->capacity == 0) {
        new_capacity = 64;
      } else {
        if (p->capacity > 0x7fffffff / 2)
          return -1;
        new_capacity = p->capacity * 2;
      }

      new_digits = malloc(new_capacity);
      if (new_digits == 0)
        return -1;

      if (p->length > 0)
        memmove(new_digits, p->digits, p->length);

      if (p->digits != 0)
        free(p->digits);

      p->digits = new_digits;
      p->capacity = new_capacity;
    }

    p->digits[p->length++] = c;
    p->remainder = (p->remainder * 10 + c - '0') % 30;
  }

  return 0;
}

void
parser_destroy(struct sixfive_parser *p)
{
  if (p->digits != 0)
    free(p->digits);
  parser_init(p);
}
