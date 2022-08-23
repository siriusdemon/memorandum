#include "manda.h"

Type* ty_int = &(Type){TY_INT};
Type* ty_void = &(Type){TY_VOID};

bool is_integer(Type* ty) {
  return ty->kind == TY_INT;
}

Type* pointer_to(Type* base) {
  Type* ty = calloc(1, sizeof(Type));
  ty->kind = TY_PTR;
  ty->base = base;
  return ty;
}

Type* new_int_type() {
  Type* ty = calloc(1, sizeof(Type));
  ty->kind = TY_INT;
  return ty;
}

void add_type(Node* node) {
  if (!node || node->ty)
    return;

  add_type(node->lhs);
  add_type(node->rhs);
  add_type(node->cond);
  add_type(node->then);
  add_type(node->els);


  switch (node->kind) {
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_EQ:
  case ND_LT:
  case ND_LE:
  case ND_GE:
  case ND_GT:
  case ND_NUM:
    node->ty = ty_int;
    return;
  case ND_VAR:
    node->ty = node->var->ty;
    return;
  case ND_SET:
  case ND_LET:
  case ND_WHILE:
    node->ty = ty_void;
    return;
  case ND_ADDR:
    node->ty = pointer_to(node->lhs->ty);
    return;
  case ND_DEREF:
    if (node->lhs->ty->kind != TY_PTR) {
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
  }
  error_tok(node->tok, "can't assign type of kind %d\n", node->kind);
}