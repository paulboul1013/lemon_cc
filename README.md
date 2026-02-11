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