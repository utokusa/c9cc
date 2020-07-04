#include "9cc.h"

/*********************************************
* ...parser...
*********************************************/

// Scope for local variables, global variables, typedefs
// or enum constants
typedef struct VarScope VarScope;
struct VarScope
{
  VarScope *next;
  char *name;
  int depth;

  Var *var;
  Type *type_def;
  Type *enum_ty;
  int enum_val;
};

// Scope for struct, union or enum tags
typedef struct TagScope TagScope;
struct TagScope
{
  TagScope *next;
  char *name;
  int depth;
  Type *ty;
};

// Variable attributes such as typedef or extern.
typedef struct
{
  bool is_typedef;
  bool is_static;
} VarAttr;

// Local variables
static VarList *locals;
// Global variables
static VarList *globals;

// C has two block scopes; one is for variables/typedefs
// and the other is for struct/union/enum tags.
static VarScope *var_scope;
static TagScope *tag_scope;

// scope_depth is incremented by one at "{" and decremented
// by one at "}"
static int scope_depth;

// Points to the function object the parse is currently parsing.
static Var *current_fn;

static bool is_typename(Token *tok);
static Type *typespec(Token **rest, Token *tok, VarAttr *attr);
static Type *enum_specifier(Token **rest, Token *tok);
static Type *declarator(Token **rest, Token *tok, Type *ty);
static Function *funcdef(Token **rest, Token *tok);
static Node *declaration(Token **rest, Token *tok);
static Node *compound_stmt(Token **Rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *new_add(Node *lhs, Node *rhs, Token *tok);
static Node *new_sub(Node *lhs, Node *rhs, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *cast(Token **rest, Token *tok);
static Type *struct_decl(Token **rest, Token *tok);
static Type *union_decl(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *postfix(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

static Node *new_node(NodeKind kind, Token *tok)
{
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  return node;
}

static Node *new_node_num(int val, Token *tok)
{
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  return node;
}

static Node *new_node_var(Var *var, Token *tok)
{
  Node *node = new_node(ND_VAR, tok);
  node->var = var;
  return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok)
{
  Node *node = new_node(kind, tok);
  node->lhs = expr;
  return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok)
{
  Node *node = new_node(kind, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_add(Node *lhs, Node *rhs, Token *tok)
{
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_ADD, lhs, rhs, tok);
  if (lhs->ty->base && is_integer(rhs->ty))
    return new_binary(ND_PTR_ADD, lhs, rhs, tok);
  if (is_integer(lhs->ty) && rhs->ty->base)
    return new_binary(ND_PTR_ADD, lhs, rhs, tok);
  error_tok(tok, "invalid operands");
}

static Node *new_sub(Node *lhs, Node *rhs, Token *tok)
{
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_SUB, lhs, rhs, tok);
  if (lhs->ty->base && is_integer(rhs->ty))
    return new_binary(ND_PTR_SUB, lhs, rhs, tok);
  if (lhs->ty->base && rhs->ty->base)
    return new_binary(ND_PTR_DIFF, lhs, rhs, tok);
  error_tok(tok, "invalid operands");
}

static void enter_scope()
{
  scope_depth++;
}

static void leave_scope()
{
  scope_depth--;
  while (var_scope && var_scope->depth > scope_depth)
    var_scope = var_scope->next;

  while (tag_scope && tag_scope->depth > scope_depth)
    tag_scope = tag_scope->next;
}

// Search variable or a typedef by name. Return NULL if not found.
static VarScope *find_var(Token *tok)
{
  for (VarScope *sc = var_scope; sc; sc = sc->next)
  {
    if (strlen(sc->name) == tok->len && !strncmp(tok->loc, sc->name, tok->len))
      return sc;
  }
  return NULL;
}

// Search struct tag by name. Return NULL if not found.
static TagScope *find_tag(Token *tok)
{
  for (TagScope *sc = tag_scope; sc; sc = sc->next)
  {
    if (strlen(sc->name) == tok->len && !strncmp(tok->loc, sc->name, tok->len))
      return sc;
  }
  return NULL;
}

Node *new_cast(Node *expr, Type *ty)
{
  add_type(expr);

  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_CAST;
  node->tok = expr->tok;
  node->lhs = expr;
  node->ty = copy_type(ty);
  return node;
}

static VarScope *push_scope(char *name)
{
  VarScope *sc = calloc(1, sizeof(VarScope));
  sc->next = var_scope;
  sc->name = name;
  sc->depth = scope_depth;
  var_scope = sc;
  return sc;
}

static void push_tag_scope(Token *tok, Type *ty)
{
  TagScope *sc = calloc(1, sizeof(TagScope));
  sc->next = tag_scope;
  sc->name = strndup(tok->loc, tok->len);
  sc->depth = scope_depth;
  sc->ty = ty;
  tag_scope = sc;
}

// Return new variable
static Var *new_var(char *name, Type *ty, bool is_local)
{
  Var *var = calloc(1, sizeof(Var));
  var->name = name;
  var->ty = ty;
  var->is_local = is_local;
  return var;
}

// Return new local variable
static Var *new_lvar(char *name, Type *ty)
{
  Var *var = new_var(name, ty, true);
  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = var;
  vl->next = locals;
  locals = vl;
  push_scope(name)->var = var;
  return var;
}

// Return new global variable
static Var *new_gvar(char *name, Type *ty, bool emit)
{
  Var *var = new_var(name, ty, false);
  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = var;
  if (emit)
  {
    vl->next = globals;
    globals = vl;
  }
  push_scope(name)->var = var;
  return var;
}

static char *new_gvar_name()
{
  static int cnt = 0;
  char *buf = malloc(20);
  sprintf(buf, ".L.data.%d", cnt++);
  return buf;
}

static Var *new_string_literal(char *p, int len)
{
  Type *ty = array_of(char_type, len);
  Var *var = new_gvar(new_gvar_name(), ty, true);
  var->init_data = p;
  return var;
}

static char *get_ident(Token *tok)
{
  if (tok->kind != TK_IDENT)
    error_tok(tok, "expected an identifier");
  return strndup(tok->loc, tok->len);
}

static Type *find_typedef(Token *tok)
{
  if (tok->kind == TK_IDENT)
  {
    VarScope *sc = find_var(tok);
    if (sc)
      return sc->type_def;
  }
  return NULL;
}

static int get_number(Token *tok)
{
  if (tok->kind != TK_NUM)
    error_tok(tok, "expected a number");
  return tok->val;
}

// program = (global-var | funcdef)*
Program *parse(Token *tok)
{
  Function head = {};
  Function *cur = &head;
  globals = NULL;

  while (tok->kind != TK_EOF)
  {
    Token *start = tok;
    VarAttr attr = {};
    Type *basety = typespec(&tok, tok, &attr);
    Type *ty = declarator(&tok, tok, basety);

    // Typedef
    if (attr.is_typedef)
    {
      for (;;)
      {
        push_scope(get_ident(ty->name))->type_def = ty;
        if (equal(tok, ";"))
        {
          tok = tok->next;
          break;
        }
        tok = skip(tok, ",");
        ty = declarator(&tok, tok, basety);
      }
      continue;
    }

    // Function
    if (ty->kind == TY_FUNC)
    {
      current_fn = new_gvar(get_ident(ty->name), ty, false);
      if (!equal(tok, ";"))
        cur = cur->next = funcdef(&tok, start);
      else
        tok = tok->next;

      continue;
    }

    // Global variable
    for (;;)
    {
      new_gvar(get_ident(ty->name), ty, true);
      if (equal(tok, ";"))
      {
        tok = tok->next;
        break;
      }
      tok = skip(tok, ",");
      ty = declarator(&tok, tok, basety);
    }
  }

  Program *prog = calloc(1, sizeof(Program));
  prog->globals = globals;
  prog->fns = head.next;
  return prog;
}

// Returns true if a givin token represents a type
static bool is_typename(Token *tok)
{
  static char *kw[] =
      {
          "void",
          "_Bool",
          "char",
          "short",
          "int",
          "long",
          "struct",
          "union",
          "typedef",
          "enum",
          "static",
      };

  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (equal(tok, kw[i]))
      return true;
  return find_typedef(tok);
}

// funcdef = typespec declarator compound-stmt
static Function *funcdef(Token **rest, Token *tok)
{
  locals = NULL;

  VarAttr attr = {};
  Type *ty = typespec(&tok, tok, &attr);
  ty = declarator(&tok, tok, ty);

  Function *fn = calloc(1, sizeof(Function));
  fn->name = get_ident(ty->name);
  fn->is_static = attr.is_static;

  enter_scope();
  for (Type *t = ty->params; t; t = t->next)
    new_lvar(get_ident(t->name), t);
  fn->params = locals;

  tok = skip(tok, "{");
  fn->node = compound_stmt(rest, tok)->body;
  fn->locals = locals;
  leave_scope();
  return fn;
}

// typespec = "void" | "_Bool" | "char" | "short" | "int" | "long"
//          | struct-decl | union-decl | typedef-name
static Type *typespec(Token **rest, Token *tok, VarAttr *attr)
{
  enum
  {
    VOID = 1 << 0,
    BOOL = 1 << 2,
    CHAR = 1 << 4,
    SHORT = 1 << 6,
    INT = 1 << 8,
    LONG = 1 << 10,
    OTHER = 1 << 12,
  };

  Type *ty = int_type;
  int counter = 0;

  while (is_typename(tok))
  {
    // Handle storage class specifiers.
    if (equal(tok, "typedef") || equal(tok, "static"))
    {
      if (!attr)
        error_tok(tok, "storage class specifier is not allowed in this context.");
      if (equal(tok, "typedef"))
        attr->is_typedef = true;
      else
        attr->is_static = true;

      if (attr->is_typedef + attr->is_static > 1)
        error_tok(tok, "typedef and static may not be used together");
      tok = tok->next;
      continue;
    }

    // Handle user-defined types.
    Type *ty2 = find_typedef(tok);
    if (equal(tok, "struct") || equal(tok, "union") ||
        equal(tok, "enum") || ty2)
    {
      if (counter)
        break;

      if (equal(tok, "struct"))
        ty = struct_decl(&tok, tok->next);
      else if (equal(tok, "union"))
        ty = union_decl(&tok, tok->next);
      else if (equal(tok, "enum"))
        ty = enum_specifier(&tok, tok->next);
      else
      {
        ty = ty2;
        tok = tok->next;
      }
      counter += OTHER;
      continue;
    }

    // Handle built-in types.
    if (equal(tok, "void"))
      counter += VOID;
    else if (equal(tok, "_Bool"))
      counter += BOOL;
    else if (equal(tok, "char"))
      counter += CHAR;
    else if (equal(tok, "short"))
      counter += SHORT;
    else if (equal(tok, "int"))
      counter += INT;
    else if (equal(tok, "long"))
      counter += LONG;
    else
      error_tok(tok, "internal error");

    switch (counter)
    {
    case VOID:
      ty = void_type;
      break;
    case BOOL:
      ty = bool_type;
      break;
    case CHAR:
      ty = char_type;
      break;
    case SHORT:
    case SHORT + INT:
      ty = short_type;
      break;
    case INT:
      ty = int_type;
      break;
    case LONG:
    case LONG + INT:
    case LONG + LONG:
    case LONG + LONG + INT:
      ty = long_type;
      break;
    default:
      error_tok(tok, "invalid type");
    }

    tok = tok->next;
  }

  *rest = tok;
  return ty;
}

// enum_specifier = ident? "{" enum-list? "}"
//                | ident ("{" enum-list? "}")?
//
// enum-list      = ident ("=" num)? ("," ident ("=" num)?)*
static Type *enum_specifier(Token **rest, Token *tok)
{
  Type *ty = enum_type();

  // Read a struct tag.
  Token *tag = NULL;
  if (tok->kind == TK_IDENT)
  {
    tag = tok;
    tok = tok->next;
  }

  if (tag && !equal(tok, "{"))
  {
    TagScope *sc = find_tag(tag);
    if (!sc)
      error_tok(tag, "unknown enum type");
    if (sc->ty->kind != TY_ENUM)
      error_tok(tag, "not an enum tag");
    *rest = tok;
    return sc->ty;
  }

  tok = skip(tok, "{");

  // Read an enum-list.
  int i = 0;
  int val = 0;
  while (!equal(tok, "}"))
  {
    if (i++ > 0)
      tok = skip(tok, ",");

    char *name = get_ident(tok);
    tok = tok->next;

    if (equal(tok, "="))
    {
      val = get_number(tok->next);
      tok = tok->next->next;
    }

    VarScope *sc = push_scope(name);
    sc->enum_ty = ty;
    sc->enum_val = val++;
  }

  *rest = tok->next;

  if (tag)
    push_tag_scope(tag, ty);
  return ty;
}

// func-params = (param ("," param)*)? ")"
// param       = typespec declarator
static Type *func_params(Token **rest, Token *tok, Type *ty)
{
  Type head = {};
  Type *cur = &head;

  while (!equal(tok, ")"))
  {
    if (cur != &head)
      tok = skip(tok, ",");
    Type *basety = typespec(&tok, tok, NULL);
    Type *ty = declarator(&tok, tok, basety);
    cur = cur->next = copy_type(ty);
  }

  ty = func_type(ty);
  ty->params = head.next;
  *rest = tok->next;
  return ty;
}

// type-suffix = "(" func-params
//             | "[" num "]" type-suffix
//             | ε
static Type *type_suffix(Token **rest, Token *tok, Type *ty)
{
  if (equal(tok, "("))
    return func_params(rest, tok->next, ty);

  if (equal(tok, "["))
  {
    int len = get_number(tok->next);
    tok = skip(tok->next->next, "]");
    ty = type_suffix(rest, tok, ty);
    return array_of(ty, len);
  }

  *rest = tok;
  return ty;
}

// declarator = "*"* ("(" declarator ")" | ident) type-suffix
static Type *declarator(Token **rest, Token *tok, Type *ty)
{
  while (equal(tok, "*"))
  {
    tok = tok->next;
    ty = pointer_to(ty);
  }

  if (equal(tok, "("))
  {
    Type *placeholder = calloc(1, sizeof(Type));
    Type *new_ty = declarator(&tok, tok->next, placeholder);
    tok = skip(tok, ")");
    *placeholder = *type_suffix(rest, tok, ty);
    return new_ty;
  }

  if (tok->kind != TK_IDENT)
    error_tok(tok, "expected a variable name");
  ty = type_suffix(rest, tok->next, ty);
  ty->name = tok;
  return ty;
}

// abstract-declarator = "*"* ("(" abstract-declarator ")")? type-suffix
static Type *abstract_declarator(Token **rest, Token *tok, Type *ty)
{
  while (equal(tok, "*"))
  {
    ty = pointer_to(ty);
    tok = tok->next;
  }

  if (equal(tok, "("))
  {
    Type *placeholder = calloc(1, sizeof(Type));
    Type *new_ty = abstract_declarator(&tok, tok->next, placeholder);
    tok = skip(tok, ")");
    *placeholder = *type_suffix(rest, tok, ty);
    return new_ty;
  }

  return type_suffix(rest, tok, ty);
}

// type-name = typespec abstract-declarator
static Type *typename(Token **rest, Token *tok)
{
  Type *ty = typespec(&tok, tok, NULL);
  return abstract_declarator(rest, tok, ty);
}

// declaration = typespec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
static Node *declaration(Token **rest, Token *tok)
{
  VarAttr attr = {};
  Type *basety = typespec(&tok, tok, &attr);

  Node head = {};
  Node *cur = &head;
  int cnt = 0;

  while (!equal(tok, ";"))
  {
    if (cnt++ > 0)
      tok = skip(tok, ",");

    Type *ty = declarator(&tok, tok, basety);
    if (ty->kind == TY_VOID)
      error_tok(tok, "variable declared void");

    if (attr.is_typedef)
    {
      push_scope(get_ident(ty->name))->type_def = ty;
      continue;
    }

    Var *var = new_lvar(get_ident(ty->name), ty);

    if (!equal(tok, "="))
      continue;

    Node *lhs = new_node_var(var, ty->name);
    Node *rhs = assign(&tok, tok->next);
    Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
    cur = cur->next = new_unary(ND_EXPR_STMT, node, tok);
  }

  Node *node = new_node(ND_BLOCK, tok);
  node->body = head.next;
  *rest = tok->next;
  return node;
}

// compound-stmt = (declaration | stmt)* "}"
static Node *compound_stmt(Token **rest, Token *tok)
{
  Node *node = new_node(ND_BLOCK, tok);
  Node head = {};
  Node *cur = &head;

  enter_scope();

  while (!equal(tok, "}"))
  {
    if (is_typename(tok))
      cur = cur->next = declaration(&tok, tok);
    else
      cur = cur->next = stmt(&tok, tok);
    add_type(cur);
  }

  leave_scope();

  node->body = head.next;
  *rest = tok->next;
  return node;
}

// expr-stmt = expr
static Node *expr_stmt(Token **rest, Token *tok)
{
  Node *node = new_node(ND_EXPR_STMT, tok);
  node->lhs = expr(rest, tok);
  return node;
}

// stmt = expr ";"
//      | "{" compound-stmt
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "while" "(" expr ")" stmt
//      | "for" "(" (expr? ";" | declaration) expr? ";" expr? ")" stmt
//      | "return" expr ";"
static Node *stmt(Token **rest, Token *tok)
{
  if (equal(tok, "return"))
  {
    Node *node = new_node(ND_RETURN, tok);
    Node *exp = expr(&tok, tok->next);
    *rest = skip(tok, ";");

    add_type(exp);
    node->lhs = new_cast(exp, current_fn->ty->return_ty);
    return node;
  }

  if (equal(tok, "if"))
  {
    Node *node = new_node(ND_IF, tok);
    tok = skip(tok->next, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");
    node->then = stmt(&tok, tok);
    if (equal(tok, "else"))
      node->els = stmt(&tok, tok->next);
    *rest = tok;
    return node;
  }

  if (equal(tok, "while"))
  {
    Node *node = new_node(ND_WHILE, tok);
    tok = skip(tok->next, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");

    node->then = stmt(&tok, tok);
    *rest = tok;
    return node;
  }

  if (equal(tok, "for"))
  {
    Node *node = new_node(ND_FOR, tok);
    tok = skip(tok->next, "(");

    enter_scope();

    if (is_typename(tok))
    {
      node->init = declaration(&tok, tok);
    }
    else
    {
      if (!equal(tok, ";"))
        node->init = expr_stmt(&tok, tok);
      tok = skip(tok, ";");
    }

    // For "for (a; b; c) {...}" we regard b as true if b is empty.
    node->cond = new_node_num(1, tok);
    if (!equal(tok, ";"))
      node->cond = expr(&tok, tok);
    tok = skip(tok, ";");

    if (!equal(tok, ")"))
      node->inc = expr_stmt(&tok, tok);
    tok = skip(tok, ")");

    node->then = stmt(&tok, tok);

    leave_scope();
    *rest = tok;
    return node;
  }

  if (equal(tok, "{"))
    return compound_stmt(rest, tok->next);

  Node *node = expr_stmt(&tok, tok);
  *rest = skip(tok, ";");
  return node;
}

// expr = assign ("," expr)?
static Node *expr(Token **rest, Token *tok)
{
  Node *node = assign(&tok, tok);

  if (equal(tok, ","))
    return new_binary(ND_COMMA, node, expr(rest, tok->next), tok);

  *rest = tok;
  return node;
}

// Convert `A op= B` to `tmp = &A, *tmp = *tmp op B`
// where tmp is a fresh pointer variable.
static Node *to_assign(Node *binary)
{
  add_type(binary->lhs);
  add_type(binary->rhs);

  Var *var = new_lvar("", pointer_to(binary->lhs->ty));
  Token *tok = binary->tok;

  Node *expr1 = new_binary(ND_ASSIGN, new_node_var(var, tok),
                           new_unary(ND_ADDR, binary->lhs, tok), tok);
  Node *expr2 = new_binary(ND_ASSIGN,
                           new_unary(ND_DEREF, new_node_var(var, tok), tok),
                           new_binary(binary->kind,
                                      new_unary(ND_DEREF, new_node_var(var, tok), tok),
                                      binary->rhs, tok),
                           tok);

  return new_binary(ND_COMMA, expr1, expr2, tok);
}

// assign = eauality (assign-op assign)?
// assign-op = "=" | "+=" | "-=" | "*=" | "/=" | "%="
static Node *assign(Token **rest, Token *tok)
{
  Node *node = equality(&tok, tok);

  if (equal(tok, "="))
  {
    Node *rhs = assign(&tok, tok->next);
    node = new_binary(ND_ASSIGN, node, rhs, tok);
  }

  if (equal(tok, "+="))
    return to_assign(new_add(node, assign(rest, tok->next), tok));

  if (equal(tok, "-="))
    return to_assign(new_sub(node, assign(rest, tok->next), tok));

  if (equal(tok, "*="))
    return to_assign(new_binary(ND_MUL, node, assign(rest, tok->next), tok));

  if (equal(tok, "/="))
    return to_assign(new_binary(ND_DIV, node, assign(rest, tok->next), tok));

  if (equal(tok, "%="))
    return to_assign(new_binary(ND_MOD, node, assign(rest, tok->next), tok));

  *rest = tok;
  return node;
}

// eauality = relationam ("==" relationall | "!=" relational)*
static Node *equality(Token **rest, Token *tok)
{
  Node *node = relational(&tok, tok);

  for (;;)
  {
    if (equal(tok, "=="))
    {
      Node *rhs = relational(&tok, tok->next);
      node = new_binary(ND_EQ, node, rhs, tok);
      continue;
    }
    else if (equal(tok, "!="))
    {
      Node *rhs = relational(&tok, tok->next);
      node = new_binary(ND_NE, node, rhs, tok);
      continue;
    }
    *rest = tok;
    return node;
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(Token **rest, Token *tok)
{
  Node *node = add(&tok, tok);

  for (;;)
  {
    if (equal(tok, "<"))
    {
      Node *rhs = add(&tok, tok->next);
      node = new_binary(ND_LT, node, rhs, tok);
      continue;
    }

    if (equal(tok, "<="))
    {
      Node *rhs = add(&tok, tok->next);
      node = new_binary(ND_LE, node, rhs, tok);
      continue;
    }

    if (equal(tok, ">"))
    {
      Node *rhs = add(&tok, tok->next);
      node = new_binary(ND_LT, rhs, node, tok);
      continue;
    }

    if (equal(tok, ">="))
    {
      Node *rhs = add(&tok, tok->next);
      node = new_binary(ND_LE, rhs, node, tok);
      continue;
    }

    *rest = tok;
    return node;
  }
}

// add = mul ("+" mul | "-" mul)*
static Node *add(Token **rest, Token *tok)
{
  Node *node = mul(&tok, tok);

  for (;;)
  {
    if (equal(tok, "+"))
    {
      Node *rhs = mul(&tok, tok->next);
      // node = new_node_binary(ND_ADD, node, rhs, tok);
      node = new_add(node, rhs, tok);
      continue;
    }

    if (equal(tok, "-"))
    {
      Node *rhs = mul(&tok, tok->next);
      // node = new_node_binary(ND_SUB, node, rhs, tok);
      node = new_sub(node, rhs, tok);
      continue;
    }

    *rest = tok;
    return node;
  }
}

// mul = cast ("*" cast | "/" cast | "%" cast)*
static Node *mul(Token **rest, Token *tok)
{
  Node *node = cast(&tok, tok);

  for (;;)
  {
    if (equal(tok, "*"))
    {
      Node *rhs = cast(&tok, tok->next);
      node = new_binary(ND_MUL, node, rhs, tok);
      continue;
    }

    if (equal(tok, "/"))
    {
      Node *rhs = cast(&tok, tok->next);
      node = new_binary(ND_DIV, node, rhs, tok);
      continue;
    }

    if (equal(tok, "%"))
    {
      Node *rhs = cast(&tok, tok->next);
      node = new_binary(ND_MOD, node, rhs, tok);
      continue;
    }

    *rest = tok;
    return node;
  }
}

// cast = "(" type-name ")" cast | unary
static Node *cast(Token **rest, Token *tok)
{
  if (equal(tok, "(") && is_typename(tok->next))
  {
    Node *node = new_node(ND_CAST, tok);
    node->ty = typename(&tok, tok->next);
    tok = skip(tok, ")");
    node->lhs = cast(rest, tok);
    add_type(node->lhs);
    return node;
  }

  return unary(rest, tok);
}

// unary = ("sizeof" | "+" | "-" | "*" | "&" | "!" | "~")? cast
//       | ("++" | "--") unary
//       | "sizeof" "(" type-name ")"
//       | postfix
static Node *unary(Token **rest, Token *tok)
{
  if (equal(tok, "sizeof") && equal(tok->next, "(") &&
      is_typename(tok->next->next))
  {
    Type *ty = typename(&tok, tok->next->next);
    *rest = skip(tok, ")");
    return new_node_num(size_of(ty), tok);
  }
  if (equal(tok, "sizeof"))
  {
    Node *node = cast(rest, tok->next);
    add_type(node);
    return new_node_num(size_of(node->ty), tok);
  }
  if (equal(tok, "+"))
    return cast(rest, tok->next);
  if (equal(tok, "-"))
    return new_binary(ND_SUB, new_node_num(0, tok), cast(rest, tok->next), tok);
  if (equal(tok, "*"))
    return new_unary(ND_DEREF, cast(rest, tok->next), tok);
  if (equal(tok, "&"))
    return new_unary(ND_ADDR, cast(rest, tok->next), tok);

  if (equal(tok, "!"))
    return new_unary(ND_NOT, cast(rest, tok->next), tok);

  if (equal(tok, "~"))
    return new_unary(ND_BITNOT, cast(rest, tok->next), tok);

  // Read ++i as i+=1
  if (equal(tok, "++"))
    return to_assign(new_add(unary(rest, tok->next), new_node_num(1, tok), tok));
  // Read --i as i-=1
  if (equal(tok, "--"))
    return to_assign(new_sub(unary(rest, tok->next), new_node_num(1, tok), tok));

  return postfix(rest, tok);
}

// struct-members = (typespec declarator ("," declarator)* ";")*
static Member *struct_members(Token **rest, Token *tok)
{
  Member head = {};
  Member *cur = &head;

  while (!equal(tok, "}"))
  {
    Type *basety = typespec(&tok, tok, NULL);
    int cnt = 0;

    while (!equal(tok, ";"))
    {
      if (cnt++)
        tok = skip(tok, ",");

      Member *mem = calloc(1, sizeof(Member));
      mem->ty = declarator(&tok, tok, basety);
      mem->name = mem->ty->name;
      cur = cur->next = mem;
    }
    tok = tok->next;
  }

  *rest = tok->next;
  return head.next;
}

// struct-union-decl = ident? ("{" struct-members)?
static Type *struct_union_decl(Token **rest, Token *tok)
{
  // Read a tag.
  Token *tag = NULL;
  if (tok->kind == TK_IDENT)
  {
    tag = tok;
    tok = tok->next;
  }

  if (tag && !equal(tok, "{"))
  {
    TagScope *sc = find_tag(tag);
    if (!sc)
      error_tok(tag, "unknown struc type");
    *rest = tok;
    return sc->ty;
  }

  // Construct a struct object.
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_STRUCT;
  ty->members = struct_members(rest, tok->next);

  // Register the struct type if a name was given.
  if (tag)
    push_tag_scope(tag, ty);
  return ty;
}

// sturct-decl = struct-union-decl
static Type *struct_decl(Token **rest, Token *tok)
{
  Type *ty = struct_union_decl(rest, tok);

  // Assign offsets within the struct to members.
  int offset = 0;
  for (Member *mem = ty->members; mem; mem = mem->next)
  {
    offset = align_to(offset, mem->ty->align);
    mem->offset = offset;
    offset += size_of(mem->ty);

    if (ty->align < mem->ty->align)
      ty->align = mem->ty->align;
  }
  ty->size = align_to(offset, ty->align);

  return ty;
}

// union-decl = struct-union-decl
static Type *union_decl(Token **rest, Token *tok)
{
  Type *ty = struct_union_decl(rest, tok);

  // If union, we don't have to assign offsets because they are
  // already initialized to zero.
  for (Member *mem = ty->members; mem; mem = mem->next)
  {
    if (ty->align < mem->ty->align)
      ty->align = mem->ty->align;
    if (ty->size < size_of(mem->ty))
      ty->size = size_of(mem->ty);
  }
  ty->size = align_to(size_of(ty), ty->align);
  return ty;
}

static Member *get_struct_member(Type *ty, Token *tok)
{
  for (Member *mem = ty->members; mem; mem = mem->next)
    if (mem->name->len == tok->len &&
        !strncmp(mem->name->loc, tok->loc, tok->len))
      return mem;
  error_tok(tok, "no such member");
}

// reference to a struct member
// object.member
// ---> lhs->tok->loc: object...., tok->loc: member...
static Node *struct_ref(Node *lhs, Token *tok)
{
  add_type(lhs);
  if (lhs->ty->kind != TY_STRUCT)
    error_tok(lhs->tok, "not a struct");

  Node *node = new_unary(ND_MEMBER, lhs, tok);
  node->member = get_struct_member(lhs->ty, tok);
  return node;
}

// Convert A++ to `tmp = &A, *tmp = *tmp + 1,  *tmp - 1`
// where tmp is a fresh pointer variable.
static Node *new_inc_dec(Node *node, Token *tok, int addend)
{
  add_type(node);
  Var *var = new_lvar("", pointer_to(node->ty));

  Node *expr1 = new_binary(ND_ASSIGN, new_node_var(var, tok),
                           new_unary(ND_ADDR, node, tok), tok);
  Node *expr2 = new_binary(ND_ASSIGN,
                           new_unary(ND_DEREF, new_node_var(var, tok), tok),
                           new_add(new_unary(ND_DEREF, new_node_var(var, tok), tok),
                                   new_node_num(addend, tok), tok),
                           tok);
  Node *expr3 = new_add(new_unary(ND_DEREF, new_node_var(var, tok), tok),
                        new_node_num(-addend, tok), tok);

  return new_binary(ND_COMMA, expr1, new_binary(ND_COMMA, expr2, expr3, tok), tok);
}

// postfix = primary ("[" expr "]" | "." ident | "->" ident | "++" | "--")*
static Node *postfix(Token **rest, Token *tok)
{
  Node *node = primary(&tok, tok);

  for (;;)
  {
    if (equal(tok, "["))
    {
      // x[y] is short for *(x + y)
      Token *start = tok;
      Node *idx = expr(&tok, tok->next);
      tok = skip(tok, "]");
      node = new_unary(ND_DEREF, new_add(node, idx, start), start);
      continue;
    }

    if (equal(tok, "."))
    {
      node = struct_ref(node, tok->next);
      tok = tok->next->next;
      continue;
    }

    if (equal(tok, "->"))
    {
      // x->y is short for (*x).y
      node = new_unary(ND_DEREF, node, tok);
      node = struct_ref(node, tok->next);
      tok = tok->next->next;
      continue;
    }

    if (equal(tok, "++"))
    {
      node = new_inc_dec(node, tok, 1);
      tok = tok->next;
      continue;
    }

    if (equal(tok, "--"))
    {
      node = new_inc_dec(node, tok, -1);
      tok = tok->next;
      continue;
    }

    *rest = tok;
    return node;
  }
}

// funcall = ident "(" (assign ("," assign)*)? ")"
//
// foo(a, b, c) is compiled to ({t1=a; t2=b; t3=c; foo(t1, t2, t3); })
// where t1, t2 and t3 are fresh local variables.
static Node *funcall(Token **rest, Token *tok)
{
  Token *start = tok;
  tok = tok->next->next;

  VarScope *sc = find_var(start);
  Type *ty;
  if (sc)
  {
    if (!sc->var || sc->var->ty->kind != TY_FUNC)
      error_tok(start, "not a function");
    ty = sc->var->ty;
  }
  else
  {
    warn_tok(start, "implicit declaration of a function");
    ty = func_type(int_type);
  }

  Node *node = new_node(ND_NULL_EXPR, tok);
  Var **args = NULL;
  int nargs = 0;
  Type *param_ty = ty->params;

  while (!equal(tok, ")"))
  {
    if (nargs)
      tok = skip(tok, ",");

    Node *arg = assign(&tok, tok);
    add_type(arg);

    if (param_ty)
    {
      arg = new_cast(arg, param_ty);
      param_ty = param_ty->next;
    }

    Var *var = arg->ty->base ? new_lvar("", pointer_to(arg->ty->base))
                             : new_lvar("", arg->ty);
    args = realloc(args, sizeof(*args) * (nargs + 1));
    args[nargs] = var;
    nargs++;

    Node *expr = new_binary(ND_ASSIGN, new_node_var(var, tok), arg, tok);
    node = new_binary(ND_COMMA, node, expr, tok);
  }

  *rest = skip(tok, ")");

  Node *funcall = new_node(ND_FUNCALL, start);
  funcall->funcname = strndup(start->loc, start->len);
  funcall->func_ty = ty;
  funcall->ty = ty->return_ty;
  funcall->args = args;
  funcall->nargs = nargs;
  return new_binary(ND_COMMA, node, funcall, tok);
}

// primary = "(" "{" stmt stmt* "}" ")"
//         | "(" expr ")"
//         | num
//         | str
//         | ident ( "(" args? ")" )?
// args = (expr ",")*  expr
static Node *primary(Token **rest, Token *tok)
{
  if (equal(tok, "(") && equal(tok->next, "{"))
  {
    // This is a GNU statement expression.
    Node *node = new_node(ND_STMT_EXPR, tok);
    Node head = {};
    Node *cur = &head;
    node->body = compound_stmt(&tok, tok->next->next)->body;
    *rest = skip(tok, ")");

    cur = node->body;
    while (cur->next)
      cur = cur->next;

    if (cur->kind != ND_EXPR_STMT)
      error_tok(cur->tok, "statement expression returning void is not supported");
    return node;
  }

  if (equal(tok, "("))
  {
    Node *node = expr(&tok, tok->next);
    *rest = skip(tok, ")");
    return node;
  }

  if (tok->kind == TK_IDENT)
  {
    // Function call
    if (equal(tok->next, "("))
      return funcall(rest, tok);

    // Variable or enum constant
    VarScope *sc = find_var(tok);
    if (!sc || (!sc->var && !sc->enum_ty))
      error_tok(tok, "undefind variable");

    Node *node;
    if (sc->var)
      node = new_node_var(sc->var, tok);
    else
      node = new_node_num(sc->enum_val, tok);

    *rest = tok->next;
    return node;
  }

  // String literal
  if (tok->kind == TK_STR)
  {
    Var *var = new_string_literal(tok->contents, tok->cont_len);
    *rest = tok->next;
    return new_node_var(var, tok);
  }

  // Numeric literal
  if (tok->kind != TK_NUM)
    error_tok(tok, "expected expression");

  *rest = tok->next;
  return new_node_num(get_number(tok), tok);
}