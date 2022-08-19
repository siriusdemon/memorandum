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

echo OK
