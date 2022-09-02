#!/bin/bash
cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}
EOF

assert() {
  expected="$1"
  input="$2"

  echo "$input" | ./manda -o tmp.s - || exit
  gcc -static -o tmp tmp.s tmp2.o
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 5 "(def main() -> int
             (let c :[2 2 int])
             (set (iget c 0) #a[1 2])
             (set (iget c 1) #a[3 4])
             (+ (iget (iget c 0) 1)
                (iget (iget c 1) 0)))"
assert 42 "(def main() -> int
              (let a :[2 int] #a[22 20])
              (let b :[1 int] #a[0])
              (+ (iget a 0) (iget a 1) (iget b 0)))"
assert 248 "(def main() -> int (sra (- 0 32) 2))"
assert 42 "(def main() -> int (- (sll 2 5) (srl 44 1)))"
assert 32 "(def main() -> int (sll 1 5))"
assert 1 "(def main() -> int 
            (let age :int 20)
            (< 0 age 100))"
assert 1 "(def main() -> int (> 10 2 1 0))"
assert 0 "(def main() -> int (< 10 2 1 0))"
assert 0 "(def main() -> int (and true false true))"
assert 1 "(def main() -> int (or false false true))"
assert 4 "(def main() -> int (bitxor #b010 #b110))"
assert 0 "(def main() -> int (bitand #b1111 #b111 0))"
assert 11 "(def main() -> int (bitor #b1010 #b0010 #b10 #b1))"
assert 1 "(def main() -> int (bitand #b1111 #b111 #b11 #b1))"
assert 0 "(def main() -> int (mod 18 6))"
assert 5 "(def main() -> int (mod 17 6))"
assert 1 "(def main() -> int (mod 3 2))"
assert 250 "(def main() -> int (bitnot #b101))"
assert 0 "(def main() -> int (not true))"
# 42 = 16 + 15 + 8 + 3
assert 42 "(def main() -> int (+ #x10 #o17 #b1000 #b11))"
assert 4 "(def main() -> int #b100)"
assert 97 "(def main() -> int (let x :char 'a') x)"
assert 2 "(def main() -> int (let x :bool true) (+ (sizeof bool) (cast x bool)))"
assert 0 "(def main() -> int (let x :bool false) (cast x bool))"
assert 0 "(def main() -> int 
            (let x :int 256) 
            (cast x char))"
assert 1 "(def main() -> int 8590066177)"
                
assert 36 "(def main() -> int
            (defstruct X gender int age int)
            (let x :X)
            (let y :(typeof x))
            (set y.age 20)
            (+ (sizeof X) (sizeof (typeof y)) y.age))"

assert 20 "(def main() -> int (* 5 (sizeof (typeof 1))))"

assert 28 "(def main() -> int
            (defstruct Man gender int age int)
            (deftype X Man)
            (let c :X)
            (set c.age 20)
            (+ (sizeof X) c.age))"

assert 12 "(def main() -> int 
            (deftype X short) 
            (let X :X 10)
            (+ X (sizeof X)))"
assert 2 "(def main() -> int (deftype MyType short) (sizeof MyType))"
assert 2 "(def main() -> int (sizeof short))"
assert 15 "(def main() -> int 
             (+ (sizeof short)
                (sizeof int) 
                (sizeof char)
                (sizeof long)))"

assert 217 "(def main() -> long
                (defstruct Life money long health int)
                (let my-life :Life)
                (set my-life.money (* 10 20))
                (set my-life.health 1)
                (+ my-life.money my-life.health (sizeof Life))
            )"

assert 200 "(def main() -> long
            (let a :long (* 10 20))
            200)"
assert 8 "(def main() -> int (sizeof long))"

assert 26 "(defstruct S A int B int C int D int)
           (def main() -> int
             (let S :S)
             (set S.A 10)
             (+ (sizeof S) S.A))"

assert 101 "(defstruct S2 age int gender int)
           (defunion U2 X int Y S2)
           (def main() -> int
             (let a :U2) 
             (set a.X 1)
             (set a.Y.gender 100)
             (let b :U2 a)
             (+ b.Y.age b.Y.gender))"

assert 184 "(defstruct Color R int G int B int)
            (def main() -> int
              (let a : Color) 
              (set a.R 100)
              (set a.G 42)
              (set a.B 42)
              (let b : Color a)
              (+ b.R b.G b.B))"

assert 100 "(defstruct S2 age int gender int)
           (defunion U2 X int Y S2)
           (def main() -> int
             (let a: U2) 
             (set a.X 100)
             a.Y.age)"

assert 10 "(defstruct S1 age int)
           (defunion U1 gender int X S1)
           (def main() -> int
              (let a: U1) 
              (set a.X.age 10)
              a.gender)"

assert 1 "(defstruct Man age int)
          (def main() -> int
            (let a :Man)
            (set a.age 10)
            (defstruct Man gender int)
            (let b: Man)
            (set b.gender 1)
            b.gender)"
assert 1 "(def main() -> int
            (defstruct Man gender int)
            1)"
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
assert 3 "(def main() -> int (let a :int (do 1 2 3)) a)"
assert 3 "(def main() -> int (do 1 2 3))"
assert 42 "(defstruct My gender int age int)
           (def main() -> int 42)"
assert 12 "(defstruct My gender int age int)
           (def main() -> int
            (let a :My) 
            (set a.gender 1)
            (set a.age 12)
            a.age)"
assert 0 "(def main() -> int 0)"
assert 42 "(def main() -> int 42)"
assert 42 "(def main() -> int (+ 20 22))"
assert 42 "(def main() -> int (- 50 8))"
assert 42 "(def main() -> int [+ 20 22])"
assert 42 "(def main() -> int [- 50 8])"
assert 42 "(def main() -> int (* 6 7))"
assert 42 "(def main() -> int (/ 84 2))"
assert 42 "(def main() -> int (* (/ 42 (+ 1 6)) (- 9 2)))"
assert 42 "(def main() -> int (* 2 3 7))"
assert 42 "(def main() -> int (+ 1 2 3 4 5 6 7 (* 1 2 7)))"
assert 3 "(def main() -> int 1 2 3)"
assert 42 "(def main() -> int (+ 1 2 3) (* 6 7))"
assert 42 "(def main() -> int (let a :int 42) a)"
assert 32 "(def main() -> int (let a :int 42) (let b :int 10) (- a b))"
assert 1 "(def main() -> int (> 2 1))"
assert 0 "(def main() -> int (< 2 1))"
assert 0 "(def main() -> int (<= 2 1))"
assert 1 "(def main() -> int (>= 2 1))"
assert 1 "(def main() -> int (= 1 1))"
assert 1 "(def main() -> int (if (< 1 2) 1 2))"
assert 2 "(def main() -> int (if (> 1 2) 1 2))"
assert 42 "(def main() -> int (if (> 1 2) 1 (* 6 7)))"
assert 42 "(def main() -> int (let i :int 1) (set i (* 6 7)) i)"
assert 10 "(def main() -> int (let i :int 1) (while (< i 10) (set i (+ i 1))) i)"
assert 42 "(def main() -> int (let i :int 42) (let b :int i) b)"
assert 42 "(def main() -> int (let i :int 42) (let b :*int &i) b.*)"
assert 42 "(def main() -> int (let i :int 42) (let b :*int &i) (let c :**int &b) c.*.*)"
assert 42 "(def main() -> int (let i :int 42) (let b :*int (addr i)) (deref b))"
assert 3 "(def main() -> int (ret3))"
assert 5 "(def main() -> int (ret3) (ret5))"
assert 136 "(def main() -> int (add6 1 2 3 4 5 (add6 6 7 8 9 10  (add6 11 12 13 14 15 16))))"
assert 21 "(def main() -> int (add6 1 2 (ret3) 4 (ret5) 6))"
assert 42 "(def main() -> int (ret42)) (def ret42() -> int 42)"
assert 42 "(def main() -> int (add 20 22)) (def add(a int b int) -> int (+ a b))"
assert 11 "(def main() -> int (cc 1 2 3 4 5 6)) 
           (def cc(a int b int c int d int e int f int) -> int 
            (* a (- e d) (+ b c f)))"

assert 0 "(def main() -> int (let a: [32 int]) 0)"
assert 0 "(def main() -> int (let a: [32 *int]) 0)"
assert 0 "(def main() -> int (let a: *[32 int]) 0)"
assert 0 "(def main() -> int (let a :[32 int]) (let b :[32 int])  0)"
assert 0 "(def main() -> int 
            (let a :*[32 int]) 
            (let b :[32 *int])  
            (let c :*[32 *[32 int]])  
            0)"

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

assert 42 "(def main() -> int 
            (let a :[32 [32 int]]) 
            (iset (iget a 0) 0 42) 
            (iget (iget a 0) 0))"

assert 42 "(def main() -> int 
            (let a :[32 32 int]) 
            (iset (iget a 0) 0 42) 
            (iget (iget a 0) 0))"

assert 8 "(def main() -> int (sizeof *int))"
assert 40 "(def main() -> int (sizeof [5 *int]))"
assert 100 "(def main() -> int (sizeof [5 5 int]))"

assert 0 "(let g :int) (def main() -> int g)"
assert 42 "(let g :int) (def main() -> int (set g 42) g)"
assert 42 "(let g :int) (def main() -> int (let g :int 42) g)"

assert 1 "(def main() -> int (let c: char 1) c)"
assert 1 "(def main() -> int (sizeof char))"

assert 97 '(def main() -> int (iget "abc" 0))'
assert 98 '(def main() -> int (iget "abc" 1))'
assert 99 '(def main() -> int (iget "abc" 2))'
assert 0 '(def main() -> int (iget "abc" 3))'
assert 0 '(def main() -> int (iget "abc" 3))'

assert 7 '(def main() -> int (iget "\a" 0))'
assert 8 '(def main() -> int (iget "\b" 0))'
assert 9 '(def main() -> int (iget "\t" 0))'
assert 10 '(def main() -> int (iget "\n" 0))'
assert 11 '(def main() -> int (iget "\v" 0))'
assert 12 '(def main() -> int (iget "\f" 0))'
assert 13 '(def main() -> int (iget "\r" 0))'
assert 27 '(def main() -> int (iget "\e" 0))'

assert 42 "(def main() -> int 
            ; (let hahahh: NonExist)
            42);"

assert 42 ";;; 
            (def main() -> ;int 
            int
            ; (let hahahh: NonExist)
            42;;;;;;
            );"
            

echo OK
