#!/bin/bash
set -e

TMP=$1
CC=$2
OUTPUT=$3

rm -rf $TMP
mkdir -p $TMP

kiwicc() {
  cat <<EOF > $TMP/$1
typedef struct FILE FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

typedef struct
{
  int gp_offset;
  int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
} va_list[1];

void *malloc(long size);
void *calloc(long nmemb, long size);
void *realloc(void *buf, long size);
int *__errno_location();
char *strerror(int errnum);
FILE *fopen(char *pathname, char *mode);
long fread(void *ptr, long size, long nmemb, FILE *stream);
int fclose(FILE *fp);
static void assert() {}
int strcmp(char *s1, char *s2);
int strncasecmp(char *s1, char *s2);
int printf(char *fmt, ...);
int sprintf(char *buf, char *fmt, ...);
int fprintf(FILE *fp, char *fmt, ...);
int vfprintf(FILE *fp, char *fmt, va_list ap);
long strlen(char *p);
int strncmp(char *p, char *q);
void *memcpy(char *dst, char *src, long n);
char *strndup(char *p, long n);
int isspace(int c);
int isdigit(int c);
int isxdigit(int c);
char *strstr(char *haystack, char *needle);
long strtoul(char *nptr, char **endptr, int base);
void exit(int code);
EOF

  grep -v '^#' kiwicc.h >> $TMP/$1
  grep -v '^#' $1 >> $TMP/$1
  sed -i 's/\bbool\b/_Bool/g' $TMP/$1
  sed -i 's/\berrno\b/*__errno_location()/g' $TMP/$1
  sed -i 's/\btrue\b/1/g; s/\bfalse\b/0/g;' $TMP/$1
  sed -i 's/\bNULL\b/0/g' $TMP/$1
  sed -i 's/\bva_start\b/__builtin_va_start/g' $TMP/$1

  (cd $TMP; ../$CC $1 > ${1%.c}.s)
  gcc -c -o $TMP/${1%.c}.o $TMP/${1%.c}.s
}

kiwicc main.c
kiwicc parse.c
kiwicc codegen.c
kiwicc tokenize.c
kiwicc type.c

(cd $TMP; gcc -static -o ../$OUTPUT *.o)
