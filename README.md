# memorandum

2022.10.13 

minimanda 是因缘和合之下的产物。本来我只想研习 chibicc 顺便做一点笔记，但是 chibicc 的 parser 越来越复杂，到后面读起来十分费劲，所以我才开始尝试将它的 parser 改成 S-表达式，于是就有了 minimanda。

但是不得不说，chibicc 的 parser 实现是非常工整的，作为非 S-表达式的 parser 非常值得模仿。只是由于 C 的语法设计问题，拆解出来的层级就比较多，有的层级只有词法而没有语义，理解就要比较费劲。S-表达式就不存在这样的问题。

因而，minimanda 最初被设计成一个有着 S-表达式语法的 C 语言。

这样一路跟着作者一点点添加功能，没什么问题。直到作者将他的测试用例从 BASH 转移到 C 语言，他使用了 #define 宏来打印输出。但是 minimanda 没有宏，所以我依旧使用 BASH 写测试用例。但是，用 BASH 有个约束，它的返回值只有一个字节。这意味着你无法测试像 long 这种多字节的类型。我思考良久，在 lexer 到 parser 之间增加了一个 S-表达式的 parser，并在这个层面上实现了一个小型的 LISP 解释器，从而实现了 LISP 风格的元编程（LISP宏）。

项目的结束同样是巧合，因为我突然发现真正需要在意这些东西的人并不在意。当然不只是技术，还有其他很重要的东西他们也不在意。这都是私心作怪。但无论如何，对于那些想要学习编译器的，正被困在家中、学校里、野外等地方的学生来说，chibicc 绝对值得消化咀嚼，也希望 minimanda 会有一定的启发意义。

最后我想罗列下 minimanda 的 commit 历史和语法实现：

[提交历史入口](https://github.com/siriusdemon/memorandum/commits/main/minimanda)
[第一次提交](https://github.com/siriusdemon/memorandum/commit/cd9bef22b70be8be65a132bbdb51963cba445ce6)

#### 01: manda journey
基本上和 chibicc 一样，支持简单的整数。
```sh
assert 0 0
assert 42 42
```

#### 02: Support + - operation
实现加减运算
```sh
assert 42 "(+ 20 22)"
assert 42 "(- 50 8)"
```

这一节引入了很多东西，tokenize，parse，codegen，整个编译框架原型显现。

#### 03: add [] as alternative list expression
像 scheme 一样，支持 `[]` 作为括号提升可读性。
```sh
assert 42 "[+ 20 22]"
assert 42 "[- 50 8]"
```

#### 04: Add * /; use stack machine
添加了乘除运算，并将后端改成栈机来实现，就像 JVM 一样。不过这个栈机非常简易。
```sh
assert 42 "(* 6 7)"
assert 42 "(/ 84 2)"
```

#### 05: add nested expression
允许表达式嵌套。
```sh
assert 42 "(* (/ 42 (+ 1 6)) (- 9 2))"
```

#### 06: split into several files
重构代码，将编译器的各个部分拆开。
```sh
codegen.c  manda.h  main.c  parse.c  tokenize.c
```

#### 07: improve error report
重构代码，改进错误提示。这些都是 chibicc 作者写的代码。

#### 08: support more operands for some operation
模仿 Lisp，支持无限操作数。
```sh
assert 42 "(* 2 3 7)"
assert 42 "(+ 1 2 3 4 5 6 7 (* 1 2 7))"
```
#### 09: [update README]
今天是个纪念日。

#### 10: support multi-expression
支持输入为多个表达式，以最后一个表达式的值作为返回值。
```sh
assert 3 "1 2 3"
assert 42 "(+ 1 2 3) (* 6 7)"
```

#### 11: support local variables
支持变量定义，这个提交信息量比较多。我提前添加了许多以后要用到的语法关键词。minimanda 是用 let 语句来定义变量的。

```sh
assert 42 "(let a 42) a"
assert 32 "(let a 42) (let b 10) (- a b)"
```

#### 12: add < > = <= >=
支持比较运算。一股 C 味扑面而来。

```sh
assert 1 "(> 2 1)"
assert 0 "(< 2 1)"
assert 0 "(<= 2 1)"
assert 1 "(>= 2 1)"
assert 1 "(= 1 1)"
```

#### 13: add if
if 语句的实现。非测试的代码增量有 50 行。codegen 使用一个静态变量来生成不同的标签。

```sh
assert 1 "(if (< 1 2) 1 2)"
assert 2 "(if (> 1 2) 1 2)"
assert 42 "(if (> 1 2) 1 (* 6 7))"
```

#### 14: support set
支持修改变量的值。
```sh
assert 42 "(let i 1) (set i (* 6 7)) i"
```

#### 15: add while
minimanda 有 while 循环！（好像没什么奇怪的。）
```sh
assert 10 "(let i 1) (while (< i 10) (set i (+ i 1))) i"
```

#### 16: addr (&a) and deref (a.*)
这指针如你所愿。

```sh
assert 42 "(let i 42) (let b i) b"
assert 42 "(let i 42) (let b &i) b.*"
assert 42 "(let i 42) (let b &i) (let c &b) c.*.*"
```

#### 17: limit operator &
限制了 & 的使用范围。只能用于变量。至于为什么，我忘记了。

#### 18: add type
添加了 int 类型和对应的指针类型。熟悉的 C 味。

```sh
assert 42 "(let i :int 1) (set i (* 6 7)) i"
assert 10 "(let i :int 1) (while (< i 10) (set i (+ i 1))) i"
assert 42 "(let i :int 42) (let b :int i) b"
assert 42 "(let i :int 42) (let b :*int &i) b.*"
assert 42 "(let i :int 42) (let b :*int &i) (let c :**int &b) c.*.*"
```

#### 19: add deref addr
新增 deref addr 两个操作符作为 a.* &a 的替代。后面写 LISP 解释器的时候会有用。

```sh
assert 42 "(let i :int 42) (let b :*int (addr i)) (deref b)"
```

#### 20: zero-args function call
支持无参数的函数调用。还不支持定义函数，只能调用外部链接进来的 C 函数。
```sh
# 链接函数
int ret3() { return 3; }
int ret5() { return 5; }

# 调用函数
assert 3 "(ret3)"
assert 5 "(ret3) (ret5)"
```

#### 21: support up to 6 args function call
使用六个寄存器保存调用参数。

```sh
int add6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}

assert 136 "(add6 1 2 3 4 5 (add6 6 7 8 9 10  (add6 11 12 13 14 15 16)))"
assert 21 "(add6 1 2 (ret3) 4 (ret5) 6)"
```

#### 22: add zero-arity function definition
添加无参数的函数定义。

```sh
assert 42 "(def main() -> int 
                (ret0)) 
           (def ret0() -> int 8)"
```

嗯，minimanda 的函数长得有点随意。

#### 23: up to 6 six args function definition
支持六个参数的函数定义。

```sh
assert 42 "(def main() -> int 
                (add 20 22)) 
           (def add(a int b int) -> int 
                (+ a b))"
```
函数的类型前要不要带冒号，这个问题我想了很久，还是没定下来。

#### 24: allow parsing array

数组的内容比较多，拆成了两个 commit，这是第一个，支持对数组的 parsing。

```sh
assert 0 "(def main() -> int (let a: [32 int]) 0)"
assert 0 "(def main() -> int (let a: [32 *int]) 0)"
assert 0 "(def main() -> int (let a: *[32 int]) 0)"
assert 0 "(def main() -> int (let a :[32 int]) (let b :[32 int])  0)"
assert 0 "(def main() -> int 
            (let a :*[32 int]) 
            (let b :[32 *int])  
            (let c :*[32 *[32 int]])  
            0)"
```

#### 25: add iset iget for array

数据元素的 set 和 get。

```sh
assert 1 "(def main() -> int
            (let a :[32 int])
            (iset a 0 1)
            (iset a 1 2)
            (iget a 0))"

assert 42 "(def main() -> int
            (let a :[32 int])
            (let p :*[32 int] &a)
            (iset p.* 0 42)
            (iget p.* 0))"
```
#### 26: allow multi array
多维数组。

```sh
assert 42 "(def main() -> int 
            (let a :[32 [32 int]]) 
            (iset (iget a 0) 0 42) 
            (iget (iget a 0) 0))"
```

#### 27: add sizeof
编译时计算的 sizeof，由于 minimanda 是类型严格的，sizeof 只支持传类型不支持传表达式。后面会实现 typeof 来获取表达式的类型。

```sh
assert 42 "(def main() -> int 
            (let a :[32 32 int]) 
            (iset (iget a 0) 0 42) 
            (iget (iget a 0) 0))"

assert 8 "(def main() -> int (sizeof *int))"
assert 40 "(def main() -> int (sizeof [5 *int]))"
assert 200 "(def main() -> int (sizeof [5 5 int]))"
```

#### 28: add global variable without init
全局变量声明，不支持同时初始化。

```sh
assert 0 "(let g :int) (def main() -> int g)"
assert 42 "(let g :int) (def main() -> int (set g 42) g)"
assert 42 "(let g :int) (def main() -> int (let g :int 42) g)"
```

#### 29: add char && fix parse array bug
支持 char，修bug。我忘记是什么了。

```sh
assert 1 "(def main() -> int (let c: char 1) c)"
assert 1 "(def main() -> int (sizeof char))"
```

#### 30: add string literal
支持 C 风格的字符串字面量，被实现为一个匿名全局数组。

```sh
assert 97 '(def main() -> int (iget "abc" 0))'
assert 98 '(def main() -> int (iget "abc" 1))'
assert 99 '(def main() -> int (iget "abc" 2))'
assert 0 '(def main() -> int (iget "abc" 3))'
```

#### 31: support escape char
支持转义字符。

```sh
assert 7 '(def main() -> int (iget "\a" 0))'
assert 8 '(def main() -> int (iget "\b" 0))'
assert 9 '(def main() -> int (iget "\t" 0))'
assert 10 '(def main() -> int (iget "\n" 0))'
assert 11 '(def main() -> int (iget "\v" 0))'
assert 12 '(def main() -> int (iget "\f" 0))'
assert 13 '(def main() -> int (iget "\r" 0))'
assert 27 '(def main() -> int (iget "\e" 0))'
```

#### 32: read from file instead of stdin
重构输入。

#### 33: [refactor]
重构，将 print 用 println 替代。

#### 34: add -o and --help option
和 chibicc 一样。

#### 35: add ; comment
添加 LISP 风格注释。
```sh
assert 42 "(def main() -> int 
            ; (let hahahh: NonExist)
            42);"

 assert 42 ";;; 
            (def main() -> ;int 
            int
            ; (let hahahh: NonExist)
            42;;;;;;
            );"
```

#### 36: precompute line number
没啥好说的。

#### 37: emit .file .loc assembler directive
codegen 带文件名和行数，便于调试。

#### 38: align local variable

对齐局部变量的内存地址，后面如结构体等也需要对齐。


#### 39: support defstruct; fix tokenize
支持结构定义。

```sh
assert 12 "(defstruct My gender int age int)
           (def main() -> int
            (let a :My) 
            (set a.gender 1)
            (set a.age 12)
            a.age)"
```
#### 40: add do
相当于 scheme 中的 begin。
```sh
assert 3 "(def main() -> int (let a :int (do 1 2 3)) a)"
assert 3 "(def main() -> int (do 1 2 3))"
```

#### 41: add variable scope
添加词法作用域。实现方法与 chibicc 不同。

```sh
assert 1 "(def ret (a int) -> int a) (def main() -> int (ret 1))"
assert 12 "(def main() -> int
            (let a :int 1)
            (let b :int 2)
            (+ (do 
                (let c :int 9) 
                (let b :int 8)
                (let a :int 7)
                c)
                a b))"

assert 3 "(def main() -> int 
            (let a :int 3)
            (do (let a: int 42))
            a)"
```

#### 42: add struct scope
作用域拓展到结构体。
```sh
assert 1 "(defstruct Man age int)
          (def main() -> int
            (let a :Man)
            (set a.age 10)
            (defstruct Man gender int)
            (let b: Man)
            (set b.gender 1)
            b.gender)"
```

#### 43: clean up dead code

#### 44: add union
C 风格的联合体。

```sh
assert 10 "(defstruct S1 age int)
           (defunion U1 gender int X S1)
           (def main() -> int
              (let a: U1) 
              (set a.X.age 10)
              a.gender)"
```

#### 45: support struct/union assignment

#### 46: change int size to 32bit

```sh
assert 26 "(defstruct S A int B int C int D int)
           (def main() -> int
             (let S :S)
             (set S.A 10)
             (+ (sizeof S) S.A))"
```

#### 47: add long type
```sh
assert 8 "(def main() -> int (sizeof long))"
```

#### 48: add short
```sh
assert 2 "(def main() -> int (sizeof short))"
assert 15 "(def main() -> int 
             (+ (sizeof short)
                (sizeof int) 
                (sizeof char)
                (sizeof long)))"
```

#### 49: add deftype
类似 C 的 typedef。

```sh
assert 28 "(def main() -> int
            (defstruct Man gender int age int)
            (deftype X Man)
            (let c :X)
            (set c.age 20)
            (+ (sizeof X) c.age))"
```

#### 50: add typeof

奇妙的 typeof，后面发现 chibicc 也实现了。

```sh
assert 36 "(def main() -> int
            (defstruct X gender int age int)
            (let x :X)
            (let y :(typeof x))
            (set y.age 20)
            (+ (sizeof X) (sizeof (typeof y)) y.age))"

assert 20 "(def main() -> int (* 5 (sizeof (typeof 1))))"
```

#### 51: Use 32 bit registers for char, short and int. But why?

现在我知道答案了。那就是——

#### 52: add type cast
类型转换！
```sh
assert 0 "(def main() -> int 
            (let x :int 256) 
            (cast x char))"
assert 1 "(def main() -> int 8590066177)"
```

codegen.c 的这个表就是上面问题的答案。
```sh
static char* cast_table[][4] = {
  {NULL,  NULL,   NULL, i32i64}, // i8
  {i32i8, NULL,   NULL, i32i64}, // i16
  {i32i8, i32i16, NULL, i32i64}, // i32
  {i32i8, i32i16, NULL, NULL},   // i64
};
```

#### 53: a lossy boolean type
minimanda 有原生的 bool 类型。

```sh
assert 2 "(def main() -> int (let x :bool true) (+ (sizeof bool) (cast x bool)))"
assert 0 "(def main() -> int (let x :bool false) (cast x bool))"
```

说得跟真的一样。

#### 54: support 16-base 8-base 2-base integer literal

Lisp 风格的二进制、八进制和十六进制常量。

```sh
assert 42 "(def main() -> int (+ #x10 #o17 #b1000 #b11))"
assert 4 "(def main() -> int #b100)"
assert 97 "(def main() -> int (let x :char 'a') x)"
```

#### 55: add not
```sh
assert 0 "(def main() -> int (not true))"
```

#### 56: add bitnot
```sh
assert 250 "(def main() -> int (bitnot #b101))"
```
之所以是 250，是因为 BASH 返回值只有一个字节。如果是 4 个字节的 int 则不是这个结果。

#### 57: add mod bitand bitor bitxor

```sh
assert 4 "(def main() -> int (bitxor #b010 #b110))"
assert 0 "(def main() -> int (bitand #b1111 #b111 0))"
assert 11 "(def main() -> int (bitor #b1010 #b0010 #b10 #b1))"
assert 1 "(def main() -> int (bitand #b1111 #b111 #b11 #b1))"
assert 0 "(def main() -> int (mod 18 6))"
assert 5 "(def main() -> int (mod 17 6))"
assert 1 "(def main() -> int (mod 3 2))"
```

#### 58: add logand logor
```sh
assert 0 "(def main() -> int (and true false true))"
assert 1 "(def main() -> int (or false false true))"
```

#### 59: support near compose for compare operators
对 > < 这种操作也添加无限参数支持。

```sh
assert 1 "(def main() -> int 
            (let age :int 20)
            (< 0 age 100))"
assert 1 "(def main() -> int (> 10 2 1 0))"
assert 0 "(def main() -> int (< 10 2 1 0))"
```

#### 60: add sra srl sll

```sh
assert 248 "(def main() -> int (sra (- 0 32) 2))"
assert 42 "(def main() -> int (- (sll 2 5) (srl 44 1)))"
assert 32 "(def main() -> int (sll 1 5))"
```

#### 61: support simple array literal
支持数据字面量。相当于 C 的 {1, 2, 3}。

```sh
assert 42 "(def main() -> int
              (let a :[2 int] #a[22 20])
              (let b :[1 int] #a[0])
              (+ (iget a 0) (iget a 1) (iget b 0)))"
```

#### 62: set support simple array literal
set 操作也支持字面量

```sh
assert 5 "(def main() -> int
             (let c :[2 2 int])
             (set (iget c 0) #a[1 2])
             (set (iget c 1) #a[3 4])
             (+ (iget (iget c 0) 1)
                (iget (iget c 1) 0)))"
```

#### 63: iget support left compose
相当甜的糖。
```sh
assert 5 "(def main() -> int
             (let c :[2 2 int])
             (set (iget c 0) #a[1 2])
             (set (iget c 1) #a[3 4])
             (+ (iget c 0 1)
                (iget c 1 0)))"
```

#### 64: refactor

#### 65: support compose literal
支持字面量嵌套。

```sh
assert 5 "(def main() -> int
             (let c :[5 5 int] 
              #a[#a[1 0 0 0 0]
                 #a[0 1 0 0 0]
                 #a[0 0 1 0 0]
                 #a[0 0 0 1 0]
                 #a[0 0 0 0 1]])
             (+ (iget c 0 0)
                (iget c 1 1)
                (iget c 2 2)
                (iget c 3 3)
                (iget c 4 4)))"
```

#### 66: support string literal

```sh
assert 97 '(def main() -> int
            (let a :[5 char] "abcd")
            (iget a 0))'
```


#### 67: support string compose literal; it's time to test in C

支持字符串和数据字面量组合。

```sh
assert 147 '(def main() -> int
            (let a :[2 5 char] #a("ab" "cd"))
            (let b :[2 5 char] #a("ef" "gh"))
            (+ (iget a 0 0) (iget a 1 1)
               (iget b 0 1) (iget b 1 1)
               ))'
```

开始着手把测试搬到 C 上，chibicc 很早就实现了，我拖着是因为我解决不了宏的问题。现在，要解决它了！

#### 68: test0: simple test glue
由于没有宏，写一个测试是如此繁琐。我必须把输入的表达式写两遍。

```sh
(def main() -> int
  (assert 0 0 "0")
  (assert 42 42 "42")
  (assert 42 (+ 20 22) "(+ 20 22)")
  0
)
```

#### 69: macro system start

经过一周的思考和实验，宏系统有了雏形。这次提交代码增量为 473+，102-。

宏系统的输入的 S 表达式，输出为 Node。作用在 lexer 和 parser 之间。

```lisp
(defmacro ASSERT (actual expected)
  (assert actual expected (str expected)))

(def main() -> int
  (ASSERT 0 0)
  (ASSERT 42 42)
  0
)
```

宏系统包含了一个 S-表达式的 parser，以及一个小型的类 LISP 解释器。恐繁不述。

从第 70 到第 99 次提交，都是逐渐拓展宏系统，使之支持我们上述所涉及的所有的表达式。

解释器方式实现的宏系统比 C 语言的字符器替换的宏系统更加灵活，不易出错。不过我只实现了一个宏 str 就是了。

#### 100: merge macro.c and parse.c

第 100 个提交，将原来的 parser 完全抛弃，不管使用不使用宏，都是先 lexer 转 S-表达式，再转到 Node。

这次提交的代码增量为: 725 additions and 1,372 deletions.

我也不知道为什么刚好提交的次数是 100。