# 第十六章：字元與字串（Characters and Strings）

## 1. 這章在做什麼

第 15 章已能處理陣列與指標。本章加入三種 1-byte 整數型別，以及表示文字所需的常數與初始化規則：

- `char`、`signed char`、`unsigned char`。
- 字元常數，例如 `'a'`、`'\n'`。
- 字串字面值，例如 `"abc"`，以及相鄰字串的合併。
- 用字串初始化字元陣列，或取得靜態字串物件的位址。

本章的關鍵是區分**字元值、陣列內容與指標位址**。字元運算大多沿用既有的整數處理；字串則需要 type checker 按使用位置分流，並讓符號表、TACKY 與資料輸出階段保存常數物件。

> 本篇依原書第 16 章、印刷頁 423–454 整理。目前 lemon_cc 支援到第 15 章，本篇記錄的是本章規則與實作方向，不表示 `main.c` 已完成這些功能。
>
> 配合記憶體圖閱讀：[ch16 視覺化頁面](ch16-characters-and-strings.html)。

---

## 2. 字元型別、字元常數與整數提升

### 2.1 三種字元型別彼此不同

本書的 x86-64 目標使用 8-bit byte，並將普通 `char` 當成有號型別：

| 型別 | 大小 | 本書的數值範圍 | 擴大成 int |
|---|---|---|---|
| `char` | 1 byte | -128 到 127 | 符號延伸 |
| `signed char` | 1 byte | -128 到 127 | 符號延伸 |
| `unsigned char` | 1 byte | 0 到 255 | 零延伸 |

普通 `char` 是否有號由實作決定，不能把本書的選擇當成所有 C 平台的規則。`char` 與 `signed char` 即使具有相同範圍，仍是不同型別：

```c
char c;
signed char c;  // 同一個全域物件的宣告型別衝突
```

這和 `int`、`signed int` 指定相同型別的情況不同。

### 2.2 單引號常數的型別是 int

在本章採用的 C17 規則下，`'a'` 的型別是 `int`，值為 ASCII 編碼 97。它不會因為寫在單引號內，就具有 `char` 型別。

```c
char c = 'a';  // 先有 int 值 97，再轉換並存入 1-byte 物件 c
```

也要分清數值零與字元零：

| 寫法 | 值 | 用途 |
|---|---|---|
| `0` | 0 | 可作為字串結尾的零 byte |
| `'0'` | 48 | 可見的數字字元 |
| `'\n'` | 10 | 換行字元 |

### 2.3 先提升，再運算

字元參與 usual arithmetic conversions，或作為一元 `-`、`~` 的運算元時，要先做 integer promotions。本書的 `int` 能容納三種字元型別的全部數值，因此全部提升成 `int`。

```c
unsigned char u = 255;
int sum = u + 1;               // 256，運算不是在 unsigned char 寬度內進行
unsigned char wrapped = sum;   // 轉回 unsigned char，256 modulo 256 得到 0
```

擴展時依來源型別保留原本的數值。以下都是十六進位 byte，最低位在左：

```text
signed char   -1：FF → FF FF FF FF：int -1
unsigned char 255：FF → FF 00 00 00：int 255
```

Type checker 的 `get_common_type` 先把字元型別視為 `Int`，再沿用既有的共同型別判定。建立 typed AST 時，也要插入對應轉型，不能只更改結果型別。

---

## 3. 同一個字串字面值有兩種用途

### 3.1 初始化陣列：填入字元內容

```c
char a[4] = "abc";
a[0] = 'x';
```

這裡建立自己的可寫陣列 a，再把字元與結尾零填入其中。初始化不需要另外保留一份唯讀字串物件。

```text
元素       a[0]  a[1]  a[2]  a[3]
初始化後    97    98    99     0
改寫後     120    98    99     0
```

### 3.2 一般運算式：代表可取址的字串物件

```c
char *p = "abc";
```

`"abc"` 具有 `char[4]` 型別，代表具有 static storage duration 的字串物件。一般使用時，它沿用第 15 章的 array-to-pointer decay，轉成指向第一個字元的 `char *`。

```text
p：存放位址 ──→ 字串物件：[97][98][99][0]
```

本書把這個字串物件放在唯讀區段。`p[0] = 'x'` 即使能通過編譯，修改字串字面值物件仍屬於 undefined behavior。指標型別是 `char *`，不代表所指物件可修改。

字串字面值也是 lvalue，可以顯式取址。此時保留整個陣列型別：

```c
char (*whole)[4] = &"abc";  // char (*)[4]，指向整個四元素陣列
```

`p + 1` 前進 1 byte，`whole + 1` 前進 4 bytes。一般轉指標與顯式取址的差異，和第 15 章相同。

### 3.3 容量決定是否保留結尾零

假設解碼後的實際字元數為 L，目標陣列容量為 N：

| 條件 | 結果 |
|---|---|
| N < L | 拒絕，實際字元放不下 |
| N = L | 合法，省略結尾零 |
| N > L | 合法，剩下 N - L 格全部補零 |

```c
char a[3] = "abc";  // 97, 98, 99
char b[4] = "abc";  // 97, 98, 99, 0
char c[6] = "abc";  // 97, 98, 99, 0, 0, 0
```

a 是合法的字元陣列，卻不是以零終止的字串。不能直接交給 `puts` 這類以結尾零判斷停止位置的函式，否則可能讀出物件範圍。

字串可以初始化任一字元型別的陣列，包括 `signed char[N]` 與 `unsigned char[N]`。但一般字串運算式轉換後是 `char *`，不能隱式轉成 `signed char *` 或 `unsigned char *`。

---

## 4. Lexer 與 parser：先解碼，再計算長度

### 4.1 新 token 與跳脫序列

Lexer 新增 `char` 關鍵字、字元常數 token 與字串 token。字元常數包含一個普通字元或一個跳脫序列；字串則可以包含零個或多個字元。

本章支援的簡單跳脫序列如下：

| 跳脫 | ASCII 值 | 意義 |
|---|---|---|
| `\'` | 39 | 單引號 |
| `\"` | 34 | 雙引號 |
| `\?` | 63 | 問號 |
| `\\` | 92 | 反斜線 |
| `\a` | 7 | 響鈴 |
| `\b` | 8 | 退格 |
| `\f` | 12 | 換頁 |
| `\n` | 10 | 換行 |
| `\r` | 13 | 歸位 |
| `\t` | 9 | 水平定位 |
| `\v` | 11 | 垂直定位 |

跳脫可在 Lexer 或 Parser 解碼，但進入 type checker 前，每個字元必須表示成一個 byte：

```text
原始碼 "a\nb" → 解碼後 [97, 10, 98] → 長度 3
一般運算式的型別 → Array(Char, 4)，包含結尾零
```

如果把反斜線與 n 當成兩個資料 byte，會同時計錯字串長度與初始化內容。未閉合引號、不合法跳脫、未跳脫的換行，都要在 lex 階段拒絕。

### 4.2 AST 新增的結構

```text
type  += Char | SChar | UChar
exp   += String(decoded_bytes)
const += ConstChar(integer) | ConstUChar(integer)
```

Parser 將 `'a'` 轉成 `Constant(ConstInt(97))`，不是 `ConstChar(97)`。`ConstChar` 與 `ConstUChar` 供後續補零、轉型與 TACKY 初始化使用，沒有對應的本章原始碼常數字面值。

字串是獨立的 expression 節點，不放進 `const`。它需要陣列型別、取址與依初始化目標分流的處理方式。

型別 specifier 的解析也要接受 `char`、`signed char`、`unsigned char`。順序不影響意義，但 `char` 不能和 `int`、`long`、`double` 組合。

### 4.3 相鄰字串在 parser 合併

```c
char a[4] = "ab" "c";
```

兩個相鄰字串 token 合併成一個 `String("abc")`。後續只處理合併後的 byte 序列，不在片段中間插入結尾零。

---

## 5. Type checker：按使用位置分流

### 5.1 一般字串運算式

字串的型別是 `Array(Char, length(bytes) + 1)`。把 `String` 納入 lvalue 判定，讓顯式 `&` 能取得陣列位址；一般使用則透過既有的 `typecheck_and_convert` 插入陣列轉指標。

### 5.2 字元陣列初始化是特例

遇到 `Array(element, N)` 搭配 `SingleInit(String(bytes))` 時，先檢查元素是否為字元型別，再檢查實際字元數是否超過 N。保留目標陣列型別，讓後續知道該寫入幾個零。

**不能先把所有 String 轉成指標**，否則 `char a[4] = "abc"` 會失去陣列逐格初始化的資訊。只有初始化指標的路徑，才使用一般運算式轉換。

一般的大括號初始值仍沿用第 15 章的遞迴處理，但補零函式需要能產生 `ConstChar(0)` 與 `ConstUChar(0)`。

### 5.3 Static 初始值與常數屬性

`static_init` 增加下列表示法：

| 表示法 | 保存的資訊 |
|---|---|
| `CharInit(value)` | 普通 char 或 signed char 的初始值 |
| `UCharInit(value)` | unsigned char 的初始值 |
| `StringInit(bytes, null_terminated)` | 字串內容，以及是否附加一個結尾零 |
| `PointerInit(name)` | 另一個靜態物件的符號位址 |

符號表增加 `ConstantAttr(init)`，保存唯讀字串。它與可寫 static 變數使用的 `StaticAttr` 分開，也不需要對外的 global 旗標。

例如 `static char a[6] = "abc"` 使用：

```text
StaticAttr(Initial([
    StringInit("abc", true),
    ZeroInit(2)
]))
```

`StringInit` 已包含一個結尾零，再補兩個零，總共正好 6 bytes。

`static char *p = "abc"` 則需要兩個符號表項目：

```text
string.0:
    type  = Array(Char, 4)
    attrs = ConstantAttr(StringInit("abc", true))

p:
    type  = Pointer(Char)
    attrs = StaticAttr(Initial([PointerInit("string.0")]))
```

string.0 是內容；p 是保存位址的物件。這也解釋了為什麼無法用一個整數或一個 String 節點代替兩者。

---

## 6. TACKY：複製內容與取得位址

### 6.1 Automatic 陣列沿用 CopyToOffset

`char a[4] = "abc"` 可以逐 byte 初始化。以下省略部分 `Constant`、`Var` 包裝：

```text
CopyToOffset(ConstChar(97), a, 0)
CopyToOffset(ConstChar(98), a, 1)
CopyToOffset(ConstChar(99), a, 2)
CopyToOffset(ConstChar(0),  a, 3)
```

Offset 以 byte 計算。也可一次複製 4 或 8 bytes，但不能越過陣列邊界。在 little-endian 下，`97, 98, 99, 0` 可合成 `ConstInt(0x00636261)`；容量只有 3 的陣列就不能用這個 4-byte 寫入方式。

### 6.2 一般字串先登記常數，再取址

遇到一般運算式中的字串時，為它建立唯一符號，加入 `ConstantAttr`，再把符號視為可取址的陣列物件：

```text
GetAddress(Var("string.0"), Var("tmp"))
Copy(Var("tmp"), Var("p"))
```

字串初始化 static 指標時，符號已在 type checking 加入；函式內的一般字串運算式則可在 TACKY generation 加入。

### 6.3 新增頂層 StaticConstant

```text
top_level += StaticConstant(name, type, static_init)
```

先將函式本體轉成 TACKY，才遍歷符號表輸出所有頂層定義。若順序反過來，就會漏掉函式內才登記的字串常數。

字元轉型沿用 `SignExtend`、`ZeroExtend`、`Truncate` 與既有整數和 double 轉換指令，不需要為每種字元型別另建一套 TACKY 算術。

---

## 7. Assembly generation：位寬與符號分開保留

### 7.1 Byte 型別與延伸指令

新增組語型別 `Byte`，三種字元型別都映射到它。符號差異由轉型指令保留，不能因為映射到同一個 Byte 就丟掉。

| 操作 | 組語例子 |
|---|---|
| 複製或截成一個 byte | `movb` |
| 有號 byte → 32-bit | `movsbl` |
| 有號 byte → 64-bit | `movsbq` |
| 無號 byte → 32-bit | `movzbl` |
| 無號 byte → 64-bit | `movzbq` |

Assembly AST 的 `Movsx` 與 `MovZeroExtend` 都要保存來源與目的地型別，才能輸出兩個寬度的 suffix。

暫存器也使用 byte 別名，例如 `%al`、`%dil`、`%r10b`。`movb` 寫入暫存器時不會清空其餘高位，後續只能依該值的型別讀取正確部分。

### 7.2 字元與 double 之間經過 int

沒有直接在 double 與 1-byte 整數間轉換的 x86 指令。本書透過 int 寬度銜接：

```text
char / signed char → 符號延伸成 int → Cvtsi2sd
unsigned char      → 零延伸成 int   → Cvtsi2sd
double             → Cvttsd2si 成 int → 截成 byte
```

這描述的是本書的指令選擇，不表示超出目標範圍的浮點轉整數具有 modulo 語意。

### 7.3 Pseudo replacement 與 instruction fix-up

Byte 物件在堆疊占 1 byte，基本對齊為 1 byte。字元陣列仍沿用第 15 章的陣列變數對齊規則：總大小至少 16 bytes 時，以 16 bytes 對齊整個變數。

字串常數在後端符號表標為 static，存取時使用 `Data(name)`。若沿用第 13 章可選的 `is_constant` 欄位，也要設為 true，讓 code emission 使用常數的局部標籤。

`movzbl` 的來源不能是立即值，目的地必須是暫存器。例如這個中間形式：

```asm
movzbl $10, -4(%rbp)
```

必須修補成合法指令：

```asm
movb   $10, %r10b
movzbl %r10b, %r11d
movl   %r11d, -4(%rbp)
```

來源為 32-bit 的 `MovZeroExtend`，仍沿用先前以 `movl` 寫入暫存器清空高位的修補方式。byte 立即值過大時則保留低 8 bits，例如 `movb $258, %al` 改成 `movb $2, %al`。

---

## 8. Code emission：內容、補零與位址

| 初始值表示法 | 輸出 |
|---|---|
| `CharInit(i)`、`UCharInit(i)` | 非零用 `.byte i`；零可用 `.zero 1` |
| `StringInit(s, false)` | `.ascii "s"`，不附加零 |
| `StringInit(s, true)` | `.asciz "s"`，附加一個零 |
| `ZeroInit(n)` | `.zero n` |
| `PointerInit(label)` | `.quad label` |

可寫 static 陣列放可寫資料區段；字串常數在 Linux 放 `.rodata`，在 macOS 放 `.cstring`。例如 Linux 上的 `static char *p = "abc"`：

```asm
    .section .rodata
.Lstring.0:
    .asciz "abc"

    .data
    .align 8
p:
    .quad .Lstring.0
```

這是簡化標籤的示意。p 本身占 8 bytes，字串物件占 4 bytes。`.quad` 交由組譯與連結流程填入位址，不能改成字元資料，也不能靠執行期的 `lea` 完成 static 初始化。

輸出字串時要重新跳脫雙引號、反斜線與換行。C 與組語接受的跳脫序列不同，固定三位的八進位跳脫較容易明確表示任意 byte。**原始碼不支援八進位跳脫，和後端輸出八進位跳脫是兩件事。**

---

## 9. 本章範圍與 lemon_cc 的後續改動

### 9.1 本書這一章的範圍

| 功能 | 處理方式 |
|---|---|
| 三種字元型別、ASCII 字元與字串 | 本章加入 |
| 相鄰字串字面值 | Parser 合併 |
| 字串初始化三種字元陣列 | 支援，按容量補零或省略結尾零 |
| 字串隱式轉成 signed char * 或 unsigned char * | Type checker 拒絕 |
| 修改字串字面值物件 | Undefined behavior，不保證靜態拒絕 |
| 多字元常數，例如 `'ab'` | 不支援 |
| 八進位、十六進位的 C 跳脫序列 | 不支援，包含 `\0` |
| 寬字元、寬字串、UTF-8 字面值 | 不支援 |
| `const` qualifier、完整標準函式庫標頭 | 尚未實作 |
| `sizeof`、`void` 型別 | 第 17 章 |

### 9.2 接到目前專案時要改哪些地方

以下是依現有第 15 章架構整理的實作方向，不是已完成的改動清單：

| 階段 | 要延伸的內容 |
|---|---|
| Lexer | char 關鍵字、兩種 literal token、引號與跳脫驗證 |
| 型別與 parser | 三種字元型別、String expression、字元常數解碼、相鄰字串合併 |
| Type checker | 整數提升、String lvalue、陣列初始化特例、字元指標相容性 |
| 符號表與 static 初始值 | CharInit、UCharInit、StringInit、PointerInit、ConstantAttr |
| TACKY | 字串複製、字串取址、StaticConstant、頂層定義生成順序 |
| Assembly generation | Byte、雙寬度延伸指令、double 轉換、byte 堆疊配置與 fix-up |
| Code emission | byte suffix、資料指令、字串跳脫、唯讀區段與常數標籤 |

書中另外提醒窄字元參數在跨編譯器呼叫時的延伸慣例差異。這是原書 2024 年的互通性說明；本專案實作時仍需對目標工具鏈做呼叫與回傳測試。

---

## 10. 用完整例子核對語意

下面使用標準標頭，可交給系統 GCC 驗證：

```c
#include <stdio.h>

int main(void) {
	char a[4] = "abc";
	char *p = "abc";
	a[0] = 'x';
	puts(a);
	puts(p);
	return a[0] - p[0];
}
```

預期標準輸出：

```text
xbc
abc
```

在本書的 ASCII 目標上，回傳值為 `120 - 97 = 23`。a 的內容改變，p 指向的字串沒有變；兩個 char 值在相減前先提升成 int。

此例已在建立視覺化頁面時用系統 GCC 驗證，並非 lemon_cc 的 ch16 測試結果。本書因未支援 const 與完整標頭，會自行宣告 `int puts(char *s);`；該宣告與標準函式庫的 `int puts(const char *s);` 不完全相容。

---

## 11. 實作後的驗證重點

書附測試工具提供下列分階段命令。它們是 ch16 實作完成後的驗收入口，目前尚未宣稱通過：

```sh
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 16 --stage lex
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 16 --stage parse
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 16 --stage validate
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 16 --stage tacky
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 16 --stage codegen
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 16
```

特別核對這些跨階段條件：

- `'a'` 產生 int 常數；`'\n'` 解碼成 10。
- 空字串作為運算式時是 `char[1]`，包含一個結尾零。
- `"ab" "c"` 合併後只有一個字串物件的長度與結尾零。
- 容量不足時拒絕，容量剛好時省略零，容量較大時補足全部剩餘 byte。
- 字串初始化字元陣列時不先 decay；初始化指標時才保留常數位址。
- 有號負字元與無號高位字元，提升後保留不同數值。
- 批次複製字元不能越界，static 資料總大小必須等於物件大小。
- 轉換函式本體後才收集 StaticConstant，避免漏掉函式內字串。
- Linux 資料段、局部標籤、byte 指令與組語跳脫能正確組譯及連結。

---

## 12. 速記重點

```text
'a'                  → int 值 97，不是 char 常數
char                  → 1-byte 儲存；算術前通常先提升成 int
"abc" 作為運算式     → char[4]；一般使用轉成 char *
char a[N] = "abc"    → 複製內容；容量決定補零數量
char *p = "abc"      → 保存靜態字串物件的位址
automatic 陣列初始化 → CopyToOffset
static 字串與指標    → StringInit / PointerInit
唯讀字串頂層定義    → StaticConstant
後端                 → Byte + 正確延伸 + 正確區段與資料指令
```

## 13. 參考資料

- Nora Sandler，《Writing a C Compiler》第 16 章，印刷頁 423–454。
- [第十五章：陣列與指標運算](ch15-arrays-and-pointer-arithmetic.md)，陣列轉指標、初始化與 byte offset 的先備內容。
- [ch16 視覺化頁面](ch16-characters-and-strings.html)，同一主例的記憶體圖與編譯流程。
- [視覺化驗證紀錄](../output/ch16-explanation/proof.md)，GCC 範例結果與 HTML 檢查範圍。
