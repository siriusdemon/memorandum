#include "manda.h"


static void gen_addr(Node* node);
static void gen_expr(Node* node);


// codegen
static FILE *output_file;
static int depth;
static char *argreg8[] = {"%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b"};
static char *argreg16[] = {"%di", "%si", "%dx", "%cx", "%r8w", "%r9w"};
static char *argreg32[] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
static char *argreg64[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
static Node* current_fn;

static void println(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(output_file, fmt, ap);
  fprintf(output_file, "\n");
}

static int count(void) {
  static int i = 1;
  return i++;
}

static void push(void) {
  println("  push %%rax");
  depth++;
}

static void pop(char *arg) {
  println("  pop %s", arg);
  depth--;
}

// Load a value from where %rax is pointing to.
static void load(Type *ty) {
  if (ty->kind == TY_ARRAY || ty->kind == TY_UNION || ty->kind == TY_STRUCT) {
    // If it is an array, do not attempt to load a value to the
    // register because in general we can't load an entire array to a
    // register. As a result, the result of an evaluation of an array
    // becomes not the array itself but the address of the array.
    // This is where "array is automatically converted to a pointer to
    // the first element of the array in C" occurs.
    return;
  }

 if (ty->size == 1)
    println("  movsbq (%%rax), %%rax");
  else if (ty->size == 2)
    println("  movswq (%%rax), %%rax");
  else if (ty->size == 4)
    println("  movsxd (%%rax), %%rax");
  else
    println("  mov (%%rax), %%rax");
}

// Store %rax to an address that the stack top is pointing to.
static void store(Type* ty) {
  pop("%rdi");

  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    for (int i = 0; i < ty->size; i++) {
      println("  mov %d(%%rax), %%r8b", i);
      println("  mov %%r8b, %d(%%rdi)", i);
    }
    return;
  }

  if (ty->size == 1)
    println("  mov %%al, (%%rdi)");
  else if (ty->size == 2)
    println("  mov %%ax, (%%rdi)");
  else if (ty->size == 4)
    println("  mov %%eax, (%%rdi)");
  else
    println("  mov %%rax, (%%rdi)");
}

static void store_gp(int r, int offset, int sz) {
  switch (sz) {
  case 1:
    println("  mov %s, %d(%%rbp)", argreg8[r], offset);
    return;
  case 2:
    println("  mov %s, %d(%%rbp)", argreg16[r], offset);
    return;
  case 4:
    println("  mov %s, %d(%%rbp)", argreg32[r], offset);
    return;
  case 8:
    println("  mov %s, %d(%%rbp)", argreg64[r], offset);
    return;
  }
  unreachable();
}

int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

static void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local) {
      // Local variable
      println("  lea %d(%%rbp), %%rax", node->var->offset);
    } else {
      // Global variable
      println("  lea %s(%%rip), %%rax", node->var->name);
    }
    return;
  case ND_STRUCT_REF:
    gen_addr(node->lhs);
    println(" add $%d, %%rax", node->member->offset);
    return;
  }
    
  error_tok(node->tok, "not an lvalue");
}

// Assign offsets to local variables.
static void assign_lvar_offsets(Node* prog) {
  for (Node* fn = prog; fn; fn = fn->next) {
    int offset = 0;
    for (Var *var = fn->locals; var; var = var->next) {
      offset += var->ty->size;
      offset = align_to(offset, var->ty->align);
      var->offset = -offset;
    }
    fn->stack_size = align_to(offset, 16);
  }
}


enum { I8, I16, I32, I64 };

static int getTypeId(Type* ty) {
  switch (ty->kind) {
  case TY_CHAR:
    return I8;
  case TY_SHORT:
    return I16;
  case TY_INT:
    return I32;
  }
  // rest type, may be u64 i64 or pointer
  return I64;
}

// The table for type casts
static char i32i8[] = "movsbl %al, %eax";
static char i32i16[] = "movswl %ax, %eax";
static char i32i64[] = "movsxd %eax, %rax";

static char* cast_table[][4] = {
  {NULL,  NULL,   NULL, i32i64}, // i8
  {i32i8, NULL,   NULL, i32i64}, // i16
  {i32i8, i32i16, NULL, i32i64}, // i32
  {i32i8, i32i16, NULL, NULL},   // i64
};

static void cast(Type* from, Type* to) {
  int t1 = getTypeId(from);
  int t2 = getTypeId(to);
  if (cast_table[t1][t2])
    println("  %s", cast_table[t1][t2]);
}

static void gen_expr(Node* node) {
  println(" .loc 1 %d", node->tok->line_no);
  switch(node->kind) {
  case ND_DEFSTRUCT:
  case ND_DEFUNION:
  case ND_DEFTYPE:
  case ND_DEFMACRO:
    return;
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
    println("  mov $%ld, %%rax", node->val);
    return;
  case ND_VAR:
  case ND_STRUCT_REF:
    gen_addr(node);
    load(node->ty);
    return;
  case ND_IF: {     // {} is needed here to declare `c`.
    int c = count();
    gen_expr(node->cond);
    println("  cmp $0, %%rax");
    println("  je  .L.else.%d", c);
    gen_expr(node->then);
    println("  jmp .L.end.%d", c);
    println(".L.else.%d:", c);
    if (node->els)
      gen_expr(node->els);
    println(".L.end.%d:", c);
    return;
  }
  case ND_AND: {
    int c = count();
    gen_expr(node->lhs);
    println("  cmp $0, %%rax");
    println("  je .L.false.%d", c);
    gen_expr(node->rhs);
    println("  cmp $0, %%rax");
    println("  je .L.false.%d", c);
    println("  mov $1, %%rax");
    println("  jmp .L.end.%d", c);
    println(".L.false.%d:", c);
    println("  mov $0, %%rax");
    println(".L.end.%d:", c);
    return;
  }
  case ND_OR: {
    int c = count();
    gen_expr(node->lhs);
    println("  cmp $0, %%rax");
    println("  jne .L.true.%d", c);
    gen_expr(node->rhs);
    println("  cmp $0, %%rax");
    println("  jne .L.true.%d", c);
    println("  mov $0, %%rax");
    println("  jmp .L.end.%d", c);
    println(".L.true.%d:", c);
    println("  mov $1, %%rax");
    println(".L.end.%d:", c);
    return;
  }
  case ND_DO: 
    for (Node* n = node->body; n; n = n->next)
      gen_expr(n);
    return;
  case ND_WHILE: {
    int c = count();
    println(".L.while.%d:", c);
    gen_expr(node->cond);
    println("  cmp $0, %%rax");
    println("  je  .L.end.%d", c);
    for (Node* n = node->then; n; n = n->next)
      gen_expr(n);
    println("  jmp .L.while.%d", c);
    println(".L.end.%d:", c);
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
    println("  mov $0, %%rax");
    println("  call %s", node->fn);
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
    case ND_CAST:
      gen_expr(node->lhs);
      cast(node->lhs->ty, node->ty);
      return;
    case ND_NOT:
      gen_expr(node->lhs);
      println("  cmp $0, %%rax");
      println("  sete %%al");
      println("  movzx %%al, %%rax");
      return;
    case ND_BITNOT:
      gen_expr(node->lhs);
      println("  not %%rax");
      return;
  } 

  // iget iset
  switch (node->kind) {
    case ND_ISET:
      gen_expr(node->lhs);
      push();
      gen_expr(node->mhs);
      println("  imul $%d, %%rax", node->lhs->ty->base->size);
      pop("%rdi");
      println("  add %%rdi, %%rax");
      push();
      gen_expr(node->rhs);
      store(node->lhs->ty->base);
      return;
    case ND_IGET:
      gen_expr(node->lhs);
      push();
      gen_expr(node->rhs);
      println("  imul $%d, %%rax", node->lhs->ty->base->size);
      pop("%rdi");
      println("  add %%rdi, %%rax");
      load(node->lhs->ty->base);
      return;
  }

  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop("%rdi");
  
  char *ax, *di;
  if (node->lhs->ty->kind == TY_LONG || node->lhs->ty->base) {
    ax = "%rax";
    di = "%rdi";
  } else {
    ax = "%eax";
    di = "%edi";
  }
  // for compare operation.
  char* cc = NULL;

  switch (node->kind) {
  case ND_ADD:
    println("  add %s, %s", di, ax);
    return;
  case ND_SUB:
    println("  sub %s, %s", di, ax);
    return;
  case ND_MUL:
    println("  imul %s, %s", di, ax);
    return;
  case ND_DIV:
  case ND_MOD:
    if (node->lhs->ty->size == 8) {
      println("  cqo");
    } else {
      println("  cdq");
    }
    println("  idiv %s", di);
    if (node->kind == ND_MOD) {
      println("  mov %%rdx, %%rax");
    }
    return;
  case ND_BITAND:
    println("  and %s, %s", di, ax);
    return;
  case ND_BITOR:
    println("  or %s, %s", di, ax);
    return;
  case ND_BITXOR:
    println("  xor %s, %s", di, ax);
    return;
  case ND_SRA:
    println("  mov %%rdi, %%rcx");
    println("  sar %%cl, %%rax");
    return;
  case ND_SRL:
    println("  mov %%rdi, %%rcx");
    println("  shr %%cl, %%rax");
    return;
  case ND_SLL:
    println("  mov %%rdi, %%rcx");
    println("  shl %%cl, %%rax");
    return;
  case ND_EQ: cc = "e"; break;
  case ND_LT: cc = "l"; break;
  case ND_LE: cc = "le"; break;
  case ND_GE: cc = "ge"; break;
  case ND_GT: cc = "g"; break;
 }
  if (cc) {
    println("  cmp %s, %s", di, ax);
    println("  set%s %%al", cc);
    println("  movzb %%al, %%rax");
    return;
  }

  error("invalid expression");
}


// emit global variable
static void emit_data(Node* prog) {
  for (Node* node = prog; node; node = node->next) {
    if (node->kind != ND_LET)
      continue;

    println("  .data");
    println("  .globl %s", node->lhs->var->name);
    println("%s:", node->lhs->var->name);
    if (node->rhs && node->rhs->ty->kind == TY_ARRAY) {
      for (int i = 0; i < node->lhs->ty->size; i++)
        println("  .byte %d", node->rhs->str[i]);
    } else {
      println("  .zero %d", node->lhs->var->ty->size);
    }
  }
}


void codegen(Node* prog, FILE* out) {
  output_file = out;

  emit_data(prog);
  assign_lvar_offsets(prog);
  for (Node* fn = prog; fn; fn = fn->next) {
    if (fn->kind != ND_FUNC) 
      continue;

    println("  .globl %s", fn->fn);
    println("%s:", fn->fn);
    current_fn = fn;

    // Prologue
    println("  push %%rbp");
    println("  mov %%rsp, %%rbp");
    println("  sub $%d, %%rsp", fn->stack_size);

    int i = 0;
    for (Node* arg = fn->args; arg; arg = arg->next)
      store_gp(i++, arg->var->offset, arg->var->ty->size);
    // Emit code
    for (Node* e = fn->body; e; e = e->next) {
      gen_expr(e);
    }
    assert(depth == 0);

    // Epilogue
    println(".L.return.%s:", fn->fn);
    println("  mov %%rbp, %%rsp");
    println("  pop %%rbp");
    println("  ret");
  }
}
