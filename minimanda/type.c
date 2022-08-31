#include "manda.h"

Type* ty_bool = &(Type){.kind = TY_BOOL, .size = 1, .align = 1};
Type* ty_int =  &(Type){.kind = TY_INT,  .size = 4, .align = 4};
Type* ty_short= &(Type){.kind = TY_SHORT,.size = 2, .align = 2};
Type* ty_long = &(Type){.kind = TY_LONG, .size = 8, .align = 8};
Type* ty_char = &(Type){.kind = TY_CHAR, .size = 1, .align = 1};
Type* ty_void = &(Type){.kind = TY_VOID, .size = 0, .align = 0};


static Type* new_type(TypeKind kind, int size, int align) {
  Type* ty = calloc(1, sizeof(Type));
  ty->kind = kind;
  ty->size = size;
  ty->align = align;
  return ty;
}


Type* pointer_to(Type* base) {
  Type* ty = new_type(TY_PTR, 8, 8);
  ty->base = base;
  return ty;
}

Type* array_of(Type* base, int len) {
  Type* ty = new_type(TY_ARRAY, base->size * len, base->align);
  ty->base = base;
  ty->array_len = len;
  return ty;
}

Type* new_struct_type(int size, int align, Member* members) {
  Type* ty = new_type(TY_STRUCT, size, align);
  ty->members = members;
  return ty;
}

Type* new_union_type(int size, int align, Member* members) {
  Type* ty = new_type(TY_UNION, size, align);
  ty->members = members;
  return ty;
}


static Node* last_expr(Node* node) {
  Node* cur = node;
  while (cur->next) {
    cur = cur->next;
  }
  return cur;
}

void add_type(Node* node) {
  if (!node || node->ty)
    return;

  add_type(node->lhs);
  add_type(node->mhs);
  add_type(node->rhs);
  add_type(node->cond);
  add_type(node->then);
  add_type(node->els);

  for (Node* n = node->body; n; n = n->next)
    add_type(n);
  for (Node *n = node->args; n; n = n->next)
    add_type(n);


  switch (node->kind) {
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_NUM:
    node->ty = ty_int;
    return;
  case ND_BOOL:
  case ND_EQ:
  case ND_LT:
  case ND_LE:
  case ND_GE:
  case ND_GT:
    node->ty = ty_bool;
    return;
  case ND_VAR:
    node->ty = node->var->ty;
    return;
  case ND_STR:
    node->ty = node->var->ty;
    return;
  case ND_IGET:
    if (node->lhs->ty->kind != TY_ARRAY) {
      error_tok(node->tok, "not an array\n");
    }
    node->ty = node->lhs->ty->base;
    return;
  case ND_STRUCT_REF:
    node->ty = node->member->ty;
    return;
  case ND_SET:
  case ND_ISET:
  case ND_LET:
  case ND_WHILE:
  case ND_FUNC:
  case ND_DEFSTRUCT:
  case ND_DEFUNION:
  case ND_DEFTYPE:
    node->ty = ty_void;
    return;
  case ND_ADDR:
    if (node->lhs->ty->kind == TY_ARRAY) 
      node->ty = pointer_to(node->lhs->ty->base);
    else 
      node->ty = pointer_to(node->lhs->ty);
    return;
  case ND_DEREF:
    if (!node->lhs->ty->base) {
      error_tok(node->tok, "invalid pointer dereference\n");
    } 
    node->ty = node->lhs->ty->base;
    return;
  case ND_IF:
    if (!node->els && node->then->ty != ty_void) {
      error_tok(node->tok, "if expression type mismatch\n");
    } else if (!node->els) {
      node->ty = ty_void;
    } else if (node->els->ty == node->then->ty) {
      node->ty = node->then->ty;
    }
    return;
  case ND_DO:
    node->ty = last_expr(node->body)->ty;
    return;
  case ND_APP:
    node->ty = ty_int;
    return;
  }
  error_tok(node->tok, "can't assign type of kind %d\n", node->kind);
}