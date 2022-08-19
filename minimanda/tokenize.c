#include "manda.h"

// Input string
static char* current_input;

// Reports an error and exit.
void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// Reports an error location and exit.
static void verror_at(char* loc, char* fmt, va_list ap) {
  int pos = loc - current_input;
  fprintf(stderr, "%s\n", current_input);
  fprintf(stderr, "%*s", pos, ""); // print pos spaces.
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void error_at(char* loc, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(loc, fmt, ap);
}

void error_tok(Token* tok, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(tok->loc, fmt, ap);
}


// Ensure that the current token is `s`.
bool equal(Token* tok, char* s) {
  return strlen(s) == tok->len &&
         !strncmp(tok->loc, s, tok->len);
}

// Consumes the current token if it matches `s`.
Token* skip(Token* tok, char* s) {
  if (!equal(tok, s))
    error("expected '%s'", s);
  return tok->next;
}


static bool is_ident1(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_' || c == '-' 
      || c == '+' || c == '<' || c == '>' || c == '%' || c == '$';
}

// Returns true if c is valid as a non-first character of an identifier.
static bool is_ident2(char c) {
  return is_ident1(c) || ('0' <= c && c <= '9') || c == '*' || c == '/' || c == '?' 
      || c == '!' || c == '^' || c == '=';
}

static bool is_keyword(Token* tok) {
  static char *kw[] = {"let", "const", "set", "do", "def", "lambda", "if", "iset", "iget", 
    "while", "asm", "defstruct", "defenum", "match", "deftype", "defmodule", "import", 
    "export", "async", "defasync", "await", "defmacro", "with",
  };

  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (equal(tok, kw[i]))
      return true;
  return false;
}

static bool is_primitive(Token* tok) {
  static char *kw[] = {"+", "-", "*", "/", "<", ">=", "<=", "=", "and", "or",
    "not", "xor", "sra", "srl", "sll", "bitand", "bitor", "bitnot", "bitxor",
  };
  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (equal(tok, kw[i]))
      return true;
  return false;
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

void correct_tokens(Token* tok) {
  for (Token *t = tok; t->kind != TK_EOF; t = t->next)
    if (is_keyword(t) || is_primitive(t))
      t->kind = TK_RESERVED;
}


// Tokenize `p` and returns new tokens.
Token* tokenize(char* p) {
  current_input = p;
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

    if (*p == '[') {
      cur = new_token(TK_LBRACKET, cur, p++, 1);
      continue;
    }
    
    if (*p == ']') {
      cur = new_token(TK_RBRACKET, cur, p++, 1);
      continue;
    }

    // Identifier, some operators and keywords may be collected as identifier.
    // function `correct_token` corrects this.
    if (is_ident1(*p)) {
      char *q = p++;
      while (is_ident2(*p))
        p++;
      cur = new_token(TK_IDENT, cur, q, p - q);
      continue;
    }

    // Punctuator
    if (*p == '+' || *p == '-' || *p == '*' || *p == '/') {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    error("invalid token");
  }

  new_token(TK_EOF, cur, p, 0);
  correct_tokens(head.next);
  return head.next;
}
