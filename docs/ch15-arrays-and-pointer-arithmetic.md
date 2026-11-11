# 第十五章：陣列與指標運算（Arrays and Pointer Arithmetic）

## 1. 這章在做什麼

加入第一種**聚合型別（aggregate type）**：陣列，以及操作陣列需要的功能：

- 固定長度陣列與多維陣列，例如 `int a[3]`、`long matrix[2][4]`。
- 大括號初始值，例如 `{1, 2, 3}`，以及巢狀初始值與補零。
- 下標運算 `a[i]`。
- 指標加減整數、指標相減，以及指標的 `<`、`<=`、`>`、`>=` 比較。

前幾章的整數、`double` 和指標都是**純量型別（scalar type）**，各自表示單一值。陣列則把多個同型別物件連續放在記憶體中。

這章最重要的工作在 **type checker**：把隱式的陣列轉指標明確寫進 AST，並留下計算指標步長所需的型別資訊。TACKY 再把陣列操作拆成位址計算與讀寫；組語階段不需要新增指令，只需要新的運算元與記憶體配置方式。

> 本篇依原書第 15 章整理。`main.c` 已加入本章的陣列與指標運算，並延續前幾章的 extra credit 運算子支援。

---

## 2. 記憶體布局與陣列轉指標

### 2.1 多維陣列也是連續記憶體

```c
int flat[6] = {10, 20, 30, 40, 50, 60};
int grid[2][3] = {{10, 20, 30}, {40, 50, 60}};
```

兩者都佔 24 bytes，純量元素的排列順序相同：

```text
byte offset     0    4    8   12   16   20
內容           10   20   30   40   50   60
grid 的元素    └── 第 0 列 ──┘└── 第 1 列 ──┘
```

差別在**型別如何解讀這塊記憶體**。`flat` 的元素是 `int`，`grid` 的元素是 `int[3]`，所以移動一個元素的距離不同。

```text
size(Array(element, count)) = size(element) × count
```

### 2.2 Array-to-pointer decay

在本章支援的運算式中，陣列除了作為一元 `&` 的運算元，其他使用情境都會轉成「指向第一個元素的指標」。`sizeof` 也是不轉換的例外，但留到第 17 章。

```c
int a[3] = {10, 20, 30};
int *p = a;             // a 轉成 int *，指向 a[0]
int (*whole)[3] = &a;   // &a 是 int (*)[3]，指向整個 a
```

| 運算式 | 型別 | 意義 |
|---|---|---|
| `a`，轉換後 | `int *` | 指向第一個 `int` |
| `&a` | `int (*)[3]` | 指向整個三元素陣列 |
| `a + 1` | `int *` | 往後移動 4 bytes |
| `&a + 1` | `int (*)[3]` | 往後移動 12 bytes，來到整個陣列之後 |

`a` 轉換後和 `&a` 指向相同的起始位置，但**型別不同，指標運算的步長也不同**。陣列物件本身不會變成一個存放位址的指標變數。

### 2.3 多維陣列每次只轉換一層

```c
int grid[2][3];
int (*row)[3] = grid;  // grid 轉成指向第一列的指標
int *cell = grid[0];   // grid[0] 是 int[3]，再轉成 int *
```

`grid` 不會直接轉成 `int **`。`int **` 指向的是指標物件，而 `grid` 裡放的是連續的 `int`，沒有額外的列指標。

---

## 3. 指標運算與下標

### 3.1 加減整數的單位是元素

```text
p + n 的位址 = p 的位址 + n × size(p 指向的型別)
p - n 的位址 = p 的位址 - n × size(p 指向的型別)
```

以 `int grid[2][3]` 為例，`grid + 1` 移動一整列，也就是 12 bytes；`grid[0] + 1` 才是移動一個 `int`，也就是 4 bytes。

### 3.2 下標是指標加法加上解參考

```c
a[i]       // 等同 *(a + i)
grid[i][j] // 等同 *(*(grid + i) + j)
```

因為整數加指標也合法，`i[a]` 和 `a[i]` 等價。type checker 不能假設 `[]` 左邊一定是指標。

下標也適用於一般指標：`int x = 7; int *p = &x;` 之後，`p[0]` 就是 `x`。做指標運算時，單一物件視為長度為 1 的陣列。

### 3.3 指標相減與比較

| 運算 | 條件 | 結果型別 |
|---|---|---|
| `pointer + integer`、`integer + pointer` | 整數可為任何已支援的整數型別 | 原指標型別 |
| `pointer - integer` | 同上 | 原指標型別 |
| `pointer - pointer` | 兩個指標型別相同 | `long` |
| 指標的 `<`、`<=`、`>`、`>=` | 兩個指標型別相同 | `int` |

指標相減回傳**元素索引的差**，不是 byte 數：

```c
int a[5];
long distance = &a[4] - &a[1];  // 3，位址差 12 bytes 再除以 4
```

本書用 `long` 表示指標差，對應目標 64 位元平台常見的 `ptrdiff_t`。目前沒有 `typedef`，所以不處理 `<stddef.h>` 的型別別名。

**合法型別不保證執行期合法**：本章陣列情境中的指標相減與大小比較，必須在同一陣列內，包含尾端後一格的位置。超出這個範圍的使用可能是 undefined behavior，編譯器不新增執行期邊界檢查。

### 3.4 One-past-the-end pointer

對長度為 `n` 的陣列 `a`，可以形成 `a + n`，用來判斷迴圈結束或做合法範圍內的指標運算，但不能解參考它。形成更遠的越界指標本身就可能是 UB。

另一個容易混淆的地方是 null pointer constant：

```c
p == 0;  // 合法，第 14 章已支援
p < 0;   // 本書拒絕，大小比較不接受 null pointer constant 配對
0 - p;   // 非法，整數不能減指標
p - 0;   // 合法，指標減整數，沒有移動
```

---

## 4. 本章的支援範圍

| 功能 | 本章處理方式 |
|---|---|
| 固定長度陣列、多維陣列 | 支援，維度使用整數常數字面值 |
| Variable-length array，例如 `int a[n]` | 不支援 |
| 未指定長度，例如 `int a[]` | 不支援，宣告和定義都要寫維度 |
| Compound literal，例如 `(int[]){1, 2}` | 不支援 |
| 每層陣列都有大括號，例如 `{{1, 2}, {3, 4}}` | 支援 |
| 省略內層大括號，例如二維陣列寫成 `{1, 2, 3, 4}` | 不支援 |
| 純量加大括號，例如 `int x = {1}` | 不支援 |
| Designated initializer，例如 `{[2] = 5}` | 不支援 |
| 元素不足，例如 `int a[3] = {1}` | 支援，其餘元素補零 |
| 結尾逗號，例如 `{1, 2,}` | 支援 |

書中 initializer 文法要求大括號內至少一個 initializer，不包含空的 `{}`。維度不能是浮點常數；標準要求維度大於 0，是否接受零長陣列擴充由實作決定，書附測試不涵蓋。

完全沒有初始值時，automatic 陣列不自動清零；static storage duration 的陣列則全部初始化為零。

---

## 5. Lexer 與 parser

### 5.1 AST 新增三種結構

```text
type        += Array(element_type, element_count)
exp         += Subscript(left, right)
initializer  = SingleInit(expression)
             | CompoundInit(initializer_list)
```

變數宣告的初始值從可選的 `exp` 改成可選的 `initializer`。每個 initializer 和 expression 一樣，都需要型別註記。

```text
int a[2][3]
→ Array(Array(Int, 3), 2)
```

Lexer 新增 `[`、`]` token。

### 5.2 Declarator 延續第 14 章的兩階段解析

先解析宣告的語法結構，再由 `process_declarator` 套用型別推導：

```text
declarator += ArrayDeclarator(inner, count)

遇到 ArrayDeclarator(inner, count)：
    把目前的 base type 包成 Array(base, count)
    再處理 inner
```

例如 `a[2][3]` 的中間結構是：

```text
ArrayDeclarator(ArrayDeclarator(Ident("a"), 2), 3)
```

處理時先把 `Int` 包成 `Array(Int, 3)`，再包成 `Array(Array(Int, 3), 2)`。

| 宣告 | 意義 |
|---|---|
| `int *a[3]` | 三個 `int *` 組成的陣列 |
| `int (*p)[3]` | 指向三個 `int` 組成之陣列的指標 |
| `int a[2][3]` | 兩個 `int[3]` 組成的陣列 |

`[]` 和函式參數列表一樣，是比前綴 `*` 優先的 declarator suffix。abstract declarator 也要支援陣列，才能解析 `(int (*)[3])p` 這種 cast。

### 5.3 下標的優先序高於一元運算子

```text
primary expression → 常數、變數、括號運算式、函式呼叫
postfix expression → primary 後面接零個或多個 [expression]
unary expression   → 前綴運算、cast，或 postfix expression
```

因此 `*a[i]` 是 `*(a[i])`，`(*a)[i]` 才是先解參考再下標。連續的 `a[i][j]` 從左往右建立巢狀 `Subscript`。

lemon_cc 已有 postfix `++`、`--`，實作時需要一起保留在 postfix 解析流程中。

### 5.4 初始值遞迴解析

看到 `{` 就解析 initializer list，否則解析單一運算式。列表裡的每一項仍是 initializer，因此自然支援巢狀大括號與結尾逗號。

Identifier resolution 也要走訪新結構：解析 `Subscript` 的兩個運算元，並遞迴處理 initializer 裡的所有運算式。

---

## 6. Type checker 是本章的核心

### 6.1 新增 typecheck-and-convert 入口

一般運算式在完成型別檢查後，如果結果是 `Array(T, n)`，就包上一個 `AddrOf`，並把新節點的型別標成 `Pointer(T)`。

```text
一般運算式：型別檢查 → 若是陣列，插入轉指標的 AddrOf
顯式 & 的運算元：只做型別檢查，保留陣列型別
```

這裡有兩種 `AddrOf`，雖然都取得起始位址，型別不同：

| 來源 | 原運算元型別 | `AddrOf` 結果型別 |
|---|---|---|
| 隱式 array decay | `Array(T, n)` | `Pointer(T)` |
| 原始碼明寫的 `&` | `Array(T, n)` | `Pointer(Array(T, n))` |

不要對插入的 decay 節點重新套用「顯式 `&`」的結果型別規則。

### 6.2 Lvalue 與陣列賦值

Lvalue 的語法形式增加 `Subscript`，變成 `Var`、`Dereference`、`Subscript`。

但陣列不能整體賦值。依書中做法，先對賦值左側做 typecheck-and-convert，再檢查是否為 lvalue。陣列左側轉換後是 `AddrOf`，不是 lvalue，因此會被拒絕。

```c
int a[2][3];
a[0][1] = 9;  // 合法，左邊是 int 物件
a[0] = 0;     // 非法，左邊是整列陣列，decay 後不是 lvalue
```

### 6.3 指標運算與下標的型別

- 指標加減整數：將索引轉成 `long`，結果保留指標型別。
- 指標相減：兩邊型別必須相同，結果標成 `long`。
- 指標大小比較：兩邊型別必須相同，結果標成 `int`。
- 下標：一邊必須是指標，另一邊必須是整數；索引轉成 `long`，結果型別是指標指向的型別。
- Cast：不能把運算式轉成陣列型別；轉成「指向陣列的指標」則可以。

把索引統一轉成 `long` 是本書的編譯器設計選擇，讓後端以 64 位元索引計算位址，不是 C 規定所有索引必須使用 `long`。

### 6.4 函式參數只調整最外層

```c
int sum(int a[3]);       // 調整為 int sum(int *a)
int sum(int a[2][3]);    // 調整為 int sum(int (*a)[3])
```

這是**宣告中的參數型別調整**，和運算式裡的 array decay 分開處理。只移除最外層陣列，內層維度仍決定步長。

要先調整型別，再檢查同名函式宣告是否相容；AST 與 symbol table 都必須使用調整後的型別。函式不能回傳陣列，但可以回傳指向陣列的指標。

### 6.5 初始值逐層檢查與補零

| 目標型別與初始值 | 處理方式 |
|---|---|
| 純量 + 單一運算式 | 做一般型別檢查，再套第 14 章的 assignment conversion |
| 陣列 + initializer list | 以元素型別遞迴檢查，每層都核對數量 |
| 元素不足 | 依元素型別產生零值，陣列元素則遞迴補零 |
| 元素過多、陣列配單一運算式、純量配列表 | 報錯 |

```c
int a[2][3] = {{7}, {8, 9}};
// 補成 {{7, 0, 0}, {8, 9, 0}}
```

Static 陣列同樣要驗證形狀與型別，元素還必須符合目前 static 初始值的常數限制。最後把巢狀結構攤平成 `static_init` 列表，新增 `ZeroInit(bytes)` 表示一段連續的零 bytes。

```text
static int a[2][3] = {{7}, {8, 9}};
→ [IntInit(7), ZeroInit(8), IntInit(8), IntInit(9), ZeroInit(4)]
```

純量 static 初始值也統一使用單元素列表。用 `ZeroInit` 表示浮點零時，必須保留 `-0.0` 的符號位，不能把它當成全零 bytes。

---

## 7. TACKY：位址計算與物件初始化

### 7.1 新增兩個指令

| 指令 | 意義 |
|---|---|
| `AddPtr(ptr, index, scale, dst)` | `dst = ptr + index × scale`，scale 是編譯期已知的元素 byte 數 |
| `CopyToOffset(src, object, offset)` | 把純量值寫到指定物件起點後 offset bytes 的位置 |

`CopyToOffset` 的目的地是**物件名稱**，不是一個需要讀取內容的指標變數。它用於已知物件的初始化；一般下標存取仍透過指標與 `Load`、`Store`。

### 7.2 三種指標加減分開翻譯

```text
p + n → AddPtr(p, n, size(*p), result)
p - n → 先算 -n，再做 AddPtr
p - q → 先算 byte 差，再以有號 long 除以 size(*p)
```

這裡的 `size(*p)` 是編譯器根據 referenced type 算出的大小，不是新增原始碼的 `sizeof` 功能。

指標相減的中間差值要能表示負數，除法不能因為來源是指標而誤選無號整數除法。

### 7.3 下標先回傳物件的位置

下標先用 `AddPtr` 算位址，再回傳第 14 章的 `DereferencedPointer` 結果，等外層決定怎麼使用：

| 情境 | 後續處理 |
|---|---|
| 讀取純量 `a[i]` | `Load` |
| 寫入 `a[i] = value` | `Store` |
| 取址 `&a[i]` | 直接使用已算好的位址 |
| `a[i]` 仍是陣列，繼續 decay | 同樣保留位址，不讀取整列內容 |

例如 `int grid[2][3]` 的 `return grid[i][j]`，省略索引轉型的 TACKY 可寫成：

```text
base = GetAddress(grid)
row  = AddPtr(base, i, 12)
cell = AddPtr(row, j, 4)
value = Load(cell)
Return(value)
```

多維陣列中間層的「解參考，再取址」互相抵消，最後只需要一次純量 `Load`。不要把連續的二維陣列當成 `int **`，在每層額外讀取不存在的指標。

### 7.4 初始值依照 byte offset 攤平

Automatic 陣列的 initializer 遞迴走訪到純量葉節點，計算每個元素的 byte offset，再產生 `CopyToOffset`。型別檢查階段補出的零值也需要實際寫入。

Static 陣列不產生這些執行期指令，而是把 `static_init` 列表帶入 TACKY 的 `StaticVariable`。Tentative array definition 則以 `ZeroInit(整個陣列的 byte 數)` 初始化。

---

## 8. 組語：新的運算元與陣列配置

### 8.1 三個後端結構

| 結構 | 用途 |
|---|---|
| `Indexed(base, index, scale)` | 位址 `base + index × scale`，輸出為 `(%base, %index, scale)` |
| `PseudoMem(name, offset)` | 尚未配置位址的聚合物件，以及物件內的 byte offset |
| `ByteArray(size, alignment)` | 陣列所需的 byte 數與對齊，後端不再保留巢狀元素型別 |

純量仍使用 `Pseudo`，陣列使用 `PseudoMem`。陣列必須有記憶體位置，即使之後加入暫存器分配也一樣。

### 8.2 AddPtr 使用 lea，但 scale 有限制

x64 indexed addressing 的 scale 只能是 **1、2、4、8**：

- Scale 可直接編碼：把指標與索引放進暫存器，再用 `lea`。
- Scale 是其他值，例如一列三個 `int` 的 12：先用 `imul` 計算索引乘 12，再以 scale 1 做 `lea`。
- Index 是常數：編譯期先算出 `index × scale`，使用固定 displacement 的 `Memory` 運算元。

```asm
# int *p 向後移動 i 個元素，假設 p、i 已在 RAX、RDX
leaq (%rax, %rdx, 4), %rcx

# int (*)[3] 向後移動 i 列
imulq $12, %rdx
leaq (%rax, %rdx, 1), %rcx
```

`CopyToOffset` 則轉成一般 `Mov`，目的地是 `PseudoMem(object, offset)`。指標大小比較使用無號 condition code：`B`、`BE`、`A`、`AE`。

### 8.3 對齊以整個陣列變數為單位

依本書採用的 System V x64 ABI：

- 陣列變數總大小小於 16 bytes：沿用最底層純量元素的對齊。
- 總大小大於或等於 16 bytes：整個陣列變數以 16 bytes 對齊。

**不能把每個內層陣列都補齊到 16 bytes**。例如 `int grid[2][3]` 總大小是 24 bytes，起點要對齊 16，但第二列從 offset 12 開始，列與列之間沒有 padding。

### 8.4 PseudoMem 替換與 fix-up

Stack 配置需要保留整個 `ByteArray` 的空間，再按 alignment 調整物件起點。替換 `PseudoMem` 時，把物件起點相對 RBP 的 offset 加上物件內 offset。

```text
物件起點：Memory(BP, -32)
物件內元素：PseudoMem("a", 8)
替換結果：Memory(BP, -24)
```

Static 陣列起點用既有的 `Data(name)`。本章 static 初始化走資料段，因此不需要用帶非零 offset 的 `PseudoMem` 寫 static 元素；一般存取則先取得指標。

沒有新增指令 fix-up 規則，但判斷「是否為記憶體運算元」的地方必須納入 `Indexed`，既有的 `lea` 目的地暫存器限制也繼續適用。輸出位址中的 base 與 index 都使用 64 位元暫存器名稱。

---

## 9. Code emission 與 static 資料

StaticVariable 的初始值改成列表後，依序輸出每一項。新增 `ZeroInit(n)` → `.zero n`。

```c
int a[4] = {7, 8};
```

在本專案 Linux 目標下，對應的資料可寫成：

```asm
    .globl a
    .data
    .align 16
a:
    .long 7
    .long 8
    .zero 8
```

只以 `ZeroInit` 初始化的整塊物件可放 `.bss`，其他放 `.data`。若保留原本的整數零初始值表示法，也要延續原有的 `.bss` 判斷。

---

## 10. lemon_cc 的改動對照

下表保留從第 14 章到第 15 章的改動脈絡，對應各階段的修改位置。

| 項目 | 第 14 章狀態 | 第 15 章改動 |
|---|---|---|
| Lexer、型別 | 有指標，沒有陣列型別與方括號 token | 新增 `[`、`]`、陣列型別，讓大小與型別相等檢查支援遞迴陣列 |
| `parse_declarator`、`process_declarator` | 已共用一般與 abstract declarator 的流程 | 加入陣列 suffix，保留指標與括號的優先序 |
| `parse_factor` | 已處理一元、cast、前後置遞增遞減 | 整理 primary、postfix、unary 流程，加入連續下標 |
| 變數初始值 | 宣告直接持有 `Exp *init` | 改成可遞迴的 initializer，更新所有走訪與初始化路徑 |
| `typecheck_expression` | 原地修改節點，回傳 `void` | 新增能替換運算式節點的 decay 流程，例如接收 `Exp **`，顯式 `&` 保留原型別 |
| 函式型別 | 支援純量與指標參數 | 相容性檢查前調整陣列參數，禁止回傳陣列 |
| `InitialValue`、TACKY 與組語 static variable | 各自持有單一 `StaticInit` | 改為列表並加入 `ZeroInit`，連同 tentative definition 一起更新 |
| `TackyLvalue` | `gen_tacky_lvalue` 已區分變數與解參考 | 下標回傳相同的位置表示，沿用 load、store 與取址處理 |
| TACKY 指令 | 有 `GET_ADDRESS`、`LOAD`、`STORE` | 加入 `AddPtr`、`CopyToOffset`，區分指標減整數與指標相減 |
| 組語型別與運算元 | 有固定寬度 `AsmType` 與 `Memory` | 加入具大小、對齊資訊的陣列表示，以及 `PseudoMem`、`Indexed` |
| 額外運算子 | 已有 compound assignment、`++`、`--` | 若延續 extra credit，指標 `+=`、`-=`、遞增遞減都必須按元素大小移動；左側只求值一次 |

指標算術必須同時處理型別檢查與步長計算。若只放行型別而沒有計算 scale，`int *p` 的 `p + 1` 就會錯誤地只增加 1 byte。

實作與書中表示法的差異：

- 陣列型別沿用 `Type.referenced` 表示元素型別，另以 `unsigned long count` 保存維度。未使用的巨大 `extern` 陣列不配置空間，因此可以保留超過 32 位元的長度。
- 保留純量指令使用的 `AsmType` enum。書中的 `ByteArray` 資訊改存於 `AsmSymbol` 的大小、對齊與聚合物件欄位，避免每個純量指令都必須攜帶複合型別。
- 本版的實際物件大小與指標步長受既有 `int` byte offset 表示範圍限制。需要配置或計算過大步長時清楚報錯，不把未配置的 `extern` 宣告一併拒絕。
- 下標延伸既有的 `TackyLvalue` 流程。複合賦值、前置與後置遞增遞減共用有型別的算術產生流程，保留左側只求值一次的行為。

---

## 11. 驗證重點

書附測試工具提供分階段執行方式：

```sh
./test_compiler /path/to/your_compiler --chapter 15 --stage lex
./test_compiler /path/to/your_compiler --chapter 15 --stage parse
./test_compiler /path/to/your_compiler --chapter 15 --stage validate
./test_compiler /path/to/your_compiler --chapter 15 --stage tacky
./test_compiler /path/to/your_compiler --chapter 15 --stage codegen
./test_compiler /path/to/your_compiler --chapter 15
./test_compiler /path/to/your_compiler --chapter 15 --extra-credit
```

本專案另有針對巨大維度、求值次數、指標複合賦值與負零的 CLI 回歸測試：

```sh
make -B
python3 tests/ch15_regressions.py ./main
```

第 1～15 章含 extra credit 的 895 個書附測試，以及這 8 個補充回歸測試，已在本章實作提交上通過。

除了章節測試，實作時特別留意這些容易漏掉的情境：

- `int *a[3]` 和 `int (*a)[3]` 得到不同型別。
- 二維陣列、反向下標 `i[a]`，以及取址 `&a[i]`。
- 指標相減得到負值；指向陣列的指標使用整列大小作 scale。
- 巢狀初始值每層補零，元素過多與不合法的大括號結構要拒絕。
- Static 資料列表的總 byte 數等於物件大小，`double` 的 `-0.0` 保留符號。
- 16-byte 對齊只套用整個陣列變數，內層陣列不增加 padding。
- 陣列參數調整後的重複宣告相容，傳入多維陣列時保留內層維度。

---

## 12. 速記重點

```text
型別      → 第一個 aggregate type；Array(T, n)，大小遞迴相乘
布局      → 多維陣列連續排列，內層不插入 padding
decay     → 陣列運算式轉 Pointer(element)，顯式 & 保留整個陣列型別
下標      → a[i] = *(a + i)，也接受 i[a]
指標加減  → 整數索引乘 referenced type 大小；指標差除以同一個大小
parser    → array declarator、遞迴 initializer、postfix subscript
typecheck → 明確插入 decay、索引轉 long、檢查 lvalue、逐層補零
參數      → Array(T, n) 調整為 Pointer(T)，只調整最外層
TACKY     → AddPtr + CopyToOffset；中間列只算位址，最後純量才 Load/Store
static    → 初始值列表 + ZeroInit(bytes)，維持 -0.0 的位元表示
asm       → Indexed + PseudoMem + ByteArray；lea scale 限 1/2/4/8
配置      → 陣列變數總大小 >= 16 bytes 時，起點對齊 16
```

---

## 13. 參考資料

- Nora Sandler，《Writing a C Compiler》，第 15 章 **Arrays and Pointer Arithmetic**，書面頁碼 383–420。來源為 [docs 內的原書 PDF](<Writing a C Compiler Build a Real Programming Language From Scratch (Nora Sandler) (Z-Library).pdf>)。
- 前置內容：[第十四章：指標](ch14-pointers.md)。
- 本章對應的 C17 條文：§6.3.2.1 陣列轉指標、§6.5.2.1 下標、§6.5.6 指標加減、§6.5.8 大小比較、§6.7.6.3 函式參數調整、§6.7.9 初始化。以上語意與本書的支援範圍需分開理解。
