# 第十二章：無號整數（Unsigned Integers）

## 1. 這章在做什麼

加入兩個無號整數型別：`unsigned int` 和 `unsigned long`。

- 擴充 usual arithmetic conversions，讓有號和無號混用時能找到 common type。
- 支援有號和無號之間的 cast。
- 後端加入幾個無號專用的指令：無號比較、無號除法、zero extension。

第十一章已經打好「推斷並追蹤型別」的基礎，所以這章加新型別的工作量不大。identifier resolution 和 loop labeling 都不用改。

---

## 2. 型別轉換：值和表示法

每個整數轉換都要同時看兩件事：**值**怎麼變、**二進位表示法**怎麼變。書上分成四種情況：

| 情況 | 表示法怎麼變 | 值怎麼變 |
|---|---|---|
| 同大小的有號 ↔ 無號（`int` ↔ `unsigned int`、`long` ↔ `unsigned long`） | 完全不變，只換解讀方式 | 最高位元是 0 時值不變；最高位元是 1 時，轉成無號會 +2^N，轉成有號會 −2^N |
| `unsigned int` → 較大型別 | **zero extension**：高位補 0 | 一定不變 |
| `int` → 較大型別（`long` 或 `unsigned long`） | **sign extension**：高位補符號位 | 正數不變；負數轉成 `unsigned long` 會 +2^64，符合標準 |
| 較大型別 → 較小型別 | **truncation**：砍掉高位 | 等於對 2^32 取模 |

這些行為都符合 C 標準 §6.3.1.3。轉成無號型別的規則是標準規定的；轉成有號型別則沿用上一章選的 GCC 行為，也就是對 2^N 取模。

---

## 3. 前端：新的 token 和型別寫法

**Lexer** 新增 4 種 token：

- 關鍵字：`signed`、`unsigned`
- 無號常數：`123u` / `123U`
- 無號 long 常數：`123ul`、`123lu`，大小寫不限

**Parser** 的 AST 新增 `UInt`、`ULong` 兩個型別，以及 `ConstUInt`、`ConstULong` 兩種常數。

這章麻煩的地方是**同一個型別有很多種寫法**，而且 specifier 的順序不重要：

```c
long  /  long int  /  signed long  /  signed long int  /  int signed long ...
```

判斷規則：

- **不合法**：沒有 specifier、同一個 specifier 寫兩次、`signed` 和 `unsigned` 同時出現。
- **基本型別**：預設是 `int`；有 `long` 就是 long；有 `unsigned` 就是無號。

常數的型別：

- `u` 常數的值在 0 到 2^32−1 之間是 `unsigned int`，超過就變成 `unsigned long`。
- `ul` 常數一律是 `unsigned long`。
- 值超過 2^64−1 就報錯。

---

## 4. 型別檢查：common type 規則

C 標準 §6.3.1.8 的 usual arithmetic conversions 有五條規則。在 System V x64 上（`int` 是 4 bytes、`long` 是 8 bytes），可以濃縮成三條：

1. 兩個型別相同，就用它。
2. 大小相同，就用**無號**的那個。
3. 大小不同，就用**比較大**的那個。

| 運算元型別 | common type |
|---|---|
| `int` + `unsigned int` | `unsigned int` |
| `int` + `unsigned long` | `unsigned long` |
| `unsigned int` + `long` | `long`（`long` 裝得下所有 `unsigned int` 的值） |
| `long` + `unsigned long` | `unsigned long` |

> 「`unsigned int` + `long` → `long`」只在 `long` 比 `int` 大的平台成立。如果 `long` 和 `int` 一樣大（很多 32 位元系統是這樣），答案會變成 `unsigned long`。這是第五條規則的情況，對我們的編譯器不適用。

這也是很多 C 陷阱的來源，例如 `-1 < 0u` 是**假**：`-1` 會先轉成 `unsigned int`，變成 4294967295。

**Static 初始值**也要照第 2 節的規則轉換成變數的型別，例如：

- `static unsigned int u = 4294967299L;` 的實際初始值是 3。
- `static int i = 4294967246u;` 的實際初始值是 −50。

---

## 5. TACKY：多一個 ZeroExtend

cast 依照第 2 節的四種情況產生不同指令：

| 轉換 | TACKY 指令 |
|---|---|
| 同大小（只換有號或無號） | `Copy` |
| 大 → 小 | `Truncate` |
| 小 → 大，來源有號 | `SignExtend` |
| 小 → 大，來源無號 | `ZeroExtend`（本章新增） |

同大小時看起來什麼都不用做，但還是要 `Copy` 到一個新型別的暫存變數。原因是後端會依照運算元是有號還是無號，產生不同的組語。

---

## 6. 組語：無號要怎麼運算

大部分算術指令（`add`、`sub`、`imul`、`cmp`）**不分有號無號**：同一串位元，兩種解讀都會得到正確答案。例如 4 位元的 `1000 + 0010 = 1010`，當成無號是 8 + 2 = 10，當成有號是 −8 + 2 = −6。

只有兩種運算要分開處理：**比較**和**除法**。

### 6.1 無號比較：改看 CF

有號比較看 SF 和 OF，但這兩個旗標只對有號的值有意義。無號比較改用 **CF（carry flag）**：只要無號的結果繞回（wrap around），CF 就會被設起來。

`cmp b, a` 計算 a − b。對無號數來說：

- a == b → ZF = 1
- a > b → ZF = 0、CF = 0
- a < b → 結果繞回，CF = 1

新的 condition code（A = above，B = below）：

| 比較 | 有號 | 無號 |
|---|---|---|
| `>` | G | **A** |
| `>=` | GE | **AE** |
| `<` | L | **B** |
| `<=` | LE | **BE** |
| `==` / `!=` | E / NE | E / NE（共用） |

### 6.2 無號除法：`div`

有號和無號的除法沒辦法用同一個指令。例如 4 位元的 `1000 / 0010`：當成有號是 −8 / 2 = −4，當成無號是 8 / 2 = 4。

- 新指令 `div`，用法和 `idiv` 一樣。
- 被除數不是用 `cdq` 做 sign extension，而是**把 EDX/RDX 清成 0**。

### 6.3 Zero extension

把 4 bytes 的值 `movl` 進暫存器時，CPU 會自動把高 4 bytes 清成 0，所以不需要特別的指令。

### 6.4 Unsigned wraparound 不是 UB

有號整數溢位是 undefined behavior，但**無號整數永遠不會溢位**。結果超出範圍就對 2^N 取模（§6.2.5），組語的算術指令本來就是這樣運作的。到 Part III 做 constant folding 時才需要特別處理。

---

## 7. 後端其他部分

- 轉成組語型別後，有號和無號的差別就不見了：`int` 和 `unsigned int` 都是 Longword，`long` 和 `unsigned long` 都是 Quadword。
- 有號無號的差別只在「TACKY → 組語」這一步用到，用來挑選 condition code，以及選 `idiv` 還是 `div`。之後的 pseudo 替換不用分。
- Emission：無號的 static 初始值和有號的輸出方式完全一樣（`.long` / `.quad` / `.zero`），因為組譯器寫出的 bytes 相同。

---

## 8. 一句話總結

無號整數的位元和有號整數完全一樣，差別只在**解讀方式**。所以這章的重點是：型別轉換和 common type 規則要寫對，比較要改看 CF（A/B 系列的 condition code），除法要改用 `div`。

---

## 9. 速記重點

```text
lexer     -> signed、unsigned、123u、123ul
parser    -> UInt / ULong；specifier 順序任意，不可重複、不可 signed+unsigned
typecheck -> common type：同型別 > 同大小選無號 > 選大的
TACKY     -> 同大小 Copy、變小 Truncate、有號變大 SignExtend、無號變大 ZeroExtend
asm       -> 無號比較用 A/AE/B/BE（看 CF）、無號除法用 div + 清 EDX
重點      -> 無號不會溢位，只會取模；-1 < 0u 是假
```
