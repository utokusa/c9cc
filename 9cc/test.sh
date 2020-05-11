#! /bin/bash
cat <<EOF | cc -xc -c -o tmp2.o -
int ret3() {return 3;}
int ret5() {return 5;}
int add(int x, int y) {return x+y;}
int sub(int x, int y) {return x-y;}
int add6(int a, int b, int c, int d, int e, int f) {return a+b+c+d+e+f;}
int retx(int x) {return x;}
EOF

assert() {
  expected="$1"
  input="$2"

  ./9cc "$input" > tmp.s
  cc -o tmp tmp.s tmp2.o
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 'int main() { return 0; }'
assert 42 'int main() { return 42; }'
assert 21 'int main() { return 5+20-4; }'
assert 5 'int main() { return 1+1+1+1+1; }'
assert 0 'int main() { return 1+1+1+1+1-1-1-1-1-1; }'
assert 41 'int main() { return 12 + 34 - 5; }'
assert 47 'int main() { return 5+6*7; }'
assert 15 'int main() { return 5*(9-6); }'
assert 4 'int main() { return (3+5)/2; }'
assert 8 'int main() { return -5 + 5 * 3 - 2 * 1; }'
assert 2 'int main() { return +1+1; }'
assert 1 'int main() { return 2==2; }'
assert 0 'int main() { return 1==2; }'
assert 1 'int main() { return 2!=1; }'
assert 0 'int main() { return 2!=2; }'
assert 1 'int main() { return 1<2; }'
assert 0 'int main() { return 1<1; }'
assert 1 'int main() { return 2<=2; }'
assert 0 'int main() { return 2<=1; }'
assert 0 'int main() { return 1>2; }'
assert 1 'int main() { return 2>1; }'
assert 1 'int main() { return 2>=2; }'
assert 0 'int main() { return 1>=2; }'
assert 1 'int main() { return 1 + 2 * 15 / 3 + 2 - 1 == 12 * 100 / 100; }'
assert 1 'int main() { return 1 + 2 * 15 / 3 + 2 - 1 > 12 * 100 / 100 - 1; }'
assert 1 'int main() { int a; int b; return a=1; b=2; b-a; }'
assert 50 'int main() { int a=1000; int b=20; int c; return c=a/b; }'
assert 100 'int main() { int a=1000; int b=20; int c=a/b; return (c + 50) * (10 - 9); }'
assert 5 'int main() { int num = 1; return num * 5; }'
assert 5 'int main() { int num0 = 1; int num1 = num0 > 0; return num0 * num1 * 5; }'
assert 10 'int main() { int _NUM_TEST; return _NUM_TEST = 10; }'
assert 10 'int main() { 0; return 10; }'
assert 10 'int main() { return 10; return 20; }'
assert 5 'int main() { int num0; int num1; num0 = 1; num1 = num0 > 0; return num0 * num1 * 5; return 0; }'
assert 5 'int main() { return -(-5); }'
assert 1 'int main() { int v0=1; int v1=1; int v2=1; int v3=1; int v4=1; int v5=1; int v6=1; int v7=1; int v8=1; int v9=1; int v10=1; return v0; }'
assert 10 'int main() { if (1) return 10; return 5; }'
assert 5 'int main() { if (0) return 10; else return 5; }'
assert 5 'int main() { int a; int b; int c; a = 5; b = 1; if (b - a < 0) return a * b; else return 0; }'
assert 100 'int main() { int a; a=10; if (a == 0) return 0; else if (a == 10) return 100; return 1; }'
assert 1 'int main() { int a; a=10; if(a==0)return 0;else if(a==10)return 1; else return 3; }'
assert 3 'int main() { int a; a=10; if(a==0)return 0;else if(a==1)return 1; else return 3; }'
assert 9 'int main() { int a; a=10; a = a - 1; return a; }'
assert 2 'int main() { int a; a=10; while(a > 2) a = a - 1; return a; }'
assert 1 'int main() { int a; a=10; while(a > 3) a = a - 3; return a; }'
assert 5 'int main() { int a; a=10; while(a > 0) if (a == 5) return a; else a = a - 1; return 100; }'
assert 10 'int main() { int a; int sum; a=0; sum=0; for (a=0;a<5;a=a+1) sum = sum+a; return sum; }'
assert 10 'int main() { int a; int sum; sum=0; for (a=0;a<5;a=a+1) sum = sum+a; return sum; }'
assert 0 'int main() { for (;;) return 0; }'
assert 10 'int main() { int a; for (a=0;a<5;) a = a + 10; return a; }'
assert 10 'int main() { int a; for (a=0;;a=a+1) if (a >= 10) return a; return 0; }'
assert 10 'int main() { int a; a=0; for (;a<10;a=a+1) a; return a; }'
assert 10 'int main() { int a; a=0; for (;;a=a+1) if (a >= 10) return a; return 0; }'
assert 3 'int main() { int a; int sum; sum=0; for (a=0;a<3;a=a+1){sum = sum + a;} return sum; }'
assert 33 'int main() { int a; int b; int sum; sum=0; a=10; for(b=0;b<3;b=b+1){sum=sum+b; sum=sum+a;} return sum; }'
assert 15 'int main() { int i; int sum; sum=0; i=1; while(i<=5){sum=sum+i; i=i+1;} return sum; }'
assert 0 'int main() { int i; for(i=0;i<5;i=i+1){} return 0; }'
assert 2 'int main() { int a; a=0; if(a==0)a=2; return a; }'
assert 2 'int main() { int a; a=0; if(a==0){a=2;} return a; }'
assert 8 'int main() { int a; a=0; if(a==0){a=2; a=a*2; a=a*2;} return a; }'
assert 3 'int main() { return ret3(); }'
assert 5 'int main() { return ret5(); }'
assert 3 'int main() { return add(1, 2); }'
assert 6 'int main() { return sub(10, 4); }'
assert 10 'int main() { return add6(1,1,1,1,1,5); }'
assert 5 'int main() { return retx(5); }'
assert 5 'int main() { int x=5; return retx(x); }'
assert 123 'int ret123(){return 123;} int main(){return ret123();}'
assert 123 'int main(){return ret123();} int ret123(){return 123;} '
assert 123 'int main(){return retval(123);} int retval(int x){return x;} '
assert 7 'int main() { return add2(3,4); } int add2(int x, int y) {return x+y;}'
assert 1 'int main() { return sub2(4,3); } int sub2(int x, int y) {return x-y;}'
assert 55 'int main() { return fib(9); } int fib(int x) { if (x <= 1) return 1; return fib(x-1)+fib(x-2); }'
assert 3 'int main() {int x=3; int *y=&x; return *y;}'
assert 5 'int main() {int x=5; int *y=&x; int **z=&y; return **z;}'
assert 10 'int main() {int x=10; int y=5; int *z=&y-1; return *z;}'
assert 5 'int main() {int x=5; return *&x;}'
assert 10 'int main() { int x = 5; int *px = &x; *px = 10; return *px; }'
assert 10 'int main() { int x = 5; int *px = &x; *px = 10; return x; }'
assert 7 'int main() { int x=3; int y=5; *(&x+1)=7; return y; }'
assert 7 'int main() { int x=3; int y=5; *(&y-1)=7; return x; }'
assert 5 'int main() { int x = 3; return (&x+5) - (&x); }'
assert 8 'int main() { int x = 0; return sizeof(x); }'
assert 8 'int main() { return sizeof(1); }'
assert 8 'int main() { return sizeof(sizeof(1)); }'
assert 8 'int main() { int x = 0; return sizeof(&x); }'
assert 8 'int main() { int x = 0; return sizeof(&x + 2); }'
assert 8 'int main() { int x = 0; return sizeof(sizeof(&x)); }'
assert 11 'int main() { int a[3]; *a=10; *(a+1)=1; *(a+2)=*a+*(a+1); return *(a+2);}'
assert 11 'int main() { int a[3]; int *b=a+2; *b=11; return *(a+2);}'
assert 64 'int main() { int a[8]; return sizeof(a);}'
assert 8 'int main() { int a[8]; int *b = a; return sizeof(b); }'
assert 0 'int main() { int x[2][3]; int *y=x; *y=0; return **x; }'
assert 1 'int main() { int x[2][3]; int *y=x; *(y+1)=1; return *(*x+1); }'
assert 2 'int main() { int x[2][3]; int *y=x; *(y+2)=2; return *(*x+2); }'
assert 3 'int main() { int x[2][3]; int *y=x; *(y+3)=3; return **(x+1); }'
assert 4 'int main() { int x[2][3]; int *y=x; *(y+4)=4; return *(*(x+1)+1); }'
assert 5 'int main() { int x[2][3]; int *y=x; *(y+5)=5; return *(*(x+1)+2); }'
assert 11 'int main() { int a[100]; a[99]=11; return a[99];}'
assert 11 'int main() { int a[3]; a[0]=10; a[1]=1; a[2]=*a+*(a+1); return *(a+2);}'
assert 11 'int main() { int a[3]; int *b=a+2; b[0]=11; return a[2];}'
assert 0 'int main() { int x[2][3]; x[0][0]=0; return **x; }'
assert 1 'int main() { int x[2][3]; x[0][1]=1; return *(*x+1); }'
assert 2 'int main() { int x[2][3]; x[0][2]=2; return *(*x+2); }'
assert 3 'int main() { int x[2][3]; x[1][0]=3; return **(x+1); }'
assert 4 'int main() { int x[2][3]; x[1][1]=4; return *(*(x+1)+1); }'
assert 5 'int main() { int x[2][3]; x[1][2]=5; return *(*(x+1)+2); }'
assert 1 'int main() { int x[2][3]; x[0][1]=1; return x[0][1]; }'
assert 2 'int main() { int x[2][3]; x[0][2]=2; return x[0][2]; }'
assert 3 'int main() { int x[2][3]; x[1][0]=3; return x[1][0]; }'
assert 4 'int main() { int x[2][3]; x[1][1]=4; return x[1][1]; }'
assert 5 'int main() { int x[2][3]; x[1][2]=5; return x[1][2]; }'
assert 1 'int main() { int x[2][3]; x[0][1]=1; return *(*x+1); }'
assert 2 'int main() { int x[2][3]; x[0][2]=2; return *(*x+2); }'
assert 3 'int main() { int x[2][3]; x[0][3]=3; return **(x+1); }'
assert 4 'int main() { int x[2][3]; x[0][4]=4; return *(*(x+1)+1); }'
assert 5 'int main() { int x[2][3]; x[0][5]=5; return *(*(x+1)+2); }'
assert 5 'int main() {int x[3]; x[0]=1;x[1]=2;x[2]=2; return sum(x);} int sum(int *a) { return a[0]+a[1]+a[2]; }'

echo OK