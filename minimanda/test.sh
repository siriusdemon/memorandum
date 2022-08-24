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

  ./manda "$input" > tmp.s || exit
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

echo OK
