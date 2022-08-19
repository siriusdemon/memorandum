#include "manda.h"

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

static Node* parse_expr(Token**, Token*, Node*);

static Node* parse_list(Token** rest, Token* tok, Node* cur) {
  char* pair = equal(tok, "(") ? ")" : "]";
  tok = tok->next;

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
  Node tmp;
  Node* lhs = parse_expr(&tok, tok, &tmp);
  Node* rhs = parse_expr(&tok, tok, &tmp);
  Node* node = new_binary(kind, lhs, rhs, tok);

  tok = skip(tok, pair);
  cur->next = node;
  *rest = tok;
  return node;
}

static Node* parse_number(Token** rest, Token* tok, Node* cur) {
  Node* node = new_num(tok->val, tok);
  cur->next = node;
  *rest = tok->next;
  return node;
}

// every thing is an expr
static Node* parse_expr(Token** rest, Token *tok, Node* cur) {
  if (tok->kind == TK_LPAREN || tok->kind == TK_LBRACKET) {
    cur = parse_list(&tok, tok, cur);
    *rest = tok;
    return cur;
  }

  if (tok->kind == TK_NUM) {
    cur = parse_number(&tok, tok, cur);
    *rest = tok;
    return cur;
  }

  error_tok(tok, "Invalid code!");
}

Node* parse(Token* tok) {
  Node head = {};
  Node* cur = &head;
  while (tok->kind != TK_EOF) {
    cur = parse_expr(&tok, tok, cur);
  }
  return head.next;
}
