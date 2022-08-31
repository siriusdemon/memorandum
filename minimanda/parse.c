#include "manda.h"

Node* prog = &(Node){};
Var* locals;
Var* globals;

typedef struct Env Env;
struct Env {
  Var* var;
  Env* next;
  bool is_tag;
};

Env* new_env() {
  Env* env = calloc(1, sizeof(Env));
  return env;
}

Env* add_var(Env* oldenv, Var* var) {
  Env* env = new_env();
  env->var = var;
  env->next = oldenv;
  env->is_tag = false;
  return env;
}

Env* add_tag(Env* oldenv, Var* var) {
  Env* env = new_env();
  env->var = var;
  env->next = oldenv;
  env->is_tag = true;
  return env;
}

Var* lookup_var(Env* env, Token* tok) {
  while (env!=NULL) {
    if (!env->is_tag && equal(tok, env->var->name))
      return env->var;
    else 
      env = env->next;
  }
  return NULL;
}

Type* lookup_tag(Env* env, Token* tok) {
  while (env!=NULL) {
    if (env->is_tag && equal(tok, env->var->name))
      return env->var->ty;
    else 
      env = env->next;
  }
  return NULL;
}

static Node* parse_global_var(Token** rest, Token* tok, Env** newenv, Env* env);
static Node* parse_list(Token** rest, Token* tok, Env** newenv, Env* env);
static Node* parse_expr(Token** rest, Token* tok, Env** newenv, Env* env);
static Node* parse_let(Token** rest, Token* tok, Env** newenv, Env* env, Var* (*alloc_var)(char* name, Type* ty));
static Node* parse_set(Token** rest, Token* tok, Env* env);
static Node* parse_do(Token** rest, Token* tok, Env* env);
static Node* parse_if(Token** rest, Token* tok, Env* env);
static Node* parse_while(Token** rest, Token* tok, Env* env);
static Node* parse_number(Token** rest, Token* tok, Env* env);
static Node* parse_bool(Token** rest, Token* tok, Env* env);
static Node* parse_str(Token** rest, Token* tok, Env* env);
static Node* parse_primitive(Token** rest, Token* tok, Env* env);
static Node* parse_unary(Token** rest, Token* tok, Env* env, NodeKind kind);
static Node* parse_binary(Token** rest, Token* tok, Env* env, NodeKind kind, bool left_compose);
static Node* parse_triple(Token** rest, Token* tok, Env* env, NodeKind kind);
static Node* parse_cast(Token** rest, Token* tok, Env* env);
static Node* parse_application(Token** rest, Token* tok, Env* env);
static Node* parse_def(Token** rest, Token* tok, Env* env);
static Node* parse_defstruct(Token** rest, Token* tok, Env** newenv, Env* env);
static Node* parse_defunion(Token** rest, Token* tok, Env** newenv, Env* env);
static Node* parse_deftype(Token** rest, Token* tok, Env** newenv, Env* env);
static Type* parse_type(Token** rest, Token* tok, Env* env);

static bool stop_parse(Token* tok) {
  return tok->kind == TK_EOF || tok->kind == TK_RPAREN || tok->kind == TK_RBRACKET;
}

static bool is_list(Token* tok) {
  return tok->kind == TK_LPAREN || tok->kind == TK_LBRACKET;
}

static Var* new_var(char* name, Type* ty) {
  Var* var = calloc(1, sizeof(Var));
  var->name = name;
  var->ty = ty;
  return var;
}

static Var* new_lvar(char* name, Type *ty) {
  Var* var = new_var(name, ty);
  var->next = locals;
  var->is_local = true;
  locals = var;
  return var;
}

static Var* new_gvar(char* name, Type *ty) {
  Var* var = new_var(name, ty);
  var->next = globals;
  var->is_local = false;
  globals = var;
  return var;
}

static Member* new_member(Token* tok, Type* ty) {
  Member* mem = calloc(1, sizeof(Member));
  mem->tok = tok;
  mem->ty = ty;
  return mem;
}

static char* new_unique_name(void) {
  static int id = 0;
  char *buf = calloc(1, 20);
  sprintf(buf, ".L..%d", id++);
  return buf;
}

static Var* new_anon_gvar(Type* ty) {
  return new_gvar(new_unique_name(), ty);
}

// ----- nodes
static Node* new_node(NodeKind kind, Token* tok) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  return node;
}

static Node* new_unary(NodeKind kind, Node* lhs, Token* tok) {
  Node* node = new_node(kind, tok);
  node->lhs = lhs;
  return node;
}
 
static Node* new_binary(NodeKind kind, Node* lhs, Node* rhs, Token* tok) {
  Node* node = new_node(kind, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node* new_triple(NodeKind kind, Node* lhs, Node* mhs, Node* rhs, Token* tok) {
  Node* node = new_node(kind, tok);
  node->lhs = lhs;
  node->mhs = mhs;
  node->rhs = rhs;
  return node;
}

static Node* new_num(int64_t val, Token* tok) {
  Node* node = new_node(ND_NUM, tok);
  node->val = val;
  return node;
}

static Node* new_bool(bool val, Token* tok) {
  Node* node = new_node(ND_NUM, tok);
  node->val = val;
  return node;
}

static Node* new_var_node(Var* var, Token* tok) {
  Node* node = new_node(ND_VAR, tok);
  node->var = var;
  return node;
}

static Node* new_str_node(char* str, Token* tok) {
  Node* node = new_node(ND_STR, tok);
  node->str = str;
  return node;
}

static Node* new_let(Node* lhs, Node* rhs, Token* tok) {
  Node* node = new_node(ND_LET, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node* new_set(Node* lhs, Node* rhs, Token* tok) {
  Node* node = new_node(ND_SET, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}


static Node* new_if(Node* cond, Node* then, Node* els, Token* tok) {
  Node* node = new_node(ND_IF, tok);
  node->cond = cond;
  node->then = then;
  node->els = els;
  return node;
}

static Node* new_do(Node* exprs, Token* tok) {
  Node* node = new_node(ND_DO, tok);
  node->body = exprs;
  return node;
}
static Node* new_while(Node* cond, Node* then, Token* tok) {
  Node* node = new_node(ND_WHILE, tok);
  node->cond = cond;
  node->then = then;
  return node;
}

static Node* new_app(char* fn, Node* args, Token* tok) {
  Node* node = new_node(ND_APP, tok);
  node->args = args;
  node->fn = fn;
  return node;
}

static Node* new_function(char* fn, Type* ret_ty, Node* args, Node* body, Token* tok) {
  Node* node = new_node(ND_FUNC, tok);
  node->args = args;
  node->fn = fn;
  node->locals = locals;
  node->body = body;
  node->ret_ty = ret_ty;
  return node;
}

static int get_number(Node* node) {
  if (node->kind != ND_NUM) {
    error_tok(node->tok, "not a number");
  }
  return node->val;
}


// ------- AST Node
static Node* parse_list(Token** rest, Token* tok, Env** newenv, Env* env) {
  Node* node;
  char* pair = equal(tok, "(") ? ")" : "]";
  tok = tok->next;
  if (equal(tok, "let")) {
    node = parse_let(&tok, tok, &env, env, new_lvar);
  } else if (equal(tok, "set")) {
    node = parse_set(&tok, tok, env);
  } else if (equal(tok, "if")) {
    node = parse_if(&tok, tok, env);
  } else if (equal(tok, "while")) {
    node = parse_while(&tok, tok, env);
  } else if (equal(tok, "def")) {
    node = parse_def(&tok, tok, env);
  } else if (equal(tok, "do")) {
    node = parse_do(&tok, tok, env);
  } else if (equal(tok, "defstruct")) {
    node = parse_defstruct(&tok, tok, &env, env);
  } else if (equal(tok, "defunion")) {
    node = parse_defunion(&tok, tok, &env, env);
  } else if (equal(tok, "deftype")) {
    node = parse_deftype(&tok, tok, &env, env);
  } else if (is_primitive(tok)) {  
    node = parse_primitive(&tok, tok, env);
  }  else {
    node = parse_application(&tok, tok, env);
  }
  tok = skip(tok, pair);
  *rest = tok;
  *newenv = env;
  return node;
}

static Node* parse_if(Token** rest, Token* tok, Env* env) {
  Token* tok_if = tok;
  tok = skip(tok, "if");
  Node* cond = parse_expr(&tok, tok, &env, env);
  Node* then = parse_expr(&tok, tok, &env, env);
  Node* els = NULL;
  if (!stop_parse(tok)) {
    els = parse_expr(&tok, tok, &env, env);
  }
  Node* node = new_if(cond, then, els, tok_if);
  *rest = tok;
  return node;
}

static Node* parse_do(Token** rest, Token* tok, Env* env) {
  Token* tok_do = tok;
  tok = skip(tok, "do");
  Node head = {};
  Node* cur = &head;
  while (!stop_parse(tok)) {
    cur->next = parse_expr(&tok, tok, &env, env);
    cur = cur->next;
  }
  Node* node = new_do(head.next, tok_do);
  *rest = tok;
  return node;
}

static Node* parse_while(Token** rest, Token* tok, Env* env) {
  Token* tok_while = tok;
  tok = skip(tok, "while");
  Node* cond = parse_expr(&tok, tok, &env, env);
  Node head = {};
  Node* cur = &head;
  while (!stop_parse(tok)) {
    cur->next = parse_expr(&tok, tok, &env, env);
    cur = cur->next;
  }
  Node* node = new_while(cond, head.next, tok_while);
  *rest = tok;
  return node;
}


static Node* parse_let(Token** rest, Token* tok, Env** newenv, Env* env, Var* (*alloc_var)(char* name, Type* ty)) {
  Token* tok_let = tok;
  tok = skip(tok, "let");
  Token* tok_var = tok;
  if (tok_var->kind != TK_IDENT) {
    error_tok(tok_var, "bad identifier");
  }
  tok = skip(tok->next, ":");
  Type* ty = parse_type(&tok, tok, env);
  Var* var = alloc_var(strndup(tok_var->loc, tok_var->len), ty);
  *newenv = add_var(env, var);  
  Node* lhs = new_var_node(var, tok_var);
  Node* rhs = NULL;
  if (!stop_parse(tok)) { rhs = parse_expr(&tok, tok, &env, env); }
  Node* node = new_let(lhs, rhs, tok_let);
  *rest = tok;
  return node;
}

static Node* parse_set(Token** rest, Token* tok, Env* env) {
  Token* tok_set = tok;
  tok = skip(tok, "set");
  Node* lhs = parse_expr(&tok, tok, &env, env);
  Node* rhs = parse_expr(&tok, tok, &env, env);
  Node* node = new_set(lhs, rhs, tok_set);
  *rest = tok;
  return node;
}

static Node* parse_sizeof(Token** rest, Token* tok, Env* env) {
  Token* tok_sizeof = tok;
  tok = tok->next;
  Type* ty = parse_type(&tok, tok, env);
  *rest = tok;
  return new_num(ty->size, tok_sizeof);
}

static Node* parse_primitive(Token** rest, Token* tok, Env* env) {
#define Match(t, handle)    if (equal(tok, t)) return handle;
  Match("+", parse_binary(rest, tok, env, ND_ADD, true))
  Match("-", parse_binary(rest, tok, env, ND_SUB, true))
  Match("*", parse_binary(rest, tok, env, ND_MUL, true))
  Match("/", parse_binary(rest, tok, env, ND_DIV, true))
  Match("=", parse_binary(rest, tok, env, ND_EQ, false))
  Match(">", parse_binary(rest, tok, env, ND_GT, false))
  Match("<", parse_binary(rest, tok, env, ND_LT, false))
  Match("<=", parse_binary(rest, tok, env, ND_LE, false))
  Match(">=", parse_binary(rest, tok, env, ND_GE, false))
  Match("iget", parse_binary(rest, tok, env, ND_IGET, false))
  Match("iset", parse_triple(rest, tok, env, ND_ISET))
  Match("deref", parse_unary(rest, tok, env, ND_DEREF))
  Match("addr", parse_unary(rest, tok, env, ND_ADDR))
  Match("not", parse_unary(rest, tok, env, ND_NOT))
  Match("sizeof", parse_sizeof(rest, tok, env))
  Match("cast", parse_cast(rest, tok, env))
#undef Match
  error_tok(tok, "invalid primitive\n");
}

static Node* parse_unary(Token** rest, Token* tok, Env* env, NodeKind kind) {
  Token* tok_op = tok;
  tok = tok->next;
  Node* lhs = parse_expr(&tok, tok, &env, env);
  Node* node = new_unary(kind, lhs, tok_op);
  *rest = tok;
  return node;
}

static Node* parse_binary(Token** rest, Token* tok, Env* env, NodeKind kind, bool left_compose) {
  Token* op_tok = tok;
  tok = tok->next;
  Node *lhs, *rhs, *node;
  lhs = parse_expr(&tok, tok, &env, env);
  rhs = parse_expr(&tok, tok, &env, env);
  node = new_binary(kind, lhs, rhs, op_tok);

  // perform left compose
  while (left_compose && !stop_parse(tok)) {
    lhs = node;  
    rhs = parse_expr(&tok, tok, &env, env);
    node = new_binary(kind, lhs, rhs, op_tok);
  }

  *rest = tok;
  return node;
}

static Node* parse_triple(Token** rest, Token* tok, Env* env, NodeKind kind) {
  Token* op_tok = tok;
  tok = tok->next;
  Node *lhs, *mhs, *rhs, *node;
  lhs = parse_expr(&tok, tok, &env, env);
  mhs = parse_expr(&tok, tok, &env, env);
  rhs = parse_expr(&tok, tok, &env, env);
  node = new_triple(kind, lhs, mhs, rhs, op_tok);

  *rest = tok;
  return node;
}

static Node* parse_bool(Token** rest, Token* tok, Env* env) {
  bool b = TRUE;
  if (equal(tok, "false")) {
    b = FALSE; 
  }
  Node* node = new_bool(b, tok);
  *rest = tok->next;
  return node;

}

static Node* parse_number(Token** rest, Token* tok, Env* env) {
  Node* node = new_num(tok->val, tok);
  *rest = tok->next;
  return node;
}

static Node* parse_str(Token** rest, Token* tok, Env* env) {
  Type* ty = array_of(ty_char, strlen(tok->str) + 1);
  Var* var = new_anon_gvar(ty);
  Node* var_node = new_var_node(var, tok);
  var_node->ty = ty;
  Node* str_node = new_str_node(tok->str, tok);
  str_node->ty = ty;
  // since str is global data, we need to register a let node on `prog`
  Node* let = new_let(var_node, str_node, tok);
  let->next = prog;
  prog = let;
  *rest = tok->next;
  return var_node;
}

static Node* parse_cast(Token** rest, Token* tok, Env* env) {
  Token* tok_cast = tok;
  tok = tok->next;
  Node* lhs = parse_expr(&tok, tok, &env, env);
  add_type(lhs);
  Type* ty = parse_type(&tok, tok, env);
  Node* node = new_unary(ND_CAST, lhs, tok_cast);
  node->ty = ty;
  *rest = tok;
  return node;
}



static Node* parse_application(Token** rest, Token* tok, Env* env) {
  Token* tok_app = tok;
  char* fn = strndup(tok->loc, tok->len);
  Node head = {};
  Node* cur = &head;
  tok = tok->next;
  while (!stop_parse(tok)) {
    cur->next = parse_expr(&tok, tok, &env, env); 
    cur = cur->next;
  }
  *rest = tok;
  return new_app(fn, head.next, tok_app);
}

static Member* get_struct_member(Type* ty, Token* tok) {
  for (Member* mem = ty->members; mem; mem = mem->next)
    if (mem->tok->len == tok->len &&
        !strncmp(mem->tok->loc, tok->loc, tok->len))
      return mem;
  error_tok(tok, "no such member");
}

static Node* struct_ref(Node* lhs, Token* tok) {
  add_type(lhs);
  if (lhs->ty->kind != TY_STRUCT && lhs->ty->kind != TY_UNION)
    error_tok(lhs->tok, "not a struct/union");

  Node* node = new_unary(ND_STRUCT_REF, lhs, tok);
  node->member = get_struct_member(lhs->ty, tok);
  return node;
}

static Node* parse_deftype(Token** rest, Token* tok, Env** newenv, Env* env) {
  Token* tok_deftype = tok;
  tok = tok->next;
  char* name = strndup(tok->loc, tok->len);
  tok = tok->next;
  Type* ty = parse_type(&tok, tok, env);
  Var* tag = new_var(name, ty);
  *newenv = add_tag(env, tag);
  *rest = tok;
  return new_node(ND_DEFTYPE, tok_deftype);
}

static Node* parse_defunion(Token** rest, Token* tok, Env** newenv, Env* env) {
  Token* tok_union = tok;
  tok = tok->next;
  char* name = strndup(tok->loc, tok->len);
  tok = tok->next;
  // parse member
  Member head = {};
  Member* cur = &head;
  int max_align = 1;
  int max_size = 0;
  while (!stop_parse(tok)) {
    Token* mem_tok = tok;
    Type* ty = parse_type(&tok, tok->next, env);
    Member* mem = new_member(mem_tok, ty);
    mem->offset = 0;
    cur->next = mem;
    cur = mem;
    max_align = ty->align < max_align ? max_align : ty->align;
    max_size = ty->size < max_size ? max_size : ty->size;
  }
  Type* ty = new_union_type(max_size, max_align, head.next);
  Var* tag = new_var(name, ty);
  *newenv = add_tag(env, tag);
  *rest = tok;
  return new_node(ND_DEFUNION, tok_union);
}

static Node* parse_defstruct(Token** rest, Token* tok, Env** newenv, Env* env) {
  Token* tok_struct = tok;
  tok = tok->next;
  char* name = strndup(tok->loc, tok->len);
  tok = tok->next;

  // parse member
  Member head = {};
  Member* cur = &head;
  int offset = 0;
  int max_align = 1;
  while (!stop_parse(tok)) {
    Token* mem_tok = tok;
    Type* ty = parse_type(&tok, tok->next, env);
    Member* mem = new_member(mem_tok, ty);
    offset = align_to(offset, ty->align);
    mem->offset = offset;
    offset += ty->size;

    cur->next = mem;
    cur = mem;
    max_align = ty->align < max_align ? max_align : ty->align;
  }
  Type* ty = new_struct_type(align_to(offset, max_align), max_align, head.next);
  Var* tag = new_var(name, ty);
  *newenv = add_tag(env, tag);
  *rest = tok;
  return new_node(ND_DEFSTRUCT, tok_struct);
}
/* E.G.
(def main() -> int
  (ret3))
*/
static Node* parse_def(Token** rest, Token* tok, Env* env) {
  Token* tok_def = tok;
  tok = tok->next;
  char* fn = strndup(tok->loc, tok->len);
  tok = tok->next;

  // check an args list 
  char* pair;
  switch (tok->kind) {
  case TK_LPAREN: pair = ")"; break;
  case TK_LBRACKET: pair = "]"; break;
  default:
    error_tok(tok, "expect an args list");
  }
  tok = tok->next;
  
  // args parsing
  locals = NULL;
  Node head_args = {};
  Node* cur = &head_args;
  while (!stop_parse(tok)) {
    Token* tok_arg = tok;
    tok = tok->next;
    Type* ty = parse_type(&tok, tok, env);
    Var* var = new_lvar(strndup(tok_arg->loc, tok_arg->len), ty);
    cur->next = new_var_node(var, tok_arg);
    cur = cur->next;
    env = add_var(env, var);
  }
  tok = skip(tok, pair);

  // return type
  tok = skip(tok, "->");
  Type* ret_ty = parse_type(&tok, tok, env);

  // body
  Node head_body = {};
  cur = &head_body;
  while (!stop_parse(tok)) {
    cur->next = parse_expr(&tok, tok, &env, env);
    cur = cur->next;
  }
  *rest = tok;
  return new_function(fn, ret_ty, head_args.next, head_body.next, tok_def);
}


static Node* parse_expr(Token** rest, Token* tok, Env** newenv, Env* env) {
  if (tok->kind == TK_LPAREN || tok->kind == TK_LBRACKET) {
    return parse_list(rest, tok, newenv, env);
  }

  if (tok->kind == TK_NUM) {
    return parse_number(rest, tok, env);
  }

  if (tok->kind == TK_IDENT) {
    Var* var = lookup_var(env, tok);
    if (!var)
      error_tok(tok, "undefined variable");
    Node* node = new_var_node(var, tok);
    tok = tok->next;
    // either a deref or a struct/union member access
    while (equal(tok, ".")) {
      if (equal(tok->next, "*")) {
        node = new_unary(ND_DEREF, node, tok->next);
        tok = tok->next->next;
      } else {
        node = struct_ref(node, tok->next);
        tok = tok->next->next;
      }
    } 
    *rest = tok;
    return node;
  }

  if (equal(tok, "&")) {
    return parse_unary(rest, tok, env, ND_ADDR);
  }

  if (tok->kind == TK_STR) {
    return parse_str(rest, tok, env);
  }

  if (equal(tok, "true") || equal(tok, "false")) {
    return parse_bool(rest, tok, env);
  }
  
  error_tok(tok, "expect an expression");
}

// ----------- TYPE
static Type* parse_base_type(Token** rest, Token* tok, Env* env) {
  if (equal(tok, "int")) {
    *rest = tok->next;
    return ty_int;
  }
  if (equal(tok, "char")) {
    *rest = tok->next;
    return ty_char;
  }
  if (equal(tok, "short")) {
    *rest = tok->next;
    return ty_short;
  }
  if (equal(tok, "long")) {
    *rest = tok->next;
    return ty_long;
  }
  if (equal(tok, "bool")) {
    *rest = tok->next;
    return ty_bool;
  }
  Type* ty = lookup_tag(env, tok);
  if(ty) {
    *rest = tok->next;
    return ty;
  }
  error_tok(tok, "invalid base type");
}

static Type* parse_array_helper(Token** rest, Token* tok, Env* env, char* pair) {
  int len = get_number(parse_expr(&tok, tok, &env, env));
  if (tok->kind == TK_NUM) {
    Type* base = parse_array_helper(&tok, tok, env, pair);
    *rest = tok;
    return array_of(base, len);
  }
  Type* base = parse_type(&tok, tok, env);
  tok = skip(tok, pair);
  *rest = tok;
  return array_of(base, len);
}

static Type* parse_array_type(Token** rest, Token* tok, Env* env) {
  char* pair = tok->kind == TK_LBRACKET? "]" : ")";
  tok = tok->next;
  Type* ty = parse_array_helper(&tok, tok, env, pair);
  *rest = tok;
  return ty;
}


static Type* parse_pointer_type(Token** rest, Token* tok, Env* env) {
  tok = tok->next;
  if (equal(tok, "*")) {
    Type* base = parse_pointer_type(&tok, tok, env);
    *rest = tok;
    return pointer_to(base);
  }
  if (tok->kind == TK_LBRACKET || tok->kind == TK_LPAREN) {
    Type* base = parse_array_type(&tok, tok, env);
    *rest = tok;
    return pointer_to(base);
  }
  Type* base = parse_base_type(&tok, tok, env);
  *rest = tok;
  return pointer_to(base);
}

static Type* parse_typeof(Token** rest, Token* tok, Env* env) {
    char* pair = tok->kind == TK_LBRACKET ? "]" : ")";
    tok = skip(tok->next, "typeof");
    Node* e = parse_expr(&tok, tok, &env, env);
    add_type(e);
    *rest = skip(tok, pair); 
    return e->ty;
}

static Type* parse_type(Token** rest, Token* tok, Env* env) {
  if (is_list(tok) && equal(tok->next, "typeof")) {
    return parse_typeof(rest, tok, env);
  }
  if (equal(tok, "*")) {
    return parse_pointer_type(rest, tok, env);
  }
  if (tok->kind == TK_LBRACKET || tok->kind == TK_LPAREN) {
    return parse_array_type(rest, tok, env);
  }
  return parse_base_type(rest, tok, env);
}

static bool is_certain_expr(Token* tok, char* form) {
  return (tok->kind == TK_LPAREN || tok->kind == TK_LBRACKET) 
        && equal(tok->next, form);
}

static bool is_function(Token* tok) {
  return is_certain_expr(tok, "def");
}

static bool is_global_var(Token* tok) {
  return is_certain_expr(tok, "let");
}

static bool is_defstruct(Token* tok) {
  return is_certain_expr(tok, "defstruct");
}

static bool is_defunion(Token* tok) {
  return is_certain_expr(tok, "defunion");
}



static Node* parse_global_var(Token** rest, Token* tok, Env** newenv, Env* env) {
  char* pair = equal(tok, "(") ? ")" : "]";
  tok = tok->next;
  Node* node = parse_let(&tok, tok, &env, env, new_gvar);
  tok = skip(tok, pair);
  *rest = tok;
  *newenv = env;
  return node;
}


Node* parse(Token* tok) {
  Node* cur = prog;
  Env* env = NULL;
  while (tok->kind != TK_EOF) {
    if (is_function(tok)) {
      cur->next = parse_expr(&tok, tok, &env, env);
      cur = cur->next;
      add_type(cur);
    } else if (is_global_var(tok)) {
      cur->next = parse_global_var(&tok, tok, &env, env);
      cur = cur->next;  
    } else if (is_defstruct(tok)) {
      char* pair = tok->kind == TK_LPAREN ? ")" : "]";
      parse_defstruct(&tok, tok->next, &env, env);
      tok = skip(tok, pair);
    } else if (is_defunion(tok)) {
      char* pair = tok->kind == TK_LPAREN ? ")" : "]";
      parse_defunion(&tok, tok->next, &env, env);
      tok = skip(tok, pair);
    } else {
      error_tok(tok, "invalid expression");
    }
  }
  return prog;
}
