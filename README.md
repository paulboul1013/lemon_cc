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

# 1. 本章核心目標

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

# 2. Conditional Constructs

本章加入兩種 conditional：

## 2.1 If Statement

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

## 2.2 Conditional Expression

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

# 3. Statement vs Expression

這是本章非常重要的語言概念。

## 3.1 Statement

statement **沒有 value**

例如：

```
return 3;
if (a) return 1;
```

---

## 3.2 Expression

expression **有 value**

例如：

```
1 + 2
a ? b : c
```

---

## 3.3 差異

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

# 4. Else If 的語法糖

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

# 5. Grammar (文法)

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

# 6. AST 設計

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

# 7. Parsing If Statements

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

## parse_if_statement

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

# 8. Parsing Conditional Expression

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

## Grammar

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

# 9. Parsing Conditional Expression

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

# 10. TACKY IR

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

# 11. If Statement Code Generation

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

# 12. If Without Else

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

# 13. Conditional Expression Code Generation

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

# 14. Why IR Makes This Easy

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

# 15. Semantic Errors

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

# 16. Test Cases

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

# 17. 本章重要編譯器概念

## 17.1 Control Flow

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

## 17.2 Labels

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

## 17.3 Short Circuit

branching 保證：

```
only one branch executes
```

---

# 18. 本章新增 AST 節點

新增：

```
Statement::If
Expression::Conditional
```

---

# 19. Compiler Pipeline Impact

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

# 20. Summary

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

