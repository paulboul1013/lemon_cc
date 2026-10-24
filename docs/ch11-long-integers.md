# 第十一章：長整數（Long Integers）

## 1. 這章在做什麼

表面上只多了一個型別 `long` 和顯式轉型 `(long)x` / `(int)x`，但真正的重點是**替 Part II 打好型別系統的地基**：

* 追蹤每個常數、變數、運算式的型別
* 把型別資訊掛在 AST 上
* 找出所有**隱式轉型**，改寫成顯式的 `Cast`
* 讓後端知道每條指令的運算元是 4 bytes 還是 8 bytes

除了 loop labeling 之外，每個階段都要改。

---

## 2. 組語裡的 long

System V x64 ABI：`int` 4 bytes、`long` 8 bytes。

| 大小 | AT&T 名稱 | 後綴 | 暫存器 |
| :--- | :--- | :--- | :--- |
| 4 bytes | longword | `l` | `%eax` |
| 8 bytes | quadword | `q` | `%rax` |

```asm
movl $2, %eax        movq $2, %rax
addl $2, %eax        addq $2, %rax
```

---

## 3. 型別轉換（C 標準 §6.3.1.3）

* **int → long**：值一定放得下，用**符號延伸** `movslq`。
* **long → int**：可能放不下，這是 **implementation-defined**。我們跟 GCC 一樣「對 2^32 取模」，實作上就是**只複製低 4 bytes**（`movl`）。

```text
long 2147483648  = 00000000 ... 10000000 00000000 00000000 00000000
(int) 之後       =               10000000 00000000 00000000 00000000  = -2147483648
```

> implementation-defined ≠ undefined：實作必須有一致、有文件的行為，不能亂來。

---

## 4. Lexer

* 關鍵字 `long` → `TOK_LONG`
* long 常數 `[0-9]+[lL]\b` → `TOK_LONG_CONSTANT`
* `123lx`、`12ll` 這類後綴要在 lexer 就報錯

---

## 5. Parser

### 5.1 型別

書的 AST 改動（Listing 11-2）：

```text
type = Int | Long | FunType(type* params, type ret)
variable_declaration = (identifier name, exp? init, type var_type, storage_class?)
function_declaration = (identifier name, identifier* params, block? body, type fun_type, storage_class?)
exp = ... | Cast(type target_type, exp) | ...
const = ConstInt(int) | ConstLong(int)
```

lemon_cc 的對應結構：

```c
typedef struct Type {
    TypeKind kind;          // TYPE_INT / TYPE_LONG / TYPE_FUNCTION
    struct Type **params;   // 函式參數型別
    int param_count;
    struct Type *ret;       // 函式回傳型別
} Type;
```

AST 和符號表共用同一個 `Type`。變數宣告帶 `var_type`，函式宣告帶 `fun_type`（參數型別 + 回傳型別）。

### 5.2 specifier → 型別（`parse_type`）

| specifier 串列 | 型別 |
| :--- | :--- |
| `int` | `Int` |
| `long`、`long int`、`int long` | `Long` |
| 其他 | 錯誤 |

### 5.3 常數（`parse_constant`）

```text
v > 2^63 - 1                    -> 錯誤
int token 且 v <= 2^31 - 1       -> ConstInt
其他                             -> ConstLong   (沒後綴但超出 int 也會變 long)
```

只檢查上限，因為負號是獨立的 token。

### 5.4 Cast

```text
<factor> ::= "(" { <type-specifier> }+ ")" <factor> | ...
```

`(` 後面接的是 type specifier 就是 cast，否則是括號運算式。

---

## 6. 型別檢查

每個 `Exp` 多一個 `exp_type`，型別檢查時直接標上去；需要轉型的地方用 `convert_to(Exp **slot, Type *t)` 在原位置包一層 `Cast`。

| 運算式 | 結果型別 |
| :--- | :--- |
| 常數 | ConstInt → int、ConstLong → long |
| `Cast(t, e)` | `t` |
| `!`、比較、`&&`、`\|\|` | int（比較的兩邊仍要先轉成共同型別；`&&`、`\|\|` 不轉） |
| `-`、`~` | 運算元的型別 |
| 二元算術、位元運算 | 兩邊的**共同型別**（usual arithmetic conversions） |
| `<<`、`>>` | **左運算元**的型別（不做 usual arithmetic conversions） |
| 賦值 | 右邊轉成左邊型別，結果是左邊型別 |
| `?:` | 兩個分支的共同型別 |
| 函式呼叫 | 每個引數轉成參數型別，結果是回傳型別 |

共同型別目前很單純：

```c
get_common_type(t1, t2) = (t1 == t2) ? t1 : long
```

其他要做的事：

* `return` 的值轉成函式回傳型別
* 自動變數的初始值轉成變數型別
* 重複宣告必須**型別完全相同**（`int x;` 再 `long x;` 要報錯）
* static 初始值在**編譯期**轉型，存成 `StaticInit`（`INIT_INT` / `INIT_LONG`）：

```c
static int i = 2147483650L;   // 存成 IntInit(-2147483646)
```

### Extra credit 的部分

* `a op= b`：在共同型別下計算，再轉回 `a` 的型別
* `++` / `--`：用 lvalue 型別的常數 1
* `switch`：case 值要先轉成 control 的型別才檢查重複，所以 case 收集改到型別檢查**之後**（`collect_program_switch_cases`）

```c
switch (int_value) {
    case 0: ...
    case 17179869184: ...   // 2^34 轉成 int 是 0，和 case 0 重複，要報錯
}
```

---

## 7. TACKY

新增兩條指令：

```text
SignExtend(src, dst)   // int -> long
Truncate(src, dst)     // long -> int
```

* TACKY 的 `Constant` 直接沿用 AST 的 `const`（ConstInt / ConstLong）
* `Cast` 目標型別和原型別相同時，只產生內層運算式，不加轉型指令
* 每個暫存變數都用 `make_tacky_variable(type)` 建立，並把型別寫進符號表，後端靠它決定運算元大小和 stack 空間
* `StaticVariable` 多帶型別和 `StaticInit`；tentative 定義初始化成對應型別的 0
* 函式尾端補上的 `Return` 一律回傳 `ConstInt(0)`

---

## 8. 組語生成

### 8.1 指令帶上大小

```text
AsmType = Longword | Quadword
Mov / Unary / Binary / Cmp / Idiv / Cdq 都帶 AsmType
Movsx(src, dst)        // 固定 4 -> 8
```

* `SetCC`（1 byte）、`Push`（8 bytes）、`Movsx` 不需要帶型別
* `Cdq` 帶型別是因為 32 位元版把 EAX 延伸到 EDX，64 位元版把 RAX 延伸到 RDX
* 大小從運算元推出來：常數看 `ConstInt` / `ConstLong`，變數查符號表
* `SignExtend` → `Movsx`；`Truncate` → `Mov(Longword, src, dst)`
* 兩種常數都轉成 `Imm`，assembler 依指令的 operand size 決定 immediate 大小
* stack 引數：`Reg`、`Imm` 或 Quadword 直接 `Push`；Longword 的記憶體運算元要先 `movl` 進 EAX 再 push RAX
* `StaticVariable` 改帶 alignment（不帶型別）
* `AllocateStack` / `DeallocateStack` 拿掉，改成對 `%rsp` 做一般的 `subq` / `addq`
* 暫存器改成與大小無關的名稱（`REG_AX`、`REG_SP`…），輸出時才依大小選 `%eax` 或 `%rax`

| 原始型別 | AsmType | 對齊 |
| :--- | :--- | :--- |
| int | Longword | 4 |
| long | Quadword | 8 |

### 8.2 後端符號表

`tacky_to_asm` 最後把前端符號表轉成 `AsmSymbol`：

```text
ObjEntry(asm_type, is_static) | FunEntry(defined)
```

pseudo 取代、fix-up、emission 都只查後端符號表。

### 8.3 Pseudo 取代

Longword 配 4 bytes，Quadword 配 8 bytes，而且 **Quadword 必須 8-byte 對齊**：

```text
foo (4 bytes) -> -4(%rbp)
bar (8 bytes) -> -16(%rbp)   // 不是 -12，要往下對齊到 8 的倍數
```

### 8.4 Fix-up 新規則

原有的改寫規則沿用原指令的 operand size。新規則：

| 情況 | 改寫 |
| :--- | :--- |
| `movsx` 來源是 immediate / 目的地是記憶體 | 經過 R10、R11（搬來源用 Longword，搬結果用 Quadword） |
| `addq/subq/imulq/cmpq/pushq` 的 immediate 超出 int 範圍 | 先搬進 R10 |
| `movq` 大 immediate 到記憶體 | 先搬進 R10 |
| `movl` 帶 8-byte immediate | 自己先截斷，避免 GNU as 警告 |

> 為什麼 immediate 不能超過 32 位元？因為這些指令會把 immediate **從 32 位元符號延伸成 64 位元**，`4294967295` 延伸後會變成 `-1`。

---

## 9. 程式碼輸出

| Assembly | 輸出 |
| :--- | :--- |
| `Mov(t, ...)` | `movl` / `movq` |
| `Cdq(Longword)` | `cdq` |
| `Cdq(Quadword)` | `cqo` |
| `Movsx` | `movslq` |
| `IntInit(0)` / `IntInit(i)` | `.zero 4` / `.long i` |
| `LongInit(0)` / `LongInit(i)` | `.zero 8` / `.quad i` |
| 對齊 | `.balign 4` / `.balign 8`（書的 Listing 11-1 用 `.align 8`，只適用 Linux；`.balign` 在 Linux、macOS 都能用） |

```asm
    .data
    .balign 8
var:
    .quad 100
```

---

## 10. 一句話總結

> 第十一章的本質，是讓「型別」從 parser 一路流到 code emission：前端標型別、插 Cast，TACKY 記住暫存變數的型別，後端依型別選 `l` 或 `q`。

## 11. 速記重點

```text
lexer    -> long、123L
parser   -> Type、Cast、ConstInt / ConstLong
typecheck-> 標 exp_type、convert_to 插 Cast、宣告型別要一致
TACKY    -> SignExtend / Truncate、temp 進符號表
asm      -> Longword / Quadword、後端符號表、8-byte 對齊
fix-up   -> 大 immediate 經 R10、movsx 經 R10/R11
emit     -> l / q、cqo、movslq、.quad
```

---
