# lemon_cc

## return_2.c
```c
gcc -S -O -fno-asynchronous-unwind-tables -fcf-protection=none return_2.c
```

- `-fno-asynchronous-unwind-tables` 不產生用於偵錯的展開表（unwind table）。我們不需要它。

- `-fcf-protection=none` 停用控制流保護（control-flow protection），這是一項安全性功能，會增加我們不關心的額外指令。控制流保護可能已在您的系統中預設停用，在此情況下，此選項將不會有任何作用。如果您使用的舊版 GCC 或 Clang 不支援此選項，請跳過它。

```c
.globl main
main:
    movl $2, %eax
    ret
```
.globl main 是一個 彙編器指令（Assembler Directive），它是為彙編器提供指引的語句。指令總是開頭帶有一個句點。在這裡，main 是一個 符號（Symbol），代表一個記憶體位址。符號會出現在彙編指令以及彙編器指令中；例如，指令 jmp main 會跳轉到 main 符號所指向的位址。

在第二行，我們使用 main: 作為後面代碼的 標籤（Label）。標籤標記了符號所指向的位置。這個標籤將 main 定義為下一行 movl 指令的位址。彙編器目前不知道該指令最終的記憶體位址，但它知道該指令位於目標文件的哪個區段（Section），以及它相對於該區段起始位置的偏移量。main 的位址將位於 text 區段（包含機器指令的區段）。因為 main 指向此檔案中的第一條機器指令，它在 text 區段中的偏移量為 0。

最後一條機器指令是 ret，它將控制權交還給調用者。你可能會看到 retq 而不是 ret，因為該指令隱含操作一個 64 位元的回傳位址。

```c
gcc return_2.s -o return_2
./return_2 
echo $?
2
```

## 編譯器驅動程式
還需要一個 編譯器驅動程式 來依次調用預處理器、編譯器、彙編器和鏈接器。它應分三步轉換檔案：
1.  **預處理**：`gcc -E -P INPUT_FILE -o PREPROCESSED_FILE`
    `-E` 表示只執行預處理。`-P` 告訴預處理器不要生成行號標記（Linemarkers），因為我們的分析器無法處理它們。
2.  **編譯**：將預處理後的檔案編譯成 `.s` 彙編檔。目前你還沒寫好編譯器，所以先留個空殼（Stub）。
3.  **彙編與鏈接**：`gcc ASSEMBLY_FILE -o OUTPUT_FILE`



## 詞法分析器（The Lexer）

詞法分析器應讀取原始碼並產生 Token 列表。


*   **標識符（Identifier）**：ASCII 字母或下底線開頭，後跟字母、下底線或數字。區分大小寫。
*   **整數常量（Integer Constant）**：由一個或多個數字組成。

標識符和常量具有「值」，而其他類型的 Token 則沒有。請注意，目前沒有「空格 Token」，因為在 C 語言中（不像 Python），空格主要作為分隔符，本身不具語義。

你可以使用正則表達式（Regex）來識別每種 Token 類型。列出了對應的正則表達式。展示了 Token 化的虛擬碼邏輯：在循環中尋找與輸入開頭匹配的最長正則表達式，將其轉換為 Token 並從輸入中移除。

**編寫詞法分析器的小提示：**
*   **將關鍵字視為普通標識符**：先匹配出標識符，再檢查它是否屬於關鍵字。
*   **不要只靠空格拆分**：例如 `main(void)` 包含四個 Token 但沒有空格。
*   **僅需支援 ASCII 字符**：測試程式不包含 Unicode。


