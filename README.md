# lemon_cc

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