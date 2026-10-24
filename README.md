# lemon_cc

照 Nora Sandler《Writing a C Compiler》一章一章用 C 寫的 C 編譯器。

## 建置與使用

```sh
make
./main file.c        # 在 file.c 旁產生執行檔 file
./main -S file.c     # 只輸出組語 file.s
./main --lex|--parse|--validate|--tacky|--codegen file.c   # 只跑到某個階段
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
