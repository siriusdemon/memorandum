#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  TK_RESERVED, // Keywords or punctuators
  TK_NUM,      // Numeric literals
  TK_EOF,      // End-of-file markers
  TK_LPAREN,  // (
  TK_RPAREN,  // )
} TokenKind;

// Token type
typedef struct Token Token;
struct Token {
  TokenKind kind; // Token kind
  Token* next;    // Next token
  int val;        // If kind is TK_NUM, its value
  char* loc;      // Token location
  int len;        // Token length
};


typedef enum {
  ND_ADD,
  ND_SUB,
  ND_NUM,
} NodeKind;


typedef struct Node Node;
struct Node {
  NodeKind kind;
  Token* tok;
  Node* next;

  // number
  int val;


  // binary
  Node* lhs;
  Node* rhs;
};


// Reports an error and exit.
static void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// Ensure that the current token is `s`.
static bool equal(Token* tok, char* s) {
  return strlen(s) == tok->len &&
         !strncmp(tok->loc, s, tok->len);
}

// Consumes the current token if it matches `s`.
static Token* skip(Token* tok, char* s) {
  if (!equal(tok, s))
    error("expected '%s'", s);
  return tok->next;
}

// Ensure that the current token is TK_NUM.
static int get_number(Token* tok) {
  if (tok->kind != TK_NUM)
    error("expected a number");
  return tok->val;
}

// Create a new token and add it as the next token of `cur`.
static Token* new_token(TokenKind kind, Token* cur, char* str, int len) {
  Token* tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->loc = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

// Tokenize `p` and returns new tokens.
static Token* tokenize(char* p) {
  Token head = {};
  Token* cur = &head;

  while (*p) {
    // Skip whitespace characters.
    if (isspace(*p)) {
      p++;
      continue;
    }

    // Numeric literal
    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      char* q = p;
      cur->val = strtoul(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    // List expression
    if (*p == '(') {
      cur = new_token(TK_LPAREN, cur, p++, 1);
      continue;
    }
    
    if (*p == ')') {
      cur = new_token(TK_RPAREN, cur, p++, 1);
      continue;
    }

    // Punctuator
    if (*p == '+' ||* p == '-') {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    error("invalid token");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}

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

static Node* parse_list(Token** rest, Token* tok, Node* cur) {
  tok = tok->next;
  Node* lhs = new_num(tok->next->val, tok->next);
  Node* rhs = new_num(tok->next->next->val, tok->next->next);
  NodeKind kind = equal(tok, "+") ? ND_ADD : ND_SUB;
  Node* node = new_binary(kind, lhs, rhs, tok);
  tok = skip(tok->next->next->next, ")");
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

static Node* parse(Token* tok) {
  Node head = {};
  Node* cur = &head;
  while (tok->kind != TK_EOF) {
    if (tok->kind == TK_LPAREN) {
      cur = parse_list(&tok, tok, cur);
      continue;
    }

    if (tok->kind == TK_NUM) {
      cur = parse_number(&tok, tok, cur);
      continue;
    }

    error("invalid node");
  }
  return head.next;
}

static void codegen(Node* node) {
  printf("  .globl main\n");
  printf("main:\n");

  if (node->kind == ND_SUB) {
    printf("  mov $%d, %%rax\n", node->lhs->val);
    printf("  sub $%d, %%rax\n", node->rhs->val);
  }

  if (node->kind == ND_ADD) {
    printf("  mov $%d, %%rax\n", node->lhs->val);
    printf("  add $%d, %%rax\n", node->rhs->val);
  }

  if (node->kind == ND_NUM) {
    printf("  mov $%d, %%rax\n", node->val);
  }

  printf("  ret\n");
}

int main(int argc, char** argv) {
  if (argc != 2)
    error("%s: invalid number of arguments", argv[0]);

  Token* tok = tokenize(argv[1]);
  Node* node = parse(tok);
  codegen(node);
}