# 第九章：函式（Functions）

## 1. 這章在做什麼

把只能編 `main` 的編譯器，擴充成能編任意多個函式、能呼叫函式，而且遵守 **System V x64 calling convention**。遵守 calling convention 之後：

* 我們編出來的程式可以呼叫外部函式庫（包括 libc，終於能做 I/O）
* 別的編譯器編出來的程式也可以呼叫我們編的函式庫

主要工作在兩個地方：

* **Semantic analysis**：identifier resolution 要處理 linkage，另外新增一個 **type checking** pass 和 **symbol table**
* **Assembly generation**：照 calling convention 傳參數、對齊 stack、取回傳值

---

## 2. 語言特性

### 2.1 宣告與定義

```c
int foo(int param1, int param2, int param3);   // 宣告（declaration）
int foo(void);                                 // 沒有參數要寫 void

int foo(int param1, int param2, int param3) {  // 定義（definition）= 宣告 + body
    return param1 + param2 + param3;
}
```

* 所有定義都是宣告，但宣告不一定是定義。
* 本章回傳型別和參數型別都只能是 `int`。
* 同一個函式可以**宣告很多次**，但必須相容：回傳型別、參數型別、參數數量都要一樣。參數名稱可以不同，因為只有定義裡的參數名稱會被用到。
* 同一個函式**只能定義一次**，不然呼叫時不知道該執行哪一個。
* 宣告可以出現在 **top level** 或**另一個函式的 body 裡**。
* **巢狀函式定義不合法**：C 標準只允許在 top level 定義函式。有些編譯器把它當擴充功能支援，本書不做。

下面合併 Listing 9-4、9-5 示意（非書上原例；整個程式因為巢狀定義而不合法）：

```c
int main(void) {
    int foo(int a, int b);                        // OK：block 內宣告
    int bar(int a, int b) { return a + b; };      // 錯誤：巢狀定義
    return foo(1, 2);
}
```

### 2.2 函式呼叫

* 宣告裡的叫 **parameter（參數）**，呼叫時傳進去的運算式叫 **argument（引數）**。
* 呼叫前**一定要先宣告**，但不用先定義。定義可以在同一個檔案後面，也可以在別的函式庫；找定義是 **linker** 的工作，找不到就 link 失敗。
* 呼叫未宣告的函式：很多編譯器只給警告，我們直接在 semantic analysis 報錯。
* 引數數量錯誤、把變數當函式呼叫，都是錯誤。

### 2.3 不實作的部分

* old-style 函式定義（`int foo(a, b) int a, b; { ... }`，C17 是 obsolescent，C23 已移除）
* 沒有參數列表的宣告 `int foo();`（C17 裡代表「不提供參數資訊」，也是 obsolescent；C23 改成等於 `int foo(void)`）
* 非定義的宣告裡省略參數名稱（標準允許，但我們要求每個參數都有名字）
* variadic 函式、`inline` / `_Noreturn`、function pointer

### 2.4 Identifier linkage

函式名和變數名都是 identifier，在**同一個 namespace**、遵守相同的 scope 規則，可以互相 shadow：

```c
int foo(int a, int b);
int main(void) {
    int foo = 3;        // 變數 foo 遮住函式 foo，回傳 3
    return foo;
}
```

反過來，block 內的函式宣告也可以 shadow 外層變數（Listing 9-10：`int a = 3;` 的內層 block 裡宣告 `int a(void);` 再 `return a();`）。

但解析方式不同：每個區域變數宣告都是**不同的實體**，而同名的多個函式宣告都指向**同一個函式**。這由 **linkage** 決定（C 標準 §6.2.2）：

| linkage | 意義 | 本章誰是 |
| :--- | :--- | :--- |
| external | 所有宣告指向同一個物件或函式，**跨 translation unit** 也成立 | 所有函式 |
| none | 每個宣告都是獨一無二的實體 | 區域變數、參數 |

* translation unit = 前處理過的一個原始檔。
* 因為所有宣告指向同一個函式，**不同 scope 的宣告也必須相容**：

```c
int main(void) {
    int f(int x);
    int ret = f(1);
    if (ret) {
        int f(int a, int b);   // 錯誤：和外層的 f 參數數量不同
        return f(0, 1);
    }
    return 0;
}
```

> 第十章會出現第三種 **internal linkage**，而且變數也可以有 linkage。所以「函式一定有 linkage、變數一定沒有」只在本章成立。

linkage 在 link time 的樣子：

| linkage | 組語裡 |
| :--- | :--- |
| external | global symbol（`.globl`），其他 object file 可以參考；同一個 global symbol 在兩個 object file 都定義會 link 失敗 |
| internal | local symbol（沒有 `.globl`），只在同一個 object file 內解析 |
| none | linker 看不到，只是相對 RBP 的一個 stack 位址 |

---

## 3. Compiler driver：`-c`

* 沒有 `main` 的原始檔（函式庫）用現在的 driver 會 link 失敗：linker 要把程式和呼叫 `main` 的 crt0 連起來。
* 支援 `-c`：照常產生組語，然後只組譯不連結：

```bash
gcc -c ASSEMBLY_FILE -o OUTPUT_FILE
```

* 輸出檔名是原檔名換成 `.o`：`./YOUR_COMPILER -c /path/to/program.c` → `/path/to/program.o`
* 選做：接受多個輸入檔，每個各自轉組語，再用 `gcc` 一起組譯、連結。

---

## 4. Lexer

只多一個 token：`,`（參數列表和引數列表用逗號分隔）。

---

## 5. Parser

### 5.1 AST

```text
program = Program(function_declaration*)
declaration = FunDecl(function_declaration) | VarDecl(variable_declaration)
variable_declaration = (identifier name, exp? init)
function_declaration = (identifier name, identifier* params, block? body)
block_item = S(statement) | D(declaration)
for_init = InitDecl(variable_declaration) | InitExp(exp?)
exp = ...
    | FunctionCall(identifier, exp* args)
```

* `function_definition` 改名成 `function_declaration`，加上 `params`，而且 **body 變成可選**，同一個 node 同時表示宣告和定義。
* `declaration` 分成 `FunDecl` / `VarDecl`；block item 裡的 declaration 可以是任一種。
* `for` 的 init **只能是** `variable_declaration`。
* `variable_declaration` 和 `function_declaration` 沒有 constructor 名稱，這在 ASDL 叫 **product type**（恰好一個沒命名的 constructor），只是省掉多餘 constructor 名稱的語法方便；之前那些用具名 constructor 的 node 叫 **sum type**。`function_declaration` 也拿掉了原本的 `Function` constructor 名稱。
* 這個 AST 能表示巢狀函式定義，要在 semantic analysis 擋掉。

### 5.2 EBNF

```text
<program> ::= { <function-declaration> }
<declaration> ::= <variable-declaration> | <function-declaration>
<variable-declaration> ::= "int" <identifier> [ "=" <exp> ] ";"
<function-declaration> ::= "int" <identifier> "(" <param-list> ")" ( <block> | ";" )
<param-list> ::= "void" | "int" <identifier> { "," "int" <identifier> }
<block-item> ::= <statement> | <declaration>
<for-init> ::= <variable-declaration> | [ <exp> ] ";"
<factor> ::= <int> | <identifier> | <unop> <factor> | "(" <exp> ")"
           | <identifier> "(" [ <argument-list> ] ")"
<argument-list> ::= <exp> { "," <exp> }
```

* `<param-list>`、`<argument-list>` 只是為了文法好讀，AST 裡沒有對應 node。
* `<param-list>` **必填**（沒參數就寫 `void`）；`<argument-list>` **可省略**。
* 解析列表：元素之間吃掉 `,`，看到 `)` 就停。
* 函式呼叫的優先順序比所有二元、三元運算子高，所以在 `<factor>` 處理：identifier 後面**往前看一個 token**，是 `(` 就是函式呼叫，否則是變數。
* 宣告也一樣：identifier 後面是 `(` 就是函式宣告。

---

## 6. Semantic analysis

本章結束時有三個 pass：**identifier resolution → type checking → loop labeling**（loop labeling 放在哪個位置都可以）。

### 6.1 Identifier resolution

原本的 variable resolution 改名叫 **identifier resolution**，因為它也處理函式。

* **只改名沒有 linkage 的 identifier**。有 external linkage 的名字必須保留原名，因為 linker 靠原名做 symbol resolution。
* `variable_map` → `identifier_map`；`from_current_block` → `from_current_scope`（函式宣告可以在 top level，不在 block 裡）。
* map entry 新增 **`has_linkage`**（本章代表有 external linkage，下一章改成有任何 linkage）。不要寫死「函式就有、變數就沒有」，下一章就不成立。

**FunctionCall**

```text
FunctionCall(fun_name, args) ->
    fun_name 不在 identifier_map -> fail("Undeclared function!")
    換成 map 裡的 new_name（合法程式裡就是原名）
    逐一 resolve 每個 arg
```

`fun_name` 可能其實是區域變數，這是型別錯誤，留給 type checker；引數數量也留給 type checker。

**函式宣告（`resolve_function_declaration`）**

```text
1. 名字已在 map 裡，且 from_current_scope 且 not has_linkage -> fail("Duplicate declaration")
2. 加入 map：new_name = 原名，from_current_scope = True，has_linkage = True
3. inner_map = copy(identifier_map)，在 inner_map 裡逐一 resolve 參數
4. 有 body 就用 inner_map（不是它的 copy）resolve body
```

* 同一個 scope 重複宣告有 external linkage 的名字是合法的（`int foo(void); int foo(void);`），只有舊宣告沒有 linkage（區域變數）才衝突。
* 參數開啟新的 scope：可以 shadow 外層名字（`int a; int foo(int a);` 合法），但同一個宣告裡不能重名（`int foo(int a, int a);` 不合法）。
* `resolve_param` 和解析區域變數宣告的邏輯一樣（檢查重複、產生唯一名稱、加入 map），可以共用 helper。
* 參數和 body 在**同一個 scope**，所以下面是重複宣告：

```c
int foo(int a) {
    int a = 3;   // 錯誤：和參數 a 同一個 scope
    return a;
}
```

* 函式名先加進外層 map 再 copy，所以 body 裡看得到自己，可以遞迴。
* 沒有 body 的宣告也照樣 resolve 參數，處理方式統一。

**Local declarations**

* 區域變數照舊，記得標 `has_linkage = False`。
* 區域函式宣告：**有 body 就報錯**（巢狀定義），沒有 body 就呼叫 `resolve_function_declaration`。

**Top level**

依序處理每個函式宣告，函式名留在 map 裡給後面的宣告用；參數和區域變數在 inner scope，後面的函式看不到。

### 6.2 Type checker

下面這些錯誤跟「什麼名字在 scope 內」無關，是**型別錯誤**，所以放在獨立的 type checking pass：

| 錯誤 | 例子 |
| :--- | :--- |
| 把變數當函式呼叫 | `int x = 3; return x();` |
| 把函式名當變數用 | `foo` 宣告成函式，卻寫 `return foo;`（非書上原例，書上只在 Listing 9-22 列出這個錯誤） |
| 同一個函式的宣告型別不同 | `int foo(int a, int b);` 再 `int foo(int a);` |
| 引數數量錯誤 | `int foo(int a, int b);` 再 `foo(1)` |
| 同一個函式定義兩次 | 嚴格說不是型別錯誤，但在這裡查最方便 |

型別表示：

```text
type = Int | FunType(int param_count)
```

本章所有變數都是 `int`，函式型別只差在參數數量。

**Symbol table**：記錄每個 identifier 的型別；函式另外記 **`defined`**（是否已經看過有 body 的宣告）。之後的階段都會用到，建議做成全域變數。這和 object file 裡給 linker 用的 symbol table 不是同一個東西，它只存在編譯器內部。

本章的 type checker **不改寫 AST**，只填 symbol table、報錯（Part II 才會改寫 AST）。

```text
typecheck_variable_declaration(decl):
    symbols.add(decl.name, Int)          // 已經唯一命名，不會衝突
    有 init 就 typecheck init

typecheck_function_declaration(decl):
    fun_type = FunType(length(decl.params))
    has_body = decl.body is not null
    already_defined = False
    if decl.name in symbols:
        old = symbols.get(decl.name)
        1. old.type != fun_type        -> fail("Incompatible function declarations")
        already_defined = old.defined
        2. already_defined and has_body -> fail("Function is defined more than once")
    3. symbols.add(decl.name, fun_type, defined = already_defined or has_body)
    4. 有 body：每個參數 symbols.add(param, Int)，再 typecheck body

typecheck_exp:
    FunctionCall(f, args):
        f 的型別是 Int                     -> fail("Variable used as function name")
        param_count != length(args)       -> fail("Function called with the wrong number of arguments")
        逐一 typecheck args
    Var(v):
        v 的型別不是 Int                   -> fail("Function name used as variable")
```

* 步驟 3 會覆蓋舊 entry，沒關係，因為型別不會變；只要 `defined` 有考慮舊值。
* Symbol table 包含**所有已檢查過的宣告，就算已經不在 scope 內**：

```c
int main(void) {
    int foo(int a);       // 這個宣告離開 main 就不在 scope
    return foo(1);
}
int foo(int a, int b);    // 但它還在 symbol table 裡，所以能抓到衝突
```

---

## 7. TACKY

```text
program = Program(function_definition*)
function_definition = Function(identifier, identifier* params, instruction* body)
instruction = ...
    | FunCall(identifier fun_name, val* args, val dst)
```

* Program 變成**函式列表**，函式定義帶參數。
* **只有宣告、沒有 body 的函式不產生 TACKY**，跟沒有 initializer 的變數宣告一樣直接丟掉。
* `FunCall` 的引數和其他指令的運算元一樣，必須是常數或變數，不能是運算式：

```text
<instructions for e1>
v1 = <result of e1>
<instructions for e2>
v2 = <result of e2>
...
result = FunCall(fun, [v1, v2, ...])
```

* 每個函式 body 結尾照樣補 `Return(0)`。

---

## 8. Assembly generation

### 8.1 Calling convention

calling convention 是 caller 和 callee 之間的約定：引數怎麼傳（暫存器或 stack）、回傳值怎麼傳、誰清 stack 上的引數、哪些暫存器 callee 可以覆蓋。它是 **ABI（application binary interface）** 的一部分；ABI 還規定 C 型別在記憶體裡的表示法（Part II 會用到）。

macOS 和 Linux 都用 **System V ABI** 的 x64 版本。Windows 有自己的 ABI，本書不管；WSL 上照樣用 System V。

| 項目 | System V x64 規則 |
| :--- | :--- |
| 傳引數 | 前 6 個整數引數依序放 **EDI, ESI, EDX, ECX, R8D, R9D**（64 位元用 RDI…R9）；其餘的**反向 push** 到 stack |
| 回傳值 | 放在 **EAX**（64 位元用 RAX），執行 `ret` 時必須已經在那裡 |
| 清引數 | **caller** 在 callee 回傳後移除 stack 上的引數，callee 不清 |
| caller-saved | **RAX、R10、R11、所有傳參數用的暫存器**：callee 可以覆蓋，caller 之後還要用就得自己先存 |
| callee-saved | 其他暫存器：函式回傳時內容要和進入時一樣 |
| stack 對齊 | 執行 `call` 時 **RSP 必須是 16 的倍數**（有些指令要求 16-byte 對齊的運算元） |

反向 push 的好處：第 7 個引數永遠在 `16(%rbp)`，variadic 函式（如 `printf`）不用先知道總共幾個引數，就能一個一個往後讀。

### 8.2 範例

```c
int fun(int a, int b, int c, int d, int e, int f, int g, int h) {
    return a + h;
}
int caller(int arg) {
    return arg + fun(1, 2, 3, 4, 5, 6, 7, 8);
}
```

caller 端（Listing 9-27，書上為了說明 calling convention 手寫的組語；我們的編譯器會把參數先複製到 stack，所以不會有存還原 RDI 這種步驟，見 8.4）：

```asm
    pushq   %rdi          # arg 在 RDI，呼叫後還要用，先存起來
    subq    $8, %rsp      # 對齊：總共要放 3 個 8-byte 值（RDI、8、7），奇數個要補 8
    movl    $1, %edi      # 前 6 個引數放暫存器
    movl    $2, %esi
    movl    $3, %edx
    movl    $4, %ecx
    movl    $5, %r8d
    movl    $6, %r9d
    pushq   $8            # 剩下的反向 push：先 h 再 g
    pushq   $7
    call    fun
    addq    $24, %rsp     # 清掉兩個引數和 padding
    popq    %rdi
```

* `call` 做兩件事：把下一條指令的位址（**return address**）push 到 stack，再把 `fun` 的位址放進 RIP。
* `ret` 把 return address pop 出來並跳回去。
* push / pop 只能用 64 位元運算元，所以存還原暫存器用 `%rdi`；引數是 32 位元，傳參數用 `%edi`。

`fun` 的 prologue 執行完後的 stack（上面是低位址）：

```text
  0(%rbp)   caller 的 RBP          <- RBP = RSP
  8(%rbp)   return address
 16(%rbp)   7    (g，第 7 個引數，最後 push 所以最靠近)
 24(%rbp)   8    (h，第 8 個引數)
            <padding>
            15   (caller 存的 RDI)
            caller 其他的 stack frame
```

（15 是書上假設的 `arg` 值。）所以 `fun`（Listing 9-26，同樣是手寫的精簡版）用 `movl %edi, %eax` 取 `a`、`addl 24(%rbp), %eax` 加上 `h`。push 進去的是 8 bytes，`addl` 只讀低 4 bytes，高位是 0，值還是 8。

### 8.3 Assembly AST

```text
program = Program(function_definition*)
instruction = ...
    | AllocateStack(int)
    | DeallocateStack(int)
    | Push(operand)
    | Call(identifier)
    | Ret
reg = AX | CX | DX | DI | SI | R8 | R9 | R10 | R11
```

* Program 變成函式列表。
* `AllocateStack` 原本就有（輸出 `subq`），用來在呼叫前補 padding；新增 `DeallocateStack`（輸出 `addq`）在呼叫後清 stack。
* `Push`：以前只在 prologue 用，emission 時直接寫死；現在要拿來傳引數，所以放進 AST。
* 新暫存器 CX、DI、SI、R8、R9（DX 原本就有）。AST 不區分暫存器的別名，DI 輸出時依大小變成 `%rdi` / `%edi` / `%dil`。

### 8.4 Callee：把參數複製到 pseudoregister

直接用 `%edi`、`24(%rbp)` 存取參數有缺點：呼叫別的函式前後要 push/pop 參數暫存器；和其他會用這些暫存器的指令衝突（例如 `idiv` 會寫 EDX）；pseudoregister 配置時參數要特別處理。

做法：**每個函式開頭，把每個參數從 calling convention 規定的位置複製到自己 stack frame 裡的 pseudoregister**。

```c
int simple(int param) { return param; }
```

```text
Mov(Reg(DI), Pseudo("param"))
Mov(Pseudo("param"), Reg(AX))
Ret
```

`param` 被分配到 `Stack(-4)`，stack 大小 4 取整成 16，最後輸出（Listing 9-30）：

```asm
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    -4(%rbp), %eax
    movq    %rbp, %rsp
    popq    %rbp
    ret
```

* 第 1–6 個參數來自 `Reg(DI)`、`Reg(SI)`、`Reg(DX)`、`Reg(CX)`、`Reg(R8)`、`Reg(R9)`。
* 第 7 個起來自 `Stack(16)`、`Stack(24)`…（`8(%rbp)` 是 return address，之後每個參數 +8，因為 caller push 的是 8 bytes）。
* 之後參數就和區域變數一樣被分配 stack 位置，pseudoregister 取代不用特別處理。

這樣做的另一個好處：caller-saved 暫存器裡的值（參數、回傳值、`idiv` 結果、fix-up 暫用的 R10D / R11D）都是馬上用掉或馬上存到 stack，**不會跨過函式呼叫**，所以呼叫前後**不用存還原 caller-saved 暫存器**。callee-saved 暫存器除了 RBP、RSP（prologue / epilogue 已處理）之外根本沒用到。代價是多了 `mov` 和記憶體存取，Part III 的 register allocation 會消掉大部分。

### 8.5 Caller：轉換 `FunCall`

```text
convert_function_call(FunCall(fun_name, args, dst)):
    arg_registers = [DI, SI, DX, CX, R8, R9]
    register_args, stack_args = 前 6 個, 其餘

    stack_padding = 8 if length(stack_args) 是奇數 else 0
    if stack_padding != 0: emit(AllocateStack(stack_padding))

    依序 Mov(arg, Reg(arg_registers[i]))

    for arg in reverse(stack_args):
        Reg 或 Imm -> emit(Push(arg))
        否則      -> emit(Mov(arg, Reg(AX))); emit(Push(Reg(AX)))

    emit(Call(fun_name))

    bytes_to_remove = 8 * length(stack_args) + stack_padding
    if bytes_to_remove != 0: emit(DeallocateStack(bytes_to_remove))

    emit(Mov(Reg(AX), dst))
```

* **先對齊、再放 stack 引數**：padding 若夾在引數和 callee 的 stack frame 中間，callee 會找不到引數。
* 因為參數已經複製到 stack，不用存 caller-saved 暫存器，所以只看 **stack 引數個數**是奇是偶。前提是每個函式的 stack frame 大小是 16 的倍數（見 8.6）。
* 每個 stack 引數都 push 8 bytes（ABI 規定，而且 `pushq` 只吃 8-byte），callee 只用低 4 bytes。
* **記憶體運算元不直接 push**：`pushq -4(%rbp)` 會讀 4-byte 變數加上後面 4 bytes。stack 上沒事，但第十章的 static 變數可能剛好在合法記憶體區段的結尾，多讀 4 bytes 會 segfault。所以先搬進 AX。
* 只能用 AX 中轉：callee-saved 要保留、參數暫存器已經放了引數、R10/R11 留給 fix-up。
* 回傳值在 EAX，搬到 `dst`。

### 8.6 Pseudoregister 取代與 fix-up

* Pseudoregister 取代也要處理 `Push` 的運算元（現在不會直接 push pseudoregister，Part II 會）。
* **每個函式各自記 stack 大小**：記在 symbol table 或標在 assembly AST 的函式上都可以。參數不管從暫存器還是 stack 傳進來，都算進 stack 大小（因為都複製進 stack frame 了）。
* Fix-up 加 `AllocateStack` 時，把 stack 大小**往上取整到 16 的倍數**，維持呼叫時的對齊。

---

## 9. Code emission

### 9.1 `call` 的目標

| 平台 | 輸出 |
| :--- | :--- |
| macOS | `call _foo`（函式名加底線前綴），不需要 `@PLT` |
| Linux，`foo` 定義在本 translation unit | `call foo` |
| Linux，`foo` 不在本 translation unit | `call foo@PLT` |

* 判斷有沒有定義要查 **symbol table**（`defined`）。
* **PLT（procedure linkage table）** 是 ELF 執行檔裡的一個 section。shared library 在執行期才載入，linker 不知道它的位址，要靠 **dynamic linker** 在執行期解析，最常見的方式是 **lazy binding**：第一次呼叫時才找位址。`foo@PLT` 指的是 linker 產生的一小段 PLT entry 程式碼，它負責找出 `foo` 的位址再呼叫。
* `foo` 也可能在另一個會被連進來的 object file，這時不需要 PLT；但 emission 分不出這兩種情況，一律加 `@PLT`，多加無害。
* ELF 是 Linux 和多數 Unix-like 系統的格式；macOS 用 Mach-O。

### 9.2 指令與運算元

| Assembly | 輸出 |
| :--- | :--- |
| `Program(function_definitions)` | 依序印出每個函式；Linux 在檔尾加 `.section .note.GNU-stack,"",@progbits` |
| `DeallocateStack(int)` | `addq $<int>, %rsp` |
| `Push(operand)` | `pushq <operand>` |
| `Call(label)` | `call <label>` 或 `call <label>@PLT` |

暫存器名稱：

| Reg | 8-byte | 4-byte | 1-byte |
| :--- | :--- | :--- | :--- |
| AX | `%rax` | `%eax` | `%al` |
| DX | `%rdx` | `%edx` | `%dl` |
| CX | `%rcx` | `%ecx` | `%cl` |
| DI | `%rdi` | `%edi` | `%dil` |
| SI | `%rsi` | `%esi` | `%sil` |
| R8 | `%r8` | `%r8d` | `%r8b` |
| R9 | `%r9` | `%r9d` | `%r9b` |
| R10 | `%r10` | `%r10d` | `%r10b` |
| R11 | `%r11` | `%r11d` | `%r11b` |

**`push` 用 8-byte 名稱、`SetCC` 用 1-byte 名稱、其他一律用 4-byte 名稱。**

---

## 10. 呼叫函式庫

* 不能 `#include`（標準標頭檔用到太多還沒支援的語言特性），要自己宣告要用的函式。
* 目前只有 `int`，所以只能呼叫參數和回傳值都是 `int` 的函式，例如 `putchar`（印出對應的 ASCII 字元）。書上用它逐字印出 `Hello, World!`（Listing 9-32）：

```c
int putchar(int c);

int main(void) {
    putchar(72);    // 'H'
    putchar(101);   // 'e'
    ...
    putchar(33);    // '!'
    putchar(10);    // '\n'
}
```

* 測試套件的 `tests/chapter_9/valid/libraries` 有成對的 `<LIB>.c` 和 `<LIB>_client.c`，每對跑兩次：client 用我們的編譯器、library 用系統編譯器，以及反過來。這同時驗證 caller 端和 callee 端都遵守 System V。
* 建議除錯順序：沒有引數的呼叫 → 本身不呼叫別人的 library 函式（驗證參數存取）→ 少於 6 個引數 → 超過 6 個引數。

---

## 11. lemon_cc 對應

| 階段 | main.c |
| :--- | :--- |
| Lexer | `TOK_COMMA` |
| Parser | `parse_declaration`（identifier 後面看到 `(` 就交給 `parse_function_after_name`）、`parse_param_list`、`parse_argument_list`、`parse_factor`（identifier 後接 `(` 建 `EXP_FUNCTION_CALL`）、`parse_program` |
| Identifier resolution | `IdentifierMap`（`has_linkage`；scope 欄位仍叫 `from_current_block`）、`resolve_function_declaration`、`resolve_local_name`（參數）、`resolve_block_items`（擋巢狀定義）、`resolve_expression`（`EXP_FUNCTION_CALL`）、`resolve_program` |
| Type checking | `typecheck_function_declaration`、`typecheck_expression`（`EXP_FUNCTION_CALL`、`EXP_VAR`）、`symbol_find` / `symbol_add_function` / `symbol_add_local`、`typecheck_program` |
| TACKY | `TACKY_INST_FUN_CALL`；`generate_tacky` 跳過沒有 body 的函式；`gen_tacky_exp` 依序算引數再發 FunCall |
| Assembly | `ARG_REGISTERS`、`convert_tacky_function`（參數複製）、`convert_function_call`、`replace_pseudos`（每個函式的 stack 大小取整到 16） |
| Emission | `emit_call_target`（macOS 加 `_`；Linux 查後端符號表的 `defined`，沒定義就加 `@PLT`）、`reg_name_64` / `reg_name_32` / `reg_name_8` |
| Driver | `-c` → `gcc -c <asm> -o <name>.o` |

和書不同的地方（多半是第十、十一章的改動）：

* 型別已經是 `TYPE_FUNCTION` 帶參數型別和回傳型別（第 11 章），不只是參數數量。
* 已沒有 `AllocateStack` / `DeallocateStack`，改用 `Binary(Sub/Add, Quadword, Imm, Reg(SP))`，輸出成 `subq` / `addq ..., %rsp`（第 11 章）。
* stack 大小取整到 16 是在 `replace_pseudos` 算好存進 `fn->stack_size`，fix-up（`fix_and_allocate_function`）只負責插入 `subq`。
* push stack 引數時，Imm、Reg 或 Quadword 直接 push；Longword 記憶體運算元先搬進 EAX 再 push RAX（第 11 章）。

相關 commit：`2e0a5ae`（parser）、`5b6ece8`（type checker 與 identifier resolution）、`8191f06` / `b7980cf` / `bfca55c` / `676a063` / `98ad370`（TACKY）、`51cc39f`（轉組語）、`acb706e`（emission 與 `-c`）。

---

## 12. 一句話總結

> 第九章的本質，是讓多個函式依 System V calling convention 互相呼叫：前端用 linkage 決定改不改名、用 symbol table 檢查函式型別，後端把參數搬進 stack frame、對齊 stack、經暫存器和 stack 傳引數，從 EAX 拿回傳值。

## 13. 速記重點

```text
語言     -> 宣告可重複但要相容、定義只能一次、block 內可宣告不可定義、先宣告才能呼叫
driver   -> -c：gcc -c x.s -o x.o
lexer    -> ","
parser   -> FunDecl / VarDecl、params、block? body、FunctionCall、param-list 用 void
resolve  -> identifier_map + has_linkage、函式不改名、參數和 body 同 scope、擋巢狀定義
typecheck-> FunType(param_count)、defined、型別不符 / 重複定義 / 參數數量 / 變數與函式混用
TACKY    -> Program(函式列表)、FunCall(name, args, dst)、沒 body 的不產生
asm      -> DI SI DX CX R8 R9、其餘反向 push、奇數個補 8、caller 清 stack、回傳值在 EAX
callee   -> 參數搬進 pseudo：DI..R9，Stack(16)、Stack(24)...
caller   -> AllocateStack(8)? → Mov 到暫存器 → Push → Call → DeallocateStack → Mov AX
frame    -> 每個函式 stack 大小取整到 16
emit     -> call foo@PLT（Linux 未定義）、_foo（macOS）、push 用 8-byte、SetCC 用 1-byte
```

---
