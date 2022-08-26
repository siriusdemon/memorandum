#include "manda.h"

Node* prog = &(Node){};
Var* locals;
Var* globals;
// struct tags, ty->member is struct member
Var* tags;



static Node* parse_global_var(Token** rest, Token *tok);
static Node* parse_expr(Token** rest, Token* tok);
static Node* parse_list(Token** rest, Token* tok);
static Node* parse_expr(Token** rest, Token* tok);
static Node* parse_let(Token** rest, Token* tok, Var* (*alloc_var)(char* name, Type* ty));
static Node* parse_set(Token** rest, Token* tok);
static Node* parse_if(Token** rest, Token* tok);
static Node* parse_while(Token** rest, Token* tok);
static Node* parse_number(Token** rest, Token* tok);
static Node* parse_str(Token** rest, Token* tok);
static Node* parse_primitive(Token** rest, Token* tok);
static Node* parse_binary(Token** rest, NodeKind kind, bool left_compose, Token* tok);
static Node* parse_triple(Token** rest, NodeKind kind, Token* tok);
static Node* parse_deref(Token** rest, Token* tok);
static Node* parse_addr(Token** rest, Token* tok);
static Node* parse_application(Token** rest, Token* tok);
static Node* parse_def(Token** rest, Token* tok);
static Node* parse_defstruct(Token** rest, Token* tok);
static Type* parse_type(Token** rest, Token* tok);

static bool stop_parse(Token* tok) {
  return tok->kind == TK_EOF || tok->kind == TK_RPAREN || tok->kind == TK_RBRACKET;
}

// ----- variable 
static Var* find_var(Token* tok) {
  for (Var* var = locals; var; var = var->next)
    if (strlen(var->name) == tok->len && !strncmp(tok->loc, var->name, tok->len))
      return var;
    
  for (Var* var = globals; var; var = var->next)
    if (strlen(var->name) == tok->len && !strncmp(tok->loc, var->name, tok->len))
      return var;
  return NULL;
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

static void push_tags(Var* tag) {
  tag->next = tags;
  tags = tag;
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

static Node* new_num(int val, Token* tok) {
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



static Node* parse_list(Token** rest, Token* tok) {
  Node* node;
  char* pair = equal(tok, "(") ? ")" : "]";
  tok = tok->next;
  if (equal(tok, "let")) {
    node = parse_let(&tok, tok, new_lvar);
  } else if (equal(tok, "set")) {
    node = parse_set(&tok, tok);
  } else if (equal(tok, "if")) {
    node = parse_if(&tok, tok);
  } else if (equal(tok, "while")) {
    node = parse_while(&tok, tok);
  } else if (equal(tok, "def")) {
    node = parse_def(&tok, tok);
  } else if (equal(tok, "defstruct")) {
    node = parse_defstruct(&tok, tok);
  } else if (is_primitive(tok)) {  
    node = parse_primitive(&tok, tok);
  }  else {
    node = parse_application(&tok, tok);
  }
  tok = skip(tok, pair);
  *rest = tok;
  return node;
}

static Node* parse_if(Token **rest, Token *tok) {
  Token* tok_if = tok;
  tok = skip(tok, "if");
  Node* cond = parse_expr(&tok, tok);
  Node* then = parse_expr(&tok, tok);
  Node* els = NULL;
  if (!stop_parse(tok)) {
    els = parse_expr(&tok, tok);
  }
  Node* node = new_if(cond, then, els, tok_if);
  *rest = tok;
  return node;
}

static Node* parse_while(Token **rest, Token *tok) {
  Token* tok_while = tok;
  tok = skip(tok, "while");
  Node* cond = parse_expr(&tok, tok);
  Node head = {};
  Node* cur = &head;
  while (!stop_parse(tok)) {
    cur->next = parse_expr(&tok, tok);
    cur = cur->next;
  }
  Node* node = new_while(cond, head.next, tok_while);
  *rest = tok;
  return node;
}


static Node* parse_let(Token **rest, Token *tok, Var* (*alloc_var)(char* name, Type* ty)) {
  Token* tok_let = tok;
  tok = skip(tok, "let");
  Token* tok_var = tok;
  tok = skip(tok->next, ":");
  Type* ty = parse_type(&tok, tok);
  Var* var = alloc_var(strndup(tok_var->loc, tok_var->len), ty);
  Node* lhs = new_var_node(var, tok_var);
  Node* rhs = NULL;
  if (!stop_parse(tok)) { rhs = parse_expr(&tok, tok); }
  Node* node = new_let(lhs, rhs, tok_let);
  *rest = tok;
  return node;
}

static Node* parse_set(Token **rest, Token *tok) {
  Token* tok_set = tok;
  tok = skip(tok, "set");
  Node* lhs = parse_expr(&tok, tok);
  Node* rhs = parse_expr(&tok, tok);
  Node* node = new_set(lhs, rhs, tok_set);
  *rest = tok;
  return node;
}

static Node* parse_sizeof(Token **rest, Token *tok) {
  Token* tok_sizeof = tok;
  tok = tok->next;
  Type* ty = parse_type(&tok, tok);
  *rest = tok;
  return new_num(ty->size, tok_sizeof);
}

static Node* parse_primitive(Token** rest, Token* tok) {
#define Match(t, handle)    if (equal(tok, t)) return handle;
  Match("+", parse_binary(rest, ND_ADD, true, tok))
  Match("-", parse_binary(rest, ND_SUB, true, tok))
  Match("*", parse_binary(rest, ND_MUL, true, tok))
  Match("/", parse_binary(rest, ND_DIV, true, tok))
  Match("=", parse_binary(rest, ND_EQ, false, tok))
  Match(">", parse_binary(rest, ND_GT, false, tok))
  Match("<", parse_binary(rest, ND_LT, false, tok))
  Match("<=", parse_binary(rest, ND_LE, false, tok))
  Match(">=", parse_binary(rest, ND_GE, false, tok))
  Match("iget", parse_binary(rest, ND_IGET, false, tok))
  Match("iset", parse_triple(rest, ND_ISET, tok))
  Match("deref", parse_deref(rest, tok))
  Match("addr", parse_addr(rest, tok))
  Match("sizeof", parse_sizeof(rest, tok))
#undef Match
  error_tok(tok, "invalid primitive\n");
}

static Node* parse_binary(Token** rest, NodeKind kind, bool left_compose, Token* tok) {
  Token* op_tok = tok;
  tok = tok->next;
  Node *lhs, *rhs, *node;
  lhs = parse_expr(&tok, tok);
  rhs = parse_expr(&tok, tok);
  node = new_binary(kind, lhs, rhs, op_tok);

  // perform left compose
  while (left_compose && !stop_parse(tok)) {
    lhs = node;  
    rhs = parse_expr(&tok, tok);
    node = new_binary(kind, lhs, rhs, op_tok);
  }

  *rest = tok;
  return node;
}

static Node* parse_triple(Token** rest, NodeKind kind, Token* tok) {
  Token* op_tok = tok;
  tok = tok->next;
  Node *lhs, *mhs, *rhs, *node;
  lhs = parse_expr(&tok, tok);
  mhs = parse_expr(&tok, tok);
  rhs = parse_expr(&tok, tok);
  node = new_triple(kind, lhs, mhs, rhs, op_tok);

  *rest = tok;
  return node;
}

static Node* parse_number(Token** rest, Token* tok) {
  Node* node = new_num(tok->val, tok);
  *rest = tok->next;
  return node;
}

static Node* parse_str(Token** rest, Token* tok) {
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

static Node* parse_addr(Token** rest, Token* tok) {
  Token* tok_addr = tok;
  tok = tok->next;
  Var* var = find_var(tok);
  if (!var)
    error_tok(tok, "undefined variable");
  Node* var_node = new_var_node(var, tok);
  *rest = tok->next;
  return new_unary(ND_ADDR, var_node, tok_addr);
}

static Node* parse_deref(Token** rest, Token* tok) {
  Token* tok_deref = tok;
  tok = tok->next;
  Node* lhs = parse_expr(&tok, tok);
  Node* node = new_unary(ND_DEREF, lhs, tok_deref);
  *rest = tok;
  return node;
}

static Node* parse_application(Token** rest, Token* tok) {
  Token* tok_app = tok;
  char* fn = strndup(tok->loc, tok->len);
  Node head = {};
  Node* cur = &head;
  tok = tok->next;
  while (!stop_parse(tok)) {
    cur->next = parse_expr(&tok, tok); 
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
  if (lhs->ty->kind != TY_STRUCT)
    error_tok(lhs->tok, "not a struct");

  Node* node = new_unary(ND_STRUCT_REF, lhs, tok);
  node->member = get_struct_member(lhs->ty, tok);
  return node;
}

static Node* parse_defstruct(Token** rest, Token* tok) {
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
    Type* ty = parse_type(&tok, tok->next);
    Member* mem = new_member(mem_tok, ty);
    offset = align_to(offset, ty->align);
    mem->offset = offset;
    offset += ty->size;

    cur->next = mem;
    cur = mem;
    max_align = ty->align < max_align ? max_align : ty->align;
  }
  Type* ty = new_struct_type(TY_STRUCT, align_to(offset, max_align), max_align, head.next);
  Var* tag = new_var(name, ty);
  push_tags(tag);
  Node* node = new_node(ND_STRUCT, tok);
  *rest = tok;
  return node;
}
/* E.G.
(def main() -> int
  (ret3))
*/
static Node* parse_def(Token** rest, Token* tok) {
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
    Type* ty = parse_type(&tok, tok);
    Var* var = new_lvar(strndup(tok_arg->loc, tok_arg->len), ty);
    cur->next = new_var_node(var, tok_arg);
    cur = cur->next;
  }
  tok = skip(tok, pair);

  // return type
  tok = skip(tok, "->");
  Type* ret_ty = parse_type(&tok, tok);

  // body
  Node head_body = {};
  cur = &head_body;
  while (!stop_parse(tok)) {
    cur->next = parse_expr(&tok, tok);
    cur = cur->next;
  }
  *rest = tok;
  return new_function(fn, ret_ty, head_args.next, head_body.next, tok_def);
}


static Node* parse_expr(Token** rest, Token *tok) {
  if (tok->kind == TK_LPAREN || tok->kind == TK_LBRACKET) {
    return parse_list(rest, tok);
  }

  if (tok->kind == TK_NUM) {
    return parse_number(rest, tok);
  }

  if (tok->kind == TK_IDENT) {
    Var* var = find_var(tok);
    if (!var)
      error_tok(tok, "undefined variable");
    Node* node = new_var_node(var, tok);
    tok = tok->next;
    // either a deref or a struct member access
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
    return parse_addr(rest, tok);
  }

  if (tok->kind == TK_STR) {
    return parse_str(rest, tok);
  }
  error_tok(tok, "expect an expression");
}

static Type* parse_base_type(Token** rest, Token* tok) {
  if (equal(tok, "int")) {
    *rest = tok->next;
    return ty_int;
  }
  if (equal(tok, "char")) {
    *rest = tok->next;
    return ty_char;
  }
  for (Var* t = tags; t; t = t->next) {
    if (equal(tok, t->name)) {
      *rest = tok->next;
      return t->ty;
    }
  }
  error_tok(tok, "invalid base type");
}

static Type* parse_array_helper(Token** rest, Token* tok, char* pair) {
  int len = get_number(parse_expr(&tok, tok));
  if (tok->kind == TK_NUM) {
    Type* base = parse_array_helper(&tok, tok, pair);
    *rest = tok;
    return array_of(base, len);
  }
  Type* base = parse_type(&tok, tok);
  tok = skip(tok, pair);
  *rest = tok;
  return array_of(base, len);
}

static Type* parse_array_type(Token** rest, Token* tok) {
  char* pair = tok->kind == TK_LBRACKET? "]" : ")";
  tok = tok->next;
  Type* ty = parse_array_helper(&tok, tok, pair);
  *rest = tok;
  return ty;
}


static Type* parse_pointer_type(Token** rest, Token* tok) {
  tok = tok->next;
  if (equal(tok, "*")) {
    Type* base = parse_pointer_type(&tok, tok);
    *rest = tok;
    return pointer_to(base);
  }
  if (tok->kind == TK_LBRACKET || tok->kind == TK_LPAREN) {
    Type* base = parse_array_type(&tok, tok);
    *rest = tok;
    return pointer_to(base);
  }
  Type* base = parse_base_type(&tok, tok);
  *rest = tok;
  return pointer_to(base);
}

static Type* parse_type(Token** rest, Token* tok) {
  if (equal(tok, "*")) {
    return parse_pointer_type(rest, tok);
  }
  if (tok->kind == TK_LBRACKET || tok->kind == TK_LPAREN) {
    return parse_array_type(rest, tok);
  }
  return parse_base_type(rest, tok);
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



static Node* parse_global_var(Token** rest, Token *tok) {
  char* pair = equal(tok, "(") ? ")" : "]";
  tok = tok->next;
  Node* node = parse_let(&tok, tok, new_gvar);
  tok = skip(tok, pair);
  *rest = tok;
  return node;
}


Node* parse(Token* tok) {
  Node* cur = prog;
  while (tok->kind != TK_EOF) {
    if (is_function(tok)) {
      cur->next = parse_expr(&tok, tok);
      cur = cur->next;
      add_type(cur);
    } else if (is_global_var(tok)) {
      cur->next = parse_global_var(&tok, tok);
      cur = cur->next;  
    } else if (is_defstruct(tok)) {
      char* pair = tok->kind == TK_LPAREN ? ")" : "]";
      parse_defstruct(&tok, tok->next);
      tok = skip(tok, pair);
    } else {
      error_tok(tok, "invalid expression");
    }
  }
  return prog;
}
