# Milestore: Function declaration

[Commit](https://github.com/rui314/chibicc/tree/71b92d35c4cd03aeccb8212364f9caea3c94b721)


new features from global variable

+ struct
+ union
+ short, long types
+ read code from file
+ char, escape char, octal string and hexadecimal string
+ function declaration
+ scope
+ comments
+ improved error reporting


### 0. Input

`main.c`

`parse_args` and `open_file` are added to support reading code from a file.

### 1. Lexer

`tokenize.c`

Line:231 skips line comments. Line:239 skips block comments. A block comment may be unclosed, so a `error_at` is called when that happens.

Since the error reporting is improved. Let's take a close look!

Line:46 `error_at` computes the line number where the error happens, then instead calls `verror_at`.

`verror_at` set one pointer `line` to the start of the line (line:24-26), another pointer `end` to the end of the line (line:28-30). It adds some information (line:33) and then compute the position (line:37) a error message should stay in. Finally, it prints out the message and exits.

Line:129 `read_escaped_char` supports lexing escape characters.

Line 172 `string_literal_end` skip escape characters (line:177-178) to search the `"`. `read_string_literal` first allocates a large enough space to hold the string. Whne it encounters a escape character (line: 190), its total length will be smaller than the space it has owned.

Line 208 `add_line_numbers` elegantly adds a line number to every token for later use.

### 2. Parser

`parse.c`

Scope is introduced. C has two scopes: one is variable scope, another is tag scope. Consider the following example:

```c
struct tensor {int dummy;};
int main() { struct tensor tensor};
```
`tensor` can be both a struct tag and a variable name. They don't collide with each other.

Line:60 `scope_depth` traces the depth of current scope. `find_var` and `find_tag` search in a VarScope and a TagScope respectively.

Line:141 `push_scope` attaches a variable to the VarScope of current Scope. Line:204 `push_tag_scope` works similarly.

Line:186 `new_string_literal` create an anonynous variable binding for a string literal.

Line:215 `typespec` is extended to support several new types. Since the structure similarity of struct and union, they are both handled by `struct_union_decl`.

Line:654 `struct_union_decl` firstly check whether a tag is declared (line:656-659).  If so, it can be a declaration or an instantiation (line:662). If it is an instantiation, `find_tag` is used to find out the type (line:663-667). Otherwise, it is a declaration. A struct object is constructed. Note that `ty->kind` is initialized as `STRUCT` and will be overrided by the caller of `struct_union_decl`. So as the `align` field.

Line:738 `postfix` handles `a[1], a.b, a->b`. Note that `a[1]` is converted to `*(a+1)`, `a->b` is converted to `(*a).b`. So let's take a look at `struct_ref` (line:727). 

In `struct_ref`, the `lhs` aka the struct or union, is added type firstly (line:728). `tok` is exactly the field we are operating on, thus a `MEMBER` unary node is created for it (line:732-733).

### 3. Type

Nothing really new.

### 4. CodeGen

`codegen.c`

Line:62-64 in `gen_addr` handles struct or union member. Firstly, the address of the struct or union is computed, then the member can be accessed by using the offset.

Line:72 `load` remains almost the same as last milestore. 
Line:94 `store` has some interesting things. Line:97-100 handles the struct or union by copying the memory one byte each time. This is because there are some paddings between struct members.

