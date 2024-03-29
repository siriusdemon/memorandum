#include "manda.h"

// Input string
static char* current_input;
static char *current_filename;

// Reports an error and exit.
void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// Reports an error location and exit.
static void verror_at(int line_no, char *loc, char *fmt, va_list ap) {
  // Find a line containing `loc`.
  char *line = loc;
  while (current_input < line && line[-1] != '\n')
    line--;

  char *end = loc;
  while (*end != '\n')
    end++;

  // Print out the line.
  int indent = fprintf(stderr, "%s:%d: ", current_filename, line_no);
  fprintf(stderr, "%.*s\n", (int)(end - line), line);

  // Show the error message.
  int pos = loc - line + indent;
  fprintf(stderr, "%*s", pos, ""); // print pos spaces.
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void error_at(char* loc, char* fmt, ...) {
  int line_no = 1;
  for (char *p = current_input; p < loc; p++)
    if (*p == '\n')
      line_no++;

  va_list ap;
  va_start(ap, fmt);
  verror_at(line_no, loc, fmt, ap);
}

void error_tok(Token* tok, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(tok->line_no, tok->loc, fmt, ap);
}


// Ensure that the current token is `s`.
bool equal(Token* tok, char* s) {
  return strlen(s) == tok->len &&
         !strncmp(tok->loc, s, tok->len);
}

// ensure the current token is a list and get a pair of it.
char* get_pair(Token** rest, Token* tok) {
  if (is_list(tok)) {
    char* pair = tok->kind == TK_LPAREN ? ")" : "]";
    *rest = tok->next;
    return pair;
  }
  error_tok(tok, "expected a list");
}

// Consumes the current token if it matches `s`.
Token* skip(Token* tok, char* s) {
  if (!equal(tok, s))
    error_tok(tok, "expected '%s'", s);
  return tok->next;
}

bool stop_parse(Token* tok) {
  return tok->kind == TK_EOF || tok->kind == TK_RPAREN || tok->kind == TK_RBRACKET;
}

bool is_list(Token* tok) {
  return tok->kind == TK_LPAREN || tok->kind == TK_LBRACKET;
}


static bool is_ident1(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_' || c == '-' 
      || c == '+' || c == '<' || c == '>' || c == '%' || c == '$';
}

// Returns true if c is valid as a non-first character of an identifier.
static bool is_ident2(char c) {
  return is_ident1(c) || ('0' <= c && c <= '9') || c == '/' || c == '?' 
      || c == '!' || c == '^' || c == '=';
}

static bool is_keyword(Token* tok) {
  static char *kw[] = {"let", "const", "set", "do", "def", "lambda", "if", "iset", "iget", 
    "while", "asm", "defstruct", "defenum", "match", "deftype", "defmodule", "import", 
    "export", "async", "defasync", "await", "defmacro", "with", "defunion", "typeof",
    "true", "false", "pointer",
  };

  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (equal(tok, kw[i]))
      return true;
  return false;
}

bool is_primitive(Token* tok) {
  static char *kw[] = {"+", "-", "*", "/", "<", ">", ">=", "<=", "=", "and", "or", 
    "not", "xor", "sra", "srl", "sll", "bitand", "bitor", "bitnot", "bitxor",
    "addr", "deref", "iget", "iset", "sizeof", "cast", "mod", "rem", "struct-ref",
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

static void correct_tokens(Token* tok) {
  for (Token *t = tok; t->kind != TK_EOF; t = t->next)
    if (is_keyword(t) || is_primitive(t))
      t->kind = TK_RESERVED;
}


static int read_escaped_char(char *p) {
  switch (*p) {
  case 'a': return '\a';
  case 'b': return '\b';
  case 't': return '\t';
  case 'n': return '\n';
  case 'v': return '\v';
  case 'f': return '\f';
  case 'r': return '\r';
  // [GNU] \e for the ASCII escape character is a GNU C extension.
  case 'e': return 27;
  default: return *p;
  }
}

// Find a closing double-quote.
static char *string_literal_end(char *p) {
  char *start = p;
  for (; *p != '"'; p++) {
    if (*p == '\0')
      error_at(start, "unclosed string literal");
    if (*p == '\\')
      p++;
  }
  return p;
}

static Token* read_string_literal(Token* cur, char* start) {
  char* end = string_literal_end(start + 1);
  char* buf = calloc(1, end - start);
  int len = 0;

  for (char *p = start + 1; p < end;) {
    if (*p == '\\') {
      buf[len++] = read_escaped_char(p + 1);
      p += 2;
    } else {
      buf[len++] = *p++;
    }
  }
  buf[len] = '\0';
  Token* tok = new_token(TK_STR, cur, start, end - start + 1);
  tok->str = buf;
  return tok;
}

static Token *read_char_literal(Token *cur, char *start) {
  char* p = start + 1;
  if (*p == '\0')
    error_at(start, "unclosed char literal");

  char c;
  if (*p == '\\')
    c = read_escaped_char(p++);
  else
    c = *p++;

  char *end = strchr(p, '\'');
  if (!end)
    error_at(p, "unclosed char literal");

  Token *tok = new_token(TK_NUM, cur, start, end - start + 1);
  tok->val = c;
  return tok;
}

// Initialize line info for all tokens.
static void add_line_numbers(Token *tok) {
  char *p = current_input;
  int n = 1;

  do {
    if (p == tok->loc) {
      tok->line_no = n;
      tok = tok->next;
    }
    if (*p == '\n')
      n++;
  } while (*p++);
}


static Token* read_int_literal(Token* cur, char* start) {
  char *p = start;

  int base = 10;
  if (!strncasecmp(p, "#x", 2) && isalnum(p[2])) {
    p += 2;
    base = 16;
  } else if (!strncasecmp(p, "#b", 2) && isalnum(p[2])) {
    p += 2;
    base = 2;
  } else if (!strncasecmp(p, "#o", 2) && isalnum(p[2])) {
    p += 2;
    base = 8;
  }

  long val = strtoul(p, &p, base);
  if (isalnum(*p))
    error_at(p, "invalid digit");

  Token *tok = new_token(TK_NUM, cur, start, p - start);
  tok->val = val;
  return tok;
}


// Tokenize `p` and returns new tokens.
Token* tokenize(char* filename, char* p) {
  current_filename = filename;
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
      cur = read_int_literal(cur, p);
      p += cur->len;
      continue;
    }

    // hash literal
    if (*p == '#' && ((*(p+1) == 'b') || *(p+1) == 'x' || *(p+1) == 'o')) {
      cur = read_int_literal(cur, p);
      p += cur->len;
      continue;
    }
    
    // comment
    if (*p == ';') {
      while (*p && *p != '\n') p++;
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

    // string
    if (*p == '"') {
      cur = read_string_literal(cur, p);
      p += cur->len;
      continue;
    }

    if (*p == '\'') {
      cur = read_char_literal(cur, p);
      p += cur->len;
      continue;
    }

    // Identifier, some multi-character operators and keywords may be collected as identifier.
    // function `correct_token` corrects this.
    if (is_ident1(*p)) {
      char *q = p++;
      while (is_ident2(*p))
        p++;
      if (*p == '*')                                        // need `a *i8`   not `a* i8`
        error("Invalid identifier symbol '*'");
      cur = new_token(TK_IDENT, cur, q, p - q);
      continue;
    }

    // Punctuator, handles single character operation
    if (ispunct(*p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    error("invalid token");
  }

  new_token(TK_EOF, cur, p, 0);
  correct_tokens(head.next);
  add_line_numbers(head.next);
  return head.next;
}


// Returns the contents of a given file.
static char *read_file(char *path) {
  FILE *fp;

  if (strcmp(path, "-") == 0) {
    // By convention, read from stdin if a given filename is "-".
    fp = stdin;
  } else {
    fp = fopen(path, "r");
    if (!fp)
      error("cannot open %s: %s", path, strerror(errno));
  }

  int buflen = 4096;
  int nread = 0;
  char *buf = calloc(1, buflen);

  // Read the entire file.
  for (;;) {
    int end = buflen - 2; // extra 2 bytes for the trailing "\n\0"
    int n = fread(buf + nread, 1, end - nread, fp);
    if (n == 0)
      break;
    nread += n;
    if (nread == end) {
      buflen *= 2;
      buf = realloc(buf, buflen);
    }
  }

  if (fp != stdin)
    fclose(fp);

  // Make sure that the last line is properly terminated with '\n'.
  if (nread == 0 || buf[nread - 1] != '\n')
    buf[nread++] = '\n';

  buf[nread] = '\0';
  return buf;
}

Token *tokenize_file(char *path) {
  return tokenize(path, read_file(path));
}