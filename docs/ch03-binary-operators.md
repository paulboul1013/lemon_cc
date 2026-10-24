# 第三章：二元運算子（Binary Operators）

## 概述 (Overview)
將為編譯器增加五種基礎的二元運算子支持：
- **算術運算**：加法 (`+`)、減法 (`-`)、乘法 (`*`)、除法 (`/`)、取餘數 (`%`)。
- **核心技術引進**：**優先級爬升算法 (Precedence Climbing)**。這是本書最後一個主要的解析技術，能解決遞迴下降算法在處理二元運算時遇到的「左遞迴」與「歧義性」問題；有了它，之後加新語法都很省力。
- 本章不需要新的編譯階段，只擴充既有的每個階段。

---

## 詞法分析 (The Lexer)
需要新增四個 Token 類型：
1. `+` (加法)
2. `*` (乘法)
3. `/` (除法)
4. `%` (取餘數)

*注意：`-` 符號在第二章已添加。Lexer 階段不需要區分它是「一元負號」還是「二元減號」，這部分交由 Parser 處理。*

---

## 語法分析 (The Parser)

### 3.1 抽象語法樹 (AST) 的更新
表達式 (`exp`) 現在新增了一個 `Binary` 節點：
```rust
exp = Constant(int)
    | Unary(unary_operator, exp)
    | Binary(binary_operator, exp, exp) // 新增

binary_operator = Add | Subtract | Multiply | Divide | Remainder
```

- Parser 依 `-` 出現的位置，把它解析成 `Negate` 或 `Subtract`。
- AST 的形狀決定求值順序：先處理兩個子節點，再處理自己（postorder traversal）。轉 TACKY 時也是先產生兩個運算元的指令，再產生運算本身。
- 沒加括號時靠**優先級**和**結合性**分組：`*`、`/`、`%` 高於 `+`、`-`；本章運算子全部**左結合**，`1 + 2 - 3` 是 `(1 + 2) - 3`。

### 遞迴下降法的困境
如果直接使用標準的 EBNF 語法（例如：`<exp> ::= <exp> <binop> <exp>`）：
1.  **左遞迴 (Left Recursion)**：`parse_exp` 第一步就要呼叫自己，而且還沒吃掉任何 token，會無限遞迴。
2.  **歧義性 (Ambiguity)**：無法確定 `1 + 2 * 3` 應該解析成 `(1 + 2) * 3` 還是 `1 + (2 * 3)`。

### 堪用解法：改寫文法 (Refactoring the Grammar)
每個優先級一條規則：
```text
<exp>    ::= <term> { ("+" | "-") <term> }
<term>   ::= <factor> { ("*" | "/" | "%") <factor> }
<factor> ::= <int> | <unop> <factor> | "(" <exp> ")"
```
- `{ }` 表示重複，parser 再把一串 term 組成左結合的樹。
- 缺點：每多一個優先級就要多一個 symbol 和一個幾乎一樣的 parse 函式（第四章還會再加四層），所以本書改用 precedence climbing。

### 解決方案：優先級爬升 (Precedence Climbing)
這是本章的精華。每個運算子被賦予一個數值優先級：
- `*`, `/`, `%`：優先級 **50**
- `+`, `-`：優先級 **45**

數值本身不重要，只要相對大小正確；留間隔是為了之後加更低優先級的運算子。

**算法核心偽代碼：**
```python
parse_exp(tokens, min_prec):
    left = parse_factor(tokens) # 解析數字、一元運算或括號
    next_token = peek(tokens)
    while next_token 是二元運算子且 precedence(next_token) >= min_prec:
        op = parse_binop(tokens) # 吃掉運算子
        # 左結合：右側的 min_prec = 當前優先級 + 1，同級運算子不會被右側吃進去
        right = parse_exp(tokens, precedence(next_token) + 1)
        left = Binary(op, left, right)
        next_token = peek(tokens)
    return left
```
- 從其他函式呼叫 `parse_exp` 時（包括 `parse_factor` 處理括號），`min_prec` 從 **0** 開始。
- 例：`1 * 2 - 3 * (4 + 5)` 解析成 `Binary(Subtract, Binary(Multiply, 1, 2), Binary(Multiply, 3, Binary(Add, 4, 5)))`。

### 語法規則重構
為了配合算法，語法被拆分為：
```text
<exp>    ::= <factor> | <exp> <binop> <exp>
<factor> ::= <int> | <unop> <factor> | "(" <exp> ")"
<unop>   ::= "-" | "~"
<binop>  ::= "-" | "+" | "*" | "/" | "%"
```
- `<exp>`：處理二元運算，用 precedence climbing 解析。
- `<factor>`：處理最高優先級的項目（整數常數、一元運算子、括號表達式），用遞迴下降解析。
- 一元運算子的運算元從 `<exp>` 改成 `<factor>`：`(1 + 2)` 是 factor，`-1 + 2` 不是，它會解析成 `(-1) + 2`。
- `parse_exp` 和 `parse_factor` 互相遞迴，兩者都回傳 `exp` AST 節點。
- lemon_cc 對應：`parse_exp(int min_prec)`、`parse_factor()`、`get_precedence()`。

---

## TACKY 中介碼生成 (TACKY Generation)
TACKY 指令集更新，以支持二元運算：
- `Binary(binary_operator, val src1, val src2, val dst)`
- `binary_operator = Add | Subtract | Multiply | Divide | Remainder`

轉換方式和一元運算一樣，只是多一個運算元：
```text
Binary(op, e1, e2) ->
    v1 = emit_tacky(e1)
    v2 = emit_tacky(e2)
    dst = Var(make_temporary())
    append Binary(convert_binop(op), v1, v2, dst)
    return dst
```

**運算順序 (Evaluation Order)**：
根據 C 語言標準，同一個運算的子表達式通常是 **unsequenced**：可以用任何順序求值（求值順序是 unspecified，不是 undefined behavior）。如果其中有函式呼叫，則是 **indeterminately sequenced**：誰先都可以，但不能交錯。例如 `printf("Hello, ") + printf("World!")` 可能印出 `Hello, World!` 或 `World!Hello, `。在實作中，我們統一先計算左側，再計算右側；先算右側也一樣正確。
- 一定要先算左側的例外：`&&`、`||`（第四章）、`?:`（第六章）、逗號運算子（本書不實作）。

---

## 組合語言生成 (Assembly Generation)

### x64 算術指令
加減乘的形式都是 `op src, dst`：`src` 可以是 immediate、暫存器或記憶體，`dst` 是暫存器或記憶體，結果存回 `dst`。

| 指令 | 用途 | 限制 / 備註 |
| --- | --- | --- |
| `addl src, dst` | `dst += src` | 不能同時兩個操作數都在內存 |
| `subl src, dst` | `dst -= src` | 注意順序：`subl a, b` 是 `b = b - a` |
| `imull src, dst` | `dst *= src` | **目標 (dst) 必須是寄存器**，不能是內存 |
| `idivl src` | 有符號除法 | 詳見下述 |

### 處理除法與取餘數 (`idivl`)
這是 x64 最機車的指令之一，需要特殊設置：
1.  **被除數準備**：必須佔用 64 位，由 `EDX:EAX` 聯合組成（EDX 是高 32 位，EAX 是低 32 位）。
2.  **符號擴展 (Sign Extension)**：使用 `cdq` 指令將 `EAX` 的符號位擴展到 `EDX`（正數讓 EDX 全 0，負數讓 EDX 全 1）。
3.  **執行指令**：`idivl <divisor>`。唯一的運算元是除數，**不能是立即值**。
4.  **結果獲取**：
    *   商 (Quotient) 存放在 `EAX`。
    *   餘數 (Remainder) 存放在 `EDX`。

```asm
movl    $2, -4(%rbp)
movl    $9, %eax
cdq
idivl   -4(%rbp)     # EAX = 9 / 2，EDX = 9 % 2
```

### Assembly AST 的更新
```text
instruction = Mov(operand src, operand dst)
            | Unary(unary_operator, operand)
            | Binary(binary_operator, operand, operand)   // 新增
            | Idiv(operand)                               // 新增
            | Cdq                                         // 新增
            | AllocateStack(int)
            | Ret
binary_operator = Add | Sub | Mult
reg = AX | DX | R10 | R11        // 新增 DX（除法用）和 R11（fix-up 用）
```

### TACKY → Assembly
| TACKY | Assembly |
| --- | --- |
| `Binary(Divide, src1, src2, dst)` | `Mov(src1, Reg(AX))`、`Cdq`、`Idiv(src2)`、`Mov(Reg(AX), dst)` |
| `Binary(Remainder, src1, src2, dst)` | 同上，最後一條改成 `Mov(Reg(DX), dst)` |
| `Binary(op, src1, src2, dst)`（加減乘） | `Mov(src1, dst)`、`Binary(op, src2, dst)` |

- 運算子對應：`Add → Add`、`Subtract → Sub`、`Multiply → Mult`。
- `src2` 是常數時 `Idiv` 不合法，留給 fix-up 處理。
- Pseudoregister 替換：`Binary` 和 `Idiv` 裡的 pseudoregister 跟 `Mov`、`Unary` 一樣換成 stack 位址，每個新的 pseudoregister 配下一個 4-byte 位置。

---

## 指令修復與程式碼發射 (Fix-up & Emission)

### 無效指令修復 (Instruction Fix-up)
由於 x64 的硬體限制，我們必須在輸出組合語言前修正無效指令：
- **`addl` / `subl`**：如果源和目標都在內存，需先搬移至臨時寄存器 `R10D`。
- **`imull`**：如果目標在內存，必須先搬移至寄存器 `R11D` 計算，再搬回內存。
- **`idivl`**：操作數不能是立即值 (Immediate)。如果遇到 `idivl $3`，先 `movl $3, %r10d`，再 `idivl %r10d`。
- 慣例：**R10 修 source，R11 修 destination**。到 Part II 有時同一條指令兩邊都要修，用兩個暫存器才不會互相覆蓋。

`imull $3, -4(%rbp)` 改寫成：
```asm
movl    -4(%rbp), %r11d
imull   $3, %r11d
movl    %r11d, -4(%rbp)
```

### 程式碼發射 (Code Emission)
更新後的發射格式：
- `movl <src>, <dst>`
- `addl <src>, <dst>`
- `subl <src>, <dst>`
- `imull <src>, <dst>`
- `cdq` (無操作數)
- `idivl <src>`

暫存器輸出：`Reg(AX)` → `%eax`、`Reg(DX)` → `%edx`、`Reg(R10)` → `%r10d`、`Reg(R11)` → `%r11d`。

- `cdq` 不照 `l` 後綴的命名慣例。
- 減法用的 `subl` 和配置 stack 用的 `subq` 是同一個指令的 32 位與 64 位版本。

---

## Extra Credit：位元運算子 (Bitwise Operators)
- `&`、`|`、`^`、`<<`、`>>`，做法和本章的運算子差不多。書沒有給細節，要自己查它們的相對優先級和對應的 x64 指令。
- 選做，之後章節的測試不依賴它。測試時加 `--bitwise`，後面章節也要加，才會跑到用到位元運算的測資。
- lemon_cc 有做：`get_precedence` 裡 `<<`/`>>` 為 40、`&` 為 25、`^` 為 20、`|` 為 15（`<<`/`>>` 在 `+ -` 與關係運算子之間，`& ^ |` 在相等運算子與 `&&` 之間，順序同 C 標準）；組語輸出 `and`、`or`、`xor`、`sal`、`sar`。x64 的 shift 位移量只能是 immediate 或 `%cl`，所以位移量不是常數時先搬進 CX。

---

## 補充資源 (Additional Resources)

### 表達式解析與優先級爬升
- [Parsing Expressions by Precedence Climbing](https://eli.thegreenplace.net/2012/08/02/parsing-expressions-by-precedence-climbing) - Eli Bendersky (本書算法的主要參考)
- [Some Problems of Recursive Descent Parsers](https://eli.thegreenplace.net/2009/03/14/some-problems-of-recursive-descent-parsers) - Eli Bendersky
- [Pratt Parsing and Precedence Climbing Are the Same Algorithm](https://www.oilshell.org/blog/2016/11/01.html) - Andy Chu
- [Precedence Climbing Is Widely Used](https://www.oilshell.org/blog/2017/03/30.html) - Andy Chu

---

## 一句話總結

> 第三章的本質，是用 precedence climbing 一次處理二元運算的優先級與左結合；後端把加減乘寫成 `Mov` + `op src, dst`，除法與取餘數用 `cdq` + `idiv`。

## 速記重點

```text
lexer    -> + * / %（- 沿用第二章）
parser   -> <exp> 用 precedence climbing，<factor> 用遞迴下降
            右側 min_prec = prec + 1；括號內 parse_exp(0)
優先級   -> * / % = 50，+ - = 45
TACKY    -> Binary(op, src1, src2, dst)；運算元求值順序 unspecified
asm      -> Mov(src1, dst) + Binary(op, src2, dst)
            除法：Mov src1 -> AX、Cdq、Idiv src2、取 AX（商）或 DX（餘數）
fix-up   -> idiv 立即值經 R10；add/sub 雙記憶體經 R10；imul 目的地是記憶體經 R11
emit     -> addl / subl / imull / idivl / cdq，%edx、%r11d
```
