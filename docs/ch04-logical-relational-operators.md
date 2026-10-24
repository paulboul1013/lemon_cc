# 第四章：邏輯與關係運算符（Logical and Relational Operators）

加入邏輯運算符（`!`, `&&`, `||`）與關係運算符（`<`, `>`, `<=`, `>=`, `==`, `!=`），並引入了編譯器設計中至關重要的概念：**短路邏輯 (Short-circuiting)**、**控制流 (Control Flow)** 以及 **彙編中的狀態標誌 (Flags)**。

## 1. 語言特性與詞法分析 (Lexing)
### 新增運算符
*   **邏輯運算符**：`!` (NOT), `&&` (AND), `||` (OR)。
*   **關係運算符**：`==`, `!=`, `<`, `>`, `<=`, `>=`。
*   **結果**：所有運算在真時返回 `1`，假時返回 `0`。

### 詞法分析細節
*   共九個新 Token。Lexer 必須處理多字元 Token，且遵循 **「最長匹配原則」**。
    *   例如：輸入 `<=` 應被解析為 `TOK_LTE`，而非 `<` 緊跟 `=`。
*   `&&` 和 `||` 必須完整匹配，單個 `&` 或 `|` 在目前的 C 子集中應視為非法（除非做了第三章 extra credit 的位元運算子）。

---

## 2. 語法分析 (Parsing)
### AST 與文法
*   AST：`unary_operator` 加 `Not`；`binary_operator` 加 `And | Or | Equal | NotEqual | LessThan | LessOrEqual | GreaterThan | GreaterOrEqual`。
*   文法只擴充運算子清單，其他規則不變：
    ```text
    <unop>  ::= "-" | "~" | "!"
    <binop> ::= "-" | "+" | "*" | "/" | "%" | "&&" | "||"
              | "==" | "!=" | "<" | "<=" | ">" | ">="
    ```
*   `parse_factor` 用處理 `~`、`-` 的方式處理 `!`；`parse_unop`、`parse_binop` 加上新 Token 的轉換。

### 算符優先級與結合性
新運算符的優先級都低於第三章的算術運算子，而且全部**左結合**。完整的優先級數值由高到低：

| 運算符 | 優先級 |
| --- | --- |
| `*`, `/`, `%` | 50 |
| `+`, `-` | 45 |
| `<`, `<=`, `>`, `>=` | 35 |
| `==`, `!=` | 30 |
| `&&` | 10 |
| `\|\|` | 5 |

*   一元運算符（`!`, `-`, `~`）在 `<factor>` 裡處理，綁得比所有二元運算子都緊。
*   數值間隔留給第三章選做的位元運算子，最底下也留了空間給之後的 `=` 和 `?:`。

### Precedence Climbing 的應用
*   只要更新優先級表、`parse_binop`、`parse_unop`，第三章的 `parse_exp(min_prec)` 不用再改。
*   **括號處理**：括號會遞歸調用 `parse_exp(0)`，重置優先級計算。

---

## 3. TACKY 中間表示 (TACKY Generation)
### 關係運算符與 `!`
*   關係運算符和第三章的二元運算一樣轉換，例如 `e1 < e2` 變成 `Binary(LessThan, v1, v2, result)`；`!` 變成 `Unary(Not, src, dst)`。
*   TACKY 的 `binary_operator` **沒有** `And`、`Or`：它們要短路，改用跳轉實作。

### 控制流指令
為了支持 `&&` 和 `||` 的短路特性，TACKY 必須引入**標籤 (Labels)** 和 **跳轉 (Jumps)**，另外加一個 `Copy`：
*   `Copy(val src, val dst)`：把 1 或 0 複製到存放結果的暫存變數。
*   `Jump(identifier target)`：無條件跳轉。
*   `JumpIfZero(val condition, identifier target)`：若條件為 0 則跳轉。
*   `JumpIfNotZero(val condition, identifier target)`：若條件非 0 則跳轉。
*   `Label(identifier)`：定義跳轉目標點。
*   理論上只要一種條件跳轉加 `Not` 就夠，兩種都有可以讓 `&&`、`||` 的 TACKY 和最後的組語更短。

### 短路邏輯實作 (Short-circuiting)
*   **`a && b`**：
    1.  計算 `a`。
    2.  `JumpIfZero(a, false_label)`。
    3.  計算 `b`。
    4.  `JumpIfZero(b, false_label)`。
    5.  設置結果為 1，`Jump(end_label)`。
    6.  `false_label`: 設置結果為 0。
    7.  `end_label`: 結束。
*   **`a || b`**（書留作練習，改用 `JumpIfNotZero`）：
    1.  計算 `a`。
    2.  `JumpIfNotZero(a, true_label)`。
    3.  計算 `b`。
    4.  `JumpIfNotZero(b, true_label)`。
    5.  設置結果為 0，`Jump(end_label)`。
    6.  `true_label`: 設置結果為 1。
    7.  `end_label`: 結束。
*   **標籤唯一性**：必須使用全局計數器生成唯一標籤（如 `and_false_1`）。
*   標籤會出現在最後的組語裡，只能含字母、數字、`.`、`_`。標籤可以和暫存變數名或使用者函式名重複，code emission 時會加 local label 前綴避開。
*   lemon_cc 對應：`make_label("and_false")`、`"and_end"`、`"or_true"`、`"or_end"`，產生 `<prefix>_<N>`。

---

## 4. 彙編生成 (Assembly Generation)
### 狀態標誌位 (RFLAGS)
x64 指令如 `cmp` 或 `sub` 會更新 `RFLAGS` 暫存器中的標誌位：
*   **ZF (Zero Flag)**：結果為 0 時設置。
*   **SF (Sign Flag)**：結果最高位為 1（有號數為負）時設置。
*   **OF (Overflow Flag)**：發生有符號溢出時設置。
*   `add`、`sub`、`cmp` 會更新這三個標誌位，`mov` 一個都不改。

### 比較指令 `cmp`
*   `cmpl b, a`：計算 `a - b` 並更新標誌位，但不儲存結果。
*   只看 ZF 和 SF 不夠：`a - b` 可能溢位而把 SF 翻掉，這時 OF 也是 1。所以比較 SF 和 OF：`SF == OF` 表示 `a >= b`，`SF != OF` 表示 `a < b`。
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

*   `jmp label` 無條件跳轉：改寫 RIP（下一條要執行的指令位址）。assembler 和 linker 把 label 換成相對位移；label 本身不是指令，CPU 不會執行它。
*   `jCC label` 條件成立才跳，條件和 `setCC` 一樣看 RFLAGS。

### Assembly AST 的更新
```text
instruction = ... | Cmp(operand, operand)
                  | Jmp(identifier)
                  | JmpCC(cond_code, identifier)
                  | SetCC(cond_code, operand)
                  | Label(identifier)
cond_code = E | NE | G | GE | L | LE
```
所有條件跳轉共用 `JmpCC`、所有條件設置共用 `SetCC`，用 condition code 區分。`Label` 在這階段也當成指令放在指令串列裡。

### TACKY → Assembly
| TACKY | Assembly |
| --- | --- |
| `Binary(relational_op, src1, src2, dst)` | `Cmp(src2, src1)`、`Mov(Imm(0), dst)`、`SetCC(cc, dst)` |
| `Unary(Not, src, dst)` | `Cmp(Imm(0), src)`、`Mov(Imm(0), dst)`、`SetCC(E, dst)` |
| `JumpIfZero(condition, target)` | `Cmp(Imm(0), condition)`、`JmpCC(E, target)` |
| `JumpIfNotZero(condition, target)` | `Cmp(Imm(0), condition)`、`JmpCC(NE, target)` |
| `Jump(target)` | `Jmp(target)` |
| `Copy(src, dst)` | `Mov(src, dst)` |
| `Label(identifier)` | `Label(identifier)` |

*   `Cmp(src2, src1)` 的順序是反的，因為 `cmp b, a` 算的是 `a - b`。
*   `Equal`/`NotEqual`/`LessThan`/`LessOrEqual`/`GreaterThan`/`GreaterOrEqual` 對應 `E`/`NE`/`L`/`LE`/`G`/`GE`（lemon_cc：`binary_to_cond`）。
*   `!x` 等價於 `x == 0`，所以 `Not` 也用 `SetCC(E)`。
*   Pseudoregister 替換：`Cmp`、`SetCC` 的運算元也要換成 stack 位址。

---

## 5. 指令修正與代碼發射 (Fix-up & Emission)
### 修正 `cmp` 指令
*   若遇到 `cmpl mem, mem`，需將第一個運算元搬移至 `R10`。
*   若遇到 `cmpl val, imm`，需將立即值搬移至 `R11`（例如 `cmpl %eax, $5` 改成 `movl $5, %r11d`、`cmpl %eax, %r11d`）。
*   沿用第三章的慣例：R10 修第一個運算元，R11 修第二個。

### 1 位元組寄存器處理
`setCC` 指令（如 `sete`, `setl`）**只能操作 1 位元組的目標**，而且不會清掉其他位元組。
*   **正確做法**（書中順序）：
    1.  `cmp ...`
    2.  `movl $0, %eax`（先清空整個暫存器；`mov` 不改 RFLAGS，放在 `cmp` 後面是安全的）。
    3.  `setl %al` (只設置最低 8 位)。
*   目標是記憶體時，`setCC` 只設定該位址的第一個 byte。x64 是 little-endian，第一個 byte 就是最低位，所以先清 0 再 `setCC`，整個 4-byte 值就是 0 或 1。
*   **寄存器別名對照表**（`SetCC` 裡用 1-byte 名稱，其他指令用 4-byte 名稱）：
    *   `EAX` -> `%al`
    *   `EDX` -> `%dl`
    *   `R10` -> `%r10b`
    *   `R11` -> `%r11b`

### 局部標籤 (Local Labels)
*   為了避免符號衝突並友善調試器：
    *   **Linux**: 使用 `.L` 前綴（例如 `.Land_false_0`）。C 識別字不能含 `.`，所以不會和使用者名稱衝突。
    *   **macOS**: 使用 `L` 前綴。使用者名稱都會加 `_`（`main` 變成 `_main`），所以也不會衝突。
*   assembler 不會把 local label 放進 object file 的 symbol table，GDB、LLDB 才不會把它們誤認成函式名。

### 發射格式
| Assembly | 輸出 |
| --- | --- |
| `Cmp(operand, operand)` | `cmpl <operand>, <operand>` |
| `Jmp(label)` | `jmp .L<label>` |
| `JmpCC(cond_code, label)` | `j<cond_code> .L<label>` |
| `SetCC(cond_code, operand)` | `set<cond_code> <operand>` |
| `Label(label)` | `.L<label>:` |

*   condition code 後綴：`E`/`NE`/`L`/`LE`/`G`/`GE` → `e`/`ne`/`l`/`le`/`g`/`ge`。
*   `cmp` 加 `l` 後綴；`setCC` 只支援 1-byte 運算元，不加大小後綴；跳轉和標籤沒有運算元，也不加。

---

## 6. 補充知識：未定義行為 (Undefined Behavior)
*   **整數溢出**：C 語言中，有符號整數溢出是「未定義行為」。編譯器可能會假設溢出永遠不會發生而進行激進優化（例如刪除檢查代碼）。書中的 `for (int i = 2147483646; i > 0; i = i + 1)` 用 `clang -O` 編譯後，`i > 0` 被當成永遠成立而刪掉，變成無窮迴圈。
*   組語裡 OF 被設起來不代表原始程式溢位：用 `cmp` 實作 `a < 10` 可能設 OF，但結果只會是 0 或 1，沒有未定義行為。
*   **未定義 ≠ 未指定 (unspecified)**：第三章的運算元求值順序是 unspecified，可能結果有限；未定義行為一旦發生，整個程式都不能信任。
*   C23 新增 `ckd_add`、`ckd_sub`、`ckd_mul`，超出範圍時回傳錯誤碼，不會變成未定義行為。
*   **短路副作用**：C 標準保證 `&&`、`||` 會短路（§6.5.13 第 4 段、§6.5.14 第 4 段）。若 `&&` 的左側為假，或 `||` 的左側為真，右側的任何表達式（如函數調用、賦值）都不會執行，這在編譯時必須嚴格遵守。

---

## 7. 延伸閱讀資源 (Additional Resources)

### 關於未定義行為 (Undefined Behavior)
*   **John Regehr 的部落格**: [“A Guide to Undefined Behavior in C and C++, Part 1”](https://blog.regehr.org/archives/213)
    *   這篇文章深入淺出地解釋了未定義行為在 C 標準中的含義，以及它對編譯器設計的影響。
*   **Raph Levien 的部落格**: [“With Undefined Behavior, Anything Is Possible”](https://raphlinus.github.io/programming/rust/2018/08/17/undefined-behavior.html)
    *   這篇文章探討了 C 中未定義行為的一些來源，以及它最初是怎麼進入標準的。
*   **Jens Gustedt 的部落格**: [“Checked Integer Arithmetic in the Prospect of C23”](https://gustedt.wordpress.com/2022/12/18/checked-integer-arithmetic-in-the-prospect-of-c23/)
    *   介紹 C23 的 `ckd_add`、`ckd_sub`、`ckd_mul`。

---

## 8. 一句話總結

> 第四章的本質，是用 `cmp` + `setCC` 把比較結果變成 0/1，並在 TACKY 加入 `Jump`、`JumpIfZero`、`JumpIfNotZero`、`Label`、`Copy`，讓 `&&`、`||` 能短路。

## 9. 速記重點

```text
lexer    -> ! && || == != < > <= >=，最長匹配
parser   -> 關係 35、相等 30、&& 10、|| 5，全部左結合；! 在 <factor>
TACKY    -> 關係運算照一般 Binary；&& / || 用 JumpIfZero / JumpIfNotZero + Copy + Label
asm      -> Cmp(src2, src1)、Mov(Imm(0), dst)、SetCC(cc, dst)；JumpIfZero -> Cmp(Imm(0), v) + JmpCC(E)
fix-up   -> cmp 雙記憶體經 R10；cmp 第二個運算元是立即值經 R11
emit     -> cmpl、j<cc> / set<cc>、.L<label>、SetCC 用 %al %dl %r10b %r11b
```
