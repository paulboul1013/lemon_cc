# 第六章：if 陳述式與條件運算式（If Statements and Conditional Expressions）

---

## 1. 本章核心目標

本章是 **第一個 control structure（控制結構）**。程式執行陳述式的順序叫 control flow，能改變 control flow 的語言構造叫 control structure。

之前 compiler 只能做：

```
順序執行
expression evaluation
return
```

現在加入：

```
branching (分支)
```

兩個語言構造：

```
1. if statement
2. ternary conditional expression (?:)
```

第四章為了 `&&` / `||` 的短路求值，已經在 TACKY 加了條件跳躍指令，所以 **TACKY generation 之後的階段都不用改**。

---

## 2. Conditional Constructs

本章加入兩種 conditional：

### 2.1 If Statement

語法：

```
if (condition)
    statement
```

或

```
if (condition)
    statement
else
    statement
```

`condition` 又叫 controlling expression。條件規則：

```
0     → false（執行 else，若有）
non-0 → true （執行 then）
```

這與之前 boolean operators 相同。

---

### 2.2 Conditional Expression

語法：

```
a ? b : c
```

語義：

```
if a != 0
    result = b
else
    result = c
```

重要特性：

**short-circuit evaluation**：C 標準保證先求值 condition，再依結果**只**求值其中一個 clause。

例如：

```c
0 ? foo() : bar()
```

只會執行：

```
bar()
```

不能同時執行兩個 expression（例如兩個都算完再用 condition 挑結果）。

否則可能產生意料之外的：

```
side effects
segmentation fault
I/O
```

因此 compiler 必須：

```
只執行被選中的 branch
```


---

## 3. Statement vs Expression

這是本章非常重要的語言概念。

### 3.1 Statement

statement **沒有 value**

例如：

```
return 3;
if (a) return 1;
```

---

### 3.2 Expression

expression **有 value**

例如：

```
1 + 2
a ? b : c
```

---

### 3.3 差異

合法：

```c
int x = flag ? 2 : 3;
```

非法：

```c
int x = if (flag) 2 else 3;
```

原因：

```
if 是 statement
?: 是 expression
```

---

## 4. Else If 的語法糖

C 沒有真正的：

```
else if
```

一個 if 最多只有一個 else clause，`else if` 其實只是 else clause 裡面放了另一個 if statement：

```
else
    if (...)
```

例如：

```
if (a)
    ...
else if (b)
    ...
else
    ...
```

實際解析：

```
if (a)
    ...
else
    if (b)
        ...
    else
        ...
```

AST 是巢狀的 `If`：

```
If(cond=a, then=..., else=If(cond=b, then=..., else=...))
```

這對 parser 非常重要：AST 不需要 `else if` 節點。

---

## 5. Lexer

新增 4 個 token：

| Token | 用途 |
| :--- | :--- |
| `if` | if statement 開頭的 keyword |
| `else` | else clause 開頭的 keyword |
| `?` | 條件運算式第一、二個運算元之間的分隔 |
| `:` | 條件運算式第二、三個運算元之間的分隔 |

---

## 6. Grammar 與 AST

### 6.1 Grammar (EBNF)

```ebnf
<statement> ::= "return" <exp> ";"
              | <exp> ";"
              | "if" "(" <exp> ")" <statement> [ "else" <statement> ]
              | ";"
<exp> ::= <factor> | <exp> <binop> <exp> | <exp> "?" <exp> ":" <exp>
```

其餘規則（`<program>`、`<function>`、`<block-item>`、`<declaration>`、`<factor>`…）與第五章相同。

注意：

```
statement 不能是 declaration
```

例如：

非法：

```
if (x)
    int y = 3;
```

因為 if 的每個 clause 都是**單一 statement**，而 declaration 不是 statement。`if (x) { int y = 3; }` 才能在 if 裡宣告變數，但 `{ ... }` 是 compound statement，第七章才實作。

---

### 6.2 AST (ASDL)

新增 `If` 和 `Conditional`：

```
statement = Return(exp)
          | Expression(exp)
          | If(exp condition, statement then, statement? else)   // 新增
          | Null

exp = Constant(int)
    | Var(identifier)
    | Unary(unary_operator, exp)
    | Binary(binary_operator, exp, exp)
    | Assignment(exp, exp)
    | Conditional(exp condition, exp, exp)                       // 新增
```

`statement? else` 表示 else 可省略。

If 的 AST 結構：

```
If
 ├── condition
 ├── then
 └── else (optional)
```

---

## 7. Parsing If Statements

直接用 recursive descent。

Pseudo code：

```
parse_statement():
    if token == IF
        parse_if_statement()
```

---

### parse_if_statement

Pseudo code：

```
parse_if_statement():

    expect(IF)
    expect("(")

    cond = parse_expression()

    expect(")")

    then_stmt = parse_statement()

    if next_token == ELSE
        consume
        else_stmt = parse_statement()
    else
        else_stmt = NULL

    return If(cond, then_stmt, else_stmt)
```

---

### Dangling else

這條文法規則是 **ambiguous** 的：

```
if (a)
    if (a > 10)
        return a;
    else
        return 10 - a;
```

文法本身允許 else 配給外層或內層的 if。C 標準規定：**else 一律配給最近的 if**（內層）。這叫 **dangling else ambiguity**，會困擾自動由文法產生 parser 的 parser generator。

手寫的 recursive descent parser 沒有這個問題：每 parse 完一個 then 子句，就馬上檢查下一個 token 是不是 `else`。內層 `parse_if_statement` 會先看到 `else`，所以 else 自然歸給內層 if，結果正確。

---

## 8. Parsing Conditional Expression

### 把 `?:` 當成怪異的二元運算子

技巧：把 `"?" <exp> ":"` 整段當成夾在中間的「二元運算子」，它剛好包含一個完整的子運算式，前後由 `?` 和 `:` 界定。（只在 parse 時這樣想；求值方式和二元運算完全不同。）

precedence：**比 `=` 高，比其他所有運算子低**。

| Operator | Precedence |
| :--- | :--- |
| `*` `/` `%` | 50 |
| `+` `-` | 45 |
| `<` `<=` `>` `>=` | 35 |
| `==` `!=` | 30 |
| `&&` | 10 |
| `\|\|` | 5 |
| **`?`** | **3** |
| `=` | 1 |

precedence climbing 只看這個「運算子」的第一個 token，所以 `?` 進表，`:` 不進表。

例如：

```
a = 1 ? 2 : 3       →  a = (1 ? 2 : 3)
a || b ? 2 : 3      →  (a || b) ? 2 : 3
1 ? 2 : 3 || 4      →  1 ? 2 : (3 || 4)
1 ? 2 : a = 5       →  (1 ? 2 : a) = 5     // 語意分析會擋：不是合法 lvalue
```

`?` 和 `:` 之間可以放**任何**運算式（作用像括號），包含賦值和另一個條件運算式：

```
x ? x = 1 : 2       →  x ? (x = 1) : 2
a ? b ? 1 : 2 : 3   →  a ? (b ? 1 : 2) : 3
```

ternary operator 是 **right-associative**：

```
a ? b : c ? d : e   →  a ? b : (c ? d : e)
```

---

### Precedence Climbing (Listing 6-9)

```
parse_exp(tokens, min_prec):
    left = parse_factor(tokens)
    next_token = peek(tokens)
    while next_token is a binary operator and precedence(next_token) >= min_prec:
        if next_token is "=":
            take_token(tokens)
            right = parse_exp(tokens, precedence(next_token))
            left = Assignment(left, right)
        else if next_token is "?":
            middle = parse_conditional_middle(tokens)
            right = parse_exp(tokens, precedence(next_token))
            left = Conditional(left, middle, right)
        else:
            operator = parse_binop(tokens)
            right = parse_exp(tokens, precedence(next_token) + 1)
            left = Binary(operator, left, right)
        next_token = peek(tokens)
    return left
```

注意：

* `parse_conditional_middle`：吃掉 `?`，用 **min_prec = 0** parse 一個運算式，再吃掉 `:`。
* 第三個運算元跟其他右手邊一樣遞迴呼叫 `parse_exp`；因為右結合，傳 `precedence(next_token)`，**不 +1**（和 `=` 一樣）。

---

## 9. TACKY IR

TACKY 是一種 three-address code。本章**不用新增任何 TACKY 指令**，沿用第四章為 `&&` / `||` 加的：

```
Jump(target)
JumpIfZero(condition, target)
JumpIfNotZero(condition, target)
Label(identifier)
Copy(src, dst)
```

做法也和短路求值一樣：先求值 controlling expression，再用條件跳躍跳到對應的 clause。

---

## 10. If Statement Code Generation

C code：

```
if (cond)
    S1
else
    S2
```

TACKY（Listing 6-13）：

```
<instructions for condition>
c = <result of condition>
JumpIfZero(c, else_label)
<instructions for S1>
Jump(end)
Label(else_label)
<instructions for S2>
Label(end)
```

Pseudo code：

```
compile_if(stmt):

    cond = compile_expression(stmt.condition)

    else_label = new_label()
    end_label  = new_label()

    emit JumpIfZero(cond, else_label)

    compile(stmt.then)

    emit Jump(end_label)

    emit Label(else_label)

    compile(stmt.else)

    emit Label(end_label)
```

---

## 11. If Without Else

```
if (cond)
    S
```

TACKY（Listing 6-12）：

```
<instructions for condition>
c = <result of condition>
JumpIfZero(c, end)
<instructions for S>
Label(end)
```

Pseudo code：

```
end_label = new_label()

emit JumpIfZero(cond, end_label)

compile(S)

emit Label(end_label)
```

---

## 12. Conditional Expression Code Generation

C code：

```
x = <condition> ? <e1> : <e2>
```

TACKY（Listing 6-14）：

```
<instructions for condition>
c = <result of condition>
JumpIfZero(c, e2_label)
<instructions to calculate e1>
v1 = <result of e1>
result = v1
Jump(end)
Label(e2_label)
<instructions to calculate e2>
v2 = <result of e2>
result = v2
Label(end)
```

和 if/else 幾乎一樣，差別只在 expression 有值：每個 clause 結尾都把結果 `Copy` 到同一個暫存變數 `result`。

Pseudo code：

```
compile_ternary(expr):

    cond = compile(expr.condition)

    false_label = new_label()
    end_label   = new_label()

    result = new_temp()

    emit JumpIfZero(cond, false_label)

    true_val = compile(expr.true_expr)
    emit Copy(true_val, result)

    emit Jump(end_label)

    emit Label(false_label)

    false_val = compile(expr.false_expr)
    emit Copy(false_val, result)

    emit Label(end_label)

    return result
```

所有產生的 label 和暫存變數名稱都必須唯一。

---

## 13. Why IR Makes This Easy

本章的重要 insight：

```
control flow = labels + jumps
```

IR 只需要：

```
Label
Jump
JumpIfZero / JumpIfNotZero
```

就可以實現：

```
if
ternary
loops
switch
```

因此：

```
later compiler stages 不需要改
```

---

## 14. Variable Resolution（語意分析）

改動很小：讓 `resolve_statement` 和 `resolve_exp` 走訪新節點的子陳述式和子運算式，變數改名方式和其他構造完全一樣。

```
If(condition, then, else)       → resolve condition、then、else（若有）
Conditional(cond, e1, e2)       → resolve 三個運算元
```

例如：

```
if (x)
```

但：

```
x 未宣告
```

需要 semantic error。另外 `1 ? 2 : a = 5` 會 parse 成 `(1 ? 2 : a) = 5`，在 lvalue 檢查時被擋下。

---

## 15. Extra Credit：Labeled Statements 與 goto

書上的要求：

* 加入 `goto` 和 labeled statement（`goto` 要跳的目標）。
* TACKY generation 之後都不用改。
* 要偵測新的錯誤，例如**同一個函式裡兩個 labeled statement 用同一個 label**。書建議另寫一個新的語意分析 pass，不要塞進 variable resolution。
* 測試旗標：`--goto`（或 `--extra-credit`）。

C 的 label 是 function scope：整個函式內都看得到，`goto` 可以往前或往後跳；label 和變數名稱分屬不同 name space。

Grammar：

```ebnf
<statement> ::= ...
              | "goto" <identifier> ";"
              | <identifier> ":" <statement>
```

* labeled statement 要往前看兩個 token：`<identifier>` 後面接 `:` 才是 label，否則當 expression statement。
* label 後面必須接一個 statement（不能是 declaration）。

語意檢查（lemon_cc 的 `validate_program_pass`，每個函式跑一次）：

```
第一輪 check_labels_statement : 收集函式內所有 label，重複 → "duplicate label"
第二輪 check_goto_statement   : 每個 goto 的目標必須在表內，否則 → "goto to undefined label"
```

要分兩輪，因為 `goto` 可以跳到後面才定義的 label。

TACKY：

```
goto foo;        →  Jump(foo)
foo: <stmt>      →  Label(foo)
                    <instructions for stmt>
```

label 命名：C 的 label 是 function scope，但組語的 `.L` label 在整個檔案共用。lemon_cc 用 `qualify_label` 把 label 改成 `函式名.label`（例如 `main.end` → `.Lmain.end`）。`.` 不能出現在 C 識別字，所以不會和其他函式的同名 label 撞名，也不會和 compiler 產生的 label（`if_end_0`、`cond_false_2`…）撞名。

---

## 16. Test Cases

典型測試：

### basic if

```
if (1)
    return 2;
return 3;
```

---

### if else

```
if (0)
    return 1;
else
    return 2;
```

---

### nested if

```
if (a)
    if (b)
        return 1;
    else
        return 2;
```

（dangling else：else 屬於內層 `if (b)`）

---

### ternary

```
return a ? b : c;
```

---

## 17. 本章重要編譯器概念

### 17.1 Control Flow

control flow graph：

```
       cond
      /   \
   true   false
    |       |
   S1      S2
      \   /
       end
```

---

### 17.2 Labels

compiler 會生成：

```
L1
L2
L3
```

這些 label：

```
不是 C label
是 compiler internal label
```

每個 label 都要唯一。實作 goto 之後，使用者寫的 C label 也會變成 TACKY `Label`，兩者不能撞名（見第 15 節）。

---

## 18. Compiler Pipeline Impact

本章要改：

```
Lexer               (if、else、?、:)
Parser / AST        (If、Conditional)
Variable resolution (走訪新節點)
TACKY generation
```

不用改：

```
Converting TACKY to assembly
Replacing pseudoregisters
Instruction fix-up
Code emission
```

因為 IR 已經足夠強。

---

## 19. Summary

完成了：

```
第一個 control structure
```

新增：

```
if statements
ternary expressions
```

並學到：

```
control flow → jumps
```

同時延伸：

```
precedence climbing
```

來支援：

```
?: operator
```

目前 compiler 仍有限制：

```
if body 只能是單一 statement，還不能是 compound statement
```

例如：

```
if (x) {
    int y = 3;
}
```

尚未支援，所以還不能在 if 裡宣告變數或執行多條陳述式。

下一章：

```
compound statements {}
```

並加入：

```
nested scope
```

這是 semantic analysis 的重要概念。

---

## 一句話總結

> 第六章在前端加入 `If` 和 `Conditional`，把 `?:` 當成優先級 3、右結合的「二元運算子」交給 precedence climbing；後端完全沿用第四章的 `JumpIfZero` / `Jump` / `Label` / `Copy`。

## 速記重點

```text
lexer    -> if、else、?、:
parser   -> If(cond, then, else?)、Conditional(cond, e1, e2)
         -> dangling else：else 配最近的 if，recursive descent 自然做對
         -> ? 優先級 3（= 之上、|| 之下），右結合；中間用 min_prec 0
semantic -> variable resolution 走訪新節點
TACKY    -> if    : JumpIfZero(c, else) / then / Jump(end) / Label(else) / else / Label(end)
         -> ?:    : 同上，兩邊都 Copy 到同一個 result
asm      -> 不用改
extra    -> goto / label：重複 label、未定義 label；label 以函式名限定
```
