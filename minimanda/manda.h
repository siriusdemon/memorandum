#ifndef MANDA_H
#define MANDA_H
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



// 
// tokenize.c
//
typedef enum {
  TK_RESERVED,  // Keywords or punctuators
  TK_NUM,       // Numeric literals
  TK_EOF,       // End-of-file markers
  TK_LPAREN,    // (
  TK_RPAREN,    // )
  TK_LBRACKET,  // [
  TK_RBRACKET,  // ]
  TK_IDENT,     // identifier
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


bool equal(Token*, char*);
Token* skip(Token*, char*);
Token* tokenize(char*);

void error(char*, ...);
void error_at(char* loc, char* fmt, ...);
void error_tok(Token* tok, char* fmt, ...);

//
// parse.c
//
typedef struct Var Var;
typedef struct Node Node;
typedef struct Function Function;

typedef enum {
  ND_ADD,         // +
  ND_SUB,         // -
  ND_MUL,         // *
  ND_DIV,         // /
  ND_EQ,          // =
  ND_LT,          // <
  ND_LE,          // <=
  ND_GT,          // >
  ND_GE,          // >=
  ND_NUM,         // integer
  ND_VAR,         // variable
  ND_LET,         // let
} NodeKind;


struct Node {
  NodeKind kind;
  Token* tok;
  Node* next;

  // number
  int val;

  // binary
  Node* lhs;
  Node* rhs;

  // var
  Var* var;
};

struct Var {
  Var* next;
  char* name;
  int offset;
};

struct Function {
  Node *body;
  Var *locals;
  int stack_size;
}; 


Function* parse(Token*);


//
// codegen.c
//
void codegen(Function* node);
#endif