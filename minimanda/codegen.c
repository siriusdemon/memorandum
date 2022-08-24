#include "manda.h"

static void gen_addr(Node* node);
static void gen_expr(Node* node);


// codegen
static int depth;
static char* argreg[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
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

static int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

static void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    printf("  lea %d(%%rbp), %%rax\n", node->var->offset);
    return;
  }
  error_tok(node->tok, "only variables support `&` operation.");
}

// Assign offsets to local variables.
static void assign_lvar_offsets(Node* prog) {
  for (Node* fn = prog; fn; fn = fn->next) {
    int offset = 0;
    for (Var *var = fn->locals; var; var = var->next) {
      offset += 8;
      var->offset = -offset;
    }
    fn->stack_size = align_to(offset, 16);
  }
}

static void gen_expr(Node *node) {
  switch(node->kind) {
  case ND_SET:
  case ND_LET:
    gen_addr(node->lhs);
    push();
    gen_expr(node->rhs);
    pop("%rdi");
    printf("  mov %%rax, (%%rdi)\n");
    return;
  case ND_NUM:
    printf("  mov $%d, %%rax\n", node->val);
    return;
  case ND_VAR:
    gen_addr(node);
    printf("  mov (%%rax), %%rax\n");
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
      pop(argreg[i]);
    printf("  mov $0, %%rax\n");
    printf("  call %s\n", node->fn);
    return;
  }
  }

  // unary
  switch (node->kind) {
    case ND_DEREF:
      gen_expr(node->lhs);
      printf("  mov (%%rax), %%rax\n");
      return;
    case ND_ADDR: 
      gen_addr(node->lhs);
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

void codegen(Node* prog) {
  assign_lvar_offsets(prog);

  for (Node* fn = prog; fn; fn = fn->next) {
    printf("  .globl %s\n", fn->fn);
    printf("%s:\n", fn->fn);
    current_fn = fn;

    // Prologue
    printf("  push %%rbp\n");
    printf("  mov %%rsp, %%rbp\n");
    printf("  sub $%d, %%rsp\n", fn->stack_size);

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
