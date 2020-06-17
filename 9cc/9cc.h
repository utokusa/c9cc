#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*********************************************
* ...type definitions...
*********************************************/

typedef struct Type Type;

typedef enum
{
  TK_RESERVED, // Keywords or punctuators
  TK_IDENT,    // Identifier
  TK_STR,      // String literrals
  TK_NUM,      // Integer literals
  TK_EOF,      // End-of-file markers
} TokenKind;

// Token type
typedef struct Token Token;
struct Token
{
  TokenKind kind; // Token kind
  Token *next;    // Next token
  int val;        // If kind is TK_NUM, its value
  char *loc;      // Token location
  int len;        // Length of the token

  char *contents; // String literal contents including terminating '\0'
  int cont_len;   // String literal length

  int line_no; // Line number
};

// Variable
typedef struct Var Var;
struct Var
{
  char *name;    // The name of the local variable.
  Type *ty;      // Type
  bool is_local; // local or global

  // Local variable
  int offset; // The offset from RBP.

  // Global variable
  char *init_data;
};

// Linked list for Lvar
typedef struct VarList VarList;
struct VarList
{
  VarList *next;
  Var *var;
};

// Kind of AST node
typedef enum
{
  ND_ADD,       // num + num
  ND_PTR_ADD,   // ptr + num or num + ptr
  ND_SUB,       // num - num
  ND_PTR_SUB,   // ptr - num
  ND_PTR_DIFF,  // ptr - ptr
  ND_MUL,       // *
  ND_DIV,       // /
  ND_EQ,        // == equal to
  ND_NE,        // != not equal to
  ND_LT,        // <  less than
  ND_LE,        // <= less than or equal to
  ND_ASSIGN,    // = assignment
  ND_ADDR,      // & address-of
  ND_DEREF,     // * dereference (indirection)
  ND_VAR,       // variable
  ND_NUM,       // integer
  ND_NULL,      // nulls
  ND_SIZEOF,    // sizeof operator
  ND_IF,        // "if"
  ND_WHILE,     // "while"
  ND_FOR,       // "for"
  ND_BLOCK,     // {...} block
  ND_RETURN,    // "return"
  ND_FUNCALL,   // function call
  ND_EXPR_STMT, // Expression statement
  ND_STMT_EXPR, // Statement expression
} NodeKind;

// Node of AST
typedef struct Node Node;
struct Node
{
  NodeKind kind;
  Token *tok; // Representative token
  Type *ty;   // TYpe, e.g. int or pointer to int

  Node *lhs;
  Node *rhs;

  Node *next;

  // "if", "while" or "for" statement
  Node *cond;
  Node *then;
  Node *els;
  Node *init;
  Node *inc;

  // Block or statement expression
  Node *body;

  // Function call
  char *funcname;
  Node *arg;

  int val;  // Use only if kind is ND_NUM
  Var *var; // Use only if kind is ND_VAR
};

// Function
typedef struct Function Function;
struct Function
{
  Function *next;
  char *name;
  Node *node;
  VarList *locals;
  VarList *params;
  int stack_size;
};

// Program
typedef struct Program Program;
struct Program
{
  VarList *globals;
  Function *fns;
};

typedef enum
{
  TY_CHAR,
  TY_INT,
  TY_PTR,
  TY_ARR,
} TypeKind;

struct Type
{
  TypeKind kind; // type kind
  Type *base;    // base type
  int size;      // sizeof() value
  int array_len; // number of elements in an array

  // Declaration
  Token *name;
};

/*********************************************
* ...global variables...
*********************************************/

extern Type *char_type;
extern Type *int_type;

/*********************************************
* ...function declarations...
*********************************************/

// ********** tokenize.c *************

// Report error
// Take the same arguments as printf()
void error(char *fmt, ...);

// Report error and error position
void error_at(char *loc, char *fmt, ...);

void error_tok(Token *tok, char *fmt, ...);

bool equal(Token *tok, char *s);

Token *skip(Token *tok, char *s);

bool is_number();

// convert input 'user_input' to token
Token *tokenize_file(char *filename);

// ********** parse.c *************
Program *parse(Token *tok);

// ********** codegen.c *************

void codegen(Program *prog);

// ********** type.c *************

bool is_integer(Type *ty);

Type *pointer_to(Type *base);

Type *array_of(Type *base, int len);

void add_type(Node *node);
