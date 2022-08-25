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


typedef struct Type Type;
typedef struct Node Node;

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
bool is_primitive(Token*);
Token* skip(Token*, char*);
Token* tokenize(char*);

void error(char*, ...);
void error_at(char* loc, char* fmt, ...);
void error_tok(Token* tok, char* fmt, ...);

//
// parse.c
//
typedef struct Var Var;

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
  ND_ADDR,        // &
  ND_DEREF,       // a.*
  ND_NUM,         // integer
  ND_VAR,         // variable
  ND_IGET,        // iget
  ND_ISET,        // iset
  ND_LET,         // let
  ND_SET,         // set
  ND_IF,          // if
  ND_WHILE,       // while
  ND_APP,         // application
  ND_FUNC,        // function
} NodeKind;


struct Node {
  NodeKind kind;
  Token* tok;
  Node* next;
  Type* ty;

  // number
  int val;

  // binary or triple
  Node* lhs;  // left
  Node* mhs;  // middle, only for triple
  Node* rhs;  // right

  // var
  Var* var;

  // if
  Node* cond;
  Node* then;
  Node* els;

  // application // function
  char* fn;
  Node* args;
  Node* body;
  Var* locals;
  Type* ret_ty; 
  int stack_size;
  
};

struct Var {
  Var* next;
  char* name;
  Type* ty;
  int offset;
  bool is_local;
};

Node* parse(Token*);


// type.c
typedef enum {
  TY_INT,
  TY_PTR,
  TY_ARRAY,
  TY_FUNC,
  TY_VOID,
} TypeKind;

struct Type {
  TypeKind kind;
  Type* base;
  int size;
  // array
  int array_len;
};

extern Type* ty_int;
extern Type* ty_void;

bool is_integer(Type* ty);
void add_type(Node* node);
Type* new_int_type();
Type* pointer_to(Type* base);
Type* array_of(Type *base, int len);


//
// codegen.c
//
void codegen(Node* node);
#endif