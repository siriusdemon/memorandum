#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./manda "$input" > tmp.s || exit
  gcc -static -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 0
assert 42 42
assert 42 "(+ 20 22)"
assert 42 "(- 50 8)"
assert 42 "[+ 20 22]"
assert 42 "[- 50 8]"
assert 42 "(* 6 7)"
assert 42 "(/ 84 2)"
assert 42 "(* (/ 42 (+ 1 6)) (- 9 2))"
assert 42 "(* 2 3 7)"
assert 42 "(+ 1 2 3 4 5 6 7 (* 1 2 7))"
assert 3 "1 2 3"
assert 42 "(+ 1 2 3) (* 6 7)"
assert 42 "(let a 42) a"
assert 32 "(let a 42) (let b 10) (- a b)"
assert 1 "(> 2 1)"
assert 0 "(< 2 1)"
assert 0 "(<= 2 1)"
assert 1 "(>= 2 1)"
assert 1 "(= 1 1)"
assert 1 "(if (< 1 2) 1 2)"
assert 2 "(if (> 1 2) 1 2)"
assert 42 "(if (> 1 2) 1 (* 6 7))"
assert 42 "(let i 1) (set i (* 6 7)) i"
assert 10 "(let i 1) (while (< i 10) (set i (+ i 1))) i"
assert 42 "(let i 42) (let b i) b"
assert 42 "(let i 42) (let b &i) b.*"
assert 42 "(let i 42) (let b &i) (let c &b) c.*.*"

echo OK
