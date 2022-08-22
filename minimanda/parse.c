#include "manda.h"

Var* locals;

static Node* parse_expr(Token** rest, Token* tok);
static Node* parse_list(Token** rest, Token* tok);
static Node* parse_expr(Token** rest, Token* tok);
static Node* parse_let(Token** rest, Token* tok);
static Node* parse_set(Token** rest, Token* tok);
static Node* parse_if(Token** rest, Token* tok);
static Node* parse_while(Token** rest, Token* tok);
static Node* parse_number(Token** rest, Token* tok);
static Node* parse_primitive(Token** rest, Token* tok);

static bool stop_parse(Token* tok) {
  return tok->kind == TK_EOF || tok->kind == TK_RPAREN || tok->kind == TK_RBRACKET;
}

// ----- variable 
static Var* find_var(Token* tok) {
  for (Var* var = locals; var; var = var->next)
    if (strlen(var->name) == tok->len && !strncmp(tok->loc, var->name, tok->len))
      return var;
  return NULL;
}

static Var* new_lvar(char* name) {
  Var *var = calloc(1, sizeof(Var));
  var->name = name;
  var->next = locals;
  locals = var;
  return var;
}

// ----- nodes
static Node* new_node(NodeKind kind, Token* tok) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  return node;
}

static Node* new_binary(NodeKind kind, Node* lhs, Node* rhs, Token* tok) {
  Node* node = new_node(kind, tok);
  node->lhs = lhs;
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

static Node* parse_list(Token** rest, Token* tok) {
  Node* node;
  char* pair = equal(tok, "(") ? ")" : "]";
  tok = tok->next;
  if (equal(tok, "let")) {
    node = parse_let(&tok, tok);
  } else if (equal(tok, "set")) {
    node = parse_set(&tok, tok);
  } else if (equal(tok, "if")) {
    node = parse_if(&tok, tok);
  } else if (equal(tok, "while")) {
    node = parse_while(&tok, tok);
  } else {
    node = parse_primitive(&tok, tok);
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


static Node* parse_let(Token **rest, Token *tok) {
  Token* tok_let = tok;
  tok = skip(tok, "let");
  Var* var = new_lvar(strndup(tok->loc, tok->len));
  Node* lhs = new_var_node(var, tok);
  tok = tok->next;
  Node* rhs = parse_expr(&tok, tok);
  Node* node = new_let(lhs, rhs, tok);
  *rest = tok;
  return node;
}

static Node* parse_set(Token **rest, Token *tok) {
  Token* tok_set = tok;
  tok = skip(tok, "set");
  Var* var = find_var(tok);
  Node* lhs = new_var_node(var, tok);
  tok = tok->next;
  Node* rhs = parse_expr(&tok, tok);
  Node* node = new_set(lhs, rhs, tok);
  *rest = tok;
  return node;
}


static Node* parse_primitive(Token** rest, Token* tok) {
  NodeKind kind;
  bool left_compose = true;     // left compose or near compose
  bool match = false;
#define Match(t, node_kind, flag)                       \
        if (equal(tok, t)) {                            \
          kind = node_kind;                             \
          left_compose = flag;                          \
          match = true;                                 \
        }
  Match("+", ND_ADD, true) 
  Match("-", ND_SUB, true)
  Match("*", ND_MUL, true)
  Match("/", ND_DIV, true)
  Match("=", ND_EQ, false)
  Match(">", ND_GT, false)
  Match(">=", ND_GE, false)
  Match("<=", ND_LE, false)
  Match("<", ND_LT, false)
#undef Match
  if (!match) {
    error_tok(tok, "Invalid primitive");
  }
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
    node = new_binary(kind, lhs, rhs, tok);
  }

  *rest = tok;
  return node;
}

static Node* parse_number(Token** rest, Token* tok) {
  Node* node = new_num(tok->val, tok);
  *rest = tok->next;
  return node;
}

// every thing is an expr
static Node* parse_expr(Token** rest, Token *tok) {
  if (tok->kind == TK_LPAREN || tok->kind == TK_LBRACKET) {
    Node* node = parse_list(&tok, tok);
    *rest = tok;
    return node;
  }

  if (tok->kind == TK_NUM) {
    Node* node = parse_number(&tok, tok);
    *rest = tok;
    return node;
  }

  if (tok->kind == TK_IDENT) {
    Var *var = find_var(tok);
    if (!var)
      error_tok(tok, "undefined variable");
    *rest = tok->next;
    return new_var_node(var, tok);
  }

  error_tok(tok, "expect an expression");
}

Function* parse(Token* tok) {
  Node head = {};
  Node* cur = &head;
  while (tok->kind != TK_EOF) {
    cur->next = parse_expr(&tok, tok);
    cur = cur->next;
  }
  Function* prog = calloc(1, sizeof(Function));
  prog->body = head.next;
  prog->locals = locals;
  return prog;
}
