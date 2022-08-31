#ifndef MANDA_H
#define MANDA_H
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct Type Type;
typedef struct Node Node;
typedef struct Member Member;

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
  TK_STR,       // C String
} TokenKind;

// Token type
typedef struct Token Token;
struct Token {
  TokenKind kind; // Token kind
  Token* next;    // Next token
  int64_t val;        // If kind is TK_NUM, its value
  char* loc;      // Token location
  int len;        // Token length
  char* str;      // String literal content including terminating '\0'
  int line_no;    // line number
};


bool equal(Token*, char*);
bool is_primitive(Token*);
Token* skip(Token*, char*);
Token* tokenize_file(char* filename);

void error(char*, ...);
void error_at(char* loc, char* fmt, ...);
void error_tok(Token* tok, char* fmt, ...);

//
// parse.c
//
typedef struct Var Var;

typedef enum {
  ND_ADD,                   // +
  ND_SUB,                   // -
  ND_MUL,                   // *
  ND_DIV,                   // /
  ND_EQ,                    // =
  ND_LT,                    // <
  ND_LE,                    // <=
  ND_GT,                    // >
  ND_GE,                    // >=
  ND_ADDR,                  // &
  ND_DEREF,                 // a.*
  ND_NUM,                   // integer
  ND_VAR,                   // variable
  ND_STR,                   // string
  ND_IGET,                  // iget
  ND_ISET,                  // iset
  ND_LET,                   // let
  ND_SET,                   // set
  ND_IF,                    // if
  ND_DO,                    // do
  ND_WHILE,                 // while
  ND_APP,                   // application
  ND_FUNC,                  // function
  ND_DEFSTRUCT,             // defstruct
  ND_STRUCT_REF,            // struct member ref, such as a.p
  ND_DEFUNION,              // defunion
  ND_DEFTYPE,               // deftype
  ND_CAST,                  // type cast
} NodeKind;


struct Node {
  NodeKind kind;
  Token* tok;
  Node* next;
  Type* ty;

  // number
  int64_t val;

  char* str;

  // binary or triple
  Node* lhs;  // left
  Node* mhs;  // middle, only for triple
  Node* rhs;  // right

  // var
  Var* var;

  // struct member;
  Member* member;

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

struct Member {
  Member* next;
  Token* tok;
  Type* ty;
  int offset;
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
  TY_CHAR,
  TY_INT,
  TY_SHORT,
  TY_LONG,
  TY_PTR,
  TY_ARRAY,
  TY_FUNC,
  TY_VOID,
  TY_STRUCT,
  TY_UNION,
} TypeKind;

struct Type {
  TypeKind kind;
  int size;
  int align;

  // array
  int array_len;

  // struct members
  Member* members;
  
  // pointer or array
  Type* base;
};

extern Type* ty_int;
extern Type* ty_void;
extern Type* ty_char;
extern Type* ty_long;
extern Type* ty_short;

void add_type(Node* node);
Type* new_struct_type(int size, int align, Member* members);
Type* new_union_type(int size, int align, Member* members);
Type* pointer_to(Type* base);
Type* array_of(Type *base, int len);


//
// codegen.c
//
#define unreachable() \
  error("internal error at %s:%d", __FILE__, __LINE__)

void codegen(Node* prog, FILE* out);
int align_to(int n, int align);
#endif