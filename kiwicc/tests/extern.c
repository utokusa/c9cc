#include <stdarg.h>

int ext1 = 3;
int *ext2;
int ext3 = 6;
int ret_777() { return 777; }
int static_fn() { return 5; }

int false_fn() { return 512; }
int true_fn() { return 513; }

int add_all1(int x, ...)
{
  va_list ap;
  va_start(ap, x);
  for (;;)
  {
    int y = va_arg(ap, int);
    if (y == 0)
      return x;
    x += y;
  }
}

int add_all3(int x, int y, int z, ...)
{
  va_list ap;
  va_start(ap, z);
  x = x + y + z;
  for (;;)
  {
    int y = va_arg(ap, int);
    if (y == 0)
      return x;
    x += y;
  }
}
