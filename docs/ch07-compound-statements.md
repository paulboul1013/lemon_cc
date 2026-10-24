# 第七章：複合陳述式（Compound Statements）

## 核心目標

本章的核心是讓編譯器支援 **複合陳述式（Compound Statements）**，也就是用大括號包起來的一整個區塊：

```c
{
    int a = 1;
    a = a + 2;
    return a;
}
```

這件事看起來只是「多了一層大括號」，但其實它會讓編譯器正式進入 **巢狀作用域（Nested Scope）** 的世界。

為了完成這一章，我們需要做到：

1. **讓 `{ ... }` 本身可以被當成一個 statement**
2. **把函式本體與一般 block 統一成同一種結構**
3. **讓變數解析支援內外層 block**
4. **正確處理 shadowing（內層遮蔽外層同名變數）**
5. **在離開 block 後恢復外層作用域**
6. **讓 TACKY generation 可以直接展開 block 裡的內容**

---

## 1. 這一章在語言上新增了什麼？

在前面的章節中，函式本體已經可以寫成一個大括號區塊：

```c
int main(void) {
    return 0;
}
```

但這一章更進一步，讓大括號區塊不只存在於函式本體，也可以出現在任何需要 statement 的地方，例如：

```c
if (1) {
    int x = 3;
    return x;
}
```

也就是說，從這一章開始：

* `{ ... }` 不只是「語法上的包裝」
* 它是 AST 中真正的一個 **statement 節點**
* 而且它同時也是一個 **scope 邊界**

---

## 2. Scope 的核心觀念

本章最重要的新觀念，不是 parser，而是 **scope**。

### 2.1 什麼是 block scope？

只要是一個可以包含宣告、並且決定宣告可見範圍的區域，就可以看成一個 block。

在這一章裡，兩種東西是 block：

* 函式本體
* 複合陳述式 `{ ... }`

第八章的迴圈也是 block（嚴格說 `if` 也是，但對實作沒影響）。不過 AST 的 `Block` 節點只用來表示函式本體和複合陳述式，不代表 `if` 或迴圈。

---

### 2.2 變數的可見範圍從哪裡開始？

區域變數的 scope **不是從整個 block 開頭開始**，而是從「宣告點」開始。

```c
int main(void) {
    int a = 5;
    return a;
}
```

這裡 `a` 的 scope 從 initializer 前面（變數名稱之後）開始，到 block 結束為止。所以 `resolve_declaration` 要先把名字加進 map，再 resolve initializer（見第 14 節）。

---

### 2.3 內層 block 可以讀取外層變數

如果內層沒有重新宣告同名變數，就可以讀到外層的變數。

```c
int main(void) {
    int a = 2;
    {
        int b = a + 1;
        return b;
    }
}
```

這裡 `b` 初始化時用到的 `a`，是外層的 `a`。

---

### 2.4 內層 block 可以遮蔽外層同名變數

如果內層重新宣告了同名變數，之後在該 block 中會優先看到內層那個。

```c
int main(void) {
    int a = 2;
    {
        int a = 3;
        return a;
    }
}
```

這裡 `return a;` 回傳的是內層的 `a`，不是外層的。

這種行為叫做：

* **shadowing**
* 或者叫 **name hiding**

---

### 2.5 離開內層 block 後，外層名字會恢復

內層把外層遮住，不代表外層變數消失，只是暫時不可見。

```c
int main(void) {
    int a = 2;
    {
        int a = 3;
    }
    return a;
}
```

這裡最後的 `return a;` 又會回到外層的 `a`。

被遮蔽（hidden）和離開作用域（out of scope）不同：被遮蔽的變數之後還可能重新可見，離開作用域的就不行。

---

### 2.6 本章必須攔截的兩種 scope 錯誤

#### 錯誤 1：使用不在 scope 內的變數

```c
int main(void) {
    {
        int a = 3;
    }
    return a;   // 錯
}
```

因為 `a` 已經離開作用域了。

#### 錯誤 2：同一個 block 裡重複宣告同名變數

書的定義：兩個變數的 scope 結束在同一點（也就是宣告在同一個 block）就算「同一個 scope」，不能同名。

```c
int main(void) {
    int a = 1;
    int a = 2;   // 錯
    return a;
}
```

但如果是不同 block，就合法：

```c
int main(void) {
    int a = 1;
    {
        int a = 2;   // 合法
    }
    return a;
}
```

---

## 3. 詞法分析（Lexer）

這一章 **不需要修改 lexer**。

原因很簡單：

* `{`
* `}`

這兩個 token 在前面章節為了函式本體就已經存在了。

所以本章 lexer 沒有新增 token，也沒有新增關鍵字。

---

## 4. 語法分析（Parser）

本章 parser 的核心任務是：

> 讓一整個 `{ ... }` 被視為一個 statement

---

## 5. AST 結構更新

這一章最關鍵的 AST 改動，是引入 `block` 與 `Compound(block)`。

```python
program = Program(function_definition)

function_definition = Function(identifier name, block body)

block = Block(block_item*)

block_item = S(statement) | D(declaration)

declaration = Declaration(identifier name, exp? init)

statement = Return(exp)
          | Expression(exp)
          | If(exp condition, statement then, statement? else)
          | Compound(block)
          | Null

exp = Constant(int)
    | Var(identifier)
    | Unary(unary_operator, exp)
    | Binary(binary_operator, exp, exp)
    | Assignment(exp, exp)
    | Conditional(exp condition, exp true_exp, exp false_exp)

unary_operator = Complement | Negate | Not

binary_operator = Add | Subtract | Multiply | Divide | Remainder
                | And | Or
                | Equal | NotEqual
                | LessThan | LessOrEqual
                | GreaterThan | GreaterOrEqual
```

---

## 6. AST 改動的意義

### 6.1 函式本體不再只是 items，而是 `block`

以前可以把函式 body 看成一串 block item。
現在更正式地把它表示成：

```python
Function(name, block body)
```

也就是說，函式本體和一般 `{ ... }` 的本質其實是一樣的。

---

### 6.2 `Compound(block)` 讓 block 成為 statement

這是本章的關鍵。

有了這個節點之後：

```c
if (x) {
    int a = 1;
    return a;
}
```

在 AST 裡就能表示成：

```python
If(
    condition,
    Compound(
        Block([...])
    ),
    null
)
```

也就是說，`if` 後面不是直接接很多 statement，而是接 **一個 Compound statement**。

---

### 6.3 block 裡面仍然是 block item，不是純 statement list

因為 block 裡仍然可能同時出現：

* declaration
* statement

例如：

```c
{
    int a = 1;
    a = a + 2;
}
```

所以 block 不能簡化成純 statement list，仍然必須保留：

```python
block_item = S(statement) | D(declaration)
```

---

## 7. EBNF 文法整理

```ebnf
<program> ::= <function>

<function> ::= "int" <identifier> "(" "void" ")" <block>

<block> ::= "{" { <block-item> } "}"
<block-item> ::= <statement> | <declaration>

<declaration> ::= "int" <identifier> [ "=" <exp> ] ";"

<statement> ::= "return" <exp> ";"
              | <exp> ";"
              | "if" "(" <exp> ")" <statement> [ "else" <statement> ]
              | <block>
              | ";"

<exp> ::= <factor>
        | <exp> <binop> <exp>
        | <exp> "?" <exp> ":" <exp>

<factor> ::= <int>
           | <identifier>
           | <unop> <factor>
           | "(" <exp> ")"

<unop> ::= "-" | "~" | "!"
<binop> ::= "-" | "+" | "*" | "/" | "%"
          | "&&" | "||"
          | "==" | "!="
          | "<" | "<=" | ">" | ">="
          | "="

<identifier> ::= ? identifier token ?
<int> ::= ? constant token ?
```

`<block>` 裡加引號的 `"{"`、`"}"` 是真的大括號 token；沒加引號的 `{ }` 是 EBNF 的「重複零次以上」。

---

## 8. Parser 的直覺寫法

### 8.1 `parse_statement`

當 parser 看到 `{` 時，就知道自己遇到了 compound statement。

```python
parse_statement():
    if next token is "return":
        return parse_return_statement()

    if next token is "if":
        return parse_if_statement()

    if next token is "{":
        blk = parse_block()
        return Compound(blk)

    if next token is ";":
        consume(";")
        return Null

    e = parse_exp()
    consume(";")
    return Expression(e)
```

---

### 8.2 `parse_block`

`parse_block` 的工作就是：

1. 吃掉 `{`
2. 一直讀 block item
3. 直到遇到 `}`

```python
parse_block():
    consume("{")
    items = []

    while next token is not "}":
        if next token starts a declaration:
            items.append(D(parse_declaration()))
        else:
            items.append(S(parse_statement()))

    consume("}")
    return Block(items)
```

---

## 9. 語意分析（Semantic Analysis）

本章真正的重點，是把第五章的 variable resolution 升級成 **支援巢狀作用域** 的版本。

---

## 10. 變數解析的核心目標

本章的 variable resolution 需要做到：

1. 把每個區域變數重新命名成唯一名稱
2. 正確處理內外層 block 的可見性
3. 正確處理 shadowing
4. 攔截同一個 block 內的重複宣告
5. 攔截使用超出 scope 的名字

---

## 11. 為什麼要重新命名成唯一名稱？

因為一旦你把：

```c
int main(void) {
    int x = 1;
    {
        int x = 2;
        return x;
    }
    return x;
}
```

解析成：

```c
int main(void) {
    int x.0 = 1;
    {
        int x.1 = 2;
        return x.1;
    }
    return x.0;
}
```

後面的 TACKY 與 assembly backend 就不用再理解 scope。

它們只要看到：

* `x.0`
* `x.1`

是不同變數就好。

這就是 variable resolution 在前端階段把 lexical scope 問題提前消滅掉的價值。

---

## 12. Symbol map / Variable map 的概念

你可以把 variable resolution 想成維護一張表：

```text
原始名稱 -> 唯一名稱
```

例如：

```text
a -> a.0
b -> b.1
```

但在本章，這張表還需要記錄：

* 這個名稱是不是來自當前 block

所以 map entry 可以想成：

```python
entry = {
    original_name: "a",
    unique_name: "a.0",
    from_current_block: True
}
```

---

## 13. `from_current_block` 的用途

這個欄位是本章最重要的小技巧之一。

它可以幫你分辨兩種情況：

### 合法：內層遮蔽外層

```c
int a = 1;
{
    int a = 2;   // 合法
}
```

這裡雖然 `a` 已經在 map 裡，但那個 entry 不是來自「當前 block」，而是外層 block，所以這是合法 shadowing。

---

### 非法：同一 block 重複宣告

```c
int a = 1;
int a = 2;   // 非法
```

這裡 map 裡的 `a` 是來自同一個 block，所以必須報錯。

---

## 14. `resolve_declaration` 的更新版邏輯

```python
resolve_declaration(Declaration(name, init), variable_map):
    if name exists in variable_map
       and variable_map[name].from_current_block == True:
        fail("Duplicate variable declaration in same block")

    unique_name = make_temporary_name(name)

    variable_map[name] = {
        unique_name: unique_name,
        from_current_block: True
    }

    if init is not null:
        init = resolve_exp(init, variable_map)

    return Declaration(unique_name, init)
```

---

## 15. 和第五章最大的差別

第五章通常可以寫成：

```python
if name in variable_map:
    fail(...)
```

但第七章不能這樣做，因為內層 block 允許 shadowing。

所以現在的判定必須改成：

```python
if name in variable_map and entry.from_current_block:
    fail(...)
```

只攔下「同一層 block」的重複宣告。

---

## 16. 進入 block 時該怎麼做？

本章的標準做法是：

> 一進入新的 block，就複製目前的 variable map

也就是說：

* 外層 block 用自己的 map
* 內層 block 拿一份 copy
* 內層的修改不會污染外層
* 離開 block 後，外層 map 自然恢復

map 只在兩種時候改變：遇到宣告就加入 entry（覆蓋同名的舊 entry）；離開 block 就回到進入前的那張 map。書用 Listing 7-4 / Table 7-1 示範：

```c
int main(void) {        // (空)
    int x = 1;          // x -> x.0
    {
        int x = 2;      // x -> x.1
        if (x > 1) {
            x = 3;      // 仍是 x.1：內層的 x 還沒宣告
            int x = 4;  // x -> x.2（之後沒被用到）
        }
        return x;       // x -> x.1，整個程式回傳 3
    }
    return x;           // x -> x.0
}
```

---

## 17. `copy_variable_map` 的重點

複製 map 不是單純 deep copy 就好，還要把每個 entry 的：

```python
from_current_block
```

重設成：

```python
False
```

因為當你進入新的 block 時，原本從外層帶進來的名字，對這個新 block 來說都不是「當前 block 宣告的名字」。

```python
copy_variable_map(old_map):
    new_map = deep_copy(old_map)

    for each entry in new_map:
        entry.from_current_block = False

    return new_map
```

---

## 18. `resolve_statement` 如何處理 Compound

```python
resolve_statement(stmt, variable_map):
    match stmt with
    | Return(e) ->
        return Return(resolve_exp(e, variable_map))

    | Expression(e) ->
        return Expression(resolve_exp(e, variable_map))

    | If(cond, then_stmt, else_stmt) ->
        return If(
            resolve_exp(cond, variable_map),
            resolve_statement(then_stmt, variable_map),
            resolve_statement(else_stmt, variable_map) if else_stmt else null
        )

    | Compound(block) ->
        inner_map = copy_variable_map(variable_map)
        return Compound(resolve_block(block, inner_map))

    | Null ->
        return Null
```

這裡最重要的是：

```python
inner_map = copy_variable_map(variable_map)
```

這一步保證內層 block 的作用域不會污染外層。

---

## 19. `resolve_block` 的本質是順序處理 block item

```python
resolve_block(block, variable_map):
    new_items = []

    for item in block.items:
        if item is declaration:
            new_items.append(D(resolve_declaration(item.decl, variable_map)))
        else:
            new_items.append(S(resolve_statement(item.stmt, variable_map)))

    return Block(new_items)
```

這裡一定要 **照順序** 處理，因為 scope 是從宣告點開始，不是從整個 block 開頭開始。

例如：

```c
int a = 2;
{
    int b = a + 1;
    int a = 3;
}
```

在解析 `b` 初始化時，內層 `a` 還沒宣告，所以那個 `a` 應該解析成外層 `a`。

如果你不是按順序 resolve，而是先把整個 block 的宣告都收進 map，就會做錯。

---

## 20. TACKY Generation

這一章的 TACKY generation 幾乎沒有新的控制流程技巧。

核心想法是：

> Compound statement 只是把裡面的 block item 順序展開

---

## 21. Compound 的 TACKY lowering

```python
emit_tacky_statement(stmt, instructions):
    match stmt with
    | Compound(block) ->
        emit_tacky_block(block, instructions)

    | Return(e) ->
        ...

    | Expression(e) ->
        ...

    | If(cond, then_stmt, else_stmt) ->
        ...

    | Null ->
        ...
```

```python
emit_tacky_block(block, instructions):
    for item in block.items:
        if item is declaration:
            emit_tacky_declaration(item.decl, instructions)
        else:
            emit_tacky_statement(item.stmt, instructions)
```

---

## 22. 為什麼 TACKY 和後端幾乎不用改？

TACKY 只多了上面的 `Compound` 分支；lexer 和 assembly generation 之後的階段完全不用動。

因為在 variable resolution 之後，同名但不同 scope 的變數已經變成不同的唯一名稱（見第 11 節），backend 只要繼續做原本的事情：

* `Var(...)`
* pseudo register
* stack slot allocation

就夠了。

---

## 23. 本章最容易犯的錯

### 錯誤 1：進入 block 時直接共用同一張 map

這會讓內層宣告污染外層，離開 block 後也無法恢復正確綁定。

---

### 錯誤 2：還沿用第五章的 duplicate declaration 規則

只要名字在 map 裡就報錯，合法的 shadowing 也會被誤判成錯誤（見第 15 節）。

---

### 錯誤 3：忘了 scope 從宣告點開始

先把整個 block 的宣告收進 map，會讓 `int b = a + 1;` 錯拿到後面才宣告的內層 `a`（見第 19 節）。

---

### 錯誤 4：想在後端處理 shadowing

shadowing 應該在 variable resolution 就處理掉，不應該留到 TACKY 或 assembly backend。

---

## 24. 這一章對整體編譯器的意義

表面上，第七章只是多了一個：

```python
Compound(block)
```

但實際上，它的意義很大：

* 你的語言正式進入 **多層 lexical scope**
* parser 開始能處理真正的 block statement
* semantic analysis 開始承擔真正的 scope 管理責任
* 後續章節像 `for`、函式參數、檔案層級宣告、`static`、`extern` 都會建立在這個模型上；書特別提到一個 `for` 迴圈本身就含兩個不同的 scope

也就是說，第七章是你從「簡單 expression compiler」走向「真正 C-like front end」的重要一步。

---

## 25. 一句話總結

> 第七章的本質，就是把 `{ ... }` 變成一個真正的 statement，並且讓 variable resolution 正確支援巢狀作用域與 shadowing。

---

## 26. 速記重點

### 這章新增了什麼？

* 新 statement：`Compound(block)`
* 函式本體統一成 `block`
* variable resolution 支援 nested scope
* 合法 shadowing / 非法 duplicate declaration
* TACKY 只要順序展開 block item

### 最重要的實作口訣

```text
進 block -> 複製 map
宣告變數 -> 加入新 binding，標記 current block
離開 block -> 丟掉內層 map
後端 -> 完全不用懂 scope
```

---
