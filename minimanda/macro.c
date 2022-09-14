/* Macro Interpreter

Macro system is essentially an interpreter of symbol expression. It consumes symbol 
expression then outputs AST nodes.

Macro definition is likely a closed function in the interpreter. There is no need 
to capture variables.

THere are also primitives in such an interpreter. When an application is met, the 
interpreter must apply transform when it is a macro primitives.

*/

#include "manda.h"

Macro* macros = NULL;

static Node* eval_sexp(Sexp* se, MEnv* menv);
static Node* eval_list(Sexp* se, MEnv* menv);
static Node* eval_application(Sexp* se, MEnv* menv);
static Node* eval_if(Sexp* se, MEnv* menv);
static Node* eval_do(Sexp* se, MEnv* menv);
static Node* eval_macro_primitive(Sexp* se, MEnv* menv);
static Node* eval_primitive(Sexp* se, MEnv* menv);
static Node* eval_binary(Sexp* se, MEnv* menv, NodeKind kind, bool left_compose, bool near_compose);
static Node* eval_num(Sexp* se);
static Node* eval_str(Sexp* se, MEnv* menv);

MEnv* new_menv() {
  MEnv* menv = calloc(1, sizeof(MEnv));
  return menv; 
}


MEnv* add_symbol(MEnv* oldmenv, Sexp* symbol, Sexp* value) {
  MEnv* menv = new_menv();
  menv->symbol = symbol;
  menv->value = value;
  menv->next = oldmenv;
  return menv;
}

bool is_same_symbol(Sexp* s1, Sexp* s2) {
  return (s1->tok->len == s2->tok->len) && 
    !strncmp(s1->tok->loc, s2->tok->loc, s1->tok->len);
}

Sexp* lookup_symbol(MEnv* menv, Sexp* symbol) {
  MEnv* cur = menv;
  while (cur) {
    if (is_same_symbol(cur->symbol, symbol)) {
      return cur->value;
    }
    cur = cur->next;
  }
  error_tok(symbol->tok, "undefined variable!");
}

Macro* new_macro(Token* name, Sexp* args, Sexp* body) {
  Macro* t = calloc(1, sizeof(Macro));
  t->name = name;
  t->args = args;
  t->body = body;
  t->next = NULL;
  return t;
}

Sexp* new_sexp(SexpKind kind, Token* tok) {
  Sexp* s = calloc(1, sizeof(Sexp));
  s->kind = kind;
  s->tok = tok;
  s->next = NULL;
  s->elements = NULL;
}

Macro* lookup_macro(Token* t) {
  Macro* cur = macros;
  while (cur != NULL) {
    if (t->len == cur->name->len && !strncmp(t->loc, cur->name->loc, t->len))
      return cur;
    cur = cur->next;
  }
  return NULL;
}

void register_macro(Macro* t) {
  t->next = macros;
  macros = t;
}

bool is_macro(Token* t) {
  if (lookup_macro(t)) {
    return true;
  }
  return false;
}

bool is_macro_primitive(Token* tok) {
  static char *kw[] = {"str"};
  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (equal(tok, kw[i]))
      return true;
  return false;
}

Sexp* skip_sexp(Sexp* se, SexpKind kind) {
  if (se->kind == kind) {
    return se->next;
  }
  error_tok(se->tok, "expected sexpkind %d\n", kind);
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

static Node* eval_str(Sexp* se, MEnv* menv) {
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

static Node* eval_do(Sexp* se, MEnv* menv) {
  Token* tok = se->elements->tok;
  Node head = {};
  Node* cur = &head;
  Sexp* secur = se->elements->next;
  while (secur) {
    cur->next = eval_sexp(secur, menv);
    cur = cur->next;
    secur = secur->next;
  }
  Node* node = new_do(head.next, tok);
  return node;
}


static Node* eval_if(Sexp* se, MEnv* menv) {
  Token* tok = se->elements->tok;
  Node* cond = eval_sexp(se->elements->next, menv);
  Node* then = eval_sexp(se->elements->next->next, menv);
  Node* els = NULL;
  if (se->elements->next->next->next) {
    els = eval_sexp(se->elements->next->next->next, menv);
  }
  Node* node = new_if(cond, then, els, tok);
  return node;
}

static Node* eval_macro_primitive(Sexp* se, MEnv* menv) {
  if (equal(se->elements->tok, "str")) {
    return eval_str(se, menv); 
  }
  error_tok(se->tok, "unsupported yet!");
}

static Node* eval_application(Sexp* se, MEnv* menv) {
  Token* tok = se->tok;
  // function
  Sexp* secur = se->elements;
  char* fn = strndup(secur->tok->loc, secur->tok->len);
  secur = secur->next;
  // args
  Node head = {};
  Node* cur = &head;
  while (secur) {
    cur->next = eval_sexp(secur, menv);
    cur = cur->next;
    secur = secur->next;
  }
  return new_app(fn, head.next, tok);
}

static Node* eval_primitive(Sexp* se, MEnv* menv) {
  Token* tok = se->elements->tok;
#define Match(t, handle)    if (equal(tok, t)) return handle;
  Match("+", eval_binary(se, menv, ND_ADD, true, false))
  Match("-", eval_binary(se, menv, ND_SUB, true, false))
  Match("*", eval_binary(se, menv, ND_MUL, true, false))
  Match("/", eval_binary(se, menv, ND_DIV, true, false))
  Match("=", eval_binary(se, menv, ND_EQ, false, true))
  Match(">", eval_binary(se, menv, ND_GT, false, true))
  Match("<", eval_binary(se, menv, ND_LT, false, true))
  Match(">=", eval_binary(se, menv, ND_GE, false, true))
  Match("<=", eval_binary(se, menv, ND_LE, false, true))
#undef Match
}

static Node* eval_binary(Sexp* se, MEnv* menv, NodeKind kind, bool left_compose, bool near_compose) {
  Token* op_tok = se->elements->tok;
  Node* lhs = eval_sexp(se->elements->next, menv);
  Node* rhs = eval_sexp(se->elements->next->next, menv);
  Node* node = new_binary(kind, lhs, rhs, op_tok);
  Sexp* rest = se->elements->next->next->next;

  while (left_compose && rest) {
    lhs = node;
    rhs = eval_sexp(rest, menv);
    node = new_binary(kind, lhs, rhs, op_tok);
    rest = rest->next;
  }
  return node;
}

static Node* eval_num(Sexp* se) {
  Node* node = new_num(se->tok->val, se->tok);
  return node;
}

static Node* eval_list(Sexp* se, MEnv* menv) {
  if (equal(se->elements->tok, "do")) {
    return eval_do(se, menv); 
  } else if (equal(se->elements->tok, "if")) {
    return eval_if(se, menv); 
  } else if (is_macro_primitive(se->elements->tok)) {
    return eval_macro_primitive(se, menv);
  } else if (is_primitive(se->elements->tok)) {
    return eval_primitive(se, menv);
  } else {
    return eval_application(se, menv);
  }
}


static Node* eval_sexp(Sexp* se, MEnv* menv) {
  if (se->kind == SE_LIST) {
    return eval_list(se, menv);
  }
  
  Token* tok = se->tok;
  if (tok->kind == TK_NUM) {
    return eval_num(se);
  }

  if (tok->kind == TK_IDENT) {
    Sexp* val = lookup_symbol(menv, se);
    return eval_sexp(val, menv);
  }
  error("invalid symbol expression");
}


Node* macro_expand(Token** rest, Token* tok, Env* env) {
  Macro* m = lookup_macro(tok);
  tok = tok->next;
  Sexp head = {};
  Sexp* cur = &head;
  while (!stop_parse(tok)) {
    cur->next = parse_sexp(&tok, tok);
    cur = cur->next;
  }
  *rest = tok;

  // prepare the macro environment
  MEnv* menv = NULL;
  Sexp* symbol = m->args;
  for (cur = head.next; symbol && cur; symbol = symbol->next, cur = cur->next) {
    menv = add_symbol(menv, symbol, cur);
  }
  if (symbol || cur) {
    error_tok(tok, "args number mismatch!");
  }
  // perform transformation
  Node* node = eval_sexp(m->body, menv);
  return node;
}


Sexp* parse_sexp(Token** rest, Token* tok) {
  if (is_list(tok)) {
    return parse_sexp_list(rest, tok);
  } else {
    return parse_sexp_symbol(rest, tok);
  }
}

Sexp* parse_sexp_symbol(Token** rest, Token* tok) {
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