  .globl main
main:
  push %rbp
  mov %rsp, %rbp
  sub $16, %rsp
  lea -8(%rbp), %rax
  push %rax
  mov $3, %rax
  pop %rdi
  mov %rax, (%rdi)
  mov $3, %rax
  push %rax
  mov $8, %rax
  push %rax
  lea -8(%rbp), %rax
  push %rax
  mov $8, %rax
  push %rax
  mov $2, %rax
  pop %rdi
  imul %rdi, %rax
  push %rax
  lea -8(%rbp), %rax
  pop %rdi
  add %rdi, %rax
  pop %rdi
  sub %rdi, %rax
  pop %rdi
  cqo
  idiv %rdi
  pop %rdi
  add %rdi, %rax
  jmp .L.return
.L.return:
  mov %rbp, %rsp
  pop %rbp
  ret
