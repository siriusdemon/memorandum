# milestore: Global Var

[Commit](https://github.com/rui314/chibicc/tree/4ebae5dac5a425cadb5c2a7aad5d9b10f986dbdf)


compare to pointer, at this stage, the following features is added:

+ function with at most six arguments
+ global variable
+ sizeof
+ array


### 1. Lexer

`tokenize.c`

Only two modifications: a new function `consume`, two keywords including `sizeof`, `int`.

### 2. Parser

`parse.c`

A new variable chain `globals` is added to trace global variable and functions. 

Type is introduced. Line:612 `parse` starts from parse type specification. There are only two node kind: function (line:615-616) and global variable (line:621).

`is_function` will first check whether it is a type declaration (line:599). Otherwise, it call `declarator` to parse the rest.

Since the type is already be parsed by `typespec`, `declarator` starts with a dummy type. Line:167-168 consumes all the stars sign, so `tok` should contains a identifier. Line:172 `type_suffix` check whether it is a function or an array. Line:151-152 handles function while line:154-158 handles array. If a function is found, `ty` becomes a return type of the function (line:141). If an array is found, `ty`, line:157 recursively call `type_suffix` to support multi-dimensional array.

Right now, only constant array size is supported.

Although `is_function` does a lot, it does nothing except telling its caller whether a function is met. So the real parsing work can start correctly.

One note about `sizeof`. Line:531-535 handles `sizeof`. It returns the type of the node.

### 3. Type

`type.c`

Array and pointer is treated as same. See line:79-83.

### 4. CodeGen

`codegen.c`

`argreg` is introduced to hold the args of function. Line:95-108 handles function call.

`gen_addr` now use `rip` to fetch global variable.

Global variables are setup in `emit_data`.