# 第十六章實作紀錄

lemon_cc 支援 `char`、`signed char`、`unsigned char`、字元常數與字串字面值。原有陣列、指標運算與 extra-credit 功能繼續保留。

## 型別與運算

三種字元型別都占 1 byte，但 `char` 和 `signed char` 是不同型別。本專案的 x86-64 目標將普通 `char` 視為有號。

字元常數的型別是 `int`，也能作為陣列長度。字元參與算術、位元運算、一元負號、補數或 switch 時先提升成 `int`。Compound assignment 和 increment/decrement 保留原本 lvalue 的型別，計算後再縮窄，並只對 lvalue 求值一次。

## 字串與初始化

Lexer 驗證引號、換行與簡單跳脫字元；parser 合併相鄰字串。字串節點保存解碼後的內容與長度。

字串初始化字元陣列時保留內容，不先轉成指標。容量等於內容長度時省略結尾零，容量較大時補零，容量不足則拒絕。Automatic 陣列逐 byte 複製；static 陣列使用字串資料與補零初始值。

一般字串運算式代表可取址的 `char[N]` 靜態物件，通常經 array-to-pointer decay 轉成 `char *`。顯式 `&` 保留指向整個陣列的型別。字串初始化 static 指標時，資料輸出保留符號位址 relocation。

Type checker 登記唯讀字串常數，TACKY 收集頂層常數，後端將 Linux 字串物件放進 `.rodata`。產生的符號名稱含有句點，避免與使用者的 C 識別字碰撞。相同內容的字串不保證共用同一個物件。

## Byte 後端

`ASM_BYTE` 決定 1-byte 儲存與暫存器別名。延伸指令分別記錄來源與目的位寬，有號字元使用符號延伸，無號字元使用零延伸。字元與 double 之間先經過 int。堆疊傳遞的 byte 參數先讀取 1 byte，再放入參數槽，避免超出來源物件範圍。

## 驗收命令

```sh
make -B
~/writing-a-c-compiler-tests/test_compiler ./main --chapter 16 --extra-credit
python3 tests/ch15_regressions.py ./main
python3 tests/ch16_regressions.py ./main
```

`tests/ch16_regressions.py` 透過 CLI 編譯並執行程式，涵蓋型別提升、字串初始化、字元常數陣列長度、符號碰撞、錯誤階段與 GCC `-O2` 雙向呼叫。GCC 呼叫測試涵蓋暫存器參數、堆疊參數及窄型別回傳值。

## 範圍限制

本章不支援多字元常數、八進位或十六進位來源跳脫序列，包含 `\0`；也不支援寬字元、寬字串、`const`、`sizeof` 或一般的 `void` 型別。修改字串字面值的內容屬於 undefined behavior。GCC 互通驗證針對目前 Linux x86-64 環境，未在 macOS 執行。
