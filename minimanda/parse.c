#include "manda.h"

Var* locals;

static Node* parse_expr(Token** rest, Token* tok);
static Node* parse_list(Token** rest, Token* tok);
static Node* parse_expr(Token** rest, Token* tok);
static Node* parse_let(Token** rest, Token* tok);
static Node* parse_number(Token** rest, Token* tok);
static Node* parse_primitive(Token** rest, Token* tok);

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

static Node* parse_list(Token** rest, Token* tok) {
  Node* node;
  char* pair = equal(tok, "(") ? ")" : "]";
  tok = tok->next;
  if (equal(tok, "let")) {
    node = parse_let(&tok, tok);
  } else {
    node = parse_primitive(&tok, tok);
  } 
  tok = skip(tok, pair);
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

static Node* parse_primitive(Token** rest, Token* tok) {
  NodeKind kind;
  if (equal(tok, "+")) {
    kind = ND_ADD;
  } else if (equal(tok, "-")) {
    kind = ND_SUB;
  } else if (equal(tok, "*")) {
    kind = ND_MUL;
  } else if (equal(tok, "/")) {
    kind = ND_DIV;
  } else {
    error_tok(tok, "invalid operation");
  }
  tok = tok->next;
  Node *lhs, *rhs, *node;
  lhs = parse_expr(&tok, tok);
  rhs = parse_expr(&tok, tok);
  node = new_binary(kind, lhs, rhs, tok);
  // there is more operands to operate on.
  while (tok->kind != TK_EOF && tok->kind != TK_RPAREN && tok->kind != TK_RBRACKET) {
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

  error_tok(tok, "Invalid code!");
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
