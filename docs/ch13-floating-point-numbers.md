# 第十三章：浮點數（Floating-Point Numbers）

## 1. 這章在做什麼

加入 `double` 型別。前幾章的四種整數型別都只是「同一串位元、不同解讀」，但 `double` 用的是完全不同的二進位表示法（IEEE 754），所以這章有兩大工作：

1. **搞清楚要實作什麼行為**：C 標準把很多浮點行為留給實作決定，所以要改參考 IEEE 754 和 System V x64 ABI。
2. **產生組語**：浮點數不能用原本的 `add`、`cmp`、`idiv`，要改用一整套新的 **SSE 指令**和 **XMM 暫存器**。

`float` 和 `long double` 不實作。identifier resolution 和 loop labeling 一樣不用改。

我們的目標是大致對齊 GCC/Clang 的預設行為：運算照 IEEE 754、正確處理大部分特殊值，但**忽略浮點例外和非預設的 rounding mode**。

---

## 2. IEEE 754 double 格式

System V ABI 規定 `double` 用 IEEE 754 雙精度格式，共 64 位元：

```text
 63  62 ........ 52  51 .................. 0
[符號][ 指數 11 bits ][   分數 52 bits     ]
```

值 = (−1)^s × f × 2^e

- **f（significand / mantissa）**：整數部分固定是 1，52 bits 只存小數部分，所以實際有 **53 bits 精度**，1 ≤ f < 2。
- **e（exponent）**：biased 編碼，把 11 bits 當無號整數再減 1023，範圍 −1022 到 1023。
- 本質上就是**以 2 為底的科學記號**。

### 特殊值

| 值 | 編碼 | 我們要不要處理 |
|---|---|---|
| `0.0` / `-0.0` | 全 0（`-0.0` 只有符號位是 1） | 自動支援；兩者比較相等，但 `-1.0 * 0.0` 會得到 `-0.0` |
| Subnormal | 指數全 0，整數部分變 0 | 自動支援（CPU 處理） |
| ±Infinity | 指數全 1、分數全 0 | 自動支援；`1.0/0.0` 是 inf |
| NaN | 指數全 1、分數非 0 | **Extra credit**，比較時要多處理 |

---

## 3. Rounding：誤差從哪裡來

`double` 只有 64 bits，大部分實數都要捨入。我們只支援 IEEE 的預設模式：**round-to-nearest, ties-to-even**（取最近；剛好在中間就取最低位是 0 的那個）。

會發生 rounding 的三個地方：

| 情況 | 例子 | 誰負責 |
|---|---|---|
| **常數**：十進位轉二進位 | `0.1` 其實是 `0.1000000000000000055511151231257827…` | 編譯器（用實作語言內建的字串轉浮點） |
| **型別轉換**：整數轉 double | `long` 有 64 bits 精度，`double` 只有 53 bits；`9223372036854775803` 會變成 2^63 | CPU 指令（但無號要特別處理，見第 7 節） |
| **算術運算** | `+ - * /` 的結果不一定能表示 | CPU 指令自動處理，幾乎不用管 |

> 關鍵概念：**可表示的 double 之間有間隔，數值越大間隔越大**。超過 2^53 之後，間隔就大於 1，連整數都無法全部表示。

---

## 4. 前端

### 4.1 Compiler driver：`-l<lib>`

測試會呼叫 `<math.h>` 的函式，所以 driver 要能接受 `-lm` 這類參數，並放在 gcc 命令的**組語檔名之後**：

```sh
./compiler prog.c -lm   →   gcc prog.s -o prog -lm
```

### 4.2 Lexer

新 token：`double` 關鍵字、浮點常數。

浮點常數的形式：

- 小數：`1.5`、`.72`、`1.`（後面可以沒數字）
- 科學記號：`100E10`、`.05e-2`、`5.E+3`

```text
小數        [0-9]*\.[0-9]+|[0-9]+\.
科學記號底數 [0-9]*\.[0-9]+|[0-9]+\.?
指數        [Ee][+-]?[0-9]+
```

**重要改動：常數結尾的判斷**。以前用 `\b`，現在改成 `[^\w.]`，也就是**常數後面不能緊接著句點**：

| 輸入 | 結果 |
|---|---|
| `123L;` | OK |
| `123L.bar` | 錯 |
| `1.0+x` | OK |
| `1.0.+x` | 錯 |
| `1.}` | OK |
| `1..}` | 錯 |

結尾那個字元不屬於 token，所以 regex 用 capture group 包住真正的常數，只消耗 group 內容。這個改動也套用到所有整數常數：

| Token | Regex |
|---|---|
| int | `([0-9]+)[^\w.]` |
| unsigned | `([0-9]+[uU])[^\w.]` |
| long | `([0-9]+[lL])[^\w.]` |
| unsigned long | `([0-9]+([lL][uU]\|[uU][lL]))[^\w.]` |
| double | `(([0-9]*\.[0-9]+\|[0-9]+\.?)[Ee][+-]?[0-9]+\|[0-9]*\.[0-9]+\|[0-9]+\.)[^\w.]` |

### 4.3 Parser

- AST 新增型別 `Double`、常數 `ConstDouble(double)`。
- `double` **只能單獨出現**，不能和 `long`、`unsigned` 等組合（但可以和 `static`、`extern` 一起）。
- 常數要存成實作語言裡**真正的 64 位元 double**：
  - 精度太低 → 存不到最接近的 double。
  - 精度太高 → 之後再降到 double 時會發生 **double rounding error**（捨入兩次的結果可能和捨入一次不同）。
- 浮點常數**不會超出範圍**（太大就是 infinity），所以 parser 不會報錯。

---

## 5. 型別檢查

1. **Common type**：任一邊是 `double`，結果就是 `double`。

   ```text
   get_common_type(t1, t2):
     if t1 == t2: return t1
     if t1 == Double or t2 == Double: return Double
     ...
   ```

2. **新的型別錯誤**：`~` 和 `%` 只能用在整數，對 `double` 用就報錯。
3. **Static 初始值**：新增 `DoubleInit(double)`，並照執行期規則轉換：
   - double → 整數：**朝 0 截斷**（`2.8` → `2`）；超出範圍是 UB，直接報錯最乾淨。
   - 整數 → double：能精確表示就保留，否則取最近值。

---

## 6. TACKY：四個新的轉換指令

| 指令 | 用途 |
|---|---|
| `DoubleToInt` | double → `int` / `long` |
| `DoubleToUInt` | double → `unsigned int` / `unsigned long` |
| `IntToDouble` | `int` / `long` → double |
| `UIntToDouble` | `unsigned int` / `unsigned long` → double |

不分整數大小，同一個指令處理 4 bytes 和 8 bytes，差別留到組語階段再處理。

---

## 7. 組語：SSE 指令

### 7.1 基本規則

浮點運算用 **SSE2** 指令（x64 一定支援）。另一套舊的 x87 指令不用。

- 後綴：`sd` = scalar double，`pd` = packed double（向量）。我們幾乎只用 scalar。
- **XMM 暫存器**：XMM0–XMM15，每個 128 bits，只用低 64 bits。SSE 指令不能用一般暫存器，一般指令也不能用 XMM。
- **SSE 指令不能用立即值**：浮點常數要放在 `.rodata`，用 RIP-relative 存取。

```asm
    .section .rodata
    .align 8
.L_one:
    .double 1.0
    .text
    movsd  .L_one(%rip), %xmm0
    addsd  .L_one(%rip), %xmm0
```

### 7.2 Calling convention

| | 整數 | 浮點 |
|---|---|---|
| 參數暫存器 | RDI, RSI, RDX, RCX, R8, R9（6 個） | XMM0–XMM7（8 個） |
| 回傳值 | RAX | **XMM0** |

兩種暫存器**各自分配**。分配步驟：

1. 從左到右，每個參數放進自己型別的下一個空暫存器。
2. 放不下的參數（不分型別）照原順序收集起來，再**反向 push**，所以第一個 stack 參數在 `16(%rbp)`。

例：`f(double d1, long i1, ..., long i7, int i8, double d4)` 中，若整數暫存器已滿，i7、i8 進 stack，但 d4 仍進 XMM3。

### 7.3 算術

| 運算 | 指令 | 備註 |
|---|---|---|
| `+ - * /` | `addsd` `subsd` `mulsd` `divsd` | 除法和其他運算一樣，不用像整數除法那樣特殊處理 |
| 取負 | `xorpd` 搭配 `-0.0` | 沒有浮點 neg 指令；XOR 符號位就能翻轉正負 |
| 清零 | `xorpd %xmm0, %xmm0` | |

`xorpd` 是 packed 指令，記憶體運算元**必須 16-byte 對齊**，所以 `-0.0` 常數要 `.align 16`。這是唯一一個 16-byte 對齊的 double。

### 7.4 比較：`comisd`

`comisd b, a` 設定的旗標和**無號比較**一樣（ZF、CF），所以直接沿用上一章的 **A / AE / B / BE**。它正確處理 subnormal、infinity，而且把 `0.0` 和 `-0.0` 當成相等。

和 0 比較（`JumpIfZero`、`!`）時要先用 `xorpd` 清出一個 0 的 XMM 暫存器再比。

### 7.5 型別轉換（本章最難的部分）

SSE 只有**有號**轉換指令：

- `cvttsd2si{l,q}`：double → 有號整數，朝 0 截斷。超出範圍時得到 indefinite integer（最小值），反正是 UB 就不管它。
- `cvtsi2sd{l,q}`：有號整數 → double，用目前的 rounding mode。

無號要自己想辦法：

| 轉換 | 做法 |
|---|---|
| double → `unsigned int` | 用 `cvttsd2siq` 轉成 64-bit long，再取低 32 bits。`unsigned int` 的範圍都在 `long` 裡面。 |
| `unsigned int` → double | 先 zero extend 成 64 bits（用 **`MovZeroExtend`**，不要用 `Mov`，因為 Part III 的 register coalescing 可能把 `Mov` 刪掉），再 `cvtsi2sdq`。 |
| double → `unsigned long` | 見下方 A |
| `unsigned long` → double | 見下方 B |

**A. double → unsigned long**

```asm
    comisd  .L_upper_bound(%rip), %xmm0   # 2^63
    jae     .L_out_of_range
    cvttsd2siq %xmm0, %rax                # < 2^63，直接轉
    jmp     .L_end
.L_out_of_range:
    movsd   %xmm0, %xmm1
    subsd   .L_upper_bound(%rip), %xmm1   # 先減 2^63
    cvttsd2siq %xmm1, %rax
    movq    $9223372036854775808, %rdx
    addq    %rdx, %rax                    # 轉完再加回 2^63
.L_end:
```

為什麼不會有 rounding 誤差：

- 2^63 可以被 double 精確表示（`LONG_MAX` 不行，所以不用它）。
- 來源值在 [2^63, 2^64) 時可以寫成 1.x × 2^63，減掉 2^63 後剩 x × 2^62，最多 52 bits，一定能精確表示（Sterbenz lemma）。

**B. unsigned long → double：round to odd**

直覺做法：最高位是 1 時，先 `shr` 除以 2，轉成 double，再自己加自己（×2）。但這會產生 **double rounding error**：

- `shr` 會無條件捨去，例如 9 → 4。
- 若捨去後剛好落在兩個 double 的**正中間**，`cvtsi2sd` 又照 ties-to-even 再捨一次，可能往錯的方向捨。
- 例：`9223372036854776833` 應該轉成上面的 double，但用這個做法會得到下面的。

解法是除以 2 時改成 **round to odd**：若捨去了 1（原數為奇數），就把結果的最低位設成 1。

```asm
    cmpq   $0, -8(%rbp)
    jl     .L_out_of_range          # 有號看是負的 = 最高位是 1
    cvtsi2sdq -8(%rbp), %xmm0
    jmp    .L_end
.L_out_of_range:
    movq   -8(%rbp), %rax
    movq   %rax, %rdx
    shrq   %rdx                     # 除以 2
    andq   $1, %rax                 # 原數是否為奇數
    orq    %rax, %rdx               # 是的話就把最低位設成 1（round to odd）
    cvtsi2sdq %rdx, %xmm0
    addsd  %xmm0, %xmm0             # ×2
.L_end:
```

為什麼有效：這個範圍內 double 的間隔是 1024，所以「中點」一定是 512 的倍數，也就是**偶數**。結果是奇數就不可能剛好落在中點，第二次 rounding 就不會出錯。

---

## 8. 組語 AST 的變化

```text
assembly_type   += Double
top_level       += StaticConstant(name, alignment, static_init)   # 放 .rodata，不分 global
instruction     += Cvttsd2si(dst_type, src, dst) | Cvtsi2sd(src_type, src, dst)
unary_operator  += Shr
binary_operator += DivDouble | And | Or | Xor
reg             += XMM0..XMM7, XMM14, XMM15   # 14、15 留給 fix-up 當 scratch
```

### TACKY → 組語重點

- **浮點常數**：不能變成 `Imm`。每遇到一個 double 常數，就產生一個 `StaticConstant(label, 8, DoubleInit(v))`，改用 `Data(label)` 引用。整個 pass 結束後把所有常數加到 top-level。
  - 可選：去除重複常數（但 `0.0` 和 `-0.0` 要分開）。
  - 可選：用 `.L` local label。在 backend symbol table 加 `is_constant` 屬性，到 emission 階段再加前綴。
- **除法**：`Mov` + `Binary(DivDouble, ...)`，和加減乘一樣。
- **取負**：`Mov` + `Binary(Xor, Double, Data(-0.0 常數), dst)`，常數要 16-byte 對齊。
- **比較**：條件碼和無號一樣用 A/AE/B/BE。
- **函式呼叫**：新增 `classify_parameters(values)` helper，回傳三個 list：整數暫存器參數、XMM 參數、stack 參數。呼叫端（`FunCall`）和被呼叫端（設定參數）都用它。Double 的 pseudo 可以直接 `push`（8 bytes）。
- **回傳**：double 放進 XMM0。
- 型別轉換用到的暫存器：避開 callee-saved（RBX、R12–R15）和 fix-up 專用的 R10、R11、XMM14、XMM15。

### Pseudo 替換

Double 和 Quadword 一樣：8 bytes、8-byte 對齊。static 的 double 一樣換成 `Data`。

### Instruction fix-up

| 指令 | 限制 | 修正 |
|---|---|---|
| `cvttsd2si` | dst 必須是暫存器 | 經過 R11 |
| `cvtsi2sd` | src 不能是常數，dst 必須是暫存器 | src 經過 R10，dst 經過 XMM15 |
| `comisd` | 第二個運算元必須是暫存器 | 先搬進 XMM15 |
| `addsd` / `subsd` / `mulsd` / `divsd` / `xorpd` | dst 必須是暫存器 | 經過 XMM15 |
| `movsd` | 不能兩邊都是記憶體 | 經過 XMM14 |
| `and` / `or` | 和 `add` 一樣的限制 | 沿用 |
| `push` XMM | 不合法 | 等到下一章再處理（目前不會發生） |

---

## 9. Code emission

**浮點數怎麼印**（三選一）：

| 寫法 | 例子（20.0） | 備註 |
|---|---|---|
| 十六進位浮點 | `.double 0x2.8p+3` | 精確，但 **GNU as 不支援** |
| 當成整數印位元 | `.quad 4626322717216342016` | 精確，可讀性差 |
| 十進位 | `.double 20.0` | 印 **17 位有效數字**就能保證轉回原值，例如 `1.0000000000000001e-1` |

**Section**：

- Linux：常數放 `.section .rodata`。
- macOS：8-byte 對齊用 `.literal8`，16-byte 對齊用 `.literal16`，且 `-0.0` 後面要補 `.quad 0` 湊滿 16 bytes。
- **static double 一律放 `.data`，不用 `.bss` / `.zero`**，因為 `-0.0` 的位元不是全 0，這樣可以避免混淆。

**指令名稱**：

| 組語 AST | 輸出 |
|---|---|
| `Binary(Mult, Double, ...)` | `mulsd`（不是 `imulsd`） |
| `Binary(DivDouble, ...)` | `divsd` |
| `Binary(Xor, Double, ...)` | `xorpd`（packed，不加 `sd`） |
| `Cmp(Double, ...)` | `comisd` |
| `Cvtsi2sd(t, ...)` | `cvtsi2sdl` / `cvtsi2sdq` |
| `Cvttsd2si(t, ...)` | `cvttsd2sil` / `cvttsd2siq` |
| 其他 Double 運算 | 後綴 `sd` |

---

## 10. Extra credit：NaN

- 算術會自動傳遞 NaN，轉換成整數本來就是 UB，所以**只有比較要處理**。
- 跟 NaN 比較的結果是 unordered：`>`、`<`、`==` 都是假，NaN 連跟自己都不相等。
- `comisd` 遇到 unordered 時會把 **ZF、CF、PF** 都設成 1。如果不看 PF，`==` 會被誤判成真。要用 **P 條件碼**（`jp` / `setp`）另外處理。
- 測試：`--nan` 或 `--extra-credit`。

---

## 11. 一句話總結

`double` 的位元表示法和整數完全不同，所以要換一整套 SSE 指令和 XMM 暫存器。這章真正的難點是 **rounding**：常數要存成真正的 64-bit double，無號轉換要靠「減 2^63」和「round to odd」這類技巧，才能和 IEEE 754 的結果一致。

---

## 12. 速記重點

```text
driver    -> 支援 -lm，放在 .s 檔名之後
lexer     -> double、浮點常數；常數結尾改用 [^\w.]（後面不能接句點）
parser    -> Double / ConstDouble；double 不能和其他型別 specifier 混用
typecheck -> 有 double 就是 double；~ 和 % 不能用在 double；DoubleInit
TACKY     -> DoubleToInt / DoubleToUInt / IntToDouble / UIntToDouble
asm       -> 常數放 .rodata 用 Data 引用；XMM0-7 傳參、XMM0 回傳
             addsd/subsd/mulsd/divsd；取負 = xorpd -0.0（16-byte 對齊）
             comisd + A/AE/B/BE（和無號比較相同）
             unsigned 轉換：減 2^63 / round to odd
fix-up    -> XMM14（src）、XMM15（dst）當 scratch
emit      -> 17 位十進位或 .quad 位元；static double 不放 .bss
```
