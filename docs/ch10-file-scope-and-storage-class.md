# 第十章：檔案作用域變數與儲存類別指定詞（File Scope Variable Declarations and Storage-Class Specifiers）

## 1. 這章在做什麼

Part I 的最後一章，新增兩件事：

* **file scope 變數宣告**：變數可以宣告在原始檔的最上層
* **`static` / `extern`**：storage-class specifier，控制宣告的 **linkage** 和物件的 **storage duration**（物件在記憶體裡存在多久）

大部分工作在 semantic analysis：替每個宣告決定 linkage、storage duration、是不是 definition。後端只多了幾個 directive（`.data`、`.bss`、`.globl`…）和一種新運算元。

其他 storage-class specifier（`auto`、`register`、`_Thread_local`、`typedef`）不實作。

---

## 2. 名詞

| 名詞 | 意思 |
| :--- | :--- |
| file / source file | 前處理後的原始檔，也就是 C 標準說的 translation unit |
| static variable | 具有 **static storage duration** 的變數，不一定有寫 `static` |
| automatic variable | 具有 automatic storage duration 的變數；前幾章的變數全是這種 |
| external variable | 具有 internal 或 external linkage 的變數，不一定有寫 `extern` |

> 所有 external variable 都是 static variable，但 static variable 不一定是 external（例如 block scope 的 `static int x;`）。

---

## 3. 三個屬性：scope、linkage、storage duration

| 屬性 | 屬於誰 | 回答的問題 |
| :--- | :--- | :--- |
| scope | identifier（宣告） | 原始碼的哪個範圍可以用這個名字 |
| linkage | identifier（宣告） | 同名的不同宣告，是不是指向同一個實體（函式或物件） |
| storage duration | object（變數） | 物件在程式**執行期間**活多久（lifetime）；**函式沒有** storage duration |

### 3.1 Scope

變數和函式的 scope 規則一樣：可以宣告在 file scope 或 block scope，使用前必須先宣告，可以被之後的 block scope 宣告 shadow。

### 3.2 Linkage（C 標準 §6.2.2）

* **external**：同一個名字在所有檔案裡都指向同一個實體
* **internal**：和 external 一樣會合併，但只在**同一個檔案**內
* **none**：每個宣告都是不同的物件

決定規則：

| 位置 | specifier | linkage |
| :--- | :--- | :--- |
| file scope | 無（變數） | external |
| file scope | `static` | internal |
| block scope | 無（變數） | none |
| block scope | `static`（變數） | none（這裡的 `static` 管的是 storage duration，不是 linkage） |
| 任何地方 | `extern` | 有可見的前一個宣告且它有 linkage → **沿用它的 linkage**；否則 → external |
| 任何地方 | 函式、無 specifier | 當成有寫 `extern` |

* block scope 不能宣告 `static` 函式（函式沒有 storage duration）
* internal 和 external 不會互相 shadow。不同檔案的 `bar`（external）和這個檔案的 `static int bar(void);`（internal）是兩個不相干的函式
* 同一個檔案裡，同一個 identifier 同時有 internal 和 external linkage 是 undefined behavior，大部分編譯器會報錯

`extern` 的幾個例子：

```c
static int a;
extern int a;          // 看得到前一個宣告（internal）→ 也是 internal
```

```c
int a = 4;
int main(void) {
    int a = 3;
    {
        extern int a;  // 看得到的只有 no linkage 的區域 a → external
        return a;      // 指向 file scope 的 a，回傳 4
    }
}
```

```c
int main(void) {
    extern int a;      // 看不到任何宣告 → external
    return a;          // 和下面的定義是同一個物件，回傳 5
}
int a = 5;
```

```c
static int my_fun(void);
int my_fun(void) { return 0; }   // 無 specifier 等於 extern → 沿用 internal
```

所以函式宣告加 `extern` 永遠是多餘的（inline 函式除外，本書不實作）。

### 3.3 Storage duration（C 標準 §6.2.4）

| 種類 | lifetime | 誰是這種 |
| :--- | :--- | :--- |
| automatic | 進入宣告所在的 block 開始，離開 block 結束 | block scope、無 specifier 的變數；函式參數 |
| static | 整個程式執行期間；程式開始前初始化**一次** | 所有 file scope 變數；block scope 有 `static` 或 `extern` 的變數 |

（allocated storage duration 等 Part II 做 `malloc` 時再談；thread storage duration 不實作。）

```c
int recursive_call(void) {
    static int count = 0;   // 只在程式開始前初始化一次
    count = count + 1;      // 每次呼叫都是同一個 count
    ...
}
```

* 執行到 `static` 宣告時**不會**重新初始化。宣告只標記「從這裡開始進入 scope」
* scope 是**原始碼**的範圍，lifetime 是**執行期間**的範圍。automatic 變數兩者幾乎綁在一起，static 變數兩者無關
* 因為在程式開始前初始化，static 變數的初始值必須是**常數**：

```c
int first_var = 3;               // OK
int second_var = first_var + 1;  // 錯誤：不是常數
```

> C 標準允許 constant expression（如 `1 + 1`），本書為了簡單只支援常數值。

---

## 4. Definition vs. Declaration

變數被 define 才要配置空間（可能還要初始化）；只 declare 就交給 linker 去別的 object file 找。可以宣告很多次，但只能定義一次。

| 情況 | 是不是 definition |
| :--- | :--- |
| 有初始值 | 是 |
| no linkage | 是（否則這個宣告完全沒用）。automatic 不初始化；local static 沒寫初始值就**初始化成 0** |
| `extern`、沒有初始值 | 不是 |
| block scope 的 `extern` | 不能有初始值，所以**永遠不是** definition |
| 有 linkage、沒有 `extern`、沒有初始值 | **tentative definition** |

### Tentative definition

```c
int x;                  // 檔案裡沒有其他定義 → 等同 int x = 0;
```

```c
int x;                  // 被當成宣告（像有寫 extern）
int main(void) { return x; }
int x = 3;              // 明確的定義優先，x 初始化成 3
```

```c
int a;
int a;
extern int a;
int a;                  // 三個 tentative + 一個宣告，合法，等同 int a = 0;
```

> **UB 提醒**：tentative 只在自己的檔案內 tentative。另一個檔案有 `int var = 4;`、這個檔案有 `int var;` 是重複定義。GCC/Clang 在特定選項下靠 **common symbol**（binary 層級的 tentative definition）讓它能連結，本書不用 common symbol，所以編不過這種程式。

---

## 5. 總表

### Table 10-1：變數宣告

| Scope | Specifier | Linkage | Storage duration | 有初始值 | 沒有初始值 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| file | 無 | external | static | definition | tentative |
| file | `static` | internal | static | definition | tentative |
| file | `extern` | 沿用前一個可見宣告；預設 external | static | definition | 不是 definition |
| block | 無 | none | automatic | definition | definition（不初始化） |
| block | `static` | none | static | definition | definition（初始化成 0） |
| block | `extern` | 沿用前一個可見宣告；預設 external | static | **不合法** | 不是 definition |

### Table 10-2：函式宣告

| Scope | Specifier | Linkage | 有 body | 沒有 body |
| :--- | :--- | :--- | :--- | :--- |
| file | 無或 `extern` | 沿用前一個可見宣告；預設 external | definition | 不是 definition |
| file | `static` | internal | definition | 不是 definition |
| block | 無或 `extern` | 沿用前一個可見宣告；預設 external | **不合法** | 不是 definition |
| block | `static` | **不合法** | **不合法** | **不合法** |

函式定義的參數：automatic storage duration、no linkage，和 block scope 無 specifier 的變數一樣。

---

## 6. 錯誤情況

### 6.1 宣告衝突

* **同一個 local scope** 宣告兩次，且至少一個 no linkage（之前就會擋）
* **同時有 internal 和 external linkage**，就算兩個宣告離很遠也算：

```c
int main(void) {
    extern int foo;     // 這時看不到其他宣告 → external
    return foo;
}
static int foo = 3;     // internal → 衝突
```

* **型別不同**：external variable 和函式同名

```c
int foo = 3;
int main(void) {
    int foo(void);      // 兩個都有 external linkage，卻一個是變數一個是函式
    return foo();
}
```

> **UB 提醒：GCC 和 Clang 不一致的邊界情況**
>
> ```c
> static int foo = 0;
> int main(void) {
>     int foo = 1;
>     {
>         extern int foo;   // 照規則應該是 external，和第一行衝突
>         return foo;
>     }
> }
> ```
>
> 第一行的 `foo` 被區域的 `int foo` 遮住，`extern` 看不到它，所以照規則是 external。GCC 報錯（作者引用 GCC bug 90472 裡 Joseph Myers 的說明）；Clang 讓這個 `extern` 沿用 internal linkage，當作第一個宣告看得到。本書**照 Clang**，不報錯。書的虛擬碼為什麼自然會得到 Clang 的行為，見 11.4。

### 6.2 重複定義

* 同一個檔案裡 external variable 定義兩次 → 編譯器報錯
* 跨檔案重複定義 → 編譯器抓不到，交給 linker

### 6.3 沒有定義

用到只宣告、沒定義的函式或變數 → link time 錯誤，編譯器不用管。

### 6.4 不合法的初始值

* static 變數的初始值不是常數
* block scope 的 `extern` 變數有任何初始值（常數也不行）

### 6.5 Storage-class specifier 的限制

* 函式參數、`for` 迴圈標頭裡的變數宣告不能有 `static` / `extern`
* block scope 的函式宣告不能有 `static`（可以有 `extern`，但沒作用）

---

## 7. Linkage 和 storage duration 在組語裡

### 7.1 Linkage → `.globl`

external linkage 就輸出 `.globl <name>`，否則不輸出。函式和變數規則相同。label 預設只在這個 object file 內可見。

### 7.2 Storage duration → data / BSS section

* automatic 變數在 stack 上
* static 變數在 **data section**（或 **BSS section**）。這是一整塊不分 frame 的記憶體，呼叫、返回函式都不會被釋放或覆蓋，用名字存取，不需要像 RSP/RBP 那樣的專用暫存器
* assembler 預設寫入 **text section**（機器指令），`.data` 切到 data section

```asm
    .data
    .align 4
var:
    .long 3
```

```asm
    .bss
    .align 4
var:
    .zero 4
```

| directive | 作用 |
| :--- | :--- |
| `.data` / `.bss` / `.text` | 切換要寫入的 section |
| `.align n` | Linux：n-byte 對齊；macOS：**2^n**-byte 對齊（`.align 4` 在 macOS 是 16-byte） |
| `.balign n` | 兩個平台都是 n-byte 對齊 |
| `.long n` | 寫入 32-bit 整數（x64 組語的 long 是 32 bits，和 `movl` 的 `l` 一樣） |
| `.zero n` | 寫入 n 個 0 bytes |

* BSS（Block Started by Symbol）只放**初始化成 0** 的變數。檔案只要記錄 BSS 的大小，不用記內容，省磁碟空間
* 只宣告、沒定義的變數：data 和 BSS 都不輸出任何東西

### 7.3 RIP-relative 定址

```asm
movl $4, var(%rip)
```

* 不能用 RBP/RSP 相對位址（變數不在 stack 上）
* 也不能在 link time 換成絕對位址，因為我們產生的是 **position-independent code**，可以載入到任意位置
* 所以用 RIP（目前指令的位址）去算 data section 裡變數的位址

> **Global Offset Table（GOT）**：另一種在 PIC 裡找 static 變數位址的方法，常用來存取動態連結函式庫的變數。本書不用 GOT，代價是：macOS 上無法存取標準函式庫等動態函式庫定義的全域變數；Linux 上可以編出存取這類變數的執行檔，但編不出使用全域變數的動態函式庫。

---

## 8. Lexer

新增兩個關鍵字：`static`、`extern`。

---

## 9. Parser

### 9.1 AST

```text
program = Program(declaration*)
declaration = FunDecl(function_declaration) | VarDecl(variable_declaration)
variable_declaration = (identifier name, exp? init, storage_class?)
function_declaration = (identifier name, identifier* params,
                        block? body, storage_class?)
storage_class = Static | Extern
```

* 頂層從「函式」改成 `declaration`，變數宣告也能出現在最上層
* 函式和變數宣告都多一個可省略的 `storage_class`

### 9.2 文法

```text
<program> ::= { <declaration> }
<declaration> ::= <variable-declaration> | <function-declaration>
<variable-declaration> ::= { <specifier> }+ <identifier> [ "=" <exp> ] ";"
<function-declaration> ::= { <specifier> }+ <identifier> "(" <param-list> ")" ( <block> | ";")
<param-list> ::= "void" | "int" <identifier> { "," "int" <identifier> }
<specifier> ::= "int" | "static" | "extern"
```

* 新的 EBNF 記號 `{ ... }+`：至少重複一次
* `<param-list>` **沒變**，參數還是只能寫一個 `int`，看到 `static` / `extern` 參數要報錯

### 9.3 Type 和 storage class 可以任意順序

```c
static int a = 3;
int static a = 3;     // 兩個等價
```

所以 type specifier 和 storage-class specifier 合併成同一個 `<specifier>`，先把整串讀進來，再拆：

```text
parse_type_and_storage_class(specifier_list):
    types = []
    storage_classes = []
    for specifier in specifier_list:
        if specifier is "int": types.append(specifier)
        else:                  storage_classes.append(specifier)

    if length(types) != 1:          fail("Invalid type specifier")
    if length(storage_classes) > 1: fail("Invalid storage class")

    type = Int
    storage_class = (length(storage_classes) == 1)
                    ? parse_storage_class(storage_classes[0]) : null
    return (type, storage_class)
```

型別剛好一個，storage class 零或一個。Part II 加入更多 type specifier（`long`、`unsigned`）時，這個結構很好擴充。

### 9.4 函式宣告和變數宣告用同一個函式解析

要讀完整串 specifier 才知道是函式還是變數，所以寫**一個**函式解析兩者，回傳 `declaration`。`for` 的 init 只能是變數宣告：照樣解析整個宣告，是函式宣告就報錯。

> `for` 標頭裡有 storage class、block scope 的 `static` 函式宣告、block scope 的 `extern` 變數有初始值，這幾種錯誤也可以在 parser 擋。但測試把它們放在 `invalid_types`，在 parser 擋的話，跑 `--stage parse` 時那幾個測試會失敗。

---

## 10. Identifier resolution

* external variable 和函式一樣**不改名**
* identifier map 只記 `has_linkage`（有或沒有），**不區分** internal / external，那是 type checker 的事
* file scope 和 block scope 的規則不同，分開處理

### 10.1 File scope 變數

```text
resolve_file_scope_variable_declaration(decl, identifier_map):
    identifier_map.add(decl.name, MapEntry(new_name=decl.name,
                                           from_current_scope=True,
                                           has_linkage=True))
    return decl
```

* 不用產生新名字
* 不用檢查之前的宣告：之前的 file scope 宣告一定也有 linkage，會共用同一個 map entry（其他衝突交給 type checker）
* `static` 與否處理方式相同；file scope 的 `has_linkage` 一律是 True
* 不用遞迴處理初始值：它應該是常數，裡面沒有要改名的變數（不是常數的話 type checker 會抓）

### 10.2 Block scope 變數

```text
resolve_local_variable_declaration(decl, identifier_map):
    if decl.name is in identifier_map:
        prev_entry = identifier_map.get(decl.name)
        if prev_entry.from_current_scope:
            if not (prev_entry.has_linkage and decl.storage_class == Extern):
                fail("Conflicting local declarations")

    if decl.storage_class == Extern:
        identifier_map.add(decl.name, MapEntry(new_name=decl.name,
                                               from_current_scope=True,
                                               has_linkage=True))
        return decl
    else:
        unique_name = make_temporary()
        identifier_map.add(decl.name, MapEntry(new_name=unique_name,
                                               from_current_scope=True,
                                               has_linkage=False))
        --snip--   // 處理初始值，和以前一樣
```

* 同一個 scope 的兩個宣告，只有「前一個有 linkage、這一個是 `extern`」才不衝突，因為兩者指向同一個物件
* `extern` 保留原名；其他（包含 local `static`）照舊改名
* `extern` 變數不該有初始值，不用遞迴處理

### 10.3 函式宣告

幾乎不變，只多一個檢查：block scope 函式宣告有 `static` 就報錯。最方便的位置是檢查「block scope 函式不能有 body」的同一個地方（放在 type checker 或 parser 也可以）。

---

## 11. Type checking

這裡做的大多不是真的「型別檢查」，但 type、linkage、storage class 都記在 symbol table，放在這裡最自然。

### 11.1 Symbol table 的 identifier attributes

```text
identifier_attrs = FunAttr(bool defined, bool global)
                 | StaticAttr(initial_value init, bool global)
                 | LocalAttr

initial_value = Tentative | Initial(int) | NoInitializer
```

| attrs | 用在 |
| :--- | :--- |
| `FunAttr` | 函式 |
| `StaticAttr` | static storage duration 的變數 |
| `LocalAttr` | 函式參數、automatic 變數 |

| initial_value | 意思 |
| :--- | :--- |
| `Initial(i)` | 有初始值的定義 |
| `Tentative` | tentative definition |
| `NoInitializer` | `extern` 宣告，這個檔案沒定義 |

`global` 表示是否全域可見（external linkage），最後決定要不要輸出 `.globl`。每個 symbol 都有 type 和 attrs。

### 11.2 函式宣告

```text
typecheck_function_declaration(decl, symbols):
    fun_type = FunType(length(decl.params))
    has_body = decl.body is not null
    already_defined = False
    global = decl.storage_class != Static

    if decl.name is in symbols:
        old_decl = symbols.get(decl.name)
        if old_decl.type != fun_type:
            fail("Incompatible function declarations")
        already_defined = old_decl.attrs.defined
        if already_defined and has_body:
            fail("Function is defined more than once")

        if old_decl.attrs.global and decl.storage_class == Static:
            fail("Static function declaration follows non-static")
        global = old_decl.attrs.global

    attrs = FunAttr(defined=(already_defined or has_body), global=global)
    symbols.add(decl.name, fun_type, attrs=attrs)
    --snip--
```

* 先暫定：`static` → 不 global；`extern` 或沒寫 → global
* 有前一個宣告就**沿用它的 global**：
  * 這次是 `extern`（明寫或隱含）→ 沿用前一個的 linkage
  * 兩次都是 internal → 沒衝突
  * 前一個 external、這次 `static` → 報錯

### 11.3 File scope 變數

```text
typecheck_file_scope_variable_declaration(decl, symbols):
    if decl.init is constant integer i:  initial_value = Initial(i)
    else if decl.init is null:
        initial_value = (decl.storage_class == Extern) ? NoInitializer : Tentative
    else:
        fail("Non-constant initializer!")

    global = (decl.storage_class != Static)

    if decl.name is in symbols:
        old_decl = symbols.get(decl.name)
        if old_decl.type != Int:
            fail("Function redeclared as variable")
        if decl.storage_class == Extern:
            global = old_decl.attrs.global
        else if old_decl.attrs.global != global:
            fail("Conflicting variable linkage")

        if old_decl.attrs.init is a constant:
            if initial_value is a constant:
                fail("Conflicting file scope variable definitions")
            else:
                initial_value = old_decl.attrs.init
        else if initial_value is not a constant and old_decl.attrs.init == Tentative:
            initial_value = Tentative

    attrs = StaticAttr(init=initial_value, global=global)
    symbols.add(decl.name, Int, attrs=attrs)
```

合併規則：

**global**

| 這次的宣告 | 結果 |
| :--- | :--- |
| `extern` | 沿用前一個的 global |
| 其他，且和前一個不同 | 錯誤：Conflicting variable linkage |

**initial value**（優先順序：明確初始值 > Tentative > NoInitializer）

| 前一個 | 這一個 | 結果 |
| :--- | :--- | :--- |
| `Initial` | `Initial` | 錯誤：重複定義 |
| `Initial` | `Tentative` / `NoInitializer` | 前一個的 `Initial` |
| `Tentative` | `Initial` | 這一個的 `Initial` |
| `Tentative` | `Tentative` / `NoInitializer` | `Tentative` |
| `NoInitializer` | 任何 | 這一個的值 |

### 11.4 Block scope 變數

```text
typecheck_local_variable_declaration(decl, symbols):
    if decl.storage_class == Extern:
        if decl.init is not null:
            fail("Initializer on local extern variable declaration")
        if decl.name is in symbols:
            old_decl = symbols.get(decl.name)
            if old_decl.type != Int:
                fail("Function redeclared as variable")
        else:
            symbols.add(decl.name, Int, attrs=StaticAttr(init=NoInitializer, global=True))

    else if decl.storage_class == Static:
        if decl.init is constant integer i: initial_value = Initial(i)
        else if decl.init is null:          initial_value = Initial(0)
        else: fail("Non-constant initializer on local static variable")
        symbols.add(decl.name, Int, attrs=StaticAttr(init=initial_value, global=False))

    else:
        symbols.add(decl.name, Int, attrs=LocalAttr)
        if decl.init is not null:
            typecheck_exp(decl.init, symbols)
```

* **local `extern`**：沒有初始值、之前不是函式。之前沒宣告過就記成 `NoInitializer` + global；**宣告過就什麼都不做**，local `extern` 永遠不會改變已記錄的初始值或 linkage
* **local `static`**：no linkage，不用管之前的宣告。沒初始值就是 `Initial(0)`（不是 `Tentative`），global = False
* **automatic**：`LocalAttr`，其他和上一章一樣
* `for` 標頭的宣告：呼叫這個函式**之前**先檢查沒有 storage-class specifier

> **推論（書沒有直接寫）：6.1 的邊界情況為什麼會照 Clang 行為**
>
> * identifier resolution：external variable 不改名，所以內層的 `extern int foo` 和 file scope 的 `static int foo` 都解析成同一個名字 `foo`；中間那個 `int foo = 1` 被改成唯一的新名字，不佔 `foo` 這個 key
> * type checking：symbol table 只用名字查，不記「哪個宣告在這裡看得到」。處理 `extern int foo` 時查到 `foo` 已經是 `StaticAttr(Initial(0), global=False)`，照 Listing 10-27 什麼都不做
> * 結果：這個 `extern` 等於沿用 internal linkage，沒有報錯，正是 Clang 的行為
>
> 順序反過來（Listing 10-15，local `extern` 在前、`static` 在後）就會報錯：先記成 `global=True`，之後 file scope 的 `static` 走到 `Conflicting variable linkage`。lemon_cc 實測兩者都如此：邊界情況編譯成功，`foo` 放 `.bss` 且沒有 `.globl`；Listing 10-15 報 conflicting variable linkage；GCC 對邊界情況報錯。

---

## 12. TACKY

```text
program = Program(top_level*)
top_level = Function(identifier, bool global, identifier* params, instruction* body)
          | StaticVariable(identifier, bool global, int init)
```

* `function_definition` 改名成 `top_level`
* `Function` 多一個 `global`（對應 `.globl`），從 symbol table 查
* 新增 `StaticVariable`，external 和 local static 變數都用它表示
* file scope 變數宣告、local 的 `static` / `extern` 宣告**都不產生 TACKY**。local static 的定義被搬到頂層，變成 `StaticVariable`，不是函式裡的指令

走完 AST 之後，多一步掃 symbol table：

```text
convert_symbols_to_tacky(symbols):
    tacky_defs = []
    for (name, entry) in symbols:
        match entry.attrs with
        | StaticAttr(init, global) ->
            match init with
            | Initial(i)    -> tacky_defs.append(StaticVariable(name, global, i))
            | Tentative     -> tacky_defs.append(StaticVariable(name, global, 0))
            | NoInitializer -> continue      // 這個檔案沒定義
        | _ -> continue                      // 不是 static 變數
    return tacky_defs
```

> 現在先處理 AST 或 symbol table 都可以。第 16 章開始必須**先 AST 後 symbol table**，因為轉換 AST 時會往 symbol table 加入新的 static 物件。

---

## 13. 組語生成

### 13.1 Assembly AST

```text
program = Program(top_level*)
top_level = Function(identifier name, bool global, instruction* instructions)
          | StaticVariable(identifier name, bool global, int init)
operand = Imm(int) | Reg(reg) | Pseudo(identifier) | Stack(int) | Data(identifier)
```

* `Data(identifier)`：RIP-relative 存取 data / BSS section
* TACKY → assembly 只是把欄位傳過去：`Function` 多帶 `global`，`StaticVariable` 原樣轉換
* TACKY 的 `Var` 不管 storage duration 一律轉成 `Pseudo`。static 變數不可能放進暫存器，所以這裡的 pseudoregister 用法有點不尋常，但不改名

### 13.2 Pseudo 取代看 storage duration

遇到還沒對應過的 pseudoregister 就查 symbol table：

| 查到的結果 | 換成 |
| :--- | :--- |
| static storage duration | 同名的 `Data` |
| 其他 | 新的 stack slot |
| 不在 symbol table | TACKY 暫存變數（automatic）→ stack slot |

```text
Mov(Imm(0), Pseudo("foo"))  ->  Mov(Imm(0), Data("foo"))
```

static 變數不在 stack 上，**不算進**函式的 stack 大小。

### 13.3 Fix-up

`Data` 也是**記憶體運算元**，所有「兩邊都是記憶體」的規則都適用：

```text
Mov(Data("x"), Stack(-4))
->
Mov(Data("x"), Reg(R10))
Mov(Reg(R10), Stack(-4))
```

其他規則不變。

---

## 14. 程式碼輸出

| Assembly | 輸出 |
| :--- | :--- |
| `Function(name, global, ...)` | `<global-directive>`、`.text`、`<name>:`、`pushq %rbp`、`movq %rsp, %rbp`、指令 |
| `StaticVariable`，init 為 0 | `<global-directive>`、`.bss`、`<alignment-directive>`、`<name>:`、`.zero 4` |
| `StaticVariable`，init 非 0 | `<global-directive>`、`.data`、`<alignment-directive>`、`<name>:`、`.long <init>` |
| global directive | global 為 true 才輸出 `.globl <identifier>` |
| alignment directive | Linux 可用 `.align 4`；macOS 或 Linux 用 `.balign 4` |
| `Data(identifier)` | `<identifier>(%rip)` |
| `Program` | Linux 檔尾加 `.section .note.GNU-stack,"",@progbits` |

* 每個函式前面都要加 `.text`，因為現在也會寫入 data / BSS section
* macOS：symbol 前加底線（`_foo(%rip)`），而且要用 `.balign`（`.align 4` 會變 16-byte 對齊）

global 且非 0 的 static 變數（Linux）：

```asm
    .globl <name>
    .data
    .align 4
<name>:
    .long <init>
```

global 且為 0：

```asm
    .globl <name>
    .bss
    .align 4
<name>:
    .zero 4
```

不是 global 就拿掉 `.globl` 那行。

測試裡有 library + client 的多檔案程式，用來確認 linkage 正確，例如一個檔案可以定義並使用 internal 的變數，即使另一個檔案有同名的 external 變數。

---

## 15. lemon_cc 對應

| 書 | main.c |
| :--- | :--- |
| `static` / `extern` token | `TOK_STATIC`、`TOK_EXTERN` |
| `storage_class` | `StorageClass`：`STORAGE_NONE` / `STORAGE_STATIC` / `STORAGE_EXTERN` |
| specifier 列表 | `is_specifier_token`、`parse_type_and_storage_class`（回傳 `ParsedSpecifiers`） |
| 單一宣告解析 | `parse_declaration`、`parse_program`；`parse_for_init` 擋函式宣告 |
| identifier resolution | `resolve_file_scope_variable_declaration`、`resolve_local_variable_declaration`、`resolve_function_declaration`；block scope `static` 函式在 `resolve_block_items` 擋 |
| `identifier_attrs` | `SymbolAttrType`：`SYMBOL_ATTR_FUNCTION` / `SYMBOL_ATTR_STATIC` / `SYMBOL_ATTR_LOCAL`；`Symbol` 的 `defined`、`global`、`init` |
| `initial_value` | `InitialValueType`：`INITIAL_TENTATIVE` / `INITIAL_VALUE` / `INITIAL_NONE` |
| type checking | `typecheck_function_declaration`、`typecheck_file_scope_variable_declaration`、`typecheck_local_variable_declaration`；`for` 標頭的 storage class 在 `typecheck_for_init` 擋 |
| TACKY | `generate_tacky_function` 從 symbol table 取 `global`；`gen_tacky_decl` 跳過有 storage class 的宣告；`generate_tacky` 最後呼叫 `convert_symbols_to_tacky` |
| `Data` 運算元 | `AS_DATA`（欄位 `data_name`） |
| pseudo 取代 | `replace_pseudos` 查後端符號表的 `is_static`（由 `build_backend_symbols` 建立） |
| fix-up | `is_memory_operand`：`AS_STACK` 或 `AS_DATA` |
| emission | `emit_function`、`emit_static_variable`、`emit_data_op`；`SYMBOL_PREFIX`（macOS 是 `_`） |

和書上第十章版本不同的地方（lemon_cc 已做到第十一章，或刻意擴充）：

* `StaticVariable` 帶型別和 `StaticInit`，對齊用 `type_alignment` 算（4 或 8），long 輸出 `.quad` / `.zero 8`
* 兩個平台都輸出 `.balign`
* `eval_static_initializer` 接受 constant expression 並在編譯期折疊（commit `f9e1926`），書上只接受常數值
* pseudo 取代查的是第十一章加入的後端符號表，不是直接查前端 symbol table；TACKY 暫存變數也登記在符號表裡，所以 `replace_pseudos` 遇到查不到的 pseudoregister 直接報錯，不像書上當成 automatic 處理

相關 commit：

```text
d0a8664 Add static and extern token
dd7046f support file-scope declarations and storage classes
812246d implement TACKY generation for static variables
391df4e add static variable and data operand to assembly AST
89f5f50 replace static pseudoregisters with data operands
21afe1f emit data/bss sections and rip-relative operands
cd73c02 tidy static variable codegen after review
```

---

## 16. 一句話總結

> 第十章的本質，是在 semantic analysis 把 C 混亂的宣告規則整理成 symbol table 裡的三樣資訊：是不是 static 變數、初始值（Initial / Tentative / NoInitializer）、是否 global；後端只要照著它決定放 stack 還是 data/BSS、要不要 `.globl`。

## 17. 速記重點

```text
概念     -> scope 看原始碼、linkage 看同名宣告是否同一實體、storage duration 看物件活多久
static   -> file scope：internal linkage；block scope：static storage duration
extern   -> 沿用前一個可見宣告的 linkage，否則 external；block scope 不能有初始值
函式     -> 沒寫 specifier = extern；block scope 不能 static
tentative-> 有 linkage、沒 extern、沒初始值；沒別的定義就初始化成 0
parser   -> { <specifier> }+ 任意順序、storage_class?、頂層變成 declaration
resolve  -> 有 linkage 不改名；file scope / block scope 分開處理
typecheck-> FunAttr / StaticAttr / LocalAttr；合併 global 和 initial value
TACKY    -> Function 帶 global、StaticVariable 從 symbol table 產生
asm      -> Data(name)；static 變數的 Pseudo 換成 Data；Data 算記憶體運算元
emit     -> .globl、.text、.data + .long、.bss + .zero、.balign、name(%rip)
```
