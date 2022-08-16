# milestore: Pointer

Commit https://github.com/rui314/chibicc/tree/f26c8c26aae80d8a436677df86ca0856370e1ecc


At this pointer, only int and int* is available.

The AST structure is clear.


### 1. Lexer

There is no lexer, but `tokensize` in `tokensize.c`.

Function `tokensize` saves the input to a static variable `current_input`. Then creates an empty Token `head`. So `head.next`, the returned value, is the first token of input stream. 

The main process starts from line:96. Firstly, all space is skipped (line:98-100). At line:104, token of NUM is built. `new_token` create a new token, make it as `cur->next`, and return the new token. `strtoul` will parse a number and move `p` forward, so the length of token can be computed. 

Line:113 recognizes a keyword or identifier. At this time, they are all treated as IDENT. 

Line:122 recognizes multi-letter operations. Line:130 recognizes single-letter operations. They are all treated as RESERVED

Line:135 reports invalid token error.

Finally, at line:138, when reaching the end of input. a EOF token is appended into the token chain. Line:139 calls `convert_keywords` to distinguish keyword and identifier we mix at Line:113. After that, a pointer of first token is returned.


### 2. Parser

`parse.c`

The AST is consists of a chain of `Node`. There are four basic fields of a node: kind, token, type, and next. Node has many other fields which can be used depend on the kind of Node.

`locals` is a head pointer of a chain of local variables. `new_lvar` creates a new `Var` and put it at the head of the chain, then modify `locals` to point to the head. Therefore, the first created variable is deepest.

 
Line:248 handles `+` operation, a pointer is 64-bit, aka 8 bytes. So line:267 multiple the `rhs` by 8.

Line:272 handles `-` operation. Line:282 divides the `rhs` by 8, line:293 does similarly.

### 3. Type

`type.c`

There are two types right now: int and pointer to int. The `base` field of a pointer is not NULL. 

Line:16 `add_type` is the main function of type utilities. Firstly, it will check whether a node is valid or its type exists. Since a node is an all-in-one structure. `add_type` recursively call itself on other node-pointer on the node.

Starts from line:31, type of current node is choice depending on the node kind. 

Line:47 handles `&a`. The operand is `node->lhs`, so a pointer type of that type is return. 

Line:50 handles `*p`. The type of `node->lhs` is checked. If it is a pointer, the `*p` is the `base`. Otherwise, it is a int. This means `{a = 3; *a}` is a int.


### 4. CodeGen

`codegen.c`

The architeture of CodeGen is a stack machine. `push` put a value onto the stack, and `pop` fetch the top value to `arg`. `depth` tracks the depth of the stack.

By using a stack machine, only a few registers are needed. Thus simplifies the code.

`codegen` is the only inferface of this file. It starts with `assign_lvar_offsets`. Recall that `locals` is a chain of variables. All of variables are saved in the stack. We needs to know the stack size to allocate space for them. Each of variable occupies 8 bytes, so line:162 accumulates offset by adding 8. Line 165 round the offset up to multiple of 16. Line:175-177 setup the stack.

After that, line:179 starts to generate code for the program. It calls `gen_stmt`.

`gen_stmt` checks the node kind. For `if` and `for`, `count` helps to generate different labels as target of jump. For `return`, it jump to the prepared return label. `gen_stmt` illustrates the picture of code generation.


Line:30 defines `gen_addr`, it handles `&a`(ND_VAR) and `&*p`(ND_DEREF). If node is a var, we return the address, which is computed by the `rbp` and the offset. Otherwise, `&` and `*` just cancel each other out. 

Line:44 defines `gen_expr`. It emits code as follows:

number variable and unary operations:

+ NUM: simply store the value to rax. 
+ VAR: load address into rax by calling `gen_addr`, then store its value into rax.
+ DEREF: for a `*p`, we need to compute the `p` firstly, so a `gen_expr` is called.
+ ADDR: for a `&a` or `&*p`, call `gen_addr` as we have already mentioned. 
+ ASSIGN: node->lhs is a var, so `gen_addr` for it. Store the address by calling `push`. node->rhs is the value. `gen_expr` will place its value on rax. `pop(%rdi)` stores the address of var on rdi. Finally, store the value on that address.


The unary operation `+` is ignored in the parsing stage. And `-1` is treated as `0 - 1`, which is a binary operation.

binary operation:

Line:69-72 stores the value of rhs on rdi and the value of lhs on rax.  Then we emit code according to the node kind.