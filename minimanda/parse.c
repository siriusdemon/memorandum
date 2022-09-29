#include "manda.h"

// global flags
// binding_ctx: when a constant or literal is given in a binding context, this flag is set.
bool binding_ctx = false;
// array_ctx: when a constant or literal is given in an array literal, this flag is set.
bool array_ctx = false;

bool is_array_ctx() {
  return array_ctx;
}

bool is_binding_ctx() {
  return binding_ctx;
}

void set_array_ctx() {
  array_ctx = true;
}

void unset_array_ctx() {
  array_ctx = false;
}

void set_binding_ctx() {
  binding_ctx = true;
}

void unset_binding_ctx() {
  if (!is_array_ctx()) {
    binding_ctx = false;
  }
}

// programs
// prog is a linked list of AST nodes
Node* prog = &(Node){};
// locals is a linked list of local variables
Var* locals;
// globals is a linked list of globals variables
Var* globals;

// main program environment
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


Var* new_var(char* name, Type* ty) {
  Var* var = calloc(1, sizeof(Var));
  var->name = name;
  var->ty = ty;
  return var;
}

Var* new_lvar(char* name, Type *ty) {
  Var* var = new_var(name, ty);
  var->next = locals;
  var->is_local = true;
  locals = var;
  return var;
}

Var* new_gvar(char* name, Type *ty) {
  Var* var = new_var(name, ty);
  var->next = globals;
  var->is_local = false;
  globals = var;
  return var;
}

Member* new_member(Token* tok, Type* ty) {
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
Node* new_node(NodeKind kind, Token* tok) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  node->next = NULL;
  return node;
}

Node* merge_nodes(Node* cur, Node* list) {
  cur->next = list;
  while (cur->next) {
    cur = cur->next;
  }
  return cur;
}

Node* new_unary(NodeKind kind, Node* lhs, Token* tok) {
  Node* node = new_node(kind, tok);
  node->lhs = lhs;
  return node;
}
 
Node* new_binary(NodeKind kind, Node* lhs, Node* rhs, Token* tok) {
  Node* node = new_node(kind, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node* new_triple(NodeKind kind, Node* lhs, Node* mhs, Node* rhs, Token* tok) {
  Node* node = new_node(kind, tok);
  node->lhs = lhs;
  node->mhs = mhs;
  node->rhs = rhs;
  return node;
}

Node* new_num(int64_t val, Token* tok) {
  Node* node = new_node(ND_NUM, tok);
  node->val = val;
  return node;
}

Node* new_bool(bool val, Token* tok) {
  Node* node = new_node(ND_NUM, tok);
  node->val = val;
  return node;
}

Node* new_var_node(Var* var, Token* tok) {
  Node* node = new_node(ND_VAR, tok);
  node->var = var;
  return node;
}

Node* new_str_node(char* str, Token* tok) {
  Node* node = new_node(ND_STR, tok);
  node->str = str;
  return node;
}

Node* new_let(Node* lhs, Node* rhs, Token* tok) {
  Node* node = new_node(ND_LET, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node* new_set(Node* lhs, Node* rhs, Token* tok) {
  Node* node = new_node(ND_SET, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}


Node* new_if(Node* cond, Node* then, Node* els, Token* tok) {
  Node* node = new_node(ND_IF, tok);
  node->cond = cond;
  node->then = then;
  node->els = els;
  return node;
}

Node* new_do(Node* exprs, Token* tok) {
  Node* node = new_node(ND_DO, tok);
  node->body = exprs;
  return node;
}
Node* new_while(Node* cond, Node* then, Token* tok) {
  Node* node = new_node(ND_WHILE, tok);
  node->cond = cond;
  node->then = then;
  return node;
}

Node* new_app(char* fn, Node* args, Token* tok) {
  Node* node = new_node(ND_APP, tok);
  node->args = args;
  node->fn = fn;
  return node;
}

Node* new_function(char* fn, Type* ret_ty, Node* args, Node* body, Token* tok) {
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

Node* register_str(Node* str_node) {
  Var* var = new_anon_gvar(str_node->ty);
  Node* var_node = new_var_node(var, str_node->tok);
  var_node->ty = str_node->ty;
  Node* let = new_let(var_node, str_node, str_node->tok);
  let->next = prog;
  prog = let;
  return var_node;
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

static Node* literal_expand(Node* lhs, Node* rhs, Token* tok) {
  Node head = {};
  Node* cur = &head;
  int i = 0;
  // str literal
  if (rhs->kind == ND_STR) {
    for (char* p = rhs->str; *p != '\0'; p++, i++) {
      Node* n = new_triple(ND_ISET, lhs, new_num(i, tok), new_num(*p, tok), tok);
      cur = merge_nodes(cur, n);
    }
    Node* n = new_triple(ND_ISET, lhs, new_num(i, tok), new_num('\0', tok), tok);
    cur = merge_nodes(cur, n);
  }

  if (rhs->kind == ND_ARRAY_LITERAL) {
    for (Node* e = rhs->elements; e; e = e->next, i++) {
      if (e->kind == ND_ARRAY_LITERAL || e->kind == ND_STR) {
        Node* new_lhs = new_binary(ND_IGET, lhs, new_num(i, tok), tok);
        Node* list = literal_expand(new_lhs, e, tok);
        cur = merge_nodes(cur, list);
      } else {
        Node* n = new_triple(ND_ISET, lhs, new_num(i, tok), e, tok);
        cur->next = n;
        cur = n;
      }
    }
  }
  return head.next;
}

// Sexp Parser
static Sexp* parse_sexp(Token** rest, Token* tok);
static Sexp* parse_sexp_list(Token** rest, Token* tok);
static Sexp* parse_sexp_symbol(Token** rest, Token* tok);

// Sexp Evaluator
static Node* eval_sexp(Sexp* se, MEnv* menv, Env** newenv, Env* env);
static Node* eval_list(Sexp* se, MEnv* menv, Env** newenv, Env* env);
static Node* eval_application(Sexp* se, MEnv* menv, Env* env);
static Node* eval_def(Sexp* se, MEnv* menv, Env** newenv, Env* env);
static Node* eval_defstruct(Sexp* se, MEnv* menv, Env** newenv, Env* env);
static Node* eval_defunion(Sexp* se, MEnv* menv, Env** newenv, Env* env);
static Node* eval_struct_ref(Sexp* se, MEnv* menv, Env* env);
static Node* eval_array_shortcut(Sexp* se, MEnv* menv, Env* env);
static Node* eval_let(Sexp* se, MEnv* menv, Env** newenv, Env* env, Var* (*alloc_var)(char* name, Type* ty));
static Node* eval_set(Sexp* se, MEnv* menv, Env* env);
static Node* eval_while(Sexp* se, MEnv* menv, Env* env);
static Node* eval_if(Sexp* se, MEnv* menv, Env* env);
static Node* eval_do(Sexp* se, MEnv* menv, Env* env);
static Node* eval_macro_primitive(Sexp* se, MEnv* menv, Env* env);
static Node* eval_primitive(Sexp* se, MEnv* menv, Env* env);
static Node* eval_triple(Sexp* se, MEnv* menv, Env* env, NodeKind kind);
static Node* eval_binary(Sexp* se, MEnv* menv, Env* env, NodeKind kind, bool left_compose, bool near_compose);
static Node* eval_unary(Sexp* se, MEnv* menv, Env* env, NodeKind kind);
static Node* eval_sizeof(Sexp* se, MEnv* menv, Env* env);
static Node* eval_num(Sexp* se);
static Node* eval_str(Sexp* se, MEnv* menv, Env* env);
static Node* eval_cast(Sexp* se, MEnv* menv, Env* env);
static Node* eval_deftype(Sexp* se, MEnv* menv, Env** newenv, Env* env);
static Type* eval_base_type(Sexp* se, MEnv* menv, Env* env);
static Type* eval_type(Sexp* se, MEnv* menv, Env* env);
static Type* eval_typeof(Sexp* se, MEnv* menv, Env* env);

/* Macro Interpreter

Macro system is essentially an interpreter of symbol expression. It consumes symbol 
expression then outputs AST nodes.

Macro definition is likely a closed function in the interpreter. There is no need 
to capture variables.

THere are also primitives in such an interpreter. When an application is met, the 
interpreter must apply transform when it is a macro primitives.

*/
static Macro* macros = NULL;
static Node* macro_expand(Sexp* se, MEnv* menv, Env* env);

static MEnv* new_menv() {
  MEnv* menv = calloc(1, sizeof(MEnv));
  return menv; 
}

static MEnv* add_symbol(MEnv* oldmenv, Sexp* symbol, Sexp* value) {
  MEnv* menv = new_menv();
  menv->symbol = symbol;
  menv->value = value;
  menv->next = oldmenv;
  return menv;
}

static bool is_same_symbol(Sexp* s1, Sexp* s2) {
  return (s1->tok->len == s2->tok->len) && 
    !strncmp(s1->tok->loc, s2->tok->loc, s1->tok->len);
}

static Sexp* lookup_symbol(MEnv* menv, Sexp* symbol) {
  MEnv* cur = menv;
  while (cur) {
    if (is_same_symbol(cur->symbol, symbol)) {
      return cur->value;
    }
    cur = cur->next;
  }
  return NULL;
}

static Macro* new_macro(Token* name, Sexp* args, Sexp* body) {
  Macro* t = calloc(1, sizeof(Macro));
  t->name = name;
  t->args = args;
  t->body = body;
  t->next = NULL;
  return t;
}

static Sexp* new_sexp(SexpKind kind, Token* tok) {
  Sexp* s = calloc(1, sizeof(Sexp));
  s->kind = kind;
  s->tok = tok;
  s->next = NULL;
  s->elements = NULL;
}

static Macro* lookup_macro(Token* t) {
  Macro* cur = macros;
  while (cur != NULL) {
    if (t->len == cur->name->len && !strncmp(t->loc, cur->name->loc, t->len))
      return cur;
    cur = cur->next;
  }
  return NULL;
}

static void register_macro(Macro* t) {
  t->next = macros;
  macros = t;
}

static bool is_macro(Token* t) {
  if (lookup_macro(t)) {
    return true;
  }
  return false;
}

static bool is_macro_primitive(Token* tok) {
  static char *kw[] = {"str"};
  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (equal(tok, kw[i]))
      return true;
  return false;
}

bool is_type(Token* tok) {
  static char *kw[] = {"int", "char", "long", "short", "bool"};
  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (equal(tok, kw[i]))
      return true;
  return false;
}

bool is_array(Token* tok) {
  return (is_list(tok)) && (tok->next->kind == TK_NUM);
}

static Sexp* skip_sexp(Sexp* se, char* s) {
  if (!equal(se->tok, s)) {
    error_tok(se->tok, "expected '%s'", s);
  }
  return se->next;
}

static Sexp* new_symbol_with_token(char* str) {
  Token* tok = calloc(1, sizeof(Token));
  tok->kind = TK_RESERVED;
  tok->loc = str;
  tok->len = strlen(str);
  Sexp* se = new_sexp(SE_SYMBOL, tok); 
  return se;
}

static int sexp_to_str(Sexp* se, char** str) {
  if (se->kind == SE_SYMBOL) {
    *str = strndup(se->tok->loc, se->tok->len);
    return se->tok->len;
  } 
  int size = 128;
  int len = 0;
  char* s;
  char* buffer = calloc(1, size);
  buffer[len++] = '(';
  for (Sexp* cur = se->elements; cur; cur = cur->next) {
    int len0 = sexp_to_str(cur, &s);
    len += len0;
    if (len > size) {
      size = size * 2;
      buffer = realloc(buffer, size);
    }
    strncat(buffer, s, len0);
    if (cur->next) buffer[len++] = ' ';
  }
  buffer[len++] = ')';
  *str = buffer;
  return len;
}

static Node* eval_macro_str(Sexp* se, MEnv* menv, Env* env) {
  char* str; 
  Sexp* val;
  if (se->elements->next->tok->kind == TK_IDENT) {
    val = lookup_symbol(menv, se->elements->next);
  } else {
    val = se->elements->next;
  }
  int len = sexp_to_str(val, &str);
  Type* ty = array_of(ty_char, len + 1);
  Node* str_node = new_str_node(str, se->elements->tok);
  str_node->ty = ty;
  Node* var_node = register_str(str_node);
  return var_node;
}

static Node* eval_str(Sexp* se, MEnv* menv, Env* env) {
  Type* ty = array_of(ty_char, strlen(se->tok->str) + 1);
  Node* str_node = new_str_node(se->tok->str, se->tok);
  str_node->ty = ty;
  if (is_binding_ctx()) {
    unset_binding_ctx();
    return str_node;
  }
  Node* var_node = register_str(str_node);
  return var_node;
}

static Node* eval_do(Sexp* se, MEnv* menv, Env* env) {
  Token* tok = se->elements->tok;
  Node head = {};
  Node* cur = &head;
  Sexp* secur = se->elements->next;
  while (secur) {
    cur = merge_nodes(cur, eval_sexp(secur, menv, &env, env));
    secur = secur->next;
  }
  Node* node = new_do(head.next, tok);
  return node;
}

static Node* eval_while(Sexp* se, MEnv* menv, Env* env) {
  Token* tok = se->elements->tok;
  Node* cond = eval_sexp(se->elements->next, menv, &env, env);
  Node head = {};
  Node* cur = &head;
  Sexp* rest = se->elements->next->next;
  while (rest) {
    cur = merge_nodes(cur, eval_sexp(rest, menv, &env, env));
    rest = rest->next;
  }
  Node* node = new_while(cond, head.next, tok);
  return node;
}

static Node* eval_set(Sexp* se, MEnv* menv, Env* env) {
  Token* tok = se->elements->tok;
  Node* lhs = eval_sexp(se->elements->next, menv, &env, env);
  Node* rhs = eval_sexp(se->elements->next->next, menv, &env, env);
  if (rhs->kind == ND_ARRAY_LITERAL) {
    Node* node = new_let(lhs, NULL, tok);
    node->next = literal_expand(lhs, rhs, tok);
    return node;
  }
  Node* node = new_set(lhs, rhs, tok);
  return node;
}

// (let var :type val)
static Node* eval_let(Sexp* se, MEnv* menv,  Env** newenv, Env* env, Var* (*alloc_var)(char* name, Type* ty)) {
  Token* tok = se->elements->tok;
  Sexp* se_var = se->elements->next;
  Sexp* se_type = skip_sexp(se_var->next, ":");
  Sexp* se_val = se_type->next;
  if (se_var->tok->kind != TK_IDENT) {
    error_tok(se_var->tok, "bad identifier");
  }
  Type* ty = eval_type(se_type, menv, env);
  Var* var = alloc_var(strndup(se_var->tok->loc, se_var->tok->len), ty);
  *newenv = add_var(env, var);
  Node* lhs = new_var_node(var, se_var->tok);
  set_binding_ctx();

  Node* rhs = NULL;
  if (se_val) {
    rhs = eval_sexp(se_val, menv, &env, env);
    if (rhs->kind == ND_ARRAY_LITERAL || rhs->kind == ND_STR) {
      Node* node = new_let(lhs, NULL, tok);
      node->next = literal_expand(lhs, rhs, tok);
      return node;
    }
  }

  Node* node = new_let(lhs, rhs, tok);
  return node;
}

static Node* eval_if(Sexp* se, MEnv* menv, Env* env) {
  Token* tok = se->elements->tok;
  Node* cond = eval_sexp(se->elements->next, menv, &env, env);
  Node* then = eval_sexp(se->elements->next->next, menv, &env, env);
  Node* els = NULL;
  if (se->elements->next->next->next) {
    els = eval_sexp(se->elements->next->next->next, menv, &env, env);
  }
  Node* node = new_if(cond, then, els, tok);
  return node;
}

static Node* eval_macro_primitive(Sexp* se, MEnv* menv, Env* env) {
  if (equal(se->elements->tok, "str")) {
    return eval_macro_str(se, menv, env); 
  }
  error_tok(se->tok, "unsupported yet!");
}

static Node* eval_array_shortcut(Sexp* se, MEnv* menv, Env* env) {
  Token* tok = se->tok;  
  Node head = {}; 
  Node* cur = &head;
  Type* base = NULL;
  int len = 0;

  set_array_ctx();

  Sexp* se_cur = se->elements->next;
  while (se_cur) {
    cur->next = eval_sexp(se_cur, menv, &env, env);
    cur = cur->next;
    add_type(cur);
    if (!base) {
      base = cur->ty;
    } else if (base->kind != cur->ty->kind ) {
      error_tok(tok, "type mismatch!");
    }
    se_cur = se_cur->next;
    len++;
  }

  unset_array_ctx();

  Node* node = new_node(ND_ARRAY_LITERAL, tok);
  node->elements = head.next;
  node->ty = array_of(base, len);
  return node;
}

static Node* eval_struct_ref(Sexp* se, MEnv* menv, Env* env) {
  Node* lhs = eval_sexp(se->elements->next, menv, &env, env);
  add_type(lhs);
  if (lhs->ty->kind != TY_STRUCT && lhs->ty->kind != TY_UNION) {
    error_tok(lhs->tok, "not a struct");
  }
  Node* node = new_unary(ND_STRUCT_REF, lhs, se->tok);
  node->member = get_struct_member(lhs->ty, se->elements->next->next->tok);
  return node;
}

static Node* eval_defunion(Sexp* se, MEnv* menv, Env** newenv, Env* env) {
  Token* tok = se->tok;
  Sexp* se_tag = se->elements->next;
  Sexp* members = se_tag->next;

  char* name = strndup(se_tag->tok->loc, se_tag->tok->len);

  // members
  Member head = {};
  Member* cur = &head;
  int max_align = 1;
  int max_size = 0;

  while (members) {
    Token* mem_tok = members->tok;
    Type* ty = eval_type(members->next, menv, env);
    Member* mem = new_member(mem_tok, ty);
    mem->offset = 0;

    members = members->next->next;
    cur->next = mem;
    cur = cur->next;
    max_align = ty->align < max_align? max_align : ty->align;
  }

  Type* ty = new_union_type(max_size,  max_align, head.next);
  Var* tag = new_var(name, ty);
  *newenv = add_tag(env, tag);
  return new_node(ND_DEFUNION, tok);
}

static Node* eval_defstruct(Sexp* se, MEnv* menv, Env** newenv, Env* env) {
  Token* tok = se->tok;
  Sexp* se_tag = se->elements->next;
  Sexp* members = se_tag->next;

  char* name = strndup(se_tag->tok->loc, se_tag->tok->len);

  // members
  Member head = {};
  Member* cur = &head;
  int offset = 0;
  int max_align = 1;
  while (members) {
    Token* mem_tok = members->tok;
    Type* ty = eval_type(members->next, menv, env);
    Member* mem = new_member(mem_tok, ty);
    offset = align_to(offset, ty->align);
    mem->offset = offset;
    offset += ty->size;

    members = members->next->next;
    cur->next = mem;
    cur = cur->next;
    max_align = ty->align < max_align? max_align : ty->align;
  }

  Type* ty = new_struct_type(align_to(offset, max_align), max_align, head.next);
  Var* tag = new_var(name, ty);
  *newenv = add_tag(env, tag);
  return new_node(ND_DEFSTRUCT, tok);
}

static Node* eval_def(Sexp* se, MEnv* menv, Env** newenv, Env* env) {
  Token* tok = se->tok;  
  Sexp* se_fn = skip_sexp(se->elements, "def");
  Sexp* se_args = se_fn->next->elements;
  Sexp* se_type = skip_sexp(se_fn->next->next, "->");
  Sexp* se_body = se_type->next;  
  char* fn = strndup(se_fn->tok->loc, se_fn->tok->len);  
  // args parsing
  locals = NULL;
  Node head_args = {};
  Node* cur = &head_args;
  while (se_args) {
    Token* tok_arg = se_args->tok;
    Type* ty = eval_type(se_args->next, menv, env);
    Var* var = new_lvar(strndup(tok_arg->loc, tok_arg->len), ty);
    cur->next = new_var_node(var, tok_arg);
    cur = cur->next;
    se_args = se_args->next->next;
    env = add_var(env, var);
  }

  Type* ret_ty = eval_type(se_type, menv, env);

  // body
  Node head_body = {};
  cur = &head_body;
  while (se_body) {
    cur = merge_nodes(cur, eval_sexp(se_body, menv, &env, env));
    se_body = se_body->next;
  }
  return new_function(fn, ret_ty, head_args.next, head_body.next, tok);
}

static Node* eval_application(Sexp* se, MEnv* menv, Env* env) {
  Token* tok = se->tok;
  // function
  Sexp* secur = se->elements;
  char* fn = strndup(secur->tok->loc, secur->tok->len);
  secur = secur->next;
  // args
  Node head = {};
  Node* cur = &head;
  while (secur) {
    cur->next = eval_sexp(secur, menv, &env, env);
    cur = cur->next;
    secur = secur->next;
  }
  return new_app(fn, head.next, tok);
}

static Node* eval_primitive(Sexp* se, MEnv* menv, Env* env) {
  Token* tok = se->elements->tok;
#define Match(t, handle)    if (equal(tok, t)) return handle;
  Match("+", eval_binary(se, menv, env, ND_ADD, true, false))
  Match("-", eval_binary(se, menv, env, ND_SUB, true, false))
  Match("*", eval_binary(se, menv, env, ND_MUL, true, false))
  Match("/", eval_binary(se, menv, env, ND_DIV, true, false))
  Match("mod", eval_binary(se, menv, env, ND_MOD, false, false))
  Match("=", eval_binary(se, menv, env, ND_EQ, false, true))
  Match(">", eval_binary(se, menv, env, ND_GT, false, true))
  Match("<", eval_binary(se, menv, env, ND_LT, false, true))
  Match(">=", eval_binary(se, menv, env, ND_GE, false, true))
  Match("<=", eval_binary(se, menv, env, ND_LE, false, true))
  Match("iget", eval_binary(se, menv, env, ND_IGET, true, false))
  Match("iset", eval_triple(se, menv, env, ND_ISET))
  Match("addr", eval_unary(se, menv, env, ND_ADDR))
  Match("deref", eval_unary(se, menv, env, ND_DEREF))
  Match("not", eval_unary(se, menv, env, ND_NOT))
  Match("and", eval_binary(se, menv, env, ND_AND, true, false))
  Match("or", eval_binary(se, menv, env, ND_OR, true, false))
  Match("bitnot", eval_unary(se, menv, env, ND_BITNOT))
  Match("bitand", eval_binary(se, menv, env, ND_BITAND, true, false))
  Match("bitor", eval_binary(se, menv, env, ND_BITOR, true, false))
  Match("bitxor", eval_binary(se, menv, env, ND_BITXOR, false, false))
  Match("sra", eval_binary(se, menv, env, ND_SRA, false, false))
  Match("srl", eval_binary(se, menv, env, ND_SRL, false, false))
  Match("sll", eval_binary(se, menv, env, ND_SLL, false, false))
  Match("sizeof", eval_sizeof(se, menv, env))
  Match("cast", eval_cast(se, menv, env))
#undef Match
}

static Node* eval_sizeof(Sexp* se, MEnv* menv, Env* env) {
  Type* ty = eval_type(se->elements->next, menv, env);
  return new_num(ty->size, se->elements->next->tok);
}

static Node* eval_cast(Sexp* se, MEnv* menv, Env* env) {
  Token* tok = se->tok;
  Node* lhs = eval_sexp(se->elements->next, menv, &env, env);
  add_type(lhs);
  Type* ty = eval_type(se->elements->next->next, menv, env);
  Node* node = new_unary(ND_CAST, lhs, tok);
  node->ty = ty;
  return node;
}

static Node* eval_unary(Sexp* se, MEnv* menv, Env* env, NodeKind kind) {
  Token* tok = se->tok;
  Node* lhs = eval_sexp(se->elements->next, menv, &env, env);
  Node* node = new_unary(kind, lhs, tok);
  return node;
}

static Node* eval_binary(Sexp* se, MEnv* menv, Env* env, NodeKind kind, bool left_compose, bool near_compose) {
  Token* op_tok = se->elements->tok;
  Node* lhs = eval_sexp(se->elements->next, menv, &env, env);
  Node* rhs = eval_sexp(se->elements->next->next, menv, &env, env);
  Node* node = new_binary(kind, lhs, rhs, op_tok);
  Sexp* rest = se->elements->next->next->next;

  while (left_compose && rest) {
    lhs = node;
    rhs = eval_sexp(rest, menv, &env, env);
    node = new_binary(kind, lhs, rhs, op_tok);
    rest = rest->next;
  }
  return node;
}

static Node* eval_triple(Sexp* se, MEnv* menv, Env* env, NodeKind kind) {
  Token* op_tok = se->elements->tok;
  Node* lhs = eval_sexp(se->elements->next, menv, &env, env);
  Node* mhs = eval_sexp(se->elements->next->next, menv, &env, env);
  Node* rhs = eval_sexp(se->elements->next->next->next, menv, &env, env);
  Node* node = new_triple(kind, lhs, mhs, rhs, op_tok);
  return node;
}

static Node* eval_num(Sexp* se) {
  Node* node = new_num(se->tok->val, se->tok);
  return node;
}

static Node* eval_bool(Sexp* se) {
  Node* node = new_bool(se->tok->val, se->tok);
  return node;
}

static Node* eval_list(Sexp* se, MEnv* menv, Env** newenv, Env* env) {
  if (equal(se->elements->tok, "do")) {
    return eval_do(se, menv, env); 
  } else if (equal(se->elements->tok, "if")) {
    return eval_if(se, menv, env); 
  } else if (equal(se->elements->tok, "let")) {
    return eval_let(se, menv, newenv, env, new_lvar); 
  } else if (equal(se->elements->tok, "set")) {
    return eval_set(se, menv, env); 
  } else if (equal(se->elements->tok, "while")) {
    return eval_while(se, menv, env); 
  } else if (equal(se->elements->tok, "def")) {
    return eval_def(se, menv, newenv, env); 
  } else if (equal(se->elements->tok, "defstruct")) {
    return eval_defstruct(se, menv, newenv, env); 
  } else if (equal(se->elements->tok, "defunion")) {
    return eval_defunion(se, menv, newenv, env); 
  } else if (equal(se->elements->tok, "struct-ref")) {
    return eval_struct_ref(se, menv, env); 
  } else if (equal(se->elements->tok, "make-array")) {
    return eval_array_shortcut(se, menv, env); 
  } else if (equal(se->elements->tok, "deftype")) {
    return eval_deftype(se, menv, newenv, env); 
  } else if (equal(se->elements->tok, "deftype")) {
    return eval_deftype(se, menv, newenv, env); 
  } else if (is_macro_primitive(se->elements->tok)) {
    return eval_macro_primitive(se, menv, env);
  } else if (is_primitive(se->elements->tok)) {
    return eval_primitive(se, menv, env);
  } else if (is_macro(se->elements->tok)) {
    return macro_expand(se, menv, env);
  } else {
    return eval_application(se, menv, env);
  }
}


static Node* eval_deftype(Sexp* se, MEnv* menv, Env** newenv, Env* env) {
  Token* tok = se->elements->tok;
  Sexp* se_tag = se->elements->next;
  Sexp* se_ty = se_tag->next;
  char* name = strndup(se_tag->tok->loc, se_tag->tok->len);
  Type* ty = eval_type(se_ty, menv, env);
  Var* tag = new_var(name, ty);
  *newenv = add_tag(env, tag);
  return new_node(ND_DEFTYPE, tok);
}

static Type* eval_base_type(Sexp* se, MEnv* menv, Env* env) {
  if (equal(se->tok, "long")) {
    return ty_long;
  }
  if (equal(se->tok, "int")) {
    return ty_int;
  }
  if (equal(se->tok, "short")) {
    return ty_short;
  }
  if (equal(se->tok, "char")) {
    return ty_char;
  }
  if (equal(se->tok, "bool")) {
    return ty_bool;
  }
  Type* ty = lookup_tag(env, se->tok);
  if (ty) {
    return ty;
  }
  error_tok(se->tok, "invalid base type!");
}

static Type* eval_pointer_type(Sexp* se, MEnv* menv, Env* env) {
  Type* base = eval_type(se->elements->next, menv, env);
  return pointer_to(base);
}

static Type* eval_array_type_helper(Sexp* se, MEnv* menv, Env* env) {
  int len = se->tok->val;
  Sexp* se_base = se->next;
  Type* ty_base;
  if (se_base->tok->kind == TK_NUM) {
    ty_base = eval_array_type_helper(se_base, menv, env); 
  } else {
    ty_base = eval_type(se_base, menv, env);
  }
  return array_of(ty_base, len);
}

static Type* eval_array_type(Sexp* se, MEnv* menv, Env* env) {
  return eval_array_type_helper(se->elements, menv, env);
}

static Type* eval_typeof(Sexp* se, MEnv* menv, Env* env) {
  Node* node = eval_sexp(se->elements->next, menv, &env, env);
  add_type(node);
  return node->ty;
}

static Type* eval_type(Sexp* se, MEnv* menv, Env* env) {
  if (se->kind == SE_LIST) {
    if (equal(se->elements->tok, "pointer")) {
      return eval_pointer_type(se, menv, env);
    }
    if (equal(se->elements->tok, "typeof")) {
      return eval_typeof(se, menv, env);
    }
    if (se->elements->tok->kind == TK_NUM) {
      return eval_array_type(se, menv, env);
    }
  }
  return eval_base_type(se, menv, env);
}


static Node* eval_sexp(Sexp* se, MEnv* menv, Env** newenv, Env* env) {
  if (se->kind == SE_LIST) {
    return eval_list(se, menv, newenv, env);
  }
  
  Token* tok = se->tok;
  if (tok->kind == TK_NUM) {
    return eval_num(se);
  }

  if (equal(tok, "true")) {
    tok->val = TRUE;
    return eval_bool(se);
  }

  if (equal(tok, "false")) {
    tok->val = FALSE;
    return eval_bool(se);
  }

  if (tok->kind == TK_IDENT) {
    Sexp* val = lookup_symbol(menv, se); 
    if (val) {
      return eval_sexp(val, menv, newenv, env);
    }
    Var* var = lookup_var(env, tok);
    if (var) {
      return new_var_node(var, tok);
    }
    error_tok(tok, "undefined variable!");
  }

  if (tok->kind == TK_STR) {
    return eval_str(se, menv, env);
  }
  error("invalid symbol expression");
}


Node* macro_expand(Sexp* se, MEnv* menv, Env* env) {
  Macro* m = lookup_macro(se->elements->tok);
  Sexp* args = se->elements->next;

  // prepare the macro environment
  Sexp* symbol;
  for (symbol = m->args; symbol && args; symbol = symbol->next, args = args->next) {
    menv = add_symbol(menv, symbol, args);
  }
  if (symbol || args) {
    error_tok(se->tok, "args number mismatch!");
  }
  // perform transformation
  Node* node = eval_sexp(m->body, menv, &env, env);
  return node;
}


Sexp* parse_sexp(Token** rest, Token* tok) {
  if (is_list(tok)) {
    return parse_sexp_list(rest, tok);
  } else {
    return parse_sexp_symbol(rest, tok);
  }
}

Sexp* parse_sexp_array_literal(Token** rest, Token* tok) {
  Sexp* list = new_sexp(SE_LIST, tok);
  Sexp* cur = new_symbol_with_token("make-array");
  list->elements = cur;
  char* pair = get_pair(&tok, tok->next->next);
  while (!stop_parse(tok)) {
    cur->next = parse_sexp(&tok, tok);
    cur = cur->next; 
  }
  *rest = skip(tok, pair);
  return list;
}

Sexp* parse_sexp_hash_literal(Token** rest, Token* tok) {
  if (equal(tok->next, "a")) {
    return parse_sexp_array_literal(rest, tok);
  }
  error_tok(tok, "unsupported hash literal");
}

Sexp* parse_sexp_symbol(Token** rest, Token* tok) {
  if (equal(tok, "&")) {
    Sexp* list = new_sexp(SE_LIST, tok);
    list->elements = new_symbol_with_token("addr");
    list->elements->next = parse_sexp(&tok, tok->next);
    *rest = tok;
    return list;
  }

  if (equal(tok, "#")) {
    return parse_sexp_hash_literal(rest, tok);
  }

  if (tok->kind == TK_IDENT) {
    Sexp* se = new_sexp(SE_SYMBOL, tok);
    tok = tok->next;
    while (equal(tok, ".")) {
      if (equal(tok->next, "*")) {
        Sexp* list = new_sexp(SE_LIST, tok->next);
        list->elements = new_symbol_with_token("deref");
        list->elements->next = se;
        se = list;
        tok = tok->next->next;
      } else {
        // a.v -> (struct-ref a v)
        Sexp* list = new_sexp(SE_LIST, tok);
        list->elements = new_symbol_with_token("struct-ref");
        se->next = new_sexp(SE_SYMBOL, tok->next);
        list->elements->next = se;
        se = list;
        tok = tok->next->next;
      }
    }
    *rest = tok;
    return se;
  }

  // pointer type, there may be another better way to do this
  if (equal(tok, "*") && (equal(tok->next, "*") || is_type(tok->next) || is_array(tok->next))) {
    Sexp* list = new_sexp(SE_LIST, tok);
    list->elements = new_symbol_with_token("pointer");
    list->elements->next = parse_sexp(&tok, tok->next);
    *rest = tok;
    return list;
  }
  
  Sexp* s = new_sexp(SE_SYMBOL, tok);
  *rest = tok->next;
  return s;
}

Sexp* parse_sexp_list(Token** rest, Token* tok) {
  Sexp* list = new_sexp(SE_LIST, tok);
  char* pair = get_pair(&tok, tok);
  // parse elements
  Sexp head = {};
  Sexp* cur = &head;
  while (!stop_parse(tok)) {
    cur->next = parse_sexp(&tok, tok);
    cur = cur->next;
  }
  list->elements = head.next;

  tok = skip(tok, pair);
  *rest = tok;
  return list;
}
static Node* eval_defmacro(Sexp* se, MEnv* menv, Env** newenv, Env* env) {
  Token* tok = se->tok;
  Sexp* se_name = se->elements->next;
  Sexp* se_args = se_name->next;
  Sexp* se_body = se_args->next;

  // macro
  Token* name = se_name->tok;

  Macro* t = new_macro(name, se_args->elements, se_body);
  register_macro(t);

  return new_node(ND_DEFMACRO, tok);
}

static bool is_certain_expr(Sexp* se, char* form) {
  return se->kind == SE_LIST && equal(se->elements->tok, form);
}

static bool is_function(Sexp* se) {
  return is_certain_expr(se, "def");
}

static bool is_global_var(Sexp* se) {
  return is_certain_expr(se, "let");
}

static bool is_defstruct(Sexp* se) {
  return is_certain_expr(se, "defstruct");
}

static bool is_defunion(Sexp* se) {
  return is_certain_expr(se, "defunion");
}


static bool is_defmacro(Sexp* se) {
  return is_certain_expr(se, "defmacro");
}


static Node* eval_global_var(Sexp* se, MEnv* menv, Env** newenv, Env* env) {
  Node* node = eval_let(se, menv, &env, env, new_gvar);
  *newenv = env;
  return node;
}


static Sexp* program_as_sexp(Token* tok) {
  Sexp prog = {};
  Sexp* se_cur = &prog;
  while (tok->kind != TK_EOF) {
    se_cur->next = parse_sexp(&tok, tok);
    se_cur = se_cur->next;
  }
  return prog.next;
}

Node* parse(Token* tok) {
  Node* cur = prog;
  Env* env = NULL;
  
  Sexp* se = program_as_sexp(tok);
  MEnv* menv = NULL;

  while (se) {
    if (is_function(se)) {
      Node* node = eval_sexp(se, menv, &env, env);
      add_type(node);
      cur = merge_nodes(cur, node);
    } else if (is_global_var(se)) {
      cur = merge_nodes(cur, eval_global_var(se, menv, &env, env));
    } else if (is_defstruct(se)) {
      cur = merge_nodes(cur, eval_defstruct(se, menv, &env, env));
    } else if (is_defunion(se)) {
      cur = merge_nodes(cur, eval_defunion(se, menv, &env, env));
    } else if (is_defmacro(se)) {
      cur = merge_nodes(cur, eval_defmacro(se, menv, &env, env));
    } else {
      error_tok(se->tok, "invalid expression");
    }
    se = se->next;
  }
  return prog;
}
