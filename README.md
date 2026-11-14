# lemon_cc

照 Nora Sandler《Writing a C Compiler》一章一章用 C 寫的 C 編譯器。

目前支援到第十六章：三種 char、字元常數、相鄰字串、字串陣列初始化，以及靜態字串物件。實作與驗收命令見 [ch16 實作紀錄](docs/ch16-implementation.md)。

## 建置與使用

```sh
make
./main file.c        # 在 file.c 旁產生執行檔 file
./main -S file.c     # 只輸出組語 file.s
./main --lex|--parse|--validate|--tacky|--codegen file.c   # 只跑到某個階段
python3 tests/ch15_regressions.py ./main  # 第十五章補充回歸測試
python3 tests/ch16_regressions.py ./main  # 第十六章補充回歸與 GCC 呼叫互通測試
```

## 各章筆記

| 章節 | 筆記 |
|---|---|
| 第一章 | [A Minimal Compiler](docs/ch01-minimal-compiler.md) |
| 第二章 | [Unary Operators](docs/ch02-unary-operators.md) |
| 第三章 | [Binary Operators](docs/ch03-binary-operators.md) |
| 第四章 | [Logical and Relational Operators](docs/ch04-logical-relational-operators.md) |
| 第五章 | [Local Variables](docs/ch05-local-variables.md) |
| 第六章 | [If Statements and Conditional Expressions](docs/ch06-if-statements.md) |
| 第七章 | [Compound Statements](docs/ch07-compound-statements.md) |
| 第八章 | [Loops](docs/ch08-loops.md) |
| 第九章 | [Functions](docs/ch09-functions.md) |
| 第十章 | [File Scope Variable Declarations and Storage-Class Specifiers](docs/ch10-file-scope-and-storage-class.md) |
| 第十一章 | [Long Integers](docs/ch11-long-integers.md) |
| 第十二章 | [Unsigned Integers](docs/ch12-unsigned-integers.md) |
| 第十三章 | [Floating-Point Numbers](docs/ch13-floating-point-numbers.md) |
| 第十四章 | [Pointers](docs/ch14-pointers.md) |
| 第十五章 | [Arrays and Pointer Arithmetic](docs/ch15-arrays-and-pointer-arithmetic.md) |
| 第十六章 | [Characters and Strings](docs/ch16-characters-and-strings.md) |
| 第十七章 | [Supporting Dynamic Memory Allocation](docs/ch17-supporting-dynamic-memory-allocation.md) |
| 附錄 A | [Debugging Assembly Code with GDB or LLDB](docs/appA-debugging-with-gdb-lldb.md) |
