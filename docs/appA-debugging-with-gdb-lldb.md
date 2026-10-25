# 附錄 A：用 GDB 或 LLDB 除錯組語（Debugging Assembly Code with GDB or LLDB）

## 1. 這個附錄在做什麼

編譯器遲早會產生「組譯得過、但跑出錯誤結果」的組語。這時候光看 `.s` 檔很難找到問題，需要用**命令列除錯器**：

- 讓程式停在某個位置（breakpoint）。
- 一次執行一條指令。
- 隨時查看暫存器、stack、static 變數的值。

書上兩種都介紹：Linux 建議用 **GDB**（看組語的介面比較好用），macOS 建議用 **LLDB**（GDB 在 macOS 上不好裝）。這個專案在 Linux/WSL 上開發，所以本篇以 GDB 為主，LLDB 只整理對照表。

> 和平常除錯 C 程式最大的不同：我們的執行檔**沒有原始碼的除錯資訊**，所以只能以「指令」為單位操作，不能用「行」。

---

## 2. 範例程式在練習什麼

書上用一個「沒有實際用途」的組語程式 `hello_debugger.s` 當練習對象，每一段都對應一個要練的技巧：

| 程式內容 | 用來練習 |
|---|---|
| static 變數 `integer`（`.long 100`）、`dbl`（`.double 3.5`） | 印出 static 變數 |
| `callq f` | 進入函式（step into）和略過函式（step over） |
| `movl $0x87654321, %eax`、`movsd dbl(%rip), %xmm0` | 查看一般暫存器和 XMM 暫存器 |
| 把 4 個整數放到 `-4(%rbp)` 到 `-16(%rbp)` | 查看 stack 的記憶體 |
| ECX 從 25 遞減到 0 的迴圈 | 條件中斷點 |

組譯和連結：

```sh
gcc hello_debugger.s -o hello_debugger
gcc -g hello_debugger.s -o hello_debugger   # macOS 要加 -g
```

---

## 3. GDB：開啟介面

```text
$ gdb hello_debugger
(gdb) layout asm      # 組語視窗：目前執行到的指令會反白
(gdb) layout reg      # 暫存器視窗：顯示所有一般暫存器
(gdb) focus cmd       # 切換可捲動的視窗：cmd / asm / regs
```

`gdb 執行檔` 只會載入程式，**還不會執行**。暫存器視窗要等程式開始跑才會有內容。

---

## 4. GDB：讓程式跑起來、停下來

一定要**先設中斷點再執行**，不然程式會直接跑完。

```text
(gdb) break main      # 在 main 開頭設中斷點
(gdb) run             # 執行到第一個中斷點
```

停住之後，往前走的四個指令：

| 指令 | 縮寫 | 作用 |
|---|---|---|
| `continue` | `c` | 繼續跑，直到下一個中斷點或程式結束 |
| `finish` | `fin` | 跑到目前函式 return 為止 |
| `stepi [n]` | `si` | 執行 1（或 n）條指令；遇到 `call` 會**進入**被呼叫的函式 |
| `nexti [n]` | `ni` | 執行 1（或 n）條指令；遇到 `call` 會**略過**，停在 `call` 的下一條 |

> ⚠️ **不要打 `next` / `step`（`n` / `s`）**。它們是以原始碼的「行」為單位，但我們沒有原始碼資訊，結果會直接跑到函式結束。它們和 `ni` / `si` 只差一個字母，很容易打錯。

程式跑完之後要再 `run` 一次才能繼續除錯。

---

## 5. GDB：在特定指令上設中斷點

組語視窗每一行長這樣：

```text
0x555555555143 <main+26>  movl  $0xdeadbeef, -4(%rbp)
   ↑ 記憶體位址   ↑ 距離函式開頭的 byte 數
```

位址每台機器可能不同，但 **offset 是固定的**，所以用 offset 比較方便：

```text
(gdb) break *main+26          # * 代表「這是位址」，不是函式名稱
(gdb) break *0x555555555143   # 直接寫位址也可以
(gdb) info break              # 列出所有中斷點（縮寫 i b）
(gdb) delete 1                # 刪掉 1 號中斷點；不帶編號就是全部刪掉
```

---

## 6. GDB：印出值（`print`）

格式：`print/<格式> <運算式>`，縮寫 `p`。運算式可以使用暫存器（`$eax`）、位址、符號，也可以使用 C 的語法：算術、cast、`*` 取值、函式呼叫。

**一般暫存器**（GDB 預設當**有號**整數顯示）：

```text
(gdb) print $eax          # -2023406815
(gdb) print/x $eax        # 0x87654321
(gdb) print/u $eax        # 2271560481
```

常用格式：`x` 十六進位、`d` 有號、`u` 無號、`t` 二進位、`f` 浮點、`a` 位址、`c` 字元、`s` 字串（完整列表見 `help x`）。

**XMM 暫存器**：直接 `print $xmm0` 會一次列出 float、double、int8 等很多種解讀方式。我們只用低 64 bits 存一個 double，所以這樣看就好：

```text
(gdb) print $xmm0.v2_double[0]    # 3.5
```

**Static 變數**：GDB 不知道它們的型別，**要自己 cast**：

```text
(gdb) print (long) integer        # 100
(gdb) print (double) dbl          # 3.5
```

**其他運算式**：

```text
(gdb) print/x $eax + 0x10              # 算術
(gdb) print (int) f()                  # 呼叫程式裡的函式
(gdb) print/x *(int *)($rbp - 4)       # 讀 -4(%rbp)：算位址 → cast 成指標 → 取值
```

每個結果都會存成 `$1`、`$2`……（convenience variable），之後可以拿來用。

---

## 7. GDB：查看記憶體（`x`）

格式：`x/<數量><格式><單位> <位址>`

| 單位 | 大小 |
|---|---|
| `b` | 1 byte |
| `h` | 2 bytes（halfword） |
| `w` | 4 bytes（word） |
| `g` | 8 bytes（giant） |

```text
(gdb) x/1xw ($rbp - 4)    # 1 個 4-byte，十六進位 → 0xdeadbeef
(gdb) x/6xw $rsp          # 印出整個 stack frame（6 × 4 = 24 bytes）
(gdb) x/4ag $rsp          # 4 個 8-byte，當成位址顯示
```

**看懂 stack frame**（範例程式的 `main`）：

```text
低位址  $rsp → 0xfeedface   -16(%rbp)
               0xffffffff   -12(%rbp)
               0x00000000    -8(%rbp)
               0xdeadbeef    -4(%rbp)
        $rbp → 舊的 RBP（8 bytes）       ← 這個 frame 的底部
               return address（8 bytes）  ← 呼叫者 frame 的頂端
高位址
```

用 `a` 格式時，GDB 會把指向程式碼或 static 資料的值標上符號，例如 `0x7ffff7dee083 <__libc_start_main+243>`。stack、heap 或無效的位址則不會標。**stack frame 被寫壞時**，找出每個 frame 的 return address 是重新定位的好方法。

---

## 8. GDB：條件中斷點

只有條件成立（非 0）時才停下來：

```text
(gdb) break *main+65 if $ecx == 0     # 只在迴圈最後一圈停在 jne 上
(gdb) c
(gdb) print $ecx                      # 確認是 0
(gdb) info break                      # 會顯示 "stop only if $ecx == 0"
(gdb) stepi                           # 最後一圈 jne 不跳，往下一條走
```

很適合用在「迴圈跑到第 N 次才出錯」或「某個參數是特定值才出錯」的情況。

---

## 9. GDB 指令速查（Table A-1）

| 指令 | 縮寫 | 說明 |
|---|---|---|
| `run` | `r` | 開始執行 |
| `continue` | `c` | 繼續執行 |
| `finish` | `fin` | 跑到目前函式結束 |
| `stepi [n]` | `si` | 執行 n 條指令，進入函式 |
| `nexti [n]` | `ni` | 執行 n 條指令，略過函式 |
| `break <loc> [if <cond>]` | `b` | 設（條件）中斷點 |
| `info break` | `i b` | 列出中斷點 |
| `delete [id]` | `d` | 刪除中斷點 |
| `print/<fmt> <expr>` | `p` | 印出運算式 |
| `x/<n><fmt><unit> <addr>` | — | 查看記憶體 |
| `layout <window>` | `la` | 開啟視窗 |
| `focus <window>` | `fs` | 切換焦點 |
| `help <cmd>` | `h` | 查看說明 |

---

## 10. LLDB 對照（macOS 用）

功能和 GDB 幾乎一樣，主要差在指令寫法：

| 要做的事 | GDB | LLDB |
|---|---|---|
| 函式中斷點 | `break main` | `break set -n main` |
| 位址中斷點 | `break *main+26` | `break set -a '(void()) main + 26'` |
| 條件中斷點 | `break *main+65 if $ecx == 0` | `break set -a <addr> -c '$ecx == 0'` |
| 列出 / 刪除 | `info break` / `delete 3` | `break list` / `break delete 3` |
| 執行 n 條指令 | `stepi 4` | `stepi -c 4` |
| 印出值 | `print/x $eax` | `exp -f x -- $eax` |
| XMM 當 double | `print $xmm0.v2_double[0]` | `exp -f float64[] -- $xmm0` |
| 查看記憶體 | `x/6xw $rsp` | `memory read -f x -s 4 -c 6 -l 1 $rsp` |
| 反組譯 | `layout asm` | `disassemble -n main`、`disassemble --pc -c 3` |

LLDB 要注意的地方：

- 一般暫存器預設當**無號**顯示（GDB 是有號）。
- 運算式裡有空白或特殊字元時，要用單引號包起來。
- 停下來時會自動印出附近幾條指令，沒有 GDB 那種常駐的組語視窗，要看整個函式就用 `disassemble`。

---

## 11. 用在這個編譯器上

我們的編譯器預設會在連結後刪掉 `.s` 檔，所以要先用 `-S` 把組語留下來，再自己用 gcc 組譯：

```sh
./main -S prog.c            # 產生 prog.s
gcc prog.s -o prog          # 自己組譯連結（Linux 不需要 -g）
gdb prog
```

以下流程已經在本專案產生的組語上實際跑過：

```text
(gdb) break main
(gdb) run
(gdb) stepi 4
(gdb) x/2i $pc                     # 看接下來兩條指令（不開 layout 也行）
(gdb) print *(int *)($rbp - 4)     # 第一個區域變數
(gdb) print (double) d             # static double
(gdb) break f if $edi == 1238      # 第一個整數參數是特定值時才停
(gdb) c
(gdb) x/2ag $rbp                   # 舊的 RBP 和 return address（顯示 <main+113>）
```

對照我們編譯器的慣例，除錯時常看的地方：

| 想看的東西 | 在哪裡 |
|---|---|
| 區域變數、暫存值（pseudo 換成 stack slot） | `-4(%rbp)`、`-8(%rbp)`……，4-byte 用 `x/xw`，8-byte 和 double 用 `x/xg` 或 `x/fg` |
| 第 1–6 個整數參數 | 剛進函式時看 `$edi $esi $edx $ecx $r8d $r9d`（8-byte 用 `$rdi`…） |
| 第 7 個以後的參數 | `16(%rbp)`、`24(%rbp)`…… |
| double 參數 / 回傳值 | `$xmm0.v2_double[0]` ~ `$xmm7…` |
| 整數回傳值 | `$eax` / `$rax` |
| fix-up 用的暫存器 | `$r10`、`$r11`、`$xmm14`、`$xmm15` |
| 浮點常數 | `.Ldouble.N` 在 `.rodata`，用 `x/fg` 查看 |

> `break main` 實際停在 `<main+4>`，也就是 `pushq %rbp; movq %rsp, %rbp` 之後，此時 `$rbp` 已經指向這個函式的 frame。

---

## 12. 一句話總結

除錯器讓我們用「一條指令」為單位觀察程式：`break` 停下來、`si`/`ni` 往前走、`print` 和 `x` 看暫存器與記憶體、`break ... if` 只在出錯的那一刻停下來。編譯器產生的組語出錯時，這是最直接的找錯方法。

---

## 13. 速記重點

```text
準備   -> ./main -S prog.c && gcc prog.s -o prog && gdb prog
介面   -> layout asm / layout reg / focus cmd
執行   -> b main → r → si / ni（不要用 s / n）→ fin → c
位址   -> b *main+26（offset 固定，位址會變）
印值   -> p/x $eax、p $xmm0.v2_double[0]、p (double) dbl、p *(int *)($rbp-4)
記憶體 -> x/6xw $rsp（單位 b/h/w/g）、x/4ag $rsp 找 return address
條件   -> b *main+65 if $ecx == 0
LLDB   -> break set -n/-a/-c、exp -f x --、memory read、disassemble
```
