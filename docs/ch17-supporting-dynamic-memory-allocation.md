# 第十七章：支援動態記憶體配置（Supporting Dynamic Memory Allocation）

## 1. 這章在做什麼

第 16 章已能呼叫 `puts` 這類處理字串的函式。本章補上呼叫記憶體管理函式所需的最後兩項功能：

- `void` 型別：沒有值的型別。可作為函式回傳型別、`void *` 的指向型別，以及 cast 的目標。
- `sizeof` 運算子：在編譯期取得型別或運算式的大小，型別為 `unsigned long`。

`malloc`、`calloc`、`aligned_alloc`、`realloc` 都回傳 `void *`，`free` 接收 `void *` 且回傳 `void`。沒有這兩項功能，就無法宣告或呼叫它們。

本章的關鍵是兩組新分類：**complete / incomplete** 決定能否得知大小，**scalar / non-scalar** 決定能否與零比較。Type checker 要依這兩組分類，從「拒絕不合法的型別」改成「只接受合法的型別」。

> 本篇依原書第 17 章、印刷頁 457–483 整理。目前 lemon_cc 支援到第 16 章，本篇記錄的是本章規則與實作方向，不表示 `main.c` 已完成這些功能。
>
> 配合圖解閱讀：[ch17 視覺化頁面](ch17-supporting-dynamic-memory-allocation.html)。

---

## 2. void：沒有值的型別

### 2.1 標準的定義

C 標準（6.2.5 節第 19 段）：void 型別由空的值集合組成，是一個**永遠無法補完的 incomplete object type**。它沒有值，但 void 運算式仍可為了副作用而求值。

### 2.2 void 函式

回傳型別為 `void` 的函式可用不帶運算式的 `return;` 離開，也可以完全省略 `return`，執行到函式結尾時返回：

```c
void return_nothing(void) {
	return;
}

void perform_side_effect(void) {
	extern int some_variable;
	some_variable = 100;
}
```

### 2.3 產生 void 運算式的三種方式

| 方式 | 例子 |
|---|---|
| 呼叫 void 函式 | `perform_side_effect()` |
| 兩個分支都是 void 的條件運算式 | `flag ? perform_side_effect() : return_nothing()` |
| Cast 成 void | `(void) (1 + 1)` |

Cast 成 void 不影響執行結果，只告訴編譯器與讀者「這個值要丟掉」，常用來消除未使用值的警告。

條件運算式若一邊是 void、另一邊是 int，就不合法：

```c
int i = 0;
flag ? perform_side_effect() : (void) (i = 3);  // 合法：兩邊都是 void
flag ? perform_side_effect() : (i = 3);         // 本書拒絕
```

GCC、Clang 預設不會抱怨第二行，要加 `-pedantic` 才警告。本書的編譯器「永遠 pedantic」，直接拒絕。

### 2.4 void 運算式能出現的四個位置

| 位置 | 例子 |
|---|---|
| 條件運算式的分支 | `flag ? f() : g()` |
| 獨立的運算式敘述 | `perform_side_effect();` |
| `for` 標頭的第一或第三子句 | `for (f(); i < 10; f())` |
| 再 cast 成 void | `(void) perform_side_effect();` |

### 2.5 `(void)` 參數列不是 Void 型別

`int main(void)` 的 `void` 只表示「沒有參數」，不代表一個 void 型別的參數。完整支援 void 型別後，這個特例的處理方式與先前相同：

```text
int main(void);  →  FunType(params=[], ret=Int)   // params 不含 Void
```

---

## 3. 用 void * 管理記憶體

### 3.1 malloc 與 free 的簽章

```c
void *malloc(size_t size);
void free(void *ptr);
```

`size_t` 是實作定義的無號整數型別，System V x64 ABI 下為 `unsigned long`。本書不支援型別別名，所以測試程式改用：

```c
void *malloc(unsigned long size);
```

`malloc` 不知道這塊記憶體將存放什麼型別，回傳 `int *` 或 `char *` 都會誤導，因此回傳 `void *`。**不能透過 `void *` 讀寫記憶體**；要先把位址轉成其他指標型別，才能存取：

```c
int *many_ints = malloc(100 * sizeof (int));  // void * 隱式轉成 int *
many_ints[10] = 10;                           // 像一般 int 陣列一樣下標
free(many_ints);                              // int * 隱式轉回 void *
```

傳給 `free` 的值必須是先前配置函式回傳的同一個位址。`int *` 轉回 `void *` 後，得到的正是 `malloc` 當初回傳的指標。

### 3.2 第三種 storage duration

| Storage duration | 生命週期 | 誰負責追蹤 |
|---|---|---|
| automatic | 單一 block 的執行期間 | 編譯器：堆疊空間 |
| static | 整個程式執行期間 | 編譯器：資料區段 |
| allocated | 從配置到釋放 | 程式設計師與記憶體管理函式庫 |

編譯器要在符號表記錄 automatic 與 static 物件，並為它們保留空間；**allocated 物件則完全不需要編譯器知道**。

---

## 4. Complete、incomplete 與 scalar

### 4.1 Complete 與 incomplete

知道大小的 object type 是 complete，不知道大小的是 incomplete。`void` 是本書第一個 incomplete type，而且永遠無法補完。第 18 章的 incomplete structure type 則可以在之後補完。

C 標準（6.7.2.3 節註腳 132）：incomplete type 只能用在不需要物件大小的地方。例如不能定義 incomplete type 的變數，也不能對它賦值或讀取它的值，因為不知道要讀寫幾個 byte。

**所有指標都是 complete type**，即使所指型別是 incomplete。指標大小固定是 8 bytes，因此 `void *` 可以作為變數、參數、回傳值，也能轉成其他指標型別。

### 4.2 Scalar 與 non-scalar

`&&`、`||`、`!` 的運算元，條件運算式的第一個運算元，以及 `if` 與迴圈的控制條件，都要把值和零比較，因此必須是 scalar：

```text
is_scalar(t):
    match t with
    | Void                      -> False
    | Array(elem_t, size)       -> False
    | FunType(param_ts, ret_t)  -> False
    | _                         -> True
```

先前唯一的 non-scalar 型別是陣列，而陣列在需要 scalar 的地方會轉成指標，所以不會出錯。加入 void 後要明確檢查：void 不是 aggregate type，但也不是 scalar，因為它沒有值。

### 4.3 兩組分類放在一起看

| 型別 | complete？ | scalar？ |
|---|---|---|
| `char`、`int`、`long`、`unsigned`、`double` | 是 | 是 |
| 任何指標，包含 `void *` | 是 | 是 |
| `Array(T, n)` | 是（T 必須 complete） | 否 |
| `void` | 否 | 否 |

Function type 不是 object type，不在這張表內，但 `is_scalar` 對它回傳 False。

---

## 5. sizeof 運算子

### 5.1 兩種寫法

```c
sizeof (long);  // 型別名稱：一定要加括號，結果 8
sizeof 10.0;    // 運算式：可以不加括號，double 為 8
```

### 5.2 陣列不轉成指標

```c
int array[3];
return sizeof array;  // 12，三個 int 的大小；不是指標的 8
```

### 5.3 運算元不求值

C 標準要求不對 `sizeof` 的運算元求值。編譯器只推導運算元的型別，在編譯期算出大小，所以 `sizeof` 不會產生副作用：

```c
return sizeof puts("Shouting into the void");  // 不會呼叫 puts；回傳 4

double *null_ptr = 0;
return sizeof *null_ptr;  // 定義良好：不會真的解參考，回傳 8
```

唯一例外是 variable-length array，大小要在執行期才能算出。本書不支援 VLA，可以忽略。

---

## 6. Lexer 與 parser

### 6.1 Lexer：只加 sizeof

新增關鍵字 `sizeof`。`void` 已經是關鍵字，不用再加。

### 6.2 AST 的四處修改

```text
type      += Void
statement  = Return(exp?)        // 運算式改為可省略
exp       += SizeOf(exp)         // sizeof 運算式
           | SizeOfT(type)       // sizeof (型別名稱)
```

### 6.3 Grammar：把 cast 拆成獨立符號

`sizeof` 不能直接作用在沒加括號的 cast 運算式上：

```c
sizeof (int) a;     // 語法錯誤
sizeof ((int) a);   // 合法
```

這個限制讓 parser 容易分辨 `sizeof` 後面接的是型別名稱還是運算式。Grammar 為此新增 `<type-name>` 與 `<cast-exp>`：

```text
<type-name>   ::= { <type-specifier> }+ [ <abstract-declarator> ]
<exp>         ::= <cast-exp> | <exp> <binop> <exp> | <exp> "?" <exp> ":" <exp>
<cast-exp>    ::= "(" <type-name> ")" <cast-exp>
                | <unary-exp>
<unary-exp>   ::= <unop> <cast-exp>
                | "sizeof" <unary-exp>
                | "sizeof" "(" <type-name> ")"
                | <postfix-exp>
<type-specifier> += "void"
<statement>   ::= "return" [ <exp> ] ";" | ...
```

| 運算子 | 運算元符號 | 能否直接接 cast |
|---|---|---|
| `-`、`~`、`!`、`*`、`&` | `<cast-exp>` | 可以，例如 `-(int) x` |
| `sizeof` | `<unary-exp>` | 不行，要再包一層括號 |

### 6.4 型別 specifier 與參數列

`parse_type` 要拒絕 `void` 與其他型別 specifier 的組合，例如 `void long`、`unsigned void`。其他情況下，`void` 和其他型別一樣可被 pointer、array、function declarator 修飾。

| 宣告 | 語法 | 語意 |
|---|---|---|
| `void *p;` | 合法 | 合法 |
| `void f(void);` | 合法 | 合法 |
| `void x;` | 合法 | Type checker 拒絕 |
| `void arr[3];` | 合法 | Type checker 拒絕 |
| `int f(void a);` | 合法 | Type checker 拒絕 |

解析 `<param-list>` 時，`(` 之後若是 `void`，要再往前看一個 token：

```text
"(" "void" ")"      → 空參數列，params = []
"(" "void" "*" ...  → 參數宣告的開頭，例如 void *p
```

---

## 7. Type checker

### 7.1 void * 的三種隱式轉換

一般而言，不同指標型別之間不能隱式轉換。`void *` 有三個例外，剛好和 null pointer constant 能隱式轉換的情境相同：

| 情境 | 例子 | 轉換方向 |
|---|---|---|
| `==`、`!=` 比較 | `int *a; void *b; a == b` | 非 void 指標轉成 `void *` |
| 條件運算式的兩個分支 | `flag ? a : b` | 非 void 指標轉成 `void *` |
| 賦值，以及「as if by assignment」 | `void *b = a;`、`use_int_pointer(ptr)` | 雙向皆可 |

延伸第 14 章的兩個輔助函式。`get_common_pointer_type` 新增：

```text
else if e1_t == Pointer(Void) and e2_t is a pointer type:
    return Pointer(Void)
else if e2_t == Pointer(Void) and e1_t is a pointer type:
    return Pointer(Void)
```

`convert_by_assignment` 新增：

```text
if target_type == Pointer(Void) and get_type(e) is a pointer type:
    return convert_to(e, target_type)
if target_type is a pointer type and get_type(e) == Pointer(Void):
    return convert_to(e, target_type)
```

這些規則只允許 `void *` 與其他**指標**互轉，不允許和算術型別、陣列型別或 void 互轉。`convert_by_assignment` 本身沒有阻止「把 void 轉成 void」，但其他規則會確保 target type 永遠不是 void，例如 void 參數會先被拒絕。

### 7.2 Return 與回傳型別

| 函式回傳型別 | `return;` | `return 1;` |
|---|---|---|
| `void` | 合法 | 拒絕 |
| 其他型別 | 拒絕 | 合法 |

`void` 函式裡寫 `return (void) 1;` 嚴格來說也不合法。GCC、Clang 接受並在 `-pedantic` 時警告；測試沒有涵蓋，可自行選擇處理方式。

### 7.3 用 is_scalar 檢查控制條件與 cast

`!`、`&&`、`||` 的運算元，條件運算式、`if`、迴圈的條件，都要通過 `is_scalar`：

```text
| Unary(Not, inner) ->
    typed_inner = typecheck_and_convert(inner, symbols)
    if not is_scalar(get_type(typed_inner)):
        fail("Logical operators only apply to scalar expressions")
```

Cast 的規則：任何型別都能 cast 成 void；其他情況下，來源與目標都要是 scalar。

```text
| Cast(t, inner) ->
    typed_inner = typecheck_and_convert(inner, symbols)
    // 先沿用既有檢查：拒絕 double 與指標互轉
    if t == Void:
        return set_type(Cast(t, typed_inner), Void)
    if not is_scalar(t):
        fail("Can only cast to scalar type or void")
    if not is_scalar(get_type(typed_inner)):
        fail("Cannot cast non-scalar expression to scalar type")
    return set_type(Cast(t, typed_inner), t)
```

這同時拒絕 void 轉成非 void 型別，以及轉成陣列、函式型別。第 18 章的 structure 不會轉成指標，也會用到同一套檢查。

### 7.4 Incomplete type 的三個限制

| 限制 | 原因 |
|---|---|
| 不能對指向 incomplete type 的指標做加法、減法、下標 | 不知道元素大小，無法計算 scale |
| 不能對 incomplete type 取 `sizeof` | 大小未知 |
| 陣列的元素型別必須 complete | 無法配置空間 |

> GCC 與 Clang 的語言擴充允許 `void *` 指標運算與 `sizeof (void)`，當作 void 的大小為 1。本書不採用。

```text
is_complete(t):
    return t != Void

is_pointer_to_complete(t):
    match t with
    | Pointer(t) -> return is_complete(t)
    | _          -> return False
```

指標加法、減法、下標中原本檢查「t 是指標」的地方，改用 `is_pointer_to_complete`。

陣列元素檢查要遞迴進入巢狀型別。指標本身雖然是 complete，指向「void 陣列」的指標仍不合法：

```c
void (*ptr_to_void_array)[3];  // Pointer(Array(Void, 3))：拒絕
```

```text
validate_type_specifier(t):
    match t with
    | Array(elem_t, size) ->
        if not is_complete(elem_t):
            fail("Illegal array of incomplete type")
        validate_type_specifier(elem_t)
    | Pointer(referenced_t) -> validate_type_specifier(referenced_t)
    | FunType(param_ts, ret_t) ->
        for param_t in param_ts:
            validate_type_specifier(param_t)
        validate_type_specifier(ret_t)
    | _ -> return
```

在變數宣告、函式宣告、`sizeof` 與 cast 這四個出現型別 specifier 的地方，都要呼叫它。

### 7.5 只對 void 的額外限制

1. 不能宣告 void 變數或 void 參數。
2. 不能解參考 `void *`。

這兩條不適用於第 18 章的 incomplete structure type：structure 變數可以先宣告、補完型別後再定義，指向 incomplete structure 的指標也可以解參考後再取址。

書中「When void Is Valid」方框說明，這兩件事在標準中其實是灰色地帶：

- 標準允許宣告 `extern void` 變數，但 void 運算式不是 lvalue，因此連取址都不行，宣告了也無法使用。GCC、Clang 允許宣告與取址並警告；MSVC 完全不允許宣告。
- 標準沒有明文禁止 void 參數，但 GCC 會警告，Clang、MSVC 直接拒絕。它沒有用途，也和 `(void)` 空參數列的特例衝突。
- `(void) *void_ptr` 依 C 標準委員會成員 Aaron Ballman 的看法，屬於「因省略而未定義」的行為。

嚴格來說 `&*void_ptr` 是合法的，因為 `&` 與 `*` 互相抵銷（第 14 章）。本書仍一律拒絕解參考 `void *`；GCC 會警告，MSVC 拒絕，測試也未涵蓋這個邊角。

### 7.6 條件運算式允許 void 分支

```text
| Conditional(condition, e1, e2) ->
    ...
    if not is_scalar(get_type(typed_cond)):
        fail("Condition in conditional operator must be scalar")
    if t1 == Void and t2 == Void:
        result_type = Void
    else if t1 and t2 are arithmetic types:
        result_type = get_common_type(t1, t2)
    else if t1 or t2 is a pointer type:
        result_type = get_common_pointer_type(typed_e1, typed_e2)
    else:
        fail("Cannot convert branches of conditional to a common type")
```

一邊是 void、另一邊是指標或算術值時，會落到最後一個分支而失敗。

### 7.7 從「排除不合法」改成「接受合法」

先前每個運算式不是算術型別就是指標型別，所以「不是指標」就代表「是算術」。加入 void 後，這個推論不再成立：

```text
| Binary(Equal, e1, e2) ->
    if t1 or t2 is a pointer type:
        common_type = get_common_pointer_type(typed_e1, typed_e2)
    else if t1 and t2 are arithmetic types:
        common_type = get_common_type(t1, t2)
    else:
        fail("Invalid operands to equality expression")
```

例如 `Multiply`、`Divide` 要檢查兩個運算元都是算術型別，而不是檢查「它們不是指標」。所有關係與算術運算都要照這個方向收緊。

### 7.8 sizeof 的型別檢查

`sizeof` 的型別是 `size_t`，本書即 `unsigned long`：

```text
| SizeOfT(t) ->
    validate_type_specifier(t)
    if not is_complete(t):
        fail("Can't get the size of an incomplete type")
    return set_type(e, ULong)
| SizeOf(inner) ->
    typed_inner = typecheck_exp(inner, symbols)  // 不是 typecheck_and_convert
    if not is_complete(get_type(typed_inner)):
        fail("Can't get the size of an incomplete type")
    return set_type(SizeOf(typed_inner), ULong)
```

**`SizeOf` 用 `typecheck_exp` 而不是 `typecheck_and_convert`**，避免陣列轉成指標，`sizeof array` 才會是 12。對 function type 取 `sizeof` 也不合法，但 parser 已經擋掉這個錯誤。

---

## 8. TACKY generation

### 8.1 void 函式：兩個欄位改成可省略

```text
FunCall(identifier fun_name, val* args, val? dst)
Return(val?)
```

呼叫 void 函式時 `dst` 留空；不帶運算式的 `return;` 轉成沒有值的 `Return`。沒寫 `return` 的 void 函式不用特別處理，因為先前每個 TACKY 函式結尾本來就會補一條 `Return`。

### 8.2 Cast 成 void：只處理內部運算式

```text
| Cast(Void, inner) ->
    emit_tacky_and_convert(inner, instructions, symbols)
    return PlainOperand(Var("DUMMY"))
```

回傳哪個運算元都可以，呼叫端不會使用它。

### 8.3 void 條件運算式：不建立 dst

非 void 的條件運算式會建立暫存變數 `dst`，並在兩個分支各發出一條 `Copy`。void 版本保留跳躍結構，但**不建立 void 暫存變數，也不發出 Copy**：

```text
| Conditional(condition, e1, e2) ->
    cond = emit_tacky_and_convert(condition, instructions, symbols)
    instructions.append(JumpIfZero(cond, e2_label))
    if get_type(e) == Void:
        emit_tacky_and_convert(e1, instructions, symbols)
        instructions.append_all([ Jump(end), Label(e2_label) ])
        emit_tacky_and_convert(e2, instructions, symbols)
        instructions.append(Label(end))
        return PlainOperand(Var("DUMMY"))
    else:
        ...  // 和之前相同
```

### 8.4 sizeof 在這一階段變成常數

```text
| SizeOf(inner) ->
    t = get_type(inner)
    return PlainOperand(Constant(ConstULong(size(t))))
| SizeOfT(t) ->
    return PlainOperand(Constant(ConstULong(size(t))))
```

**運算元不會被轉成 TACKY**，所以執行期不會求值。`sizeof (counter = 100)` 只留下 `ConstULong(4)`，不會產生任何 Copy。

指向 void 的指標不需要任何 TACKY 改動。

---

## 9. Assembly generation

Assembly AST 沒有變，後端只有兩處小改動：

| TACKY | 有值 | 沒有值 |
|---|---|---|
| `Return(val?)` | 整數：`Mov(t, val, Reg(AX))` + `Ret`；double：`Mov(Double, val, Reg(XMM0))` + `Ret` | 只有 `Ret` |
| `FunCall(..., dst?)` | 呼叫序列 + 最後 `Mov(<dst type>, <dst register>, dst)` | 呼叫序列，省略最後的 `Mov` |

呼叫序列包含堆疊對齊、移動暫存器參數與 XMM 參數、推入堆疊參數、`Call`，以及釋放參數與 padding。Pseudo replacement、fix-up 與 code emission 都不用改。

---

## 10. 本章範圍與 lemon_cc 的後續改動

### 10.1 本書這一章的範圍

| 功能 | 處理方式 |
|---|---|
| `void` 回傳型別、`void *`、cast 成 void | 本章加入 |
| `sizeof` 運算式與 `sizeof (型別)` | 本章加入，編譯期求值 |
| `void *` 與其他指標隱式互轉 | 比較、條件運算式、as if by assignment |
| `void *` 指標運算、`sizeof (void)` | 拒絕；GCC、Clang 的擴充不採用 |
| void 變數、void 參數、解參考 `void *` | 拒絕，包含 `&*void_ptr` |
| 一邊 void 的條件運算式 | 拒絕 |
| `return (void) 1;` 於 void 函式 | 未測試，可自行決定 |
| `size_t` 型別別名 | 不支援，改用 `unsigned long` |
| Variable-length array 的 `sizeof` | 不支援 VLA，忽略 |
| Structure 與 incomplete structure type | 第 18 章 |

### 10.2 接到目前專案時要改哪些地方

以下是依現有第 16 章架構整理的實作方向，不是已完成的改動清單：

| 階段 | 要延伸的內容 |
|---|---|
| Lexer | `sizeof` 關鍵字 |
| 型別與 parser | `Void` 型別、`void` specifier 組合檢查、`<type-name>`、`<cast-exp>` 分層、`SizeOf` / `SizeOfT`、可省略的 return 運算式、`(void)` 參數列多看一個 token |
| Type checker | `void *` 隱式轉換、return 與回傳型別配對、`is_scalar`、`is_complete`、`validate_type_specifier`、void 變數 / 參數 / 解參考限制、void 條件運算式、收緊算術與比較運算元、`sizeof` 型別檢查 |
| TACKY | `Return(val?)`、`FunCall(..., dst?)`、cast 成 void、void 條件運算式、`sizeof` 轉成 `ConstULong` |
| Assembly generation | 沒有值的 `Ret`、省略呼叫後的 `Mov` |

---

## 11. 用完整例子核對語意

下面使用標準標頭，可交給系統 GCC 驗證：

```c
#include <stdlib.h>

int counter = 0;

void bump(void) {
	counter = counter + 1;
}

int main(void) {
	int array[3];
	int *many = malloc(10 * sizeof (int));  // 配置 40 bytes，void * → int *
	void *raw = many;                       // int * → void *
	many[9] = 9;
	(void) bump();                          // counter = 1
	counter ? bump() : bump();              // void 條件運算式，counter = 2
	unsigned long total = sizeof array + sizeof many + sizeof (counter = 100);
	int result = many[9] + (int) total + counter;
	free(raw);
	return result;
}
```

| 運算式 | 值 | 原因 |
|---|---|---|
| `sizeof array` | 12 | 陣列不轉成指標，3 × 4 bytes |
| `sizeof many` | 8 | 指標大小 |
| `sizeof (counter = 100)` | 4 | 只看型別 int，賦值不執行 |
| `counter` | 2 | 兩次 `bump()`，賦值 100 沒有發生 |
| `result` | 35 | 9 + 24 + 2 |

此例已用系統 GCC 13.3 以 `-std=c17 -Wall -Wextra -Werror -pedantic` 編譯，結束碼 35，見 [example-result.json](../output/ch17-explanation/example-result.json)。目前 lemon_cc（第 16 章）以 `Parse Error: invalid type specifier` 拒絕本例，這是預期中的結果。本書不支援標頭時，會自行宣告 `void *malloc(unsigned long size);` 與 `void free(void *ptr);`。

---

## 12. 實作後的驗證重點

書附測試工具提供下列分階段命令。它們是 ch17 實作完成後的驗收入口，目前尚未宣稱通過：

```sh
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 17 --stage lex
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 17 --stage parse
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 17 --stage validate
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 17 --stage tacky
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 17
```

測試目錄的分工：

| 目錄 | 內容 |
|---|---|
| `invalid_parse` | 錯誤的型別 specifier 組合、格式錯誤的 sizeof |
| `invalid_types/pointer_conversions` | 不合法的 `void *` 轉換 |
| `invalid_types/scalar_expressions` | 需要 scalar 的地方用了 non-scalar |
| `invalid_types/incomplete_types` | 需要 complete type 的地方用了 incomplete type |
| `invalid_types/void` | 其他不合法的 void，例如 void 函式回傳值、比較兩個 void 運算式 |
| `valid/void_pointer` | `void *` 的賦值、比較、轉換，以及呼叫各記憶體管理函式 |
| `valid/void` | void 函式呼叫、cast、條件運算式 |
| `valid/sizeof` | 兩種 sizeof、運算元不求值、各種型別的大小 |
| `valid/libraries` | 與系統 C 編譯器編譯的程式碼互通 |

特別核對這些跨階段條件：

- `int main(void)` 仍是 `params=[]`；`(void *p)` 才是參數。
- `sizeof (int) a` 是 parse error，`sizeof ((int) a)` 合法。
- `sizeof array` 不讓陣列轉成指標。
- `sizeof` 的運算元不產生任何 TACKY 指令。
- `void (*p)[3]` 被 `validate_type_specifier` 遞迴找到並拒絕。
- 一邊 void 的條件運算式、void 變數、解參考 `void *` 都被拒絕。
- `Multiply`、`Equal` 等運算改為接受合法型別，void 運算元不會漏網。
- 呼叫 void 函式不產生 `Mov`，void 條件運算式不建立暫存變數。

---

## 13. 速記重點

```text
void                → 沒有值、永遠 incomplete；只能為副作用求值
void *              → complete、scalar；不能解參考、不能做指標運算
void * ⇄ T *        → 比較、條件運算式、as if by assignment 可隱式轉換
is_scalar           → Void、Array、FunType 為 False
is_complete         → 本章只有 Void 為 False
sizeof              → ULong 常數；運算元不求值、陣列不 decay
sizeof (int) a      → 語法錯誤；sizeof 的運算元是 <unary-exp>
TACKY               → Return(val?)、FunCall(..., dst?)、DUMMY 運算元
後端                → 無值 Return 只發 Ret；無 dst 的呼叫省略 Mov
```

## 14. 參考資料

- Nora Sandler，《Writing a C Compiler》第 17 章，印刷頁 457–483。
- [第十四章：指標](ch14-pointers.md)，`get_common_pointer_type`、`convert_by_assignment` 與 null pointer constant。
- [第十五章：陣列與指標運算](ch15-arrays-and-pointer-arithmetic.md)，陣列轉指標與 `typecheck_and_convert`。
- [第十六章：字元與字串](ch16-characters-and-strings.md)，本章所接續的型別與 static 初始值。
- [ch17 視覺化頁面](ch17-supporting-dynamic-memory-allocation.html)，同一主例的型別分類、記憶體圖與編譯流程。
