# 第二章：一元運算子（Unary Operators）

在第一章中，我們實作了一個只能回傳常數的編譯器。本章將帶領你跨出重要的一步：處理 **一元運算子**，包含 **負號 (`-`)** 與 **位元反轉 (`~`)**。更重要的是，本章引入了編譯器設計中極其關鍵的概念：**中間表示層 (Intermediate Representation, IR)**，並把 assembly generation 拆成三個小 pass。

*   `-`（negation）：取負。
*   `~`（bitwise complement）：每個 bit 反轉。在 two's complement 表示法下，效果等於「取負再減 1」，所以 `~(-2)` 是 `1`。

## 1. 詞法分析 (Lexer) 的擴充
本章需要識別三個新 Token：
*   `~` (Tilde)：位元反轉運算子。
*   `-` (Hyphen)：負號運算子。
*   `--` (Decrement)：遞減運算子（**注意**：雖然本章不實作功能，但 Lexer 必須能識別它，否則 `--2` 會被錯誤地解析為兩個負號 `-` `-` 2，而後者在 C 語言中是合法的，前者則不是）。編譯器應該拒絕還沒實作的功能，而不是把它編錯。

> **注意**：在實作 Lexer 時，應遵循「最長匹配原則」(Longest Match Rule)。當遇到 `-` 時，要先檢查下一個字元是否也是 `-`。反過來，`~~` 要切成兩個 `~`，因為 `~~2` 是合法的。

---

## 2. 語法分析 (Parser) 與 AST
我們需要擴充抽象語法樹 (AST) 的定義，使其具有遞迴結構，以支援嵌套表達式（例如：`-~-2`）。

### AST 定義 (ASDL)
```haskell
program = Program(function_definition)
function_definition = Function(identifier name, statement body)
statement = Return(exp)
exp = Constant(int) | Unary(unary_operator, exp)  -- 遞迴定義
unary_operator = Complement | Negate
```

### 文法 (EBNF)
```text
<program> ::= <function>
<function> ::= "int" <identifier> "(" "void" ")" "{" <statement> "}"
<statement> ::= "return" <exp> ";"
<exp> ::= <int> | <unop> <exp> | "(" <exp> ")"
<unop> ::= "-" | "~"
<identifier> ::= ? An identifier token ?
<int> ::= ? A constant token ?
```
*   括號運算式在 AST 中沒有對應的節點：`1`、`(1)`、`((((1))))` 都是 `Constant(1)`。
*   `--` 不在文法裡，parser 遇到 `--` token 必須報錯。

### 遞迴下降解析 (Recursive Descent)
解析表達式時，`parse_exp` 會先 peek（看但不取出）當前 Token，決定要用哪一條規則：
1. 如果是 `-` 或 `~`，則呼叫 `parse_unop` 並**遞迴**呼叫 `parse_exp`，回傳 `Unary(op, inner)`。
2. 如果是 `(`，則取出它、解析內部的表達式並 `expect(")")`，然後**直接回傳內部的表達式**，不另建節點。
3. 如果是常數，則回傳 `Constant` 節點。
4. 都不是，就報 `Malformed expression` 的 syntax error。

（lemon_cc 從第三章起把這段邏輯放在 `parse_factor`。）

---

## 3. 中間表示層：TACKY IR
這是本章的精華。直接從 AST 生成組語不再像第一章那麼直接，因為組語指令是平坦的（Flat），而表達式是嵌套的。因此，我們引入了 **TACKY**（一種三位址碼 IR，three-address code）。三位址碼的每條指令最多用到三個值（兩個來源、一個目的），運算元只能是常數或變數，不能是巢狀運算式。

### 為什麼需要 TACKY？
*   **拆解複雜運算**：將 `-~2` 拆解為多個簡單步驟。
*   **引入臨時變數**：用來儲存中間計算結果。
*   **分開處理**：「拆掉巢狀運算式」這種結構轉換在 TACKY 完成，「哪些運算元對哪些指令合法」這類組語細節留給後面的 pass，每個 pass 都比較小、比較簡單。
*   **優化基礎**：Part III 的優化（如常數摺疊、死碼刪除）都會在這一層進行。

### TACKY 定義 (ASDL)
```text
program = Program(function_definition)
function_definition = Function(identifier, instruction* body)
instruction = Return(val) | Unary(unary_operator, val src, val dst)
val = Constant(int) | Var(identifier)
unary_operator = Complement | Negate
```
*   function body 是一串指令，不再是單一 statement。
*   ASDL 沒寫出來的規則：`Unary` 的 `dst` 一定是 `Var`，不能是 `Constant`。

### TACKY 的結構
在 TACKY 中，`Unary` 運算由 `來源 (src)` 和 `目的 (dst)` 組成。
*   **例子**：`return ~(-2);`
    ```text
    tmp.0 = Negate(2)
    tmp.1 = Complement(tmp.0)
    Return(tmp.1)
    ```

### 產生 TACKY
```text
emit_tacky(e, instructions):
    Constant(c)      -> 回傳 TACKY 的 Constant(c)，不產生指令
    Unary(op, inner) -> src = emit_tacky(inner, instructions)
                        dst = Var(make_temporary())
                        instructions.append(Unary(convert_unop(op), src, dst))
                        回傳 dst
```
*   從最內層的運算式往外產生，每個 `Unary` 的結果存進新的暫存變數。
*   `emit_tacky` 只處理運算式。`return` statement 要另外處理：先呼叫 `emit_tacky` 算出回傳值，再加上 `Return(val)`。
*   **暫存變數命名**：用全域計數器產生 `tmp.0`、`tmp.1`…。C 的 identifier 不能含 `.`，所以不會和使用者取的名字衝突。之後所有名字會放進同一張 table，必須全域唯一（lemon_cc：`make_temporary`）。
*   Driver 要新增 `--tacky` 選項：跑到 TACKY generation 為止，不產生輸出。

---

## 4. 彙編語言中的堆疊 (The Stack)
在真正的 x64 彙編中，臨時變數不能無限量存在。目前的策略是將所有變數存放在 **堆疊 (Stack)** 上。`return ~(-2);` 最後產生的組語：

```asm
    .globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $8, %rsp
    movl    $2, -4(%rbp)
    negl    -4(%rbp)
    movl    -4(%rbp), %r10d
    movl    %r10d, -8(%rbp)
    notl    -8(%rbp)
    movl    -8(%rbp), %eax
    movq    %rbp, %rsp
    popq    %rbp
    ret
```

### 關鍵暫存器
*   **RSP (Stack Pointer)**：指向堆疊頂部（目前使用的最低位址）。
*   **RBP (Base/Frame Pointer)**：指向當前函數幀 (Stack Frame) 的底部。

### 堆疊的運作
*   堆疊往低位址長。`push` 先把值寫到 `RSP - 8`，再把 RSP 減 8；`pop` 把頂端的值讀出來，再把 RSP 加 8。
*   `push` / `pop` 一律搬 8 bytes。4 bytes 的 `int` 要用 `movl` 寫進已經配置好的堆疊空間（例如 `movl $2, -4(%rbp)`）。
*   局部資料用相對 RBP 的位址存取，不需要事先知道絕對位址。frame 裡的位址都比 RBP 低，所以 offset 是負的（`-4(%rbp)`）。

### 函數序言與結尾 (Prologue & Epilogue)
每個函數開始時，必須設定它的 Stack Frame：
```asm
pushq %rbp          # 保存呼叫者的 RBP
movq %rsp, %rbp     # 設定新的 RBP
subq $n, %rsp       # 替暫存變數分配 n bytes（上面的例子是 8）
```
結束時需還原：
```asm
movq %rbp, %rsp     # 釋放空間
popq %rbp           # 還原呼叫者的 RBP
ret
```
在 assembly AST 裡，只有 `subq` 用 `AllocateStack(n)` 表示；`pushq` / `movq` 在輸出 `Function` 時印出，epilogue 在輸出 `Ret` 時印出。

---

## 5. 組語生成流程 (Assembly Generation)
將 TACKY 轉換為組語分為三個子階段。先擴充 assembly AST：

```text
program = Program(function_definition)
function_definition = Function(identifier name, instruction* instructions)
instruction = Mov(operand src, operand dst)
            | Unary(unary_operator, operand)
            | AllocateStack(int)
            | Ret
unary_operator = Neg | Not
operand = Imm(int) | Reg(reg) | Pseudo(identifier) | Stack(int)
reg = AX | R10
```
*   `Unary` 只有一個運算元，同時當來源和目的。
*   `Stack(-4)` 代表 `-4(%rbp)`。
*   `reg` 的名稱和大小無關：`AX` 依情況可以是 RAX、EAX 或 AL，`R10` 可以是 R10、R10D 或 R10B，輸出時才決定。

### A. 轉換為虛擬組語 (Converting TACKY to Assembly)
把每條 TACKY 指令轉成一小段組語指令，但此時仍使用「虛擬暫存器」（pseudoregister，如 `tmp.0`）。因為 `neg` / `not` 的來源和目的是同一個運算元，要先把 src 複製到 dst。

| TACKY | Assembly |
| :--- | :--- |
| `Return(val)` | `Mov(val, Reg(AX))`、`Ret` |
| `Unary(op, src, dst)` | `Mov(src, dst)`、`Unary(op, dst)` |
| `Complement` / `Negate` | `Not` / `Neg` |
| `Constant(int)` / `Var(identifier)` | `Imm(int)` / `Pseudo(identifier)` |

*   `tmp.0 = Negate(2)` 變換為 `Mov(Imm(2), Pseudo("tmp.0"))`、`Unary(Neg, Pseudo("tmp.0"))`，寫成組語的樣子是：
    ```text
    movl $2, %tmp.0
    negl %tmp.0
    ```
*   這一步還不會用到 `AllocateStack`、`Stack` 和 R10。

### B. 虛擬暫存器替換 (Replacing Pseudoregisters)
將每個虛擬暫存器對應到堆疊上的位址：第一個遇到的換成 `Stack(-4)`，下一個換成 `Stack(-8)`，依此類推（每個暫存變數都是 4 bytes 的 int）。
*   用一個 map 記住 identifier → offset，同一個 pseudoregister 每次出現都換成同一個位址。
*   這個 pass 要回傳最後一個暫存變數的 offset，下一步靠它決定要配置多少堆疊空間。
*   目前每個 pseudoregister 都放在記憶體；Part III 才會寫真正的 register allocator，盡量改放進暫存器。
*   lemon_cc：`replace_pseudos`。

### C. 指令修復 (Instruction Fix-up)
這個 pass 做兩件事：
1.  在指令串列的最前面插入 `AllocateStack(n)`，`n` 是上一步最後的 offset。例如最後一個暫存變數換成 `-12(%rbp)`，就插入 `AllocateStack(12)`。
2.  修正不合法的 `Mov`。**這非常重要！** x86-64 有一個限制：**`mov`（以及許多其他指令）不能同時有兩個記憶體運算元**。只要運算式有一層以上的巢狀，就會產生來源和目的都是 `Stack` 的 `Mov`。
    *   **錯誤的組語**：`movl -4(%rbp), -8(%rbp)`（組譯器會拒絕）
    *   **修復方法**：使用一個中間暫存器（`%r10d`）進行中轉：
        ```asm
        movl -4(%rbp), %r10d
        movl %r10d, -8(%rbp)
        ```

選 R10D 當 scratch register，是因為它沒有特殊用途。像 `idiv` 要求被除數放在 EAX，有些暫存器用來傳函式參數，拿它們中轉可能會蓋掉別的值。lemon_cc 的 fix-up 和配置堆疊在 `fix_and_allocate`。

---

## 6. Code Emission 與平台差異

| Assembly | 輸出 |
| :--- | :--- |
| `Function(name, instructions)` | `.globl <name>`、`<name>:`、`pushq %rbp`、`movq %rsp, %rbp`、`<instructions>` |
| `Ret` | `movq %rbp, %rsp`、`popq %rbp`、`ret` |
| `Mov(src, dst)` | `movl <src>, <dst>` |
| `Unary(Neg, operand)` / `Unary(Not, operand)` | `negl <operand>` / `notl <operand>` |
| `AllocateStack(int)` | `subq $<int>, %rsp` |
| `Reg(AX)` / `Reg(R10)` | `%eax` / `%r10d` |
| `Stack(int)` | `<int>(%rbp)` |
| `Imm(int)` | `$<int>` |

RBP 和 RSP 存的是 8 bytes 的記憶體位址，所以操作它們一律用 `q` 後綴的指令。

*   **macOS**：函數名稱前需要底線（如 `_main`）。
*   **Linux**：需要 `.section .note.GNU-stack,"",@progbits` 以確保堆疊不可執行（安全性標記）

---

## 7. 測試

```bash
./test_compiler /path/to/your_compiler --chapter 2 --stage lex
./test_compiler /path/to/your_compiler --chapter 2 --stage parse
./test_compiler /path/to/your_compiler --chapter 2 --stage tacky
./test_compiler /path/to/your_compiler --chapter 2 --stage codegen
./test_compiler /path/to/your_compiler --chapter 2
```
*   每個 stage 也會重跑第一章的測試。
*   本章的 invalid 測試都在 `tests/chapter_2/invalid_parse`：lex 階段要全部成功，parse 階段要拒絕它們。
*   TACKY 和 codegen 階段不會遇到 invalid 程式，因為 lexer 和 parser 已經先擋掉了。

---

## 總結 (Summary)

1.  **解析遞迴表達式**：利用遞迴下降法處理嵌套的一元運算。
2.  **引入 TACKY IR**：理解了為什麼三位址碼是編譯器中的橋樑。
3.  **管理堆疊空間**：學會了 RBP/RSP 的操作以及如何建立 Stack Frame。
4.  **解決硬體限制**：處理 x86 指令不能同時存取兩個記憶體位址的問題。

## 一句話總結

> 第二章在 AST 和組語之間插入 TACKY：先把巢狀運算式攤平成用暫存變數串起來的指令，再用三個小 pass（轉組語、換 pseudoregister、fix-up）把它變成合法的 x64 組語。

## 速記重點

```text
lexer    -> ~、-、--（最長匹配；~~ 是兩個 token）
parser   -> Unary(op, exp) 遞迴；括號不建節點；遇到 -- 報錯
TACKY    -> Unary(op, src, dst)、Return(val)、tmp.N 暫存變數
asm      -> Mov(src, dst) + Unary(op, dst)；先用 Pseudo 佔位
pseudo   -> 每個暫存變數 4 bytes：-4、-8、...
fix-up   -> 開頭插 AllocateStack(n)；記憶體到記憶體的 mov 經過 R10D
emit     -> prologue pushq/movq、Ret 前印 epilogue、negl/notl、subq $n, %rsp
```

## 參考資料

*   **[Two's Complement](https://www.cs.cornell.edu/~tomf/notes/cps104/twoscomp.html)** — *Thomas Finley*
    *   說明 two's complement 怎麼運作、為什麼這樣設計。
*   **The Elements of Computing Systems 第 2 章** — *Noam Nisan, Shimon Schocken*
    *   從硬體角度講同樣的內容，是 Nand to Tetris 的教材，可在 [nand2tetris.org/course](https://www.nand2tetris.org/course) 的「Project 2: Boolean Arithmetic」免費取得。
