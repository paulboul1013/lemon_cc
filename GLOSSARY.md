# lemon_cc

依照《Writing a C Compiler》實作的 C 編譯器。這裡定義各階段共用的術語，實作細節寫在 `docs/chNN-*.md`。

## 物件與值

**Object（物件）**:
記憶體中存放值的位置，有固定的位址和可更新的內容。
_Avoid_: 變數（變數只是指向物件的一種名字）

**Lvalue**:
可能指向某個 object 的運算式，目前只有 `Var` 和 `Dereference`。它是運算式，不是值。
_Avoid_: 左值的值、可賦值的東西

**Lvalue conversion**:
使用 object 時，把它換成目前內容的動作。`=` 的左邊和 `&` 的運算元不做這個轉換。

**Referenced type**:
指標指向的型別，例如 `int *` 的 referenced type 是 `int`。
_Avoid_: base type（那是 specifier 決定的型別）

**Null pointer constant**:
值為 0 的整數常數字面值，例如 `0`、`0ul`。`0.0`、`10 - 10`、`(long)0` 都不算。
_Avoid_: null 指標（null 指標是值，這個詞指的是原始碼裡的常數）

## 宣告

**Basic type**:
宣告中由 specifiers 決定的型別，還沒套用任何 declarator 推導。
_Avoid_: base type

**Declarator**:
宣告裡 specifiers 以外的部分：要宣告的名字，加上要套用在 basic type 上的型別推導。

**Abstract declarator**:
沒有名字的 declarator，用在 cast 的型別裡。
