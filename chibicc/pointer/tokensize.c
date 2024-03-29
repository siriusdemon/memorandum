#include "chibicc.h"

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

// Consumes the current token if it matches `op`.
bool equal(Token* tok, char* op) {
  return strlen(op) == tok->len &&
         !strncmp(tok->loc, op, tok->len);
}

// Ensure that the current token is `op`.
Token* skip(Token* tok, char* op) {
  if (!equal(tok, op))
    error_tok(tok, "expected '%s'", op);
  return tok->next;
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

static bool startswith(char* p, char* q) {
  return strncmp(p, q, strlen(q)) == 0;
}

// Returns true if c is valid as the first character of an identifier.
static bool is_ident1(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

// Returns true if c is valid as a non-first character of an identifier.
static bool is_ident2(char c) {
  return is_ident1(c) || ('0' <= c && c <= '9');
}

static bool is_keyword(Token* tok) {
  static char* kw[] = {"return", "if", "else", "for", "while"};

  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (equal(tok, kw[i]))
      return true;
  return false;
}

static void convert_keywords(Token* tok) {
  for (Token* t = tok; t->kind != TK_EOF; t = t->next)
    if (is_keyword(t))
      t->kind = TK_RESERVED;
}

// Tokenize a given string and returns new tokens.
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

    // Identifier or keyword
    if (is_ident1(*p)) {
      char* q = p++;
      while (is_ident2(*p))
        p++;
      cur = new_token(TK_IDENT, cur, q, p - q);
      continue;
    }

    // Multi-letter punctuators
    if (startswith(p, "==") || startswith(p, "!=") ||
        startswith(p, "<=") || startswith(p, ">=")) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }

    // Single-letter punctuators
    if (ispunct(*p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    error_at(p, "invalid token");
  }

  new_token(TK_EOF, cur, p, 0);
  convert_keywords(head.next);
  return head.next;
}