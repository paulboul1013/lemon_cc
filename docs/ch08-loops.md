# 第八章：迴圈（Loops）

## 1. 這章在做什麼

新增五種 statement：`while`、`do`、`for`、`break`、`continue`。這是書中最後一批要實作的 statement；如果 extra credit 也都做了，編譯器就能處理 C 的**所有** statement。

要改的地方：

* Lexer、Parser：五個新 statement
* 語意分析：新增一個 pass，叫 **loop labeling**，把每個 `break` / `continue` 和它所屬的迴圈連起來
* TACKY generation：用**現有的** TACKY 指令就能表達，所以 TACKY 之後的階段都不用改

```text
Semantic analysis
  ├─ Variable resolution
  └─ Loop labeling      <- 本章新增
```

---

## 2. 新 statement 的語意

### 2.1 while 與 do

* `while (cond) body`：先算 cond，是 0 就結束；不是 0 就執行 body，再回到 cond。
* `do body while (cond);`：先執行 body 再算 cond，所以 body **至少執行一次**。
* loop body 只是一個 statement，可以是含宣告的 compound statement；body 裡宣告的變數**在 cond 裡看不到**：

```c
do {
    int a = a + 1;
} while (a < 100);   // 錯誤：這裡的 a 不在 scope 內
```

### 2.2 for

```c
for (init; cond; post) body
```

* `init` 只在第一輪之前執行一次
* 每一輪：算 `cond`（false 就結束）→ 執行 `body` → 算 `post`
* 三個子句都可以省略。省略 `init` / `post` 就是什麼都不做；省略 `cond` 等於永遠為真，只能靠 `break`、`goto`、`return` 離開
* `init` 可以是**宣告**，而且 for 的 header 會開一個**新 scope**：

```c
int a = 5;                            // 第 1 個 a
for (int a = 0; a < 5; a = a + 1) {   // 第 2 個 a，遮蔽第 1 個
    int a = 1;                        // 第 3 個 a，body 是 compound，又是新 scope
    b = b + a;
}
```

* header 的三個子句都能省略，但 **body 不能省略**（三種迴圈都一樣）。body 可以是 null statement：

```c
while ((a = a + 1) < 10)
    ;
```

### 2.3 break 與 continue

* `break`：跳到**包住它的最內層迴圈**結束之後的位置。書中把這個最內層迴圈叫做 **enclosing loop**。
* `continue`：跳到 enclosing loop **body 最後一個 statement 之後**。接下來照常走：`while` 回到 cond，`for` 去執行 `post`。
* 兩者只能出現在迴圈裡（extra credit 的 `switch` 例外，`break` 也能用在 switch 裡）。出現在迴圈外是**編譯錯誤**：

```c
int main(void) {
    break;   // 錯誤
}
```

* 包在好幾層 `if` / block 裡面沒問題；同一個迴圈裡可以有多個 `break` / `continue`。

---

## 3. Lexer

新增五個關鍵字，沒有其他新 token：

```text
do  while  for  break  continue
```

lemon_cc：`TOK_DO`、`TOK_WHILE`、`TOK_FOR`、`TOK_BREAK`、`TOK_CONTINUE`（extra credit 再加 `TOK_SWITCH`、`TOK_CASE`、`TOK_DEFAULT`）。

---

## 4. Parser

### 4.1 AST（ASDL）

```text
for_init  = InitDecl(declaration) | InitExp(exp?)
statement = Return(exp)
          | Expression(exp)
          | If(exp condition, statement then, statement? else)
          | Compound(block)
          | Break
          | Continue
          | While(exp condition, statement body)
          | DoWhile(statement body, exp condition)
          | For(for_init init, exp? condition, exp? post, statement body)
          | Null
```

* `For` 最複雜：`init` 可以是宣告、運算式或什麼都沒有，所以要一個新節點 `for_init`；`condition` 和 `post` 都是 optional。
* lemon_cc：`ForInit` 帶 `FOR_INIT_DECL` / `FOR_INIT_EXP`，`exp == NULL` 表示空的 init。

### 4.2 label 要掛在哪

loop labeling 會替每個 `Break`、`Continue` 和迴圈 statement 標上一個 label，AST 需要有地方放。書中提兩種做法：

1. 每個新 constructor 多一個 `identifier label` 欄位，parse 時先放 dummy label，loop labeling 時再換成真的
2. 定義兩份 AST：labeling 前一份、labeling 後一份（帶 annotation）

看實作語言和個人喜好決定。lemon_cc 用第 1 種：`Statement` 有一個 `loop_label` 欄位，由 labeling pass 填入。

### 4.3 EBNF

```text
<for-init>  ::= <declaration> | [ <exp> ] ";"
<statement> ::= ...
              | "break" ";"
              | "continue" ";"
              | "while" "(" <exp> ")" <statement>
              | "do" <statement> "while" "(" <exp> ")" ";"
              | "for" "(" <for-init> [ <exp> ] ";" [ <exp> ] ")" <statement>
              | ";"
```

注意 `<declaration>` 本身就以 `";"` 結尾，所以不管 init 是哪一種，`<for-init>` 都會吃掉 header 的第一個分號。

### 4.4 optional expression 的 helper

書中建議寫一個 helper 解析 optional expression，並且**讓呼叫者指定結束 token**：

* 文法裡大部分 optional expression 後面接 `;`（for header 的前兩個子句、expression statement、null statement）
* for header 的第三個子句後面接 `)`

lemon_cc：`parse_optional_exp(end_token)`，下一個 token 就是 `end_token` 時回傳 `NULL`。另有 `parse_for_init`、`parse_while_statement`、`parse_do_while_statement`、`parse_for_statement`。

---

## 5. 語意分析

本章語意分析做兩件事，**分成兩個 pass**，各自走過整個程式：先 variable resolution，再 loop labeling。

### 5.1 Variable resolution

* `while`、`do`：跟 `if` 一樣，遞迴處理每個子 statement 和子運算式
* `break`、`continue`：沒有子節點，什麼都不用做
* `for`：header 會開新 scope，所以先複製 variable map

```text
resolve_statement(For(init, condition, post, body), variable_map):
    new_variable_map = copy_variable_map(variable_map)
    init      = resolve_for_init(init, new_variable_map)
    condition = resolve_optional_exp(condition, new_variable_map)
    post      = resolve_optional_exp(post, new_variable_map)
    body      = resolve_statement(body, new_variable_map)

resolve_for_init(init, variable_map):
    InitExp(e)  -> InitExp(resolve_optional_exp(e, variable_map))
    InitDecl(d) -> InitDecl(resolve_declaration(d, variable_map))
```

* 複製 map 的作用和 compound statement 一樣：header 宣告的變數在迴圈外看不到，而且遮蔽外層同名變數**不會**被當成重複宣告
* `resolve_optional_exp`：有運算式就 `resolve_exp`，沒有就什麼都不做
* init 是宣告時，`resolve_declaration` 會把新變數加進 map，所以 cond、post、body 都看得到它

lemon_cc：`resolve_statement` 的 `STMT_FOR` 分支用 `copy_identifier_map` 建內層 map，再呼叫 `resolve_for_init`。

### 5.2 Loop labeling

目標：每個迴圈拿到一個**唯一 ID**，每個 `break` / `continue` 標上 enclosing loop 的 ID；在迴圈外遇到 `break` / `continue` 就報錯。

```c
/*1*/ while (a > 0) {
/*2*/     for (int i = 0; i < 10; i = i + 1) {
              if (i % 2 == 0)
/*2*/             continue;      // 最內層是 for
              a = a / 2;
          }
          if (a == b)
/*1*/         break;             // 最內層是 while
      }
```

做法：像傳 variable map 一樣，把「目前的 loop ID」當參數往下傳。迴圈外是 null。

```text
label_statement(statement, current_label):
    Break    -> if current_label is null: fail("break statement outside of loop")
                return annotate(Break, current_label)
    Continue -> if current_label is null: fail("continue statement outside of loop")
                return annotate(Continue, current_label)
    While(condition, body) ->
                new_label = make_label()
                labeled_body = label_statement(body, new_label)
                return annotate(While(condition, labeled_body), new_label)
    DoWhile、For -> 和 While 一樣
    其他 statement -> 對每個子 statement 遞迴，傳同一個 current_label
```

* `make_label` 可以直接用 TACKY 產生 label 的那個 helper
* `annotate` 的寫法取決於 AST 怎麼存 annotation，書上沒給
* 測試 `invalid_semantics` 裡的錯誤有兩類：variable resolution 擋下的，和 loop labeling 擋下的（迴圈外的 break / continue）

lemon_cc：`label_statement(stmt, current_break_label, current_continue_label)` 和 `label_block_items`。迴圈用 `make_label("loop")` 產生 ID（如 `loop_0`），body 的 break / continue 都傳這個 ID。拆成兩個參數是為了 switch（見 7.3）。它在 `validate_program_pass` 裡、goto label 檢查（`check_labels_statement`、`check_goto_statement`）之後執行。

---

## 6. TACKY 生成

TACKY IR **不用改**，全部用現有指令。

### 6.1 break / continue

每個迴圈都產生兩個 label：

* **continue label**：放在 loop body 的指令之後
* **break label**：整個迴圈的最後一條指令

label 名稱從 loop ID 推出來，例如 ID 是 `loop0`：

```text
Break    (ID loop0) -> Jump("break_loop0")
Continue (ID loop0) -> Jump("continue_loop0")
```

命名方式可以自己定，但不能和 TACKY 程式裡其他 label 撞名。重點是：break / continue 和它的 enclosing loop 帶同一個 ID，所以兩邊能推出同一個 label。

這兩個 label 只有在 body 裡真的有 break / continue 時才會用到，但為了簡單**一律產生**，不必先檢查 body 裡有沒有。

### 6.2 do

```text
Label(start)
<instructions for body>
Label(continue_label)
<instructions for condition>
v = <result of condition>
JumpIfNotZero(v, start)
Label(break_label)
```

continue label 在 body 和 condition 之間；break label 在最後。

### 6.3 while

```text
Label(continue_label)
<instructions for condition>
v = <result of condition>
JumpIfZero(v, break_label)
<instructions for body>
Jump(continue_label)
Label(break_label)
```

* 不用額外的 Label，直接沿用開頭和結尾的 Label
* break label 同時是 `JumpIfZero` 和所有 break 的目標
* continue label 放在**開頭**：效果和放在 body 之後一樣（body 之後就是無條件跳回開頭），但 continue 可以省掉一次 `Jump`

### 6.4 for

```text
<instructions for init>
Label(start)
<instructions for condition>
v = <result of condition>
JumpIfZero(v, break_label)
<instructions for body>
Label(continue_label)
<instructions for post>
Jump(start)
Label(break_label)
```

* `init` 在 `Label(start)` 之前，所以只執行一次
* cond 為 0 就直接跳到最後，body 和 post 都不執行
* continue label 在 body 之後、**post 之前**，所以 continue 之後仍會執行 post
* 三個子句的處理：

| 子句 | 有 | 沒有 |
| :--- | :--- | :--- |
| init | 宣告或運算式，和 for 外面的處理方式一樣 | 不產生指令 |
| cond | 和 while / do 的 condition 一樣 | 書中引用 C 標準 section 6.8.5.3 第 2 段：視為「replaced by a nonzero constant」；`JumpIfZero(Const(1), ...)` 永遠不會跳，所以**整條省略** |
| post | 轉成 TACKY | 不產生指令 |

### 6.5 三種迴圈比較

| 迴圈 | continue label | break label |
| :--- | :--- | :--- |
| do | body 之後、condition 之前 | `JumpIfNotZero` 之後 |
| while | 開頭（condition 之前） | `Jump` 之後 |
| for | body 之後、post 之前 | `Jump(start)` 之後 |

lemon_cc：`gen_tacky_statement` 的 `STMT_WHILE` / `STMT_DO_WHILE` / `STMT_FOR` 分支；`make_loop_target("break", loop_label)` 產生 `break_loop_0` 這類名稱，for 的 init 由 `gen_tacky_for_init` 處理。輸出的組語 label 是 `.Lstart_loop_0`、`.Lcontinue_loop_0`、`.Lbreak_loop_0`。

### 6.6 為什麼後端不用改

迴圈只用到 `Jump`、`JumpIfZero`、`JumpIfNotZero`、`Label`，第四章做 `&&` / `||` 時就有了。assembly generation、pseudoregister 取代、fix-up、code emission 都不用動。

---

## 7. Extra credit：switch / case / default

### 7.1 書上給的方向

書中這部分只有方向，沒有給 ASDL、虛擬碼或 TACKY：

* **loop labeling 要改**：`break` 也能跳出 `switch`，但 `continue` 不能以 `switch` 為目標，所以兩者要分開處理
* **要多一個 pass 收集 case**（大概要獨立一個 pass）：產生 switch 的 TACKY 需要知道它所有的 case，但 AST 沒有直接提供。case 可能藏在好幾層裡面，switch body 也可能一個 case 都沒有，所以要把這些資訊整理後掛到 AST 上
* 測試：`./test_compiler ... --chapter 8 --switch`（或用 `--extra-credit` 開啟所有 extra credit 測試）

以下語意規則來自 C 標準，實作細節是 lemon_cc 的做法。

### 7.2 語意規則

* `case` 的值必須是**整數常數運算式**
* 同一個 switch 裡不能有兩個值相同的 case（比的是轉換成 controlling expression 型別之後的值）
* 一個 switch 最多一個 `default`
* `case` / `default` 只能出現在 switch 裡
* `break` 跳出最內層的 switch **或**迴圈；`continue` 只認迴圈。switch 在迴圈裡時，continue 跳到外層迴圈的 continue label；switch 不在任何迴圈裡時，continue 是錯誤

### 7.3 Loop labeling 的修改（lemon_cc）

`label_statement` 拆成兩個參數 `current_break_label` 和 `current_continue_label`：

| 遇到 | break 目標 | continue 目標 |
| :--- | :--- | :--- |
| `while` / `do` / `for` | 新的 `loop_N` | 新的 `loop_N` |
| `switch` | 新的 `switch_N` | **沿用外層** |
| `case` / `default` / `if` / block | 沿用 | 沿用 |

```c
for (...) {
    switch (x) {
        case 0: continue;   // -> continue_loop_0
        case 1: break;      // -> break_switch_1
    }
}
```

`make_label` 的 loop、switch、case、default 共用同一個計數器，所以編號是 `loop_0`、`switch_1`、`case_2`……，不是各自從 0 開始。

### 7.4 Case 收集（lemon_cc）

`collect_switch_cases_statement(stmt, current_switch)` 往下走，`current_switch` 是目前最內層的 switch：

* `switch`：清空自己的 case 清單，用**自己**當 `current_switch` 走 body，所以內層 switch 的 case 不會算到外層
* `case`：
  * `current_switch` 是 NULL → case 在 switch 外，報錯
  * `eval_const_expr` 折不出常數 → 報錯
  * control 是 `int` 時把值截成 `int`，再用 `switch_case_exists` 檢查重複
  * `make_label("case")` 產生 label，`add_switch_case` 把 `(value, label)` 加到這個 switch 的 `SwitchCaseInfo` 串列
* `default`：不在 switch 裡或已經有 default 都報錯；`make_label("default")`，記在 switch 的 `default_label`
* `if`、labeled statement、block、`while`、`for`、`do`：帶同一個 `current_switch` 遞迴，處理藏在深處的 case

`collect_program_switch_cases` 在 `typecheck_program` **之後**才執行，因為 case 值要先轉成 control 的型別才能判斷是否重複（第十一章的需求，見 ch11 筆記）。

### 7.5 TACKY lowering（lemon_cc）

```text
<instructions for control>
c = <result of control>
// 每個 case，照原始碼順序
tmp = Binary(Equal, c, case_value)
JumpIfNotZero(tmp, case_label)
...
Jump(default_label)        // 沒有 default 就 Jump(break_label)
<instructions for body>    // case / default 只產生 Label(它的 label)，再接子 statement
Label(break_label)
```

* 比較的部分在 `emit_switch_dispatch`
* `case` 只是在 body 中放一個 Label，所以沒寫 `break` 時會自然 fall through 到下一個 case
* switch 裡的 `break` 是 `Jump(break_switch_N)`
* 用的還是既有指令（`Binary(Equal)`、`JumpIfNotZero`、`Jump`、`Label`），後端同樣不用改

---

## 8. lemon_cc 對應

| 階段 | 函式 / 結構 |
| :--- | :--- |
| Lexer | `TOK_DO`、`TOK_WHILE`、`TOK_FOR`、`TOK_BREAK`、`TOK_CONTINUE`、`TOK_SWITCH`、`TOK_CASE`、`TOK_DEFAULT` |
| AST | `STMT_WHILE` / `STMT_DO_WHILE` / `STMT_FOR` / `STMT_BREAK` / `STMT_CONTINUE` / `STMT_SWITCH` / `STMT_CASE` / `STMT_DEFAULT`、`ForInit`、`Statement.loop_label`、`SwitchCaseInfo` |
| Parser | `parse_optional_exp`、`parse_for_init`、`parse_while_statement`、`parse_do_while_statement`、`parse_for_statement`、`parse_switch_statement`、`parse_case_statement`、`parse_default_statement` |
| Variable resolution | `resolve_statement`、`resolve_for_init`、`copy_identifier_map` |
| Loop labeling | `label_statement`、`label_block_items`（由 `validate_program_pass` 呼叫） |
| Case 收集 | `collect_program_switch_cases`、`collect_switch_cases_statement`、`eval_const_expr`、`switch_case_exists`、`add_switch_case` |
| TACKY | `gen_tacky_statement`、`gen_tacky_for_init`、`make_loop_target`、`emit_switch_dispatch` |

相關 commit：迴圈是 `6f7fb16`、`32aed18`、`0096749`、`7282e3a`；switch 從 `72d98c6` 到 `81e90f1`，之後有修正 `baf4c24`（switch parsing 與 case 收集的 bug）和 `f9e1926`（case label 依節點型別折疊）。

---

## 9. 一句話總結

> 第八章的本質，是在 parser 和 TACKY 之間多一個 loop labeling pass，讓每個 break / continue 知道自己屬於哪個迴圈；有了這個 ID，三種迴圈和 break / continue 都只是現有 Label 與 Jump 指令的排列。

## 10. 速記重點

```text
lexer    -> do while for break continue
parser   -> While / DoWhile / For / Break / Continue、for_init = InitDecl | InitExp(exp?)
            optional exp helper 指定結束 token（; 或 )）
resolve  -> for header 複製 variable map（新 scope）
labeling -> 傳 current_label 往下走；迴圈產生新 ID；break / continue 在迴圈外報錯
TACKY    -> break = Jump(break_ID)、continue = Jump(continue_ID)
            do:    start / body / continue / cond / JNZ start / break
            while: continue / cond / JZ break / body / Jump continue / break
            for:   init / start / cond / JZ break / body / continue / post / Jump start / break
            for 沒有 cond -> 不產生 JumpIfZero
backend  -> 不用改
switch   -> break 認 switch、continue 只認迴圈；收集 case（常數、不重複、最多一個 default）
            比較鏈 JNZ 到各 case label，最後 Jump default / break
```

---
