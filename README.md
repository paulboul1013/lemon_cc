# lemon_cc

# 第一章: A Minial Compiler

##  編譯器架構

編譯器並非直接進行翻譯，而是透過一系列的「階段 (Passes)」將程式碼逐步降級：

| 階段 | 輸出物 | 核心任務 |
| :--- | :--- | :--- |
| **Lexer (詞法分析)** | **Token List** | 將原始字串切成最小語義單位（單字）。 |
| **Parser (語法分析)** | **C AST** | 根據語法規則將 Token 組成一棵樹，代表程式邏輯。 |
| **CodeGen (彙編生成)** | **Asm AST** | 將抽象邏輯轉化為「指令」級別的數據結構。 |
| **Emission (代碼輸出)** | **.s 檔案** | 將指令結構印成符合 AT&T 語法的文字檔案。 |
| **Driver (驅動程式)** | **可執行檔** | 負責串接以上階段，並調用系統的 `gcc` 進行連結。 |

---

## 詞法分析 (Lexing)

### Token 定義
在第一章中，我們識別了三類 Token：
1.  **關鍵字**：`int`, `void`, `return`。
2.  **標識符**：函數名稱 `main`（規則：`[a-zA-Z_]\w*\b`）。
3.  **常量**：整數 `2`（規則：`[0-9]+\b`）。
4.  **符號**：`(`、`)`、`{`、`}`、`;`。

### 單字邊界 (Word Boundary)
*   **問題**：Lexer 必須區分 `return2`（標識符）與 `return 2`（關鍵字 + 常量）。
*   **解決**：數字常量後不能緊接標識符字元。若發現 `123abc`，Lexer 必須報錯並以 `exit(1)` 退出，這是測試套件中 `invalid_lex` 的檢查重點。

---

##  語法分析與 AST (Parsing)

### 抽象語法樹 (AST)
AST 忽略了程式碼中的「語法糖」（如分號、大括號、關鍵字 `int`），僅保留邏輯結構。第一章結構如下：
*   **Program** (根節點)
    *   **Function** (屬性：名稱 `main`)
        *   **Statement** (類型：`Return`)
            *   **Expression** (類型：`Constant`, 值：`2`)

### 遞歸下降解析 (Recursive Descent)
我們為語法中的每個「非終結符 (Non-terminal)」寫一個函數。
*   `parse_program()` 呼叫 `parse_function()`。
*   `parse_function()` 負責處理 `int`、名稱、括號，並呼叫 `parse_statement()`。
*   **Expect 機制**：如果下一個 Token 不是預期的（例如 return 後面沒接數字），直接報錯退出。

---

##  x64 彙編生成 (Code Generation)

### 暫存器與呼叫約定 (System V ABI)
*   **EAX 暫存器**：在 x86-64 系統中，函數的 **32 位元整數返回值** 必須存放在 `EAX` 暫存器中。
*   **指令字尾**：我們使用 `movl`（Move Long），其中 `l` 代表 32 位元（4 Bytes）。

### 彙編指令映射
C 語言的 `return <exp>;` 在彙編中被拆解為兩個動作：
1.  將值搬移到特定暫存器：`movl $value, %eax`
2.  通知 CPU 返回調用者：`ret`

---

##  代碼輸出與系統安全 (Emission)

### AT&T 語法規範
*   **立即數**：必須加 `$` 前綴，例如 `$2`。
*   **暫存器**：必須加 `%` 前綴，例如 `%eax`。
*   **縮排**：指令必須縮排，標籤 (Label) 則靠左。

### 安全強化 (Security Hardening)
在檔案末尾添加：
`.section .note.GNU-stack,"",@progbits`
這告訴 Linux 內核：**「這個程式不需要執行堆棧上的代碼」**。若不加這行，現代作業系統可能會因為 NX (No-eXecute) 安全機制而拒絕執行你的程式。

---

## 驅動程序與鏈接 (Driver & Linking)

### 為什麼需要 GCC 鏈接？
你編寫的 `.s` 檔案只是純文字，CPU 無法直接執行。驅動程式調用 `gcc` 的目的是：
1.  **Assembler**：將 `.s` (文字) 轉為 `.o` (二進位機器碼)。
2.  **Linker**：將你的代碼與 C 標準庫 (CRT0, C Runtime) 連結。CRT0 才是程式真正的起點，它會負責呼叫你的 `main` 函數。

### 驗證編譯結果
測試腳本會檢查程式執行後的 **退出碼 (Exit Code)**。
在 Shell 中可以使用 `echo $?` 查看：
```bash
./your_compiled_program
echo $?  # 應該輸出你在 C 程式中 return 的那個數字
```


## 測試檔案
return2.c  
writing-a-c-compiler-tests的測資

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


# 第二章：一元運算子 (Unary Operators)

在第一章中，我們實作了一個只能回傳常數的編譯器。本章將帶領你跨出重要的一步：處理 **一元運算子**，包含 **負號 (`-`)** 與 **位元反轉 (`~`)**。更重要的是，本章引入了編譯器設計中極其關鍵的概念：**中間表示層 (Intermediate Representation, IR)**。

## 1. 詞法分析 (Lexer) 的擴充
本章需要識別三個新 Token：
*   `~` (Tilde)：位元反轉運算子。
*   `-` (Hyphen)：負號運算子。
*   `--` (Decrement)：遞減運算子（**注意**：雖然本章不實作功能，但 Lexer 必須能識別它，否則 `--2` 會被錯誤地解析為兩個負號 `-` `-` 2，而後者在 C 語言中是合法的，前者則不是）。

> **注意**：在實作 Lexer 時，應遵循「最長匹配原則」(Longest Match Rule)。當遇到 `-` 時，要先檢查下一個字元是否也是 `-`。

---

## 2. 語法分析 (Parser) 與 AST
我們需要擴充抽象語法樹 (AST) 的定義，使其具有遞迴結構，以支援嵌套表達式（例如：`-~-2`）。

### AST 定義 (ASDL)
```haskell
exp = Constant(int) | Unary(unary_operator, exp)  -- 遞迴定義

unary_operator = Complement | Negate
```

### 遞迴下降解析 (Recursive Descent)
解析表達式時，`parse_exp` 會檢查當前 Token：
1. 如果是 `-` 或 `~`，則呼叫 `parse_unop` 並**遞迴**呼叫 `parse_exp`。
2. 如果是 `(`，則解析內部的表達式並檢查 `)`。
3. 如果是常數，則回傳 `Constant` 節點。

---

## 3. 中間表示層：TACKY IR
這是本章的精華。直接從 AST 生成組語非常困難，因為組語指令通常是平坦的（Flat），而表達式是嵌套的。因此，我們引入了 **TACKY**（一種三位址碼 IR）。

### 為什麼需要 TACKY？
*   **拆解複雜運算**：將 `-~2` 拆解為多個簡單步驟。
*   **引入臨時變數**：用來儲存中間計算結果。
*   **優化基礎**：之後的常數摺疊或死碼刪除都會在這一層進行。

### TACKY 的結構
在 TACKY 中，一項運算由 `來源 (src)` 和 `目的 (dst)` 組成。
*   **例子**：`return ~(-2);`
    ```text
    tmp.0 = Negate(2)
    tmp.1 = Complement(tmp.0)
    Return(tmp.1)
    ```

---

## 4. 彙編語言中的堆疊 (The Stack)
在真正的 x64 彙編中，臨時變數不能無限量存在。目前的策略是將所有變數存放在 **堆疊 (Stack)** 上。

### 關鍵暫存器
*   **RSP (Stack Pointer)**：指向堆疊頂部（目前使用的最低位址）。
*   **RBP (Base/Frame Pointer)**：指向當前函數幀 (Stack Frame) 的底部。

### 函數序言與結尾 (Prologue & Epilogue)
每個函數開始時，必須設定它的 Stack Frame：
```nasm
pushq %rbp          ; 保存呼叫者的 RBP
movq %rsp, %rbp     ; 設定新的 RBP
subq $16, %rsp      ; 替局部變數分配空間 (16  bytes)
```
結束時需還原：
```nasm
movq %rbp, %rsp     ; 釋放空間
popq %rbp           ; 還原呼叫者的 RBP
ret
```

---

## 5. 組語生成流程 (Assembly Generation)
將 TACKY 轉換為組語分為三個子階段：

### A. 轉換為虛擬組語
將 TACKY 指令一對一對應到組語指令，但此時仍使用「虛擬暫存器」（如 `tmp.0`）。
*   `tmp.0 = Negate(2)` 變換為：
    ```text
    movl $2, %tmp.0
    negl %tmp.0
    ```

### B. 虛擬暫存器替換 (Register Allocation - Spilling)
將每個虛擬暫存器對應到堆疊上的位址。例如 `tmp.0` 對應到 `-4(%rbp)`。

### C. 指令修復 (Instruction Fix-up)
**這非常重要！** x86-64 有一個限制：**一條指令不能同時有兩個記憶體運算元**。
*   **錯誤的組語**：`movl -4(%rbp), -8(%rbp)` (會編譯失敗)
*   **修復方法**：使用一個中間暫存器（如 `%r10d`）進行中轉：
    ```nasm
    movl -4(%rbp), %r10d
    movl %r10d, -8(%rbp)
    ```

---

## 6. 平台差異：Code Emission
*   **macOS**：函數名稱前需要底線（如 `_main`），且位元反轉指令可能略有不同。
*   **Linux**：需要 `.section .note.GNU-stack,"",@progbits` 以確保堆疊不可執行（安全性標記）

---

## 總結 (Summary)

1.  **解析遞迴表達式**：利用遞迴下降法處理嵌套的一元運算。
2.  **引入 TACKY IR**：理解了為什麼三位址碼是編譯器中的橋樑。
3.  **管理堆疊空間**：學會了 RBP/RSP 的操作以及如何建立 Stack Frame。
4.  **解決硬體限制**：處理 x86 指令不能同時存取兩個記憶體位址的問題。


這是一份針對《Writing a C Compiler》第三章（第 87-108 頁）「二元運算子 (Binary Operators)」的詳細重點整理。我將其格式化為適合 HackMD/Markdown 閱讀的文章結構。

---

# 第三章 二元運算子與優先級爬升

##  概述 (Overview)
將為編譯器增加五種基礎的二元運算子支持：
- **算術運算**：加法 (`+`)、減法 (`-`)、乘法 (`*`)、除法 (`/`)、取餘數 (`%`)。
- **核心技術引進**：**優先級爬升算法 (Precedence Climbing)**。這是解析表達式最有效的方法之一，能解決遞迴下降算法在處理二元運算時遇到的「左遞迴」與「歧義性」問題。

---

##  詞法分析 (The Lexer)
需要新增四個 Token 類型：
1. `+` (加法)
2. `*` (乘法)
3. `/` (除法)
4. `%` (取餘數)

*注意：`-` 符號在第二章已添加。Lexer 階段不需要區分它是「一元負號」還是「二元減號」，這部分交由 Parser 處理。*

---

##  語法分析 (The Parser)

### 3.1 抽象語法樹 (AST) 的更新
表達式 (`exp`) 現在新增了一個 `Binary` 節點：
```rust
exp = Constant(int)
    | Unary(unary_operator, exp)
    | Binary(binary_operator, exp, exp) // 新增

binary_operator = Add | Subtract | Multiply | Divide | Remainder
```

###  遞迴下降法的困境
如果直接使用標準的 EBNF 語法（例如：`<exp> ::= <exp> <binop> <exp>`）：
1.  **左遞迴 (Left Recursion)**：解析器會陷入無限循環。
2.  **歧義性 (Ambiguity)**：無法確定 `1 + 2 * 3` 應該解析成 `(1 + 2) * 3` 還是 `1 + (2 * 3)`。

###  解決方案：優先級爬升 (Precedence Climbing)
這是本章的精華。每個運算子被賦予一個數值優先級：
- `*`, `/`, `%`：優先級 **50**
- `+`, `-`：優先級 **45**

**算法核心偽代碼：**
```python
parse_exp(tokens, min_prec):
    left = parse_factor(tokens) # 解析數字、一元運算或括號
    while next_token 是二元運算子且其優先級 >= min_prec:
        op = next_token
        # 如果是左結合，下一個遞迴的 min_prec = 當前優先級 + 1
        right = parse_exp(tokens, op.precedence + 1)
        left = Binary(op, left, right)
    return left
```

###  語法規則重構
為了配合算法，語法被拆分為：
- `<exp>`：處理二元運算。
- `<factor>`：處理最高優先級的項目（整數常數、一元運算子、括號表達式）。

---

##  TACKY 中介碼生成 (TACKY Generation)
TACKY 指令集更新，以支持二元運算：
- `Binary(binary_operator, val src1, val src2, val dst)`

**運算順序 (Evaluation Order)**：
根據 C 語言標準，二元運算子的操作數計算順序通常是**未定義的 (Unsequenced)**。在實作中，我們統一先計算左側，再計算右側。

---

##  組合語言生成 (Assembly Generation)

###  x64 算術指令
| 指令 | 用途 | 限制 / 備註 |
| --- | --- | --- |
| `addl src, dst` | `dst += src` | 不能同時兩個操作數都在內存 |
| `subl src, dst` | `dst -= src` | 注意順序：`subl a, b` 是 `b = b - a` |
| `imull src, dst` | `dst *= src` | **目標 (dst) 必須是寄存器**，不能是內存 |
| `idivl src` | 有符號除法 | 詳見下述 |

###  處理除法與取餘數 (`idivl`)
這是 x64 最機車的指令之一，需要特殊設置：
1.  **被除數準備**：必須佔用 64 位，由 `EDX:EAX` 聯合組成。
2.  **符號擴展 (Sign Extension)**：使用 `cdq` 指令將 `EAX` 的符號位擴展到 `EDX`。
3.  **執行指令**：`idivl <divisor>`。
4.  **結果獲取**：
    *   商 (Quotient) 存放在 `EAX`。
    *   餘數 (Remainder) 存放在 `EDX`。

---

##  指令修復與程式碼發射 (Fix-up & Emission)

###  無效指令修復 (Instruction Fix-up)
由於 x64 的硬體限制，我們必須在輸出組合語言前修正無效指令：
- **`addl` / `subl`**：如果源和目標都在內存，需先搬移至臨時寄存器 `R10D`。
- **`imull`**：如果目標在內存，必須先搬移至寄存器 `R11D` 計算，再搬回內存。
- **`idivl`**：操作數不能是立即值 (Immediate)。如果遇到 `idivl $3`，必須先搬到寄存器。

###  程式碼發射 (Code Emission)
更新後的發射格式：
- `movl <src>, <dst>`
- `addl <src>, <dst>`
- `subl <src>, <dst>`
- `imull <src>, <dst>`
- `cdq` (無操作數)
- `idivl <src>`

---

##  補充資源 (Additional Resources)

### 連結器 (Linkers)
- [Beginner’s Guide to Linkers](https://www.lurklurk.org/linkers/linkers.html) - David Drysdale
- [Ian Lance Taylor 的 20 篇連結器專題文章](https://www.airs.com/blog/archives/38)
- [Position Independent Code (PIC) in Shared Libraries](https://eli.thegreenplace.net/2011/11/03/position-independent-code-pic-in-shared-libraries) - Eli Bendersky (包含 x86 與 x64 版本)

### 抽象語法樹 (AST) 實作
- [Abstract Syntax Tree Implementation Idioms](https://hillside.net/plop/plop2003/Papers/Jones-ImplementingASTs.pdf) - Joel Jones
- [The Zephyr Abstract Syntax Description Language (ASDL)](https://www.cs.princeton.edu/~appel/papers/asdl97.pdf) - ASDL 原始論文

### 表達式解析與優先級爬升
- [Parsing Expressions by Precedence Climbing](https://eli.thegreenplace.net/2012/08/02/parsing-expressions-by-precedence-climbing) - Eli Bendersky (本書算法的主要參考)
- [Some Problems of Recursive Descent Parsers](https://eli.thegreenplace.net/2009/03/14/some-problems-of-recursive-descent-parsers) - Eli Bendersky
- [Pratt Parsing and Precedence Climbing Are the Same Algorithm](https://www.oilshell.org/blog/2016/11/01.html) - Andy Chu
- [Precedence Climbing Is Widely Used](https://www.oilshell.org/blog/2017/03/30.html) - Andy Chu

# 第四章 邏輯與關係運算符 (Logical and Relational Operators)

加入邏輯運算符（`!`, `&&`, `||`）與關係運算符（`<`, `>`, `<=`, `>=`, `==`, `!=`），並引入了編譯器設計中至關重要的概念：**短路邏輯 (Short-circuiting)**、**控制流 (Control Flow)** 以及 **彙編中的狀態標誌 (Flags)**。

## 1. 語言特性與詞法分析 (Lexing)
### 新增運算符
*   **邏輯運算符**：`!` (NOT), `&&` (AND), `||` (OR)。
*   **關係運算符**：`==`, `!=`, `<`, `>`, `<=`, `>=`。
*   **結果**：所有運算在真時返回 `1`，假時返回 `0`。

### 詞法分析細節
*   Lexer 必須處理多字元 Token，且遵循 **「最長匹配原則」**。
    *   例如：輸入 `<=` 應被解析為 `TOK_LTE`，而非 `<` 緊跟 `=`。
*   `&&` 和 `||` 必須完整匹配，單個 `&` 或 `|` 在目前的 C 子集中應視為非法（除非後續實作位運算）。

---

## 2. 語法分析 (Parsing)
### 算符優先級與結合性
本章引入了多個優先級層級，由高到低排列如下：
1.  **一元運算符**：`!`, `-`, `~` (右結合)
2.  **關係運算符**：`<`, `<=`, `>`, `>=` (左結合)
3.  **相等運算符**：`==`, `!=` (左結合)
4.  **邏輯與**：`&&` (左結合)
5.  **邏輯或**：`||` (左結合)

### Precedence Climbing 的應用
*   為了處理這些層級，`parse_exp(min_prec)` 函數需要根據上述層級設定對應的優先級數值（例如：`||` 為 5, `&&` 為 10...）。
*   **括號處理**：括號會遞歸調用 `parse_exp(0)`，重置優先級計算。

---

## 3. TACKY 中間表示 (TACKY Generation)
### 控制流指令
為了支持 `&&` 和 `||` 的短路特性，TACKY 必須引入**標籤 (Labels)** 和 **跳轉 (Jumps)**：
*   `Jump(identifier target)`：無條件跳轉。
*   `JumpIfZero(val condition, identifier target)`：若條件為 0 則跳轉。
*   `JumpIfNotZero(val condition, identifier target)`：若條件非 0 則跳轉。
*   `Label(identifier)`：定義跳轉目標點。

### 短路邏輯實作 (Short-circuiting)
*   **`a && b`**：
    1.  計算 `a`。
    2.  `JumpIfZero(a, false_label)`。
    3.  計算 `b`。
    4.  `JumpIfZero(b, false_label)`。
    5.  設置結果為 1，`Jump(end_label)`。
    6.  `false_label`: 設置結果為 0。
    7.  `end_label`: 結束。
*   **標籤唯一性**：必須使用全局計數器生成唯一標籤（如 `and_false_1`）。

---

## 4. 彙編生成 (Assembly Generation)
### 狀態標誌位 (RFLAGS)
x64 指令如 `cmp` 或 `sub` 會更新 `RFLAGS` 暫存器中的標誌位：
*   **ZF (Zero Flag)**：結果為 0 時設置。
*   **SF (Sign Flag)**：結果為負時設置。
*   **OF (Overflow Flag)**：發生有符號溢出時設置。

### 比較指令 `cmp`
*   `cmpl b, a`：計算 `a - b` 並更新標誌位，但不儲存結果。
*   **重要限制**：
    1.  不能同時讓兩個運算元都是記憶體地址。
    2.  **第二個運算元不能是立即值**（Immediate）。

### 條件設置指令 `setCC` 與跳轉 `jCC`
根據標誌位狀態，使用對應的後綴：
*   `e` (Equal): `ZF`
*   `ne` (Not Equal): `!ZF`
*   `l` (Less): `SF != OF`
*   `le` (Less or Equal): `ZF | (SF != OF)`
*   `g` (Greater): `!ZF & (SF == OF)`
*   `ge` (Greater or Equal): `SF == OF`

---

## 5. 指令修正與代碼發射 (Fix-up & Emission)
### 修正 `cmp` 指令
*   若遇到 `cmpl mem, mem`，需搬移至 `R10`。
*   若遇到 `cmpl val, imm`，需將立即值搬移至 `R11`。

### 1 位元組寄存器處理
`setCC` 指令（如 `sete`, `setl`）**只能操作 1 位元組的目標**。
*   **正確做法**：
    1.  `movl $0, %eax` (先清空整個暫存器)。
    2.  `cmp ...`
    3.  `setl %al` (只設置最低 8 位)。
*   **寄存器別名對照表**：
    *   `EAX` -> `%al`
    *   `EDX` -> `%dl`
    *   `R10` -> `%r10b`
    *   `R11` -> `%r11b`

### 局部標籤 (Local Labels)
*   為了避免符號衝突並友善調試器：
    *   **Linux**: 使用 `.L` 前綴（例如 `.L_false_0`）。
    *   **macOS**: 使用 `L` 前綴。

---

## 6. 補充知識：未定義行為 (Undefined Behavior)
*   **整數溢出**：C 語言中，有符號整數溢出是「未定義行為」。編譯器可能會假設溢出永遠不會發生而進行激進優化（例如刪除檢查代碼）。
*   **短路副作用**：若 `&&` 的左側為假，右側的任何表達式（如函數調用、賦值）都不會執行，這在編譯時必須嚴格遵守。

---

## 7. 延伸閱讀資源 (Additional Resources)

### 關於未定義行為 (Undefined Behavior)
*   **John Regehr 的部落格**: [“A Guide to Undefined Behavior in C and C++, Part 1”](https://blog.regehr.org/archives/213)
    *   這篇文章深入淺出地解釋了未定義行為在 C 標準中的含義，以及它對編譯器優化的影響。
*   **Raph Levien 的部落格**: [“With Undefined Behavior, Anything Is Possible”](https://raphlinus.github.io/programming/rust/2018/08/17/undefined-behavior.html)
    *   這篇文章探討了未定義行為的來源及其對軟體安全性與可靠性的衝擊，並解釋了

---

# 第五章：區域變數 (Local Variables)

##  核心目標
本章的核心是讓編譯器支援**區域變數 (Local Variables)**。為了達成這個目標，我們需要引入以下新概念：
1. **變數宣告與初始化 (Declarations & Initializations)**。
2. **賦值運算式 (Assignment Expressions)** 及其**副作用 (Side Effects)**。
3. **區塊項目 (Block Items)**：讓函式主體可以包含多個陳述式 (Statements) 與宣告 (Declarations)。
4. **語意分析 (Semantic Analysis)**：新增編譯器階段來處理「變數解析 (Variable Resolution)」與「左值檢查 (Lvalue Validation)」。

---

## 1. 詞法分析 (Lexer)
只需要新增一個 Token，也就是賦值運算子 `= (Equal Sign)`。
注意：變數名稱屬於 `Identifier`（識別字），與函式名稱共用相同的 Token 類型，在 Lexer 階段不需要區分它們。

```regex
=   // 賦值運算子 (Assignment operator)
```

---

## 2. 語法分析 (Parser)

### 2.1 抽象語法樹 (AST) 擴充
為了支援變數宣告、賦值及空陳述式，AST (以 ASDL 表示) 的定義更新如下 (粗體/註解為新增部分)：

```python
program = Program(function_definition)
function_definition = Function(identifier name, block_item* body)

// 新增：區塊項目可以是陳述式或宣告
block_item = S(statement) | D(declaration)

// 新增：宣告節點 (包含變數名稱與可選的初始化表達式)
declaration = Declaration(identifier name, exp? init)

statement = Return(exp) 
          | Expression(exp)  // 新增：表達式陳述式 (例如 `a = 3;`)
          | Null             // 新增：空陳述式 (例如單獨的 `;`)

exp = Constant(int) 
    | Var(identifier)                  // 新增：讀取變數
    | Unary(unary_operator, exp) 
    | Binary(binary_operator, exp, exp) 
    | Assignment(exp, exp)             // 新增：賦值操作

unary_operator = Complement | Negate | Not
binary_operator = Add | Subtract | Multiply | Divide | Remainder | And | Or
                | Equal | NotEqual | LessThan | LessOrEqual
                | GreaterThan | GreaterOrEqual
```

### 2.2 具體語法 (Formal Grammar - EBNF)
相對應的 EBNF 語法規則更新如下。注意 `{ }` 代表重複零次或多次，`[ ]` 代表可選 (Optional)。

```ebnf
<program> ::= <function>
<function> ::= "int" <identifier> "(" "void" ")" "{" { <block-item> } "}"
<block-item> ::= <statement> | <declaration>

<declaration> ::= "int" <identifier> [ "=" <exp> ] ";"
<statement> ::= "return" <exp> ";" | <exp> ";" | ";"

<exp> ::= <factor> | <exp> <binop> <exp>
<factor> ::= <int> | <identifier> | <unop> <factor> | "(" <exp> ")"

<unop> ::= "-" | "~" | "!"
<binop> ::= "-" | "+" | "*" | "/" | "%" | "&&" | "||"
          | "==" | "!=" | "<" | "<=" | ">" | ">=" 
          | "="   // 新增賦值運算子

<identifier> ::= ? An identifier token ?
<int> ::= ? A constant token ?
```

### 2.3 優先級爬升演算法 (Precedence Climbing) 的改良
賦值運算子 `=` 與其他二元運算子最大的不同在於：它是**右結合 (Right-associative)**。
* 左結合 (`-`)：`a - b - c` 解析為 `(a - b) - c`
* 右結合 (`=`)：`a = b = c` 解析為 `a = (b = c)`

為了支援右結合，在遇到 `=` 時，遞迴呼叫 `parse_exp` 的 `min_prec` 參數**不需要 +1**。

**運算子優先級表 (Table 5-1)**:
| Operator | Precedence | Associativity |
| -------- | ---------- | ------------- |
| `*`, `/`, `%` | 50 | Left |
| `+`, `-` | 45 | Left |
| `<`, `<=`, `>`, `>=`| 35 | Left |
| `==`, `!=`| 30 | Left |
| `&&` | 10 | Left |
| `\|\|` | 5 | Left |
| **`=`** | **1** | **Right** |

**改良後的 Precedence Climbing 虛擬碼 (Listing 5-8)**：
```python
parse_exp(tokens, min_prec):
    left = parse_factor(tokens)
    next_token = peek(tokens)
    
    while next_token is a binary operator and precedence(next_token) >= min_prec:
        if next_token is "=":
            take_token(tokens) // 移除 "="
            # 注意這裡：右結合，所以傳遞 precedence(next_token) 而非 +1
            right = parse_exp(tokens, precedence(next_token))
            left = Assignment(left, right)
        else:
            operator = parse_binop(tokens)
            # 左結合，傳遞 precedence(next_token) + 1
            right = parse_exp(tokens, precedence(next_token) + 1)
            left = Binary(operator, left, right)
            
        next_token = peek(tokens)
        
    return left
```

---

## 3. 語意分析 (Semantic Analysis)
從本章開始引入「語意分析」階段。在這個階段，AST 結構完全合法，但我們需要檢查程式是否「有意義」。

### 3.1 變數解析 (Variable Resolution)
變數解析的目標是：
1. 為每個區域變數生成**全域唯一的名稱** (例如將 `x` 重命名為 `x.1`)，以利於後續作用域 (Scope) 的處理。
2. 攔截**重複宣告 (Duplicate declaration)** 錯誤。
3. 攔截**使用未宣告變數 (Undeclared variable)** 錯誤。

**處理變數宣告 (Listing 5-9)**：
```python
resolve_declaration(Declaration(name, init), variable_map):
    if name is in variable_map:
        fail("Duplicate variable declaration!")
        
    unique_name = make_temporary() # 產生如 name.1 的唯一 ID
    variable_map.add(name, unique_name)
    
    if init is not null:
        init = resolve_exp(init, variable_map)
        
    return Declaration(unique_name, init)
```

**處理變數讀取與賦值 (Listing 5-11)**：
同時必須檢查「左值 (Lvalue)」。賦值的左邊目前**必須是變數**，不能是常數或運算式。
```python
resolve_exp(e, variable_map):
    match e with
    | Assignment(left, right) ->
        if left is not a Var node:
            fail("Invalid lvalue!")  // 左值檢查： 3 = x 是錯誤的
        return Assignment(resolve_exp(left, variable_map), 
                          resolve_exp(right, variable_map))
                          
    | Var(v) ->
        if v is in variable_map:
            return Var(variable_map.get(v)) // 替換成唯一名稱
        else:
            fail("Undeclared variable!")    // 變數未宣告錯誤
    | --snip--
```

---

## 4. 中介碼生成 (TACKY Generation)
幸運的是，我們在前面的章節已經為了暫存運算結果發明了 TACKY 的 `Var` (虛擬暫存器) 和 `Copy` 指令。因此，TACKY IR **不需要新增任何結構**，我們只需要將 AST 的新節點轉換成現有的 TACKY 指令即可。

### 4.1 轉換規則
* **變數 (Var)**：直接轉換為 TACKY 的 `Var`。
* **宣告 (Declaration)**：
  * 若沒有初始化 (e.g., `int a;`)：在 TACKY 階段直接**丟棄** (TACKY 不需要宣告變數)。
  * 若有初始化 (e.g., `int a = 5;`)：視為賦值處理，產生對應的 `Copy` 或算術運算。
* **賦值 (Assignment)**：計算右半邊 (RHS) 的結果，然後 `Copy` 到左半邊 (LHS) 的變數中。
* **空陳述 (Null Statement)**：不產生任何 TACKY 指令。
* **表達式陳述 (Expression Statement)**：計算表達式，但丟棄結果的變數。

**賦值與變數的 TACKY 轉換虛擬碼 (Listing 5-12)**：
```python
emit_tacky(e, instructions):
    match e with
    | --snip--
    | Var(v) -> return Var(v)
    | Assignment(Var(v), rhs) ->
        result = emit_tacky(rhs, instructions)
        instructions.append(Copy(result, Var(v))) # 把計算結果 Copy 給變數 v
        return Var(v)  # 賦值表達式本身的回傳值是該變數的值
```

### 4.2 沒有 Return 的函式處理 (Edge Case)
C 語言規定，如果 `main` 函式執行到結尾沒有遇到 `return`，應隱式回傳 `0`。對於其他函式，如果 caller 沒有使用回傳值，則行為合法，但若 caller 試圖讀取，則是未定義行為 (Undefined Behavior)。

**解法**：
在 TACKY Generation 的最後，無條件在每個函式的指令串尾端加上 `Return(Constant(0))`。
* 若原程式已有 `return`，這個隱式 `return 0` 變成不可達代碼 (Dead code)，未來優化階段可消除。
* 若原程式沒有 `return`，這確保了程式能正常退出，且對於 `main` 函式正好符合 C 標準規範。

---

## 5. 組合語言生成 (Assembly Generation)
由於 TACKY IR 沒有改變 (還是 `Copy`, `Var` 等)，我們**完全不需要修改** Assembly Generation 或 Code Emission 階段！
在此前的實作中，遇到 TACKY 的 `Var` 時，我們就已經透過分配 Stack 的 Offset (例如 `-4(%rbp)`) 來處理了。使用者定義的區域變數現在直接享受了這套機制的紅利。

---

## 進階知識與細節 (Undefined Behavior)

本章介紹了變數後，C 語言中經典的**未定義行為 (Undefined Behavior, UB)** 也隨之而來：
1. **讀取未初始化的變數**：編譯器會配置 Stack 空間，但裡面的值是記憶體中殘留的垃圾值 (Garbage value)。(此實作不主動攔截)
2. **無順序副作用 (Unsequenced Side Effects)**：
   C 標準未規定二元運算子 (如 `+`, `*`) 左右兩邊的求值順序。如果在一行內對同一個變數賦值多次，或者同時讀取與賦值，會觸發 UB。
   > 範例：`(a = 4) + (a = 5);` 或 `return (a = 1) + a;`
   > 結果取決於編譯器實作，在標準中是不可預測的。

---


# 第六章 If Statements and Conditional Expressions

---

## 1. 本章核心目標

本章是 **第一個真正的 control flow feature**。

之前 compiler 只能做：

```
順序執行
expression evaluation
return
```

現在加入：

```
branching (分支)
```

兩個語言構造：

```
1. if statement
2. ternary conditional expression (?:)
```

---

## 2. Conditional Constructs

本章加入兩種 conditional：

### 2.1 If Statement

語法：

```
if (condition)
    statement
```

或

```
if (condition)
    statement
else
    statement
```

條件規則：

```
0     → false
non-0 → true
```

這與之前 boolean operators 相同。

---

### 2.2 Conditional Expression

語法：

```
a ? b : c
```

語義：

```
if a != 0
    result = b
else
    result = c
```

重要特性：

**short-circuit evaluation**

例如：

```c
0 ? foo() : bar()
```

只會執行：

```
bar()
```

不能同時執行兩個 expression。

否則可能：

```
side effects
segmentation fault
I/O
```

因此 compiler 必須：

```
只執行被選中的 branch
```


---

## 3. Statement vs Expression

這是本章非常重要的語言概念。

### 3.1 Statement

statement **沒有 value**

例如：

```
return 3;
if (a) return 1;
```

---

### 3.2 Expression

expression **有 value**

例如：

```
1 + 2
a ? b : c
```

---

### 3.3 差異

合法：

```c
int x = flag ? 2 : 3;
```

非法：

```c
int x = if (flag) 2 else 3;
```

原因：

```
if 是 statement
?: 是 expression
```

---

## 4. Else If 的語法糖

C 沒有真正的：

```
else if
```

其實只是：

```
else
    if (...)
```

例如：

```
if (a)
    ...
else if (b)
    ...
else
    ...
```

實際解析：

```
if (a)
    ...
else
    if (b)
        ...
    else
        ...
```

這對 parser 非常重要。

---

## 5. Grammar (文法)

新增 statement grammar：

```
statement
    : RETURN expression ";"
    | expression ";"
    | IF "(" expression ")" statement
    | IF "(" expression ")" statement ELSE statement
```

注意：

```
statement 不能是 declaration
```

例如：

非法：

```
if (x)
    int y = 3;
```

因為 compound statement `{}` 還沒實作。

---

## 6. AST 設計

新增 AST node：

```
Statement
    ├── Return
    ├── Expression
    └── If
```

If statement：

```
struct IfStatement {
    Expression condition
    Statement then_branch
    Statement else_branch (optional)
}
```

AST 結構：

```
If
 ├── condition
 ├── then_branch
 └── else_branch
```

---

## 7. Parsing If Statements

Parser 需要辨識：

```
if (...)
    statement
```

Pseudo code：

```
parse_statement():
    if token == IF
        parse_if_statement()
```

---

### parse_if_statement

Pseudo code：

```
parse_if_statement():

    expect(IF)
    expect("(")

    cond = parse_expression()

    expect(")")

    then_stmt = parse_statement()

    if next_token == ELSE
        consume
        else_stmt = parse_statement()
    else
        else_stmt = NULL

    return If(cond, then_stmt, else_stmt)
```

---

## 8. Parsing Conditional Expression

新增 precedence operator：

```
? :
```

precedence：

```
lowest
```

例如：

```
a || b ? c : d
```

應解析為：

```
(a || b) ? c : d
```

---

### Grammar

```
conditional-expression
    : logical-or-expression
    | logical-or-expression ? expression : conditional-expression
```

這個文法非常重要。

原因：

```
ternary operator 是 right-associative
```

例如：

```
a ? b : c ? d : e
```

解析為：

```
a ? b : (c ? d : e)
```

---

## 9. Parsing Conditional Expression

Pseudo code：

```
parse_conditional_expression():

    cond = parse_logical_or_expression()

    if next_token == '?'

        consume '?'

        true_expr = parse_expression()

        expect ':'

        false_expr = parse_conditional_expression()

        return Ternary(cond,true_expr,false_expr)

    return cond
```

注意：

```
false branch 需要 recursive parse
```

確保 right associativity。

---

## 10. TACKY IR

本書的 IR：

```
TACKY
```

是一種類似：

```
three-address code
```

例如：

```
t1 = a < b
if t1 goto L1
...
```

---

## 11. If Statement Code Generation

C code：

```
if (cond)
    S1
else
    S2
```

IR：

```
cond
ifz cond goto L_else

S1
goto L_end

L_else:
S2

L_end:
```

Pseudo code：

```
compile_if(stmt):

    cond = compile_expression(stmt.condition)

    else_label = new_label()
    end_label  = new_label()

    emit IFZ cond else_label

    compile(stmt.then)

    emit GOTO end_label

    emit LABEL else_label

    compile(stmt.else)

    emit LABEL end_label
```

---

## 12. If Without Else

```
if (cond)
    S
```

IR：

```
cond
ifz cond goto L_end

S

L_end:
```

Pseudo code：

```
else_label = new_label()

emit IFZ cond else_label

compile(S)

emit LABEL else_label
```

---

## 13. Conditional Expression Code Generation

C code：

```
x = a ? b : c
```

IR：

```
a
ifz a goto L_false

t = b
goto L_end

L_false:
t = c

L_end:
```

Pseudo code：

```
compile_ternary(expr):

    cond = compile(expr.condition)

    false_label = new_label()
    end_label   = new_label()

    result = new_temp()

    emit IFZ cond false_label

    true_val = compile(expr.true_expr)
    emit result = true_val

    emit GOTO end_label

false_label:

    false_val = compile(expr.false_expr)
    emit result = false_val

end_label:

    return result
```

---

## 14. Why IR Makes This Easy

本章的重要 insight：

```
control flow = labels + jumps
```

IR 只需要：

```
LABEL
GOTO
IFZ
```

就可以實現：

```
if
ternary
loops
switch
```

因此：

```
later compiler stages 不需要改
```

---

## 15. Semantic Errors

本章需要新增語意檢查：

```
undeclared variable in condition
```

例如：

```
if (x)
```

但：

```
x 未宣告
```

需要 semantic error。

---

## 16. Test Cases

典型測試：

### basic if

```
if (1)
    return 2;
return 3;
```

---

### if else

```
if (0)
    return 1;
else
    return 2;
```

---

### nested if

```
if (a)
    if (b)
        return 1;
    else
        return 2;
```

---

### ternary

```
return a ? b : c;
```

---

## 17. 本章重要編譯器概念

### 17.1 Control Flow

control flow graph：

```
       cond
      /   \
   true   false
    |       |
   S1      S2
      \   /
       end
```

---

### 17.2 Labels

compiler 會生成：

```
L1
L2
L3
```

這些 label：

```
不是 C label
是 compiler internal label
```

---

### 17.3 Short Circuit

branching 保證：

```
only one branch executes
```

---

## 18. 本章新增 AST 節點

新增：

```
Statement::If
Expression::Conditional
```

---

## 19. Compiler Pipeline Impact

本章只改：

```
Parser
AST
TACKY generation
```

不用改：

```
Assembly generation
Register allocation
Stack allocation
```

因為 IR 已經足夠強。

---

## 20. Summary

完成了：

```
第一個 control structure
```

新增：

```
if statements
ternary expressions
```

並學到：

```
control flow → jumps
```

同時延伸：

```
precedence climbing
```

來支援：

```
?: operator
```

目前 compiler 仍有限制：

```
if body 不能有 declaration
```

例如：

```
if (x) {
    int y = 3;
}
```

尚未支援。

下一章：

```
compound statements {}
```

並加入：

```
nested scope
```

這是 semantic analysis 的重要概念。

---

了解，我改成 **可直接一鍵複製的 HackMD 成品版**，把中間的參考來源全部拿掉，只在最後留一個簡單的參考資料區塊。

---

# 第七章：複合陳述式（Compound Statements）

## 核心目標

本章的核心是讓編譯器支援 **複合陳述式（Compound Statements）**，也就是用大括號包起來的一整個區塊：

```c
{
    int a = 1;
    a = a + 2;
    return a;
}
```

這件事看起來只是「多了一層大括號」，但其實它會讓編譯器正式進入 **巢狀作用域（Nested Scope）** 的世界。

為了完成這一章，我們需要做到：

1. **讓 `{ ... }` 本身可以被當成一個 statement**
2. **把函式本體與一般 block 統一成同一種結構**
3. **讓變數解析支援內外層 block**
4. **正確處理 shadowing（內層遮蔽外層同名變數）**
5. **在離開 block 後恢復外層作用域**
6. **讓 TACKY generation 可以直接展開 block 裡的內容**

---

## 1. 這一章在語言上新增了什麼？

在前面的章節中，函式本體已經可以寫成一個大括號區塊：

```c
int main(void) {
    return 0;
}
```

但這一章更進一步，讓大括號區塊不只存在於函式本體，也可以出現在任何需要 statement 的地方，例如：

```c
if (1) {
    int x = 3;
    return x;
}
```

也就是說，從這一章開始：

* `{ ... }` 不只是「語法上的包裝」
* 它是 AST 中真正的一個 **statement 節點**
* 而且它同時也是一個 **scope 邊界**

---

## 2. Scope 的核心觀念

本章最重要的新觀念，不是 parser，而是 **scope**。

### 2.1 什麼是 block scope？

只要是一個可以包含宣告、並且決定宣告可見範圍的區域，就可以看成一個 block。

在這一章裡，兩種東西是 block：

* 函式本體
* 複合陳述式 `{ ... }`

---

### 2.2 變數的可見範圍從哪裡開始？

區域變數的 scope **不是從整個 block 開頭開始**，而是從「宣告點」開始。

```c
int main(void) {
    int a = 5;
    return a;
}
```

這裡 `a` 的 scope 是從 `int a = 5;` 這一行開始，到 block 結束為止。

---

### 2.3 內層 block 可以讀取外層變數

如果內層沒有重新宣告同名變數，就可以讀到外層的變數。

```c
int main(void) {
    int a = 2;
    {
        int b = a + 1;
        return b;
    }
}
```

這裡 `b` 初始化時用到的 `a`，是外層的 `a`。

---

### 2.4 內層 block 可以遮蔽外層同名變數

如果內層重新宣告了同名變數，之後在該 block 中會優先看到內層那個。

```c
int main(void) {
    int a = 2;
    {
        int a = 3;
        return a;
    }
}
```

這裡 `return a;` 回傳的是內層的 `a`，不是外層的。

這種行為叫做：

* **shadowing**
* 或者叫 **name hiding**

---

### 2.5 離開內層 block 後，外層名字會恢復

內層把外層遮住，不代表外層變數消失，只是暫時不可見。

```c
int main(void) {
    int a = 2;
    {
        int a = 3;
    }
    return a;
}
```

這裡最後的 `return a;` 又會回到外層的 `a`。

---

### 2.6 本章必須攔截的兩種 scope 錯誤

#### 錯誤 1：使用不在 scope 內的變數

```c
int main(void) {
    {
        int a = 3;
    }
    return a;   // 錯
}
```

因為 `a` 已經離開作用域了。

#### 錯誤 2：同一個 block 裡重複宣告同名變數

```c
int main(void) {
    int a = 1;
    int a = 2;   // 錯
    return a;
}
```

但如果是不同 block，就合法：

```c
int main(void) {
    int a = 1;
    {
        int a = 2;   // 合法
    }
    return a;
}
```

---

## 3. 詞法分析（Lexer）

這一章 **不需要修改 lexer**。

原因很簡單：

* `{`
* `}`

這兩個 token 在前面章節為了函式本體就已經存在了。

所以本章 lexer 沒有新增 token，也沒有新增關鍵字。

---

## 4. 語法分析（Parser）

本章 parser 的核心任務是：

> 讓一整個 `{ ... }` 被視為一個 statement

---

## 5. AST 結構更新

這一章最關鍵的 AST 改動，是引入 `block` 與 `Compound(block)`。

```python
program = Program(function_definition)

function_definition = Function(identifier name, block body)

block = Block(block_item*)

block_item = S(statement) | D(declaration)

declaration = Declaration(identifier name, exp? init)

statement = Return(exp)
          | Expression(exp)
          | If(exp condition, statement then, statement? else)
          | Compound(block)
          | Null

exp = Constant(int)
    | Var(identifier)
    | Unary(unary_operator, exp)
    | Binary(binary_operator, exp, exp)
    | Assignment(exp, exp)
    | Conditional(exp condition, exp true_exp, exp false_exp)

unary_operator = Complement | Negate | Not

binary_operator = Add | Subtract | Multiply | Divide | Remainder
                | And | Or
                | Equal | NotEqual
                | LessThan | LessOrEqual
                | GreaterThan | GreaterOrEqual
```

---

## 6. AST 改動的意義

### 6.1 函式本體不再只是 items，而是 `block`

以前可以把函式 body 看成一串 block item。
現在更正式地把它表示成：

```python
Function(name, block body)
```

也就是說，函式本體和一般 `{ ... }` 的本質其實是一樣的。

---

### 6.2 `Compound(block)` 讓 block 成為 statement

這是本章的關鍵。

有了這個節點之後：

```c
if (x) {
    int a = 1;
    return a;
}
```

在 AST 裡就能表示成：

```python
If(
    condition,
    Compound(
        Block([...])
    ),
    null
)
```

也就是說，`if` 後面不是直接接很多 statement，而是接 **一個 Compound statement**。

---

### 6.3 block 裡面仍然是 block item，不是純 statement list

因為 block 裡仍然可能同時出現：

* declaration
* statement

例如：

```c
{
    int a = 1;
    a = a + 2;
}
```

所以 block 不能簡化成純 statement list，仍然必須保留：

```python
block_item = S(statement) | D(declaration)
```

---

## 7. EBNF 文法整理

```ebnf
<program> ::= <function>

<function> ::= "int" <identifier> "(" "void" ")" <block>

<block> ::= "{" { <block-item> } "}"
<block-item> ::= <statement> | <declaration>

<declaration> ::= "int" <identifier> [ "=" <exp> ] ";"

<statement> ::= "return" <exp> ";"
              | <exp> ";"
              | "if" "(" <exp> ")" <statement> [ "else" <statement> ]
              | <block>
              | ";"

<exp> ::= <factor>
        | <exp> <binop> <exp>
        | <exp> "?" <exp> ":" <exp>

<factor> ::= <int>
           | <identifier>
           | <unop> <factor>
           | "(" <exp> ")"

<unop> ::= "-" | "~" | "!"
<binop> ::= "-" | "+" | "*" | "/" | "%"
          | "&&" | "||"
          | "==" | "!="
          | "<" | "<=" | ">" | ">="
          | "="

<identifier> ::= ? identifier token ?
<int> ::= ? constant token ?
```

---

## 8. Parser 的直覺寫法

### 8.1 `parse_statement`

當 parser 看到 `{` 時，就知道自己遇到了 compound statement。

```python
parse_statement():
    if next token is "return":
        return parse_return_statement()

    if next token is "if":
        return parse_if_statement()

    if next token is "{":
        blk = parse_block()
        return Compound(blk)

    if next token is ";":
        consume(";")
        return Null

    e = parse_exp()
    consume(";")
    return Expression(e)
```

---

### 8.2 `parse_block`

`parse_block` 的工作就是：

1. 吃掉 `{`
2. 一直讀 block item
3. 直到遇到 `}`

```python
parse_block():
    consume("{")
    items = []

    while next token is not "}":
        if next token starts a declaration:
            items.append(D(parse_declaration()))
        else:
            items.append(S(parse_statement()))

    consume("}")
    return Block(items)
```

---

## 9. 語意分析（Semantic Analysis）

本章真正的重點，是把第五章的 variable resolution 升級成 **支援巢狀作用域** 的版本。

---

## 10. 變數解析的核心目標

本章的 variable resolution 需要做到：

1. 把每個區域變數重新命名成唯一名稱
2. 正確處理內外層 block 的可見性
3. 正確處理 shadowing
4. 攔截同一個 block 內的重複宣告
5. 攔截使用超出 scope 的名字

---

## 11. 為什麼要重新命名成唯一名稱？

因為一旦你把：

```c
int main(void) {
    int x = 1;
    {
        int x = 2;
        return x;
    }
    return x;
}
```

解析成：

```c
int main(void) {
    int x.0 = 1;
    {
        int x.1 = 2;
        return x.1;
    }
    return x.0;
}
```

後面的 TACKY 與 assembly backend 就不用再理解 scope。

它們只要看到：

* `x.0`
* `x.1`

是不同變數就好。

這就是 variable resolution 在前端階段把 lexical scope 問題提前消滅掉的價值。

---

## 12. Symbol map / Variable map 的概念

你可以把 variable resolution 想成維護一張表：

```text
原始名稱 -> 唯一名稱
```

例如：

```text
a -> a.0
b -> b.1
```

但在本章，這張表還需要記錄：

* 這個名稱是不是來自當前 block

所以 map entry 可以想成：

```python
entry = {
    original_name: "a",
    unique_name: "a.0",
    from_current_block: True
}
```

---

## 13. `from_current_block` 的用途

這個欄位是本章最重要的小技巧之一。

它可以幫你分辨兩種情況：

### 合法：內層遮蔽外層

```c
int a = 1;
{
    int a = 2;   // 合法
}
```

這裡雖然 `a` 已經在 map 裡，但那個 entry 不是來自「當前 block」，而是外層 block，所以這是合法 shadowing。

---

### 非法：同一 block 重複宣告

```c
int a = 1;
int a = 2;   // 非法
```

這裡 map 裡的 `a` 是來自同一個 block，所以必須報錯。

---

## 14. `resolve_declaration` 的更新版邏輯

```python
resolve_declaration(Declaration(name, init), variable_map):
    if name exists in variable_map
       and variable_map[name].from_current_block == True:
        fail("Duplicate variable declaration in same block")

    unique_name = make_temporary_name(name)

    variable_map[name] = {
        unique_name: unique_name,
        from_current_block: True
    }

    if init is not null:
        init = resolve_exp(init, variable_map)

    return Declaration(unique_name, init)
```

---

## 15. 和第五章最大的差別

第五章通常可以寫成：

```python
if name in variable_map:
    fail(...)
```

但第七章不能這樣做，因為內層 block 允許 shadowing。

所以現在的判定必須改成：

```python
if name in variable_map and entry.from_current_block:
    fail(...)
```

只攔下「同一層 block」的重複宣告。

---

## 16. 進入 block 時該怎麼做？

本章的標準做法是：

> 一進入新的 block，就複製目前的 variable map

也就是說：

* 外層 block 用自己的 map
* 內層 block 拿一份 copy
* 內層的修改不會污染外層
* 離開 block 後，外層 map 自然恢復

---

## 17. `copy_variable_map` 的重點

複製 map 不是單純 deep copy 就好，還要把每個 entry 的：

```python
from_current_block
```

重設成：

```python
False
```

因為當你進入新的 block 時，原本從外層帶進來的名字，對這個新 block 來說都不是「當前 block 宣告的名字」。

```python
copy_variable_map(old_map):
    new_map = deep_copy(old_map)

    for each entry in new_map:
        entry.from_current_block = False

    return new_map
```

---

## 18. `resolve_statement` 如何處理 Compound

```python
resolve_statement(stmt, variable_map):
    match stmt with
    | Return(e) ->
        return Return(resolve_exp(e, variable_map))

    | Expression(e) ->
        return Expression(resolve_exp(e, variable_map))

    | If(cond, then_stmt, else_stmt) ->
        return If(
            resolve_exp(cond, variable_map),
            resolve_statement(then_stmt, variable_map),
            resolve_statement(else_stmt, variable_map) if else_stmt else null
        )

    | Compound(block) ->
        inner_map = copy_variable_map(variable_map)
        return Compound(resolve_block(block, inner_map))

    | Null ->
        return Null
```

這裡最重要的是：

```python
inner_map = copy_variable_map(variable_map)
```

這一步保證內層 block 的作用域不會污染外層。

---

## 19. `resolve_block` 的本質是順序處理 block item

```python
resolve_block(block, variable_map):
    new_items = []

    for item in block.items:
        if item is declaration:
            new_items.append(D(resolve_declaration(item.decl, variable_map)))
        else:
            new_items.append(S(resolve_statement(item.stmt, variable_map)))

    return Block(new_items)
```

這裡一定要 **照順序** 處理，因為 scope 是從宣告點開始，不是從整個 block 開頭開始。

例如：

```c
int a = 2;
{
    int b = a + 1;
    int a = 3;
}
```

在解析 `b` 初始化時，內層 `a` 還沒宣告，所以那個 `a` 應該解析成外層 `a`。

如果你不是按順序 resolve，而是先把整個 block 的宣告都收進 map，就會做錯。

---

## 20. TACKY Generation

這一章的 TACKY generation 幾乎沒有新的控制流程技巧。

核心想法是：

> Compound statement 只是把裡面的 block item 順序展開

---

## 21. Compound 的 TACKY lowering

```python
emit_tacky_statement(stmt, instructions):
    match stmt with
    | Compound(block) ->
        emit_tacky_block(block, instructions)

    | Return(e) ->
        ...

    | Expression(e) ->
        ...

    | If(cond, then_stmt, else_stmt) ->
        ...

    | Null ->
        ...
```

```python
emit_tacky_block(block, instructions):
    for item in block.items:
        if item is declaration:
            emit_tacky_declaration(item.decl, instructions)
        else:
            emit_tacky_statement(item.stmt, instructions)
```

---

## 22. 為什麼 TACKY 和後端幾乎不用改？

因為在 variable resolution 之後：

* 同名但不同 scope 的變數，已經變成不同的唯一名稱
* 後端再也不需要理解 lexical scope

也就是說，backend 只要繼續做原本的事情：

* `Var(...)`
* pseudo register
* stack slot allocation

就夠了。

對後端來說：

```text
a.0
a.1
```

就是兩個不同變數，完全不需要知道它們原本都叫 `a`。

---

## 23. 本章最容易犯的錯

### 錯誤 1：進入 block 時直接共用同一張 map

這會讓內層宣告污染外層，離開 block 後也無法恢復正確綁定。

---

### 錯誤 2：還沿用第五章的 duplicate declaration 規則

如果你還在寫：

```python
if name in variable_map:
    fail(...)
```

那合法的 shadowing 也會被誤判成錯誤。

---

### 錯誤 3：忘了 scope 從宣告點開始

這會讓這種程式被錯誤解析：

```c
int a = 2;
{
    int b = a + 1;
    int a = 3;
}
```

---

### 錯誤 4：想在後端處理 shadowing

shadowing 應該在 variable resolution 就處理掉，不應該留到 TACKY 或 assembly backend。

---

## 24. 這一章對整體編譯器的意義

表面上，第七章只是多了一個：

```python
Compound(block)
```

但實際上，它的意義很大：

* 你的語言正式進入 **多層 lexical scope**
* parser 開始能處理真正的 block statement
* semantic analysis 開始承擔真正的 scope 管理責任
* 後續章節像 `for`、函式參數、檔案層級宣告、`static`、`extern` 都會建立在這個模型上

也就是說，第七章是你從「簡單 expression compiler」走向「真正 C-like front end」的重要一步。

---

## 25. 一句話總結

> 第七章的本質，就是把 `{ ... }` 變成一個真正的 statement，並且讓 variable resolution 正確支援巢狀作用域與 shadowing。

---

## 26.速記重點

### 這章新增了什麼？

* 新 statement：`Compound(block)`
* 函式本體統一成 `block`
* variable resolution 支援 nested scope
* 合法 shadowing / 非法 duplicate declaration
* TACKY 只要順序展開 block item

### 最重要的實作口訣

```text
進 block -> 複製 map
宣告變數 -> 加入新 binding，標記 current block
離開 block -> 丟掉內層 map
後端 -> 完全不用懂 scope
```

---
