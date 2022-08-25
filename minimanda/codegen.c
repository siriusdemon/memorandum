#include "manda.h"

static void gen_addr(Node* node);
static void gen_expr(Node* node);


// codegen
static int depth;
static char *argreg8[] = {"%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b"};
static char *argreg64[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
static Node* current_fn;

static int count(void) {
  static int i = 1;
  return i++;
}

static void push(void) {
  printf("  push %%rax\n");
  depth++;
}

static void pop(char *arg) {
  printf("  pop %s\n", arg);
  depth--;
}

// Load a value from where %rax is pointing to.
static void load(Type *ty) {
  if (ty->kind == TY_ARRAY) {
    // If it is an array, do not attempt to load a value to the
    // register because in general we can't load an entire array to a
    // register. As a result, the result of an evaluation of an array
    // becomes not the array itself but the address of the array.
    // This is where "array is automatically converted to a pointer to
    // the first element of the array in C" occurs.
    return;
  }

 if (ty->size == 1)
    printf("  movsbq (%%rax), %%rax\n");
  else
    printf("  mov (%%rax), %%rax\n");
}

// Store %rax to an address that the stack top is pointing to.
static void store(Type* ty) {
  pop("%rdi");

  if (ty->size == 1)
    printf("  mov %%al, (%%rdi)\n");
  else
    printf("  mov %%rax, (%%rdi)\n");
}

static int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

static void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local) {
      // Local variable
      printf("  lea %d(%%rbp), %%rax\n", node->var->offset);
    } else {
      // Global variable
      printf("  lea %s(%%rip), %%rax\n", node->var->name);
    }
    return;
  }
  error_tok(node->tok, "only variables support `&` operation.");
}

// Assign offsets to local variables.
static void assign_lvar_offsets(Node* prog) {
  for (Node* fn = prog; fn; fn = fn->next) {
    int offset = 0;
    for (Var *var = fn->locals; var; var = var->next) {
      offset += var->ty->size;
      var->offset = -offset;
    }
    fn->stack_size = align_to(offset, 16);
  }
}

static void gen_expr(Node *node) {
  switch(node->kind) {
  case ND_LET:
    if (node->rhs) {
      gen_addr(node->lhs);
      push();
      gen_expr(node->rhs);
      store(node->lhs->ty);
    }
    return;
  case ND_SET:
    gen_addr(node->lhs);
    push();
    gen_expr(node->rhs);
    store(node->lhs->ty);
    return;
  case ND_NUM:
    printf("  mov $%d, %%rax\n", node->val);
    return;
  case ND_VAR:
    gen_addr(node);
    load(node->ty);
    return;
  case ND_IF: {     // {} is needed here to declare `c`.
    int c = count();
    gen_expr(node->cond);
    printf("  cmp $0, %%rax\n");
    printf("  je  .L.else.%d\n", c);
    gen_expr(node->then);
    printf("  jmp .L.end.%d\n", c);
    printf(".L.else.%d:\n", c);
    if (node->els)
      gen_expr(node->els);
    printf(".L.end.%d:\n", c);
    return;
  }
  case ND_WHILE: {
    int c = count();
    printf(".L.while.%d:\n", c);
    gen_expr(node->cond);
    printf("  cmp $0, %%rax\n");
    printf("  je  .L.end.%d\n", c);
    for (Node* n = node->then; n; n = n->next)
      gen_expr(n);
    printf("  jmp .L.while.%d\n", c);
    printf(".L.end.%d:\n", c);
    return;
  }
  case ND_APP: {
    int nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      gen_expr(arg);
      push();
      nargs++;
    }

    for (int i = nargs - 1; i >= 0; i--)
      pop(argreg64[i]);
    printf("  mov $0, %%rax\n");
    printf("  call %s\n", node->fn);
    return;
  }
  }

  // unary
  switch (node->kind) {
    case ND_DEREF:
      gen_expr(node->lhs);
      load(node->ty);
      return;
    case ND_ADDR: 
      gen_addr(node->lhs);
      return;
  } 

  // iget iset
  switch (node->kind) {
    case ND_ISET:
      gen_expr(node->lhs);
      push();
      gen_expr(node->mhs);
      printf("  imul $%d, %%rax\n", node->lhs->ty->base->size);
      pop("%rdi");
      printf("  add %%rdi, %%rax\n");
      push();
      gen_expr(node->rhs);
      store(node->lhs->ty->base);
      return;
    case ND_IGET:
      gen_expr(node->lhs);
      push();
      gen_expr(node->rhs);
      printf("  imul $%d, %%rax\n", node->lhs->ty->base->size);
      pop("%rdi");
      printf("  add %%rdi, %%rax\n");
      load(node->lhs->ty->base);
      return;
  }

  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop("%rdi");
  
  // for compare operation.
  char* cc = NULL;

  switch (node->kind) {
  case ND_ADD:
    printf("  add %%rdi, %%rax\n");
    return;
  case ND_SUB:
    printf("  sub %%rdi, %%rax\n");
    return;
  case ND_MUL:
    printf("  imul %%rdi, %%rax\n");
    return;
  case ND_DIV:
    printf("  cqo\n");
    printf("  idiv %%rdi\n");
    return;
  case ND_EQ: cc = "e"; break;
  case ND_LT: cc = "l"; break;
  case ND_LE: cc = "le"; break;
  case ND_GE: cc = "ge"; break;
  case ND_GT: cc = "g"; break;
 }
  if (cc) {
    printf("  cmp %%rdi, %%rax\n");
    printf("  set%s %%al\n", cc);
    printf("  movzb %%al, %%rax\n");
    return;
  }

  error("invalid expression");
}


// emit global variable
static void emit_data(Node* prog) {
  for (Node* node = prog; node; node = node->next) {
    if (node->kind != ND_LET)
      continue;

    printf("  .data\n");
    printf("  .globl %s\n", node->lhs->var->name);
    printf("%s:\n", node->lhs->var->name);
    printf("  .zero %d\n", node->lhs->var->ty->size);
  }
}


void codegen(Node* prog) {
  emit_data(prog);

  assign_lvar_offsets(prog);
  for (Node* fn = prog; fn; fn = fn->next) {
    if (fn->kind != ND_FUNC) 
      continue;

    printf("  .globl %s\n", fn->fn);
    printf("%s:\n", fn->fn);
    current_fn = fn;

    // Prologue
    printf("  push %%rbp\n");
    printf("  mov %%rsp, %%rbp\n");
    printf("  sub $%d, %%rsp\n", fn->stack_size);

    int i = 0;
    for (Node* arg = fn->args; arg; arg = arg->next)
      if (arg->var->ty->size == 1) 
        printf("  mov %s, %d(%%rbp)\n", argreg8[i++], arg->var->offset);
      else if (arg->var->ty->size == 8)
        printf("  mov %s, %d(%%rbp)\n", argreg64[i++], arg->var->offset);
    // Emit code
    for (Node* e = fn->body; e; e = e->next) {
      gen_expr(e);
    }
    assert(depth == 0);

    // Epilogue
    printf(".L.return.%s:\n", fn->fn);
    printf("  mov %%rbp, %%rsp\n");
    printf("  pop %%rbp\n");
    printf("  ret\n");
  }
}
