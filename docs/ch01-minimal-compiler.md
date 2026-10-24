# 第一章：最小編譯器（A Minimal Compiler）

##  編譯器架構

編譯器並非直接進行翻譯，而是透過一系列的「階段 (Passes)」將程式碼逐步降級：

| 階段 | 輸出物 | 核心任務 |
| :--- | :--- | :--- |
| **Lexer (詞法分析)** | **Token List** | 將原始字串切成最小語法單位（單字）。 |
| **Parser (語法分析)** | **C AST** | 根據語法規則將 Token 組成一棵樹，代表程式邏輯。 |
| **CodeGen (彙編生成)** | **Asm AST** | 將抽象邏輯轉化為「指令」級別的數據結構（還不是文字）。 |
| **Emission (代碼輸出)** | **.s 檔案** | 將指令結構印成符合 AT&T 語法的文字檔案。 |
| **Driver (驅動程式)** | **可執行檔** | 不是 pass，而是包在外面的程式：先呼叫 `gcc -E -P` 前處理，跑以上四個階段產生 `.s`，再調用 `gcc` 組譯與連結。 |

本章的程式其實一個 pass 就能編完，先切成四個 pass 是為了之後每章擴充與新增階段。

---

## 讀懂最簡單的組語 (Hello, Assembly!)

用系統的 GCC 看 `return_2.c`（`int main(void) { return 2; }`）的組語：

```bash
gcc -S -O -fno-asynchronous-unwind-tables -fcf-protection=none return_2.c
```

`-S` 只輸出組語、不組譯不連結；`-O` 去掉無關的指令；後兩個選項關掉 unwind table 和 control-flow protection 產生的額外指令。結果：

```asm
    .globl main
main:
    movl    $2, %eax
    ret
```

*   `.globl main`：**assembler directive**（以 `.` 開頭）。宣告 `main` 是 global symbol，其他 object file 也能參照它；組譯器把這件事記在 object file 的 **symbol table**。
*   `main:`：**label**，標出 `main` 這個 symbol 指向的位址，也就是下一行 `movl` 在 text section 裡的 offset（這裡是 0）。
*   `movl $2, %eax`、`ret`：machine instruction，會出現在最終的可執行檔。

---

## 詞法分析 (Lexing)

### Token 定義
在第一章中，我們識別了以下四類 Token：
1.  **關鍵字**：`int`, `void`, `return`（規則：`int\b`、`void\b`、`return\b`）。
2.  **標識符**：函數名稱 `main`（規則：`[a-zA-Z_]\w*\b`，大小寫有別）。雖然目前只有 `main`，lexer 要能處理任意 identifier。
3.  **常量**：整數 `2`（規則：`[0-9]+\b`）。
4.  **符號**：`(`、`)`、`{`、`}`、`;`。

identifier 和 constant 的 token 要帶值（名稱、整數），其他 token 不用。沒有 whitespace token。

### 切 Token 的流程
*   輸入開頭是空白就跳過；否則在所有 regex 中找輸入開頭的**最長匹配**，轉成 token 後從輸入移除；完全沒有匹配就報錯。
*   **關鍵字當 identifier 處理**：先用 identifier 的規則找出 token 的結尾，再檢查它是不是關鍵字。
*   **不要用空白切字串**：`main(void)` 有四個 token，中間沒有空白。
*   只需要支援 ASCII。

### 單字邊界 (Word Boundary)
*   **問題**：Lexer 必須區分 `return2`（標識符）與 `return 2`（關鍵字 + 常量）。
*   **解決**：identifier、關鍵字、常量都必須結束在 word boundary（`\b`）。`123;bar` 的 `123` 是合法常量，因為 `;` 不屬於 `\w`；`123bar` 的開頭不符合任何 token 的規則，Lexer 必須報錯並以非零 exit code 退出（lemon_cc 用 `exit(1)`），這是測試套件中 `invalid_lex` 的檢查重點。

---

##  語法分析與 AST (Parsing)

### 抽象語法樹 (AST)
AST 省略了只用來標示結構邊界的 token（如分號、大括號、關鍵字 `int`），僅保留邏輯結構；保留每個 token 的叫 concrete syntax tree。第一章結構如下：
*   **Program** (根節點)
    *   **Function** (屬性：名稱 `main`)
        *   **Statement** (類型：`Return`)
            *   **Expression** (類型：`Constant`, 值：`2`)

### AST 定義 (ASDL)
```text
program = Program(function_definition)
function_definition = Function(identifier name, statement body)
statement = Return(exp)
exp = Constant(int)
```
*   ASDL 只是描述 AST 的記號，實際要用實作語言（lemon_cc 是 C 的 struct）寫出等價的資料結構。
*   `name`、`body` 是 field name，不影響結構；`identifier`、`int` 是 ASDL 內建型別。
*   之後的章節會用 `|` 分隔多個 constructor、`?` 表示可省略、`*` 表示 list，例如 `statement = Return(exp) | If(exp condition, statement then, statement? else)`。

### 文法 (EBNF)
```text
<program> ::= <function>
<function> ::= "int" <identifier> "(" "void" ")" "{" <statement> "}"
<statement> ::= "return" <exp> ";"
<exp> ::= <int>
<identifier> ::= ? An identifier token ?
<int> ::= ? A constant token ?
```
*   `<...>` 是 non-terminal，引號裡是 terminal（固定 token），`? ... ?` 是用文字描述的 token，`[ ]` 表示可省略。
*   文法和 ASDL 的結構一樣，但多寫了每個節點由哪些 token 組成，parser 靠這些資訊判斷一個節點從哪裡開始、到哪裡結束。

### 遞歸下降解析 (Recursive Descent)
我們為語法中的每個「非終結符 (Non-terminal)」寫一個函數，回傳對應的 AST 節點。
*   `parse_program()` 呼叫 `parse_function()`。
*   `parse_function()` 負責處理 `int`、名稱、`(void)`、大括號，並呼叫 `parse_statement()`。
*   **Expect 機制**：取出下一個 Token，若不是預期的固定 token（例如 `return 2` 後面沒有 `;`），直接報 syntax error。
*   整個 program 解析完後還有剩下的 token，也是 syntax error。
*   之後同一個 symbol 會有多條 production rule，parser 先 peek 幾個 token 再決定用哪一條，這叫 predictive parser。
*   建議寫一個 AST pretty-printer，錯誤訊息也要具體（例如 `Expected ";" but found "return"`），除錯會輕鬆很多。

---

##  x64 彙編生成 (Code Generation)

### 暫存器與呼叫約定 (System V ABI)
*   **EAX 暫存器**：依照平台的 calling convention，函數的 **32 位元整數返回值** 放在 `EAX` 暫存器中（64 位元的值放 `RAX`）。caller 也知道這個約定，所以會從 EAX 取回返回值。
*   **指令字尾**：我們使用 `movl`（Move Long），其中 `l` 代表 longword，也就是 32 位元（4 Bytes）；`movq` 的 `q` 是 quadword（64 位元）。注意 x64 組語的 long 是 32 位元，和多數 C 實作不同。

### Assembly AST (ASDL)
先把組語存成資料結構，不直接寫檔，之後的章節才能再改寫它：
```text
program = Program(function_definition)
function_definition = Function(identifier name, instruction* instructions)
instruction = Mov(operand src, operand dst) | Ret
operand = Imm(int) | Register
```
本章只會用到 EAX，所以 `Register` 不必指定是哪一個。

### 彙編指令映射
C 語言的 `return <exp>;` 在彙編中被拆解為兩個動作：
1.  將值搬移到特定暫存器：`movl $value, %eax`
2.  通知 CPU 返回調用者：`ret`

| AST 節點 | Assembly |
| :--- | :--- |
| `Program(function_definition)` | `Program(function_definition)` |
| `Function(name, body)` | `Function(name, instructions)` |
| `Return(exp)` | `Mov(exp, Register)`、`Ret` |
| `Constant(int)` | `Imm(int)` |

這個轉換只在 expression 能用單一 operand 表示時成立。下一章加入一元運算子後，就要先用多條指令算出 expression，再搬進 EAX。

---

##  代碼輸出與系統安全 (Emission)

### AT&T 語法規範
*   **立即數**：必須加 `$` 前綴，例如 `$2`。
*   **暫存器**：必須加 `%` 前綴，例如 `%eax`。
*   **運算元順序**：`src, dst`（Intel 語法的順序相反）。
*   **排版**：指令之間要換行。書上建議除了標籤 (Label) 以外每行都縮排，方便閱讀；`#` 之後到行尾是註解。

### 輸出格式

| Assembly | 輸出 |
| :--- | :--- |
| `Program(function_definition)` | 印出 function；Linux 在檔案末尾加 `.section .note.GNU-stack,"",@progbits` |
| `Function(name, instructions)` | `.globl <name>`、`<name>:`、`<instructions>` |
| `Mov(src, dst)` | `movl <src>, <dst>` |
| `Ret` | `ret` |
| `Register` | `%eax` |
| `Imm(int)` | `$<int>` |

macOS 上函數名稱前面要加底線（`_main`），Linux 不加。

### 安全強化 (Security Hardening)
在 Linux 上，於檔案末尾添加：
`.section .note.GNU-stack,"",@progbits`
這行表示：**「這段程式碼不需要可執行的堆疊 (executable stack)」**。讓堆疊不可執行是防禦某些攻擊的基本手段，但少數使用非標準擴充的程式真的需要可執行堆疊，所以這項防護不能對每個程式都開啟；加上這行，這項防護才能啟用。本書產生的程式碼都不需要可執行堆疊，所以一律輸出這行。

---

## 驅動程序與鏈接 (Driver & Linking)

### Driver 的三個步驟
1.  **前處理**：`gcc -E -P INPUT_FILE -o PREPROCESSED_FILE`。`-E` 只跑前處理器；`-P` 不輸出 linemarker（lexer 和 parser 處理不了）。慣例副檔名是 `.i`。
2.  **編譯**：把 `.i` 編成 `.s`，然後刪掉 `.i`。
3.  **組譯與連結**：`gcc ASSEMBLY_FILE -o OUTPUT_FILE`，然後刪掉 `.s`。

> lemon_cc 目前沒有呼叫 `gcc -E`，而是直接讀原始檔，lexer 遇到 `#` 就略過到行尾。

### 命令列介面
`test_compiler` 依賴這個介面：
*   唯一的參數是 C 原始檔路徑。成功時在同一個目錄產生同名（去掉副檔名）的可執行檔，exit code 為 0。
*   失敗時回傳非零 exit code，而且**不寫出任何組語或可執行檔**，測試靠這點確認 invalid 程式被拒絕。
*   `--lex`、`--parse`、`--codegen`：分別跑到 lexer、parser、assembly generation 為止，都不產生輸出檔。
*   `-S`（選用）：只輸出組語，不組譯也不連結。Part III 的測試會用到。

### 為什麼需要 GCC 鏈接？
你編寫的 `.s` 檔案只是純文字，CPU 無法直接執行。驅動程式調用 `gcc` 的目的是：
1.  **Assembler**：將 `.s` (文字) 轉為 `.o` (二進位機器碼)。
2.  **Linker**：透過 symbol resolution 把 symbol table 中每個 symbol 對應到記憶體位址，再做 relocation，把所有用到 symbol 的地方改成實際位址。Linker 也會加上 **crt0**（C Runtime）這段包裝程式碼。crt0 才是程式真正的起點：它呼叫你的 `main`（所以 `main` 必須是 `.globl`）、取回返回值，再用 `exit` system call 把返回值變成行程的 exit code。

### 驗證編譯結果
`gcc` 會把副檔名為 `.s` 的輸入當成組語，直接組譯與連結。測試腳本會檢查程式執行後的 **退出碼 (Exit Code)**。
在 Shell 中可以使用 `echo $?` 查看：
```bash
gcc return_2.s -o return_2
./return_2
echo $?  # 應該輸出你在 C 程式中 return 的那個數字
```


## 測試檔案
`return_2.c`（Listing 1-1）

writing-a-c-compiler-tests 的測資，每個階段都可以單獨測：
```bash
./test_compiler /path/to/your_compiler --chapter 1 --stage lex      # invalid_lex 要失敗，其餘要成功
./test_compiler /path/to/your_compiler --chapter 1 --stage parse    # invalid_parse 要失敗，valid 要成功
./test_compiler /path/to/your_compiler --chapter 1 --stage codegen  # valid 不能讓 codegen 崩潰
./test_compiler /path/to/your_compiler --chapter 1                  # 編譯 valid、執行並比對 exit code
```
這些測試只看成功或失敗，不檢查產生的 token 或 AST 是否正確，需要的話要自己補測試。

## 一句話總結

> 第一章讓 `return 2;` 走完 lexer、parser、assembly generation、code emission 四個 pass，再由 driver 交給 `gcc` 組譯連結，搭好之後每一章都要擴充的骨架。

## 速記重點

```text
lexer    -> 最長匹配、\b word boundary、關鍵字先當 identifier 再查表
parser   -> ASDL 定 AST、EBNF 定文法、每個 non-terminal 一個函式、expect
codegen  -> Return(exp) => Mov(exp, Register) + Ret
emit     -> .globl main、main:、movl $2, %eax、ret；Linux 加 .note.GNU-stack
driver   -> gcc -E -P、編譯、gcc x.s -o x；失敗回非零且不留檔
```

## 參考資料
### 連結器 (Linkers)

*   **[Beginner’s Guide to Linkers](https://www.lurklurk.org/linkers/linkers.html)** — *David Drysdale*
    *   這是一個很好的入門起點。
*   **Ian Lance Taylor 的連結器系列文章 (共 20 部分)**
    *   這系列文章深入探討了許多細節。
    *   [第一篇文章](https://www.airs.com/blog/archives/38)
    *   [文章目錄 (Table of Contents)](https://lwn.net/Articles/276782/)
*   **[Position Independent Code (PIC) in Shared Libraries](https://eli.thegreenplace.net/2011/11/03/position-independent-code-pic-in-shared-libraries)** — *Eli Bendersky*
    *   概述了編譯器、連結器和彙編器如何協作生成「位置無關代碼」，重點關注 32 位元機器。
*   **[Position Independent Code (PIC) in Shared Libraries on x64](https://eli.thegreenplace.net/2011/11/11/position-independent-code-pic-in-shared-libraries-on-x64)** — *Eli Bendersky*
    *   承接前文，重點關注 64 位元系統。

### 抽象語法樹定義 (AST Definitions)

*   **[Abstract Syntax Tree Implementation Idioms](https://hillside.net/plop/plop2003/Papers/Jones-ImplementingASTs.pdf)** — *Joel Jones*
    *   提供了如何在各種編程語言中實現 AST 的良好綜述。
*   **[The Zephyr Abstract Syntax Description Language](https://www.cs.princeton.edu/~appel/papers/asdl97.pdf)** — *Daniel Wang, Andrew Appel, Jeff Korn, and Christopher Serra*
    *   關於 ASDL 的原始論文。包含幾種不同語言的 AST 定義範例。

### 可執行堆疊 (Executable Stacks)

*   **[Executable Stack](https://www.airs.com/blog/archives/518)** — *Ian Lance Taylor*
    *   討論哪些程式需要可執行堆疊，並描述 Linux 系統如何判斷程式的堆疊是否應該具備可執行權限。
