# 第五章：區域變數（Local Variables）

##  核心目標
本章的核心是讓編譯器支援**區域變數 (Local Variables)**。為了達成這個目標，我們需要引入以下新概念：
1. **變數宣告與初始化 (Declarations & Initializations)**。
2. **賦值運算式 (Assignment Expressions)** 及其**副作用 (Side Effects)**。
3. **區塊項目 (Block Items)**：讓函式主體可以包含多個陳述式 (Statements) 與宣告 (Declarations)。
4. **語意分析 (Semantic Analysis)**：新增編譯器階段來處理「變數解析 (Variable Resolution)」與「左值檢查 (Lvalue Validation)」。

幾個語言層面的觀念：
* **賦值是運算式**：它的值是目標變數更新後的值。`2 * (a = 5)` 先把 5 存進 `a`，整個運算式的值是 `10`。
* **副作用 (Side Effect)**：效果在該語言構造之外看得到才算。更新區域變數是賦值運算式的副作用，但不是外層函式的副作用（函式外看不到）；更新全域變數則對運算式和函式都算副作用。
* **左值 (Lvalue)**：能出現在 `=` 左邊的運算式。本章只有變數是 lvalue（陣列元素、struct 成員留到 Part II）。`4 = 5;`、`a - 5 = b;` 都不合法。

---

## 1. 詞法分析 (Lexer)
只需要新增一個 Token，也就是賦值運算子 `= (Equal Sign)`。
注意：變數名稱屬於 `Identifier`（識別字），與函式名稱共用相同的 Token 類型，在 Lexer 階段不需要區分它們。

```regex
=   // 賦值運算子 (Assignment operator)
```

---

## 2. 語法分析 (Parser)

### 2.1 抽象語法樹 (AST) 擴充
為了支援變數宣告、賦值及空陳述式，AST (以 ASDL 表示) 的定義更新如下 (註解為新增部分)：

```python
program = Program(function_definition)
function_definition = Function(identifier name, block_item* body)

// 新增：區塊項目可以是陳述式或宣告
block_item = S(statement) | D(declaration)

// 新增：宣告節點 (包含變數名稱與可選的初始化表達式)
declaration = Declaration(identifier name, exp? init)

statement = Return(exp) 
          | Expression(exp)  // 新增：表達式陳述式 (例如 `a = 3;`)
          | Null             // 新增：空陳述式 (例如單獨的 `;`)

exp = Constant(int) 
    | Var(identifier)                  // 新增：讀取變數
    | Unary(unary_operator, exp) 
    | Binary(binary_operator, exp, exp) 
    | Assignment(exp, exp)             // 新增：賦值操作

unary_operator = Complement | Negate | Not
binary_operator = Add | Subtract | Multiply | Divide | Remainder | And | Or
                | Equal | NotEqual | LessThan | LessOrEqual
                | GreaterThan | GreaterOrEqual
```

幾個設計重點：
* **`Assignment(exp, exp)` 的左邊是任意 `exp`**：parser 不檢查左邊是否為變數，lvalue 檢查延到語意分析，因為後面章節會有更複雜的 lvalue。
* **宣告不是陳述式**：陳述式在執行期執行，宣告只告訴編譯器某個識別字存在。所以 `declaration` 是獨立節點，而不是 `statement` 的一種。具體差別：某些位置只能放 statement，例如 `if (a == 2) int x = 0;` 不合法（`{ ... }` 其實是一個 compound statement，第七章才實作）。
* **`Null` 空陳述式**：只有一個 `;`，用在文法要求一個 statement 但不需要做事的地方，例如 `while (*s++ != '\0') ;`。
* **`exp? init`**：問號表示可選欄位；**`block_item* body`**：星號表示 list。

### 2.2 具體語法 (Formal Grammar - EBNF)
相對應的 EBNF 語法規則更新如下。注意 `{ }` 代表重複零次或多次，`[ ]` 代表可選 (Optional)。

```ebnf
<program> ::= <function>
<function> ::= "int" <identifier> "(" "void" ")" "{" { <block-item> } "}"
<block-item> ::= <statement> | <declaration>

<declaration> ::= "int" <identifier> [ "=" <exp> ] ";"
<statement> ::= "return" <exp> ";" | <exp> ";" | ";"

<exp> ::= <factor> | <exp> <binop> <exp>
<factor> ::= <int> | <identifier> | <unop> <factor> | "(" <exp> ")"

<unop> ::= "-" | "~" | "!"
<binop> ::= "-" | "+" | "*" | "/" | "%" | "&&" | "||"
          | "==" | "!=" | "<" | "<=" | ">" | ">=" 
          | "="   // 新增賦值運算子

<identifier> ::= ? An identifier token ?
<int> ::= ? A constant token ?
```

注意沒加引號的 `{ }` 是重複，加引號的 `"{"` `"}"` 是真正的大括號 token。

Parser 實作要點：
* **函式主體**：讀完 `{` 後，不斷呼叫 `parse_block_item`，直到 `peek` 到 `}` 為止，再吃掉 `}`（Listing 5-6）。
* **block item 是宣告還是陳述式**：看第一個 token，是 `int` 就是宣告，否則是陳述式。
* **可選的初始化**：identifier 後面接 `=` 代表有 initializer，接 `;` 代表沒有。
* `=` 在文法上跟其他 `<binop>` 一樣，所以可以沿用 precedence climbing，但產生的是 `Assignment` 而不是 `Binary` 節點。

### 2.3 優先級爬升演算法 (Precedence Climbing) 的改良
賦值運算子 `=` 與其他二元運算子最大的不同在於：它是**右結合 (Right-associative)**。
* 左結合 (`-`)：`a - b - c` 解析為 `(a - b) - c`
* 右結合 (`=`)：`a = b = c` 解析為 `a = (b = c)`

為了支援右結合，在遇到 `=` 時，遞迴呼叫 `parse_exp` 的 `min_prec` 參數**不需要 +1**。
以 `a = b = c` 為例：先讀到 factor `a`，再遞迴處理右邊。如果 `min_prec` 是 `precedence("=") + 1`，遞迴呼叫只會回傳 `b`；設成 `precedence("=")`，遞迴呼叫才會把整個 `b = c` 吃進來，得到 `a = (b = c)`。

`=` 的優先級比目前所有運算子都低：

**運算子優先級表 (Table 5-1)**:
| Operator | Precedence | Associativity |
| -------- | ---------- | ------------- |
| `*`, `/`, `%` | 50 | Left |
| `+`, `-` | 45 | Left |
| `<`, `<=`, `>`, `>=`| 35 | Left |
| `==`, `!=`| 30 | Left |
| `&&` | 10 | Left |
| `\|\|` | 5 | Left |
| **`=`** | **1** | **Right** |

**改良後的 Precedence Climbing 虛擬碼 (Listing 5-8)**：
```python
parse_exp(tokens, min_prec):
    left = parse_factor(tokens)
    next_token = peek(tokens)
    
    while next_token is a binary operator and precedence(next_token) >= min_prec:
        if next_token is "=":
            take_token(tokens) // 移除 "="
            # 注意這裡：右結合，所以傳遞 precedence(next_token) 而非 +1
            right = parse_exp(tokens, precedence(next_token))
            left = Assignment(left, right)
        else:
            operator = parse_binop(tokens)
            # 左結合，傳遞 precedence(next_token) + 1
            right = parse_exp(tokens, precedence(next_token) + 1)
            left = Binary(operator, left, right)
            
        next_token = peek(tokens)
        
    return left
```

---

## 3. 語意分析 (Semantic Analysis)
從本章開始引入「語意分析」階段。在這個階段，AST 結構完全合法，但我們需要檢查程式是否「有意義」。

本章會被語意分析擋下的三種錯誤（語法都合法）：
```c
2 = a * 3;               // 對常數賦值（不是 lvalue）
int a = 3; int a;        // 同一個 scope 重複宣告
int main(void) { a = 4; return a; }   // a 尚未宣告就使用
```

### 3.1 變數解析 (Variable Resolution)
變數解析的目標是：
1. 為每個區域變數生成**全域唯一的名稱** (例如將 `x` 重命名為 `x.1`)，以利於後續作用域 (Scope) 的處理。
2. 攔截**重複宣告 (Duplicate declaration)** 錯誤。
3. 攔截**使用未宣告變數 (Undeclared variable)** 錯誤。

做法：維護一個 variable map（使用者寫的名字 → 唯一名稱），依序處理 block item；遇到宣告就加一筆，遇到使用就換成 map 裡的唯一名稱。

**唯一名稱的規則**：
* 不能和 TACKY 暫存變數撞名：用全域 counter 的話，語意分析和 TACKY generation 要**共用同一個 counter**。
* 不能和函式名、全域變數撞名（第十章的全域變數保留原名）：產生 C 語法不合法的名字，例如 `a.0`、`b.1`。建議保留原名方便除錯。書中 C 範例寫成 `a0`、`b1` 只是為了好讀，實務上會撞名（例如 `a` 和 `a1` 都可能變成 `a12`）。
* lemon_cc 的 `resolve_local_name` 呼叫 `make_temporary()` 產生 `tmp.N`，和 TACKY 暫存變數共用 `temp_counter`。

**處理變數宣告 (Listing 5-9)**：
```python
resolve_declaration(Declaration(name, init), variable_map):
    if name is in variable_map:
        fail("Duplicate variable declaration!")
        
    unique_name = make_temporary() # 產生如 name.1 的唯一 ID
    variable_map.add(name, unique_name)
    
    if init is not null:
        init = resolve_exp(init, variable_map)
        
    return Declaration(unique_name, init)
```

**在自己的 initializer 裡用到自己**：Listing 5-9 先更新 map 再處理 initializer，所以 `int foo = foo + 1;` 不會報錯。這符合 C 標準（變數在自己的 initializer 中已經在 scope 內），但讀到未初始化的 `foo` 是 UB，編譯器不必偵測。

**處理陳述式 (Listing 5-10)**：`Return(e)`、`Expression(e)` 只要 resolve 內部的運算式，`Null` 原樣回傳。

**處理變數讀取與賦值 (Listing 5-11)**：
同時必須檢查「左值 (Lvalue)」。賦值的左邊目前**必須是變數**，不能是常數或運算式。
```python
resolve_exp(e, variable_map):
    match e with
    | Assignment(left, right) ->
        if left is not a Var node:
            fail("Invalid lvalue!")  // 左值檢查： 3 = x 是錯誤的
        return Assignment(resolve_exp(left, variable_map), 
                          resolve_exp(right, variable_map))
                          
    | Var(v) ->
        if v is in variable_map:
            return Var(variable_map.get(v)) // 替換成唯一名稱
        else:
            fail("Undeclared variable!")    // 變數未宣告錯誤
    | --snip--
```

`Assignment` 的左右兩邊都遞迴呼叫 `resolve_exp`，所以賦值左邊的變數也由 `Var` 分支處理；其他運算式只要遞迴處理子運算式。

### 3.2 `--validate` 選項
compiler driver 新增 `--validate`：跑到語意分析為止，不進 TACKY generation，不產生輸出檔；成功回傳 0，失敗回傳非 0。之後語意分析有多個 pass 時，`--validate` 要全部跑完。

---

## 4. 中介碼生成 (TACKY Generation)
幸運的是，我們在前面的章節已經為了暫存運算結果發明了 TACKY 的 `Var` (虛擬暫存器) 和 `Copy` 指令。因此，TACKY IR **不需要新增任何結構**，我們只需要將 AST 的新節點轉換成現有的 TACKY 指令即可。

### 4.1 轉換規則
* **變數 (Var)**：直接轉換為 TACKY 的 `Var`。
* **宣告 (Declaration)**：
  * 若沒有初始化 (e.g., `int a;`)：在 TACKY 階段直接**丟棄** (TACKY 不需要宣告變數)。
  * 若有初始化 (e.g., `int a = 5;`)：視為賦值處理，產生對應的 `Copy` 或算術運算。
* **賦值 (Assignment)**：計算右半邊 (RHS) 的結果，然後 `Copy` 到左半邊 (LHS) 的變數中。
* **空陳述 (Null Statement)**：不產生任何 TACKY 指令。
* **表達式陳述 (Expression Statement)**：計算表達式，但丟棄結果的變數。

**賦值與變數的 TACKY 轉換虛擬碼 (Listing 5-12)**：
```python
emit_tacky(e, instructions):
    match e with
    | --snip--
    | Var(v) -> return Var(v)
    | Assignment(Var(v), rhs) ->
        result = emit_tacky(rhs, instructions)
        instructions.append(Copy(result, Var(v))) # 把計算結果 Copy 給變數 v
        return Var(v)  # 賦值表達式本身的回傳值是該變數的值
```

這種寫法不太有效率：常常先把右邊算進暫存變數，再多一個 `Copy` 到 `v`。Part III 的最佳化會消掉部分多餘的 copy。

**整個函式的例子 (Listing 5-13/5-14)**：假設 `a`、`b` 被改名為 `a.1`、`b.0`，暫存變數叫 `tmp.n`。

```c
int main(void) {
    int b;            // 沒有 initializer：不產生 TACKY
    int a = 10 + 1;
    b = a * 2;
    return b;
}
```
```text
tmp.2 = 10 + 1
a.1 = tmp.2
tmp.3 = a.1 * 2
b.0 = tmp.3
Return(b.0)
```

### 4.2 沒有 Return 的函式處理 (Edge Case)
C 語言規定，如果 `main` 函式執行到結尾沒有遇到 `return`，應隱式回傳 `0`。對於其他函式，如果 caller 沒有使用回傳值，則行為合法，但若 caller 試圖讀取，則是未定義行為 (Undefined Behavior)。

編譯 `foo` 這類函式時，不知道 caller 會不會用回傳值，只能假設是合法的情況：函式結尾仍要恢復 caller 的 stack frame 並返回；回傳值隨意，EAX 設什麼都可以（甚至不設）。

**解法**：
在 TACKY Generation 的最後，無條件在每個函式的指令串尾端加上 `Return(Constant(0))`。
* 若函式本來就以 `return` 結尾，這個隱式 `return 0` 永遠不會執行，不改變行為（Part III 會在不需要時消掉它）。
* 若原程式沒有 `return`，這確保了程式能正常退出，且對於 `main` 函式正好符合 C 標準規範。

---

## 5. 組合語言生成 (Assembly Generation)
由於 TACKY IR 沒有改變 (還是 `Copy`, `Var` 等)，我們**完全不需要修改** Assembly Generation 或 Code Emission 階段！
在此前的實作中，遇到 TACKY 的 `Var` 時，我們就已經透過分配 Stack 的 Offset (例如 `-4(%rbp)`) 來處理了。使用者定義的區域變數現在直接享受了這套機制的紅利。

---

## 6. Extra Credit：複合賦值、遞增與遞減 (Compound Assignment, Increment, Decrement)

書上的要求：
* **複合賦值**：`+=`、`-=`、`*=`、`/=`、`%=`；若第三章做了位元運算，再加 `&=`、`|=`、`^=`、`<<=`、`>>=`。
* **`++` / `--`**：前綴 `++a` 先加減，運算式的值是**新值**；後綴 `a++` 也會加減，但運算式的值是**原本的值**。
* 和本章其他功能一樣，TACKY generation 之後的階段都不用改。
* 測試旗標：`--compound`、`--increment`（位元複合賦值要同時加 `--bitwise`）；`--extra-credit` 一次涵蓋全部。

lemon_cc 的做法（main.c）：
* **Lexer**：`TOK_PLUS_ASSIGN` … `TOK_RSHIFT_ASSIGN`、`TOK_INCREMENT`、`TOK_DECREMENT`。
* **Parser**：複合賦值跟 `=` 一樣是優先級 1、右結合（遞迴呼叫傳 `prec`，不 +1），建出 `EXP_COMPOUND_ASSIGN`。前綴 `++`/`--` 在 `parse_factor` 開頭處理（`EXP_PREFIX_OP`），後綴在 `parse_factor` 結尾用迴圈吃掉（`EXP_POSTFIX_OP`），所以後綴綁得比前綴緊。
* **Lvalue 檢查**：`resolve_expression` 對複合賦值和 `++`/`--` 的運算元都要求是 `Var`，否則報 invalid lvalue（例如 `5 += 1`、`++(a + 1)`、`a++++`）。
* **TACKY**（`gen_tacky_exp`）：
  ```text
  a += b      →  v = <b>;  tmp = a + v;  a = tmp;   值為 a
  ++a         →  tmp = a + 1;  a = tmp;             值為 tmp（新值）
  a++         →  old = a;  tmp = a + 1;  a = tmp;   值為 old（原值）
  ```

---

## 進階知識與細節 (Undefined Behavior)

本章介紹了變數後，C 語言中經典的**未定義行為 (Undefined Behavior, UB)** 也隨之而來：
1. **讀取未初始化的變數**：除了少數例外，使用未初始化變數的值是 UB。常見結果是配置了 Stack 空間卻沒初始化，回傳記憶體裡殘留的垃圾值 (Garbage value)；但既然是 UB，也可能讓程式崩潰或讓其他函式出錯。(此實作不主動攔截)
2. **無順序副作用 (Unsequenced Side Effects)**：
   二元運算子 (如 `+`, `*`) 的兩個運算元是 unsequenced，可以任意順序求值。在 unsequenced 的運算式中對同一個變數賦值多次，或者一邊讀取一邊賦值，都是 UB。
   > 範例：`(a = 4) + (a = 5);` 或 `return (a = 1) + a;`
   > 看起來只是「回傳 4 或 5」二選一，但 UB 代表標準對程式行為**完全沒有限制**；實務上大多回傳 4 或 5，但沒有保證。
3. **`sizeof` 例外**：`unsigned int foo = sizeof foo;` 不是 UB，因為 `sizeof` 只需要 `foo` 的大小，不需要它的值。

**Sidebar：typedef 的麻煩**：本書的架構是先 parse 完整個程式再做變數解析，所以無法處理 `typedef`。`return (foo) * x;` 在 `foo` 是型別時是轉型加解參考，是變數時是乘法，所以 C 是 **context-sensitive grammar**，必須邊 parse 邊解析識別字。Production compiler 在 parse 時就維護 symbol table；把型別名回饋給 lexer、當成不同 token 的做法叫 **lexer hack**。

---

## 一句話總結

> 第五章在前端加入宣告、`Var`、`Assignment` 和 block item，並新增第一個語意分析 pass（variable resolution）負責改名與擋錯；TACKY 本來就有 `Var` 和 `Copy`，所以 TACKY generation 之後完全不用改。

## 速記重點

```text
lexer    -> 新 token "="（變數名就是 identifier）
parser   -> block_item = S | D、Declaration(name, exp?)、Expression / Null
         -> "=" 優先級 1、右結合：遞迴傳 prec，不 +1，產生 Assignment
semantic -> variable map：重複宣告、未宣告、invalid lvalue；改名成唯一名稱
         -> 唯一名稱與 TACKY temp 共用 counter，用 C 不合法的名字（a.0）
TACKY    -> Var → Var、Assignment → <rhs> + Copy、無 init 的宣告不產生指令
         -> 函式尾端一律補 Return(Constant(0))
asm      -> 不用改
extra    -> += -= ... 與 ++/--：前綴回新值、後綴回原值
```
