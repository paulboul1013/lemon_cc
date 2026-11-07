# 第十四章：指標（Pointers）

## 1. 這章在做什麼

加入**指標型別**（`int *`、`double **`……），以及兩個新運算子：

- `&`（取址，address-of）：拿到一個物件的位址。
- `*`（取值，dereference）：透過位址存取物件。

前幾章的型別都是**算術型別**，彼此可以隱式轉換，也都支援加減乘除。指標是第一個**非算術型別**：

- 不能乘除。
- 幾乎不能和其他型別隱式轉換。
- 有一套自己的操作。

這章會碰到編譯器的每一個階段，各階段的重點：

| 階段 | 這章的難點 |
|---|---|
| Parser | **declarator**：`int *(*p)` 這種型別寫法要怎麼解析 |
| Type checker | 新的三類型別錯誤；`lvalue` 檢查從 identifier resolution 搬過來 |
| TACKY | 同一個 `*p` 依照**用法**不同，要產生不同指令（`exp_result`） |
| 組語 | `Stack(n)` 一般化成 `Memory(reg, n)`；新增 `lea` |

指標加減、`<`/`>` 比較留到第 15 章（陣列）；`void *` 留到第 17 章；**不支援函式指標**。

---

## 2. 核心概念：物件、值、lvalue conversion

讀懂這章最重要的是分清楚三個詞：

| 名詞 | 意思 | 例子 |
|---|---|---|
| **值（value）** | 一串位元加上一個型別 | 32 個 1，型別是 `int` → `-1` |
| **物件（object）** | 記憶體中存放值的位置，有固定的**位址**和可以更新的**值** | 變數 `x` |
| **lvalue** | 「可能指向某個物件」的**運算式**（注意：lvalue 不是值） | `x`、`*ptr` |

**lvalue conversion**：在運算式中**使用**物件時，其實是用它目前的值，這個「從物件換成值」的動作就叫 lvalue conversion。

- 不做 lvalue conversion 的情況：當成 `=` 的左邊、當成 `&` 的運算元。這時候在乎的是物件的**位置**。
- 其他情況都會做，例如 `foo(x)`、`x == y`、`a = x` 右邊的 `x`。

```c
int main(void) {
    int x = 0;
    int *ptr = &x;   // &x：x 是 lvalue（不轉換）→ 結果是「值」：x 的位址
    *ptr = 4;        // ptr 做 lvalue conversion 取得位址；*ptr 是物件，被賦值（不轉換）
    return *ptr;     // *ptr 是物件，這次做 lvalue conversion → 值 4
}
```

| 位址 | 內容 |
|---|---|
| `0x7ffeee67b938`（`x`） | `0` → 之後變 `4` |
| `0x7ffeee67b940`（`ptr`） | `0x7ffeee67b938` |

> `&x` 的結果是**值**（指標型別），不是物件；`*ptr` 的結果是**物件**（lvalue）。這個差別決定了後面 type checker 和 TACKY 的做法。

---

## 3. 指標支援哪些操作

### 3.1 Null pointer 與型別轉換

| 轉換 | 隱式 | 顯式（cast） |
|---|---|---|
| null pointer constant → 任何指標 | ✅ | ✅ |
| 其他整數 ↔ 指標 | ❌（GCC 只警告，我們報錯） | ✅，行為等同 `unsigned long` |
| 指標 ↔ 另一種指標 | ❌（`void *` 例外，第 17 章） | ✅ |
| 指標 ↔ `double` | ❌ | ❌ **一律非法** |

- **null pointer constant**：書上只認**值為 0 的整數常數字面值**（`0`、`0ul`），不認 `10 - 10` 或 `(long)0`。
- 整數 ↔ 指標的 cast 等同於轉成 `unsigned long`，所以 `(int *)-1` 會變成 `0xffffffffffffffff`。
- 指標之間互相 cast 是合法的，但**透過轉出來的指標去存取**原本的物件，通常是 UB（strict aliasing，§6.5p6–7）。我們不需要偵測，照樣編譯就好。

### 3.2 比較

- 只支援 `==`、`!=`，而且兩邊型別要相同（或其中一邊是 null pointer constant）。
- 指標可以出現在所有「和 0 比較」的地方：`!`、`&&`、`||`、`?:` 的條件、`if`/迴圈條件。null 視為 0，非 null 視為非 0。

### 3.3 `&*e` 的特例

C 標準 §6.5.3.2p3：`&*e` 的 `&` 和 `*` **都不會被求值**，結果等於 `e`（但不是 lvalue）。

```c
int *null_ptr = 0;
int *ptr2 = &*null_ptr;   // 不會 crash，等同 ptr2 = null_ptr
```

---

## 4. Lexer

只需要 `&` token。lemon_cc 在第 3 章 extra credit（位元運算）已經加了 `&`（`TOK_BITWISE_AND`），`*` 也已經有了，所以 **lexer 不用改**。

---

## 5. Parser：declarator 是這章最難的部分

### 5.1 AST 的變化

```text
type = Int | Long | UInt | ULong | Double
     | FunType(type* params, type ret)
     | Pointer(type referenced)          ← 新增，遞迴定義

exp  = ... | Dereference(exp) | AddrOf(exp)   ← 新增
```

- 由其他型別組出來的型別叫 **derived type**（指標、函式，之後的陣列、struct）。
- 指標指向的型別叫 **referenced type**，例如 `int *` 的 referenced type 是 `int`。
- `*`、`&` 語法上是一元運算子，但書上建議做成**獨立的 exp 節點**，因為 type check 和 TACKY 的處理方式和其他一元運算子差很多。

### 5.2 宣告的結構

```text
static  int    **foo(int a)   ;
└ specifiers ┘ └ declarator ┘
```

- **specifiers** 決定 storage class 和 **basic type**（例如 `int`）。
- **declarator** 是剩下的部分：要宣告的名字，加上要套用在 basic type 上的一連串**型別推導**。

declarator 可以巢狀，括號只是分組：

| 宣告 | 意思 |
|---|---|
| `int var;` | `int` |
| `int *var;` 等於 `int *(var);` | 指向 `int` 的指標 |
| `int **var;` 等於 `int *(*(var));` | 指向「指向 int 的指標」的指標 |
| `int foo(void);` 等於 `int ((foo)(void));` | 回傳 `int` 的函式 |
| `int *arr[3];` 等於 `int *(arr[3]);` | 3 個 `int *` 組成的陣列（第 15 章） |
| `int (*arr)[3];` | 指向「3 個 int 的陣列」的指標（第 15 章） |
| `int (*fptr)(int a);` | 函式指標（**不支援**） |
| `int foo(void)(void);` | 語法正確，但函式不能回傳函式，**語意錯誤** |

讀法：**由外往內**套用推導。函式和陣列的 declarator（後綴）優先度比 `*` 高。

### 5.3 文法

```text
<declarator>        ::= "*" <declarator> | <direct-declarator>
<direct-declarator> ::= <simple-declarator> [ <param-list> ]
<simple-declarator> ::= <identifier> | "(" <declarator> ")"
<param-list>        ::= "(" "void" ")" | "(" <param> { "," <param> } ")"
<param>             ::= { <type-specifier> }+ <declarator>
```

注意 `<param>` 現在也是用 declarator，所以參數可以是指標。

### 5.4 為什麼不能邊解析邊建型別

以 `int (*arr)[3]` 為例，正確的型別是「指向陣列的指標」，必須**先套陣列、再套指標**。但遞迴下降 parser 會先讀到括號裡的 `*`，這時還沒看到 `[3]`，不知道該做「指向什麼」的指標。

**文法辨識的順序和型別推導的順序相反**，所以分兩步：

**第一步：先解析成貼近文法的中間結構**

```text
declarator = Ident(identifier)
           | PointerDeclarator(declarator)
           | FunDeclarator(param_info* params, declarator)
param_info = Param(type, declarator)
```

**第二步：再走訪這個結構，算出型別**

```text
process_declarator(declarator, base_type):
  Ident(name)            → (name, base_type, [])
  PointerDeclarator(d)   → process_declarator(d, Pointer(base_type))
  FunDeclarator(params, d):
      d 必須是 Ident（否則就是「回傳函式的函式」或函式指標 → 報錯）
      逐一 process_declarator 每個參數；參數是函式型別 → 報錯
      → (name, FunType(param_types, base_type), param_names)
```

整個宣告的解析流程：

```text
parse_declaration:
  specifiers  → base_type, storage_class
  declarator  → parse_declarator
  (name, type, params) = process_declarator(declarator, base_type)
  type 是函式 → 函式宣告；否則 → 變數宣告
```

### 5.5 Cast 裡的型別：abstract declarator

cast 裡的型別沒有名字，所以用**abstract declarator**（沒有 identifier 的 declarator）：

```c
(int ***) exp;
(int (*)) exp;
(int *(*)) exp;
```

```text
<factor> ::= ... | "(" { <type-specifier> }+ [ <abstract-declarator> ] ")" <factor>
<abstract-declarator>        ::= "*" [ <abstract-declarator> ] | <direct-abstract-declarator>
<direct-abstract-declarator> ::= "(" <abstract-declarator> ")"

abstract_declarator = AbstractPointer(abstract_declarator) | AbstractBase
```

目前它其實只是在數「有幾層 `*`」。這麼大費周章，是為了第 15 章加上 `(int (*)[3])` 這類陣列寫法時不用重寫。對應的 `process_abstract_declarator` 只回傳型別。

---

## 6. 型別檢查

### 6.1 三類新的型別錯誤

1. **運算子不支援這個型別**：指標不能乘除、不能 `-`、不能 `~`；非指標不能 `*`。
2. **兩個型別不相容**：例如指標和 `double` 比較。原因是指標幾乎不能隱式轉換。
3. **該用 lvalue 的地方沒用 lvalue**：`=` 的左邊、`&` 的運算元。lvalue 的定義擴大成「`Var` 或 `Dereference`」。

> 第 3 類原本是在 identifier resolution 檢查，現在要**搬到 type checker**（resolution 只負責走訪新的 `Dereference`/`AddrOf` 節點）。

### 6.2 `*` 和 `&`

```text
Dereference(inner): inner 必須是 Pointer(t) → 結果型別 t；否則報錯
AddrOf(inner):      inner 必須是 lvalue      → 結果型別 Pointer(inner 的型別)；否則報錯
```

### 6.3 `==`、`!=`、`?:`：common pointer type

```text
is_null_pointer_constant(e): e 是值為 0 的整數常數（int/uint/long/ulong）

get_common_pointer_type(e1, e2):
  型別相同              → 那個型別
  e1 是 null 常數        → e2 的型別
  e2 是 null 常數        → e1 的型別
  否則                   → 報錯（型別不相容）
```

- `==`/`!=`：任一邊是指標就用 `get_common_pointer_type`，否則用原本的 `get_common_type`。轉換後結果型別是 `int`。
- `?:`：兩個分支的規則和 `==` 一樣；條件可以是指標，也可以是算術值。

### 6.4 「as if by assignment」轉換

```text
convert_by_assignment(e, target):
  型別相同                      → 不轉
  兩邊都是算術型別               → convert_to
  e 是 null 常數且 target 是指標 → convert_to
  否則                          → 報錯
```

會用到它的地方：賦值右邊、函式呼叫的引數、變數初始值、`return` 的值。例如 `int *d = 2.0;`、在 `int *` 函式裡 `return 2.0;` 都是錯誤。

### 6.5 其他運算子

| 運算 | 對指標 |
|---|---|
| cast | 指標 ↔ `double` 非法；其餘照舊 |
| `-`、`~` | 非法 |
| `!`、`&&`、`\|\|` | 合法，而且兩邊可以混用指標和算術值 |
| `*`、`/`、`%` | 非法 |
| `+`、`-`、`<`、`>` 等 | 合法但留到第 15 章 |

### 6.6 Static 指標的初始值

- `static int *ptr = 0;` → 用 `ULongInit(0)` 表示（指標就是 64 位元無號整數）。
- `static int *p = &a;` 是合法的 C，但**我們不支援**，因為 static 初始值只接受常數字面值。

---

## 7. TACKY：同一個 `*p`，三種翻譯

### 7.1 新指令

```text
GetAddress(val src, val dst)    dst = &src（src 必須是變數）
Load(val src_ptr, val dst)      dst = *src_ptr
Store(val src, val dst_ptr)     *dst_ptr = src
```

### 7.2 依照用法決定翻譯

| 原始碼 | TACKY |
|---|---|
| `*e` 被拿來用值（lvalue conversion） | `ptr = <e>`；`result = Load(ptr)` |
| `*left = right` | `ptr = <left>`；`val = <right>`；`Store(val, ptr)` |
| `&var` | `result = GetAddress(var)` |
| `&*e` | 只翻譯 `e`，`&` 和 `*` 都不產生指令 |

### 7.3 實作策略：`exp_result`

翻譯一個運算式時，還不知道外層要怎麼用它，所以先回傳一個「**還沒做 lvalue conversion**」的結果：

```text
exp_result = PlainOperand(val)          // 一般的常數或變數
           | DereferencedPointer(val)   // *ptr 指到的那個物件（val 是指標）
```

兩個函式分工：

```text
emit_tacky(e)              → exp_result（不轉換）
emit_tacky_and_convert(e)  → val：
    PlainOperand(v)        → v
    DereferencedPointer(p) → dst = Load(p)；回傳 dst
```

各種運算式的處理方式：

```text
Dereference(inner):
    p = emit_tacky_and_convert(inner)
    → DereferencedPointer(p)

Assignment(left, right):
    lval = emit_tacky(left)                 // 左邊不轉換
    rval = emit_tacky_and_convert(right)    // 右邊要轉換
    PlainOperand(obj)      → Copy(rval, obj)；回傳 lval
    DereferencedPointer(p) → Store(rval, p)；回傳 PlainOperand(rval)

AddrOf(inner):
    v = emit_tacky(inner)                   // 不轉換
    PlainOperand(obj)      → dst = GetAddress(obj)；回傳 PlainOperand(dst)
    DereferencedPointer(p) → 回傳 PlainOperand(p)    // &*p 就是 p
```

- 其他所有運算式：子運算式一律改用 `emit_tacky_and_convert`，回傳值包成 `PlainOperand`。
- 「完整運算式」（if/迴圈條件、return 值等）一定要轉換。結果沒被用到的運算式（運算式敘述、for 的第一和第三段）可以只呼叫 `emit_tacky`，省掉一個 `Load`。
- **cast**：指標在轉換時一律當成 `unsigned long`，例如 `int` → 指標用 `SignExtend`，指標 → `int` 用 `Truncate`。

> 之後的陣列索引 `a[i]`、struct 的 `p->x` 也都會產生 `DereferencedPointer`，所以這個設計現在就把它們統一了。

---

## 8. 組語

### 8.1 `Stack` → `Memory`

到目前為止，組語其實一直在讀寫記憶體（`-4(%rbp)`），只是基底一定是 RBP。現在把它一般化：

```text
operand = Imm(int) | Reg(reg) | Pseudo(identifier) | Memory(reg, int) | Data(identifier)
reg     += BP
```

原本所有 `Stack(n)` 都**機械式地**換成 `Memory(BP, n)`。`movq (%rax), %rcx` 就是 `Memory(AX, 0)`。

### 8.2 新指令與翻譯

| TACKY | 組語 |
|---|---|
| `Load(ptr, dst)` | `Mov(Quadword, ptr, Reg(R))`；`Mov(<dst 型別>, Memory(R, 0), dst)` |
| `Store(src, ptr)` | `Mov(Quadword, ptr, Reg(R))`；`Mov(<src 型別>, src, Memory(R, 0))` |
| `GetAddress(src, dst)` | `Lea(src, dst)` |

- 搬**指標**進暫存器一律用 `Quadword`（8 bytes）；透過指標讀寫的**值**用值本身的型別。
- `R` 可以是任何一般暫存器，但避開 R10、R11（fix-up 用）和 callee-saved 暫存器。書上用 AX。
- **`lea`（load effective address）**：把來源運算元的**位址**存到目的地，來源必須是記憶體運算元。
  - `lea (%rbp), %rax` 等於 `mov %rbp, %rax`。
  - `lea x(%rip), %rax` 可以拿到 static 變數 `x` 的位址。
  - 目前所有 pseudo 都放在記憶體，而 type checker 擋掉了 `&常數`，所以來源一定是記憶體。Part III 做暫存器分配時要特別處理。

### 8.3 其餘指標操作都當成 `unsigned long`

型別是 Quadword、對齊 8；比較用 `cmp`；回傳值放 RAX；參數走一般暫存器。

### 8.4 Pseudo 替換和 fix-up

- Pseudo 替換：用 `Memory` 取代 `Stack`，並處理 `lea` 的運算元。指標 pseudo 和 Quadword 一樣配置 8 bytes。
- `lea` 的目的地必須是暫存器（經過 R11）。
- 新規則：`push` 不能推 XMM 暫存器，要改寫成：

  ```asm
  subq  $8, %rsp
  movsd %xmm0, (%rsp)
  ```

  目前不會產生這種指令，Part III 做暫存器分配之後才會用到。

### 8.5 Code emission

| 組語 AST | 輸出 |
|---|---|
| `Lea(src, dst)` | `leaq <src>, <dst>` |
| `Reg(BP)` | `%rbp` |
| `Memory(reg, n)` | `n(<reg>)`；n 是 0 時可以省略 |

`Memory` 的基底暫存器和 `lea` 的目的地一律用 8-byte 名稱，因為位址是 8 bytes。

---

## 9. 對 lemon_cc 的影響（實作前的盤點）

以下是對照目前 `main.c` 的觀察，供實作時參考：

| 項目 | 目前的狀態 | 需要的改變 |
|---|---|---|
| Lexer | `&` 已是 `TOK_BITWISE_AND`，`*` 是 `TOK_MULT` | 不用改，parser 在一元位置把它們當成 `&`/`*` |
| 型別 | `Type` 已有 `params`/`ret`（函式用） | 加 `TYPE_POINTER` 和 `referenced` 欄位；`type_equal` 要遞迴比較 |
| 宣告解析 | `parse_type_and_storage_class` 之後直接讀 identifier | 改成 declarator 中間結構 + `process_declarator`；參數也要用 declarator |
| lvalue 檢查 | 在 identifier resolution（`resolve_expression` 裡的 `Invalid lvalue ...`），只接受 `EXP_VAR` | 搬到 type checker，接受 `Var` 和 `Dereference` |
| TACKY | `gen_tacky_exp` 回傳 `TackyVal *`；compound assign 和 `++`/`--` 直接用 `lvalue->var_name` | 改成 `exp_result`；compound/`++` 要支援 `*p += 1`，而且左邊**只能求值一次**（測試 `eval_compound_lhs_once`） |
| 組語運算元 | `AS_STACK` + `stack_offset` | 換成 `Memory(reg, offset)`，加 `REG_BP` |

**Extra credit 測試**（本章 `--extra-credit`）主要測：指標不能做位元運算、不能 `%=`/`*=`/`/=`、不能 `switch`；`*p` 可以做位元運算、compound 賦值和 `++`/`--`；`&label`、`*label` 是錯誤。

---

## 10. 一句話總結

指標讓「物件」和「值」的差別浮上檯面：parser 要用 declarator **先解析再推導型別**；type checker 要分清楚 lvalue、處理 null pointer constant 和 as-if-by-assignment 轉換；TACKY 用 `exp_result` 延後決定 `*p` 是要 `Load`、`Store` 還是直接拿位址；組語則把 `Stack` 一般化成 `Memory(reg, n)` 並加入 `lea`。

---

## 11. 速記重點

```text
概念      -> 值 vs 物件；lvalue = 可能指向物件的運算式；= 左邊和 & 運算元不做 lvalue conversion
lexer     -> 不用改（& 和 * 已存在）
parser    -> Pointer(type)、Dereference、AddrOf；declarator → 中間結構 → process_declarator
             cast 用 abstract declarator（*、(*)）
typecheck -> * 只能用在指標；& 只能用在 lvalue（Var / Dereference）
             null pointer constant = 值為 0 的整數字面值
             ==、!=、?: 用 get_common_pointer_type；賦值/引數/初始值/return 用 convert_by_assignment
             指標 ↔ double 一律非法；指標不能 - ~ * / %
TACKY     -> GetAddress / Load / Store；exp_result = PlainOperand | DereferencedPointer
             &*e = e；指標 cast 當成 unsigned long
asm       -> Stack(n) → Memory(BP, n)；Load/Store 先把指標 movq 進暫存器；GetAddress → leaq
             指標其餘一律當 Quadword / unsigned long
fix-up    -> lea 目的地要暫存器；push XMM 改寫成 sub + movsd
```

---

## 12. 參考資料

### 本章引用

- Steve Friedl，〈[Reading C Type Declarations](http://unixwiz.net/techtips/reading-cdecl.html)〉（2003）。書上推薦用它來理解 declarator 語法，比標準好讀。

### 本章引用的 C 標準條文

書上以 **C17（ISO/IEC 9899:2018）** 為準。條文編號在下面的草稿版本中可以找到：

| 條文 | 內容 |
|---|---|
| §5.1.2.3 ¶2 | 求值 lvalue 就是「決定它指向哪個物件」 |
| §6.3.2.1 ¶1 | lvalue 的定義：可能指向物件的運算式 |
| §6.5 ¶6–7 | 有效型別（effective type）與 strict aliasing 規則 |
| §6.5.3.2 ¶3 | `&*e` 中的 `&` 和 `*` 都不求值 |
| §6.7.6 | declarator 的完整定義 |

### C 標準取得方式（書中前言提供）

- [C17 正式版（ISO/IEC 9899:2018，ISO 販售）](https://www.iso.org/standard/74528.html)
- [N2310：免費草稿](https://www.open-std.org/JTC1/SC22/WG14/www/docs/n2310.pdf)：C23 的早期草稿，有標出和 C17 的差異，書上建議本專案用這份。
- [N3096：C23 接近定稿的草稿](https://open-std.org/JTC1/SC22/WG14/www/docs/n3096.pdf)
- [cppreference：C23 變更列表](https://en.cppreference.com/w/c/23)
