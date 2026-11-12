#!/bin/sh
# Strip MSVC EH/cookie boilerplate from Ghidra output for easier reading
grep -v -E 'ExceptionList|puStack_c|^\s*void \*local_10;|undefined1 \*puStack|__security_cookie|local_8 = 0xffffffff;|local_8\._0_1_ = |local_8 = \(uint\)local_8\._1_3_|local_8 = CONCAT31|^\s*(undefined4|int|uint) (local_8|uStack_8);|WARNING: Function: __security_check|^\s*local_8 = [0-9]+;' "$@" | cat -s
