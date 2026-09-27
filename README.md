# Computer Organization 2026 — Programming Assessments

這個儲存庫記錄我在「計算機組織」課程完成的三個 Programming Assessment（PA）。三份作業並不是彼此孤立的練習，而是一條由**指令**、**向量運算**到**記憶體階層**的學習路徑：先親自用 RISC-V 指令處理資料，再用 RVV 提升計算吞吐量，最後理解程式存取模式如何由 cache 決定實際效能。

> 目標不只是讓程式算出正確答案，而是能解釋資料放在哪裡、指令如何取得它，以及為什麼相同的演算法會因硬體而有不同速度。

## 學習地圖

```text
高階 C 演算法
     │
     ▼
PA1：Inline assembly ──► 暫存器、位址計算、分支與 ABI
     │
     ▼
PA2：RVV intrinsics ───► 資料平行、vector length 與效能導向程式設計
     │
     ▼
PA3：Cache / locality ─► cache line、set associativity、替換策略與資料布局
```

## 專案結構

```text
.
├── PA1/  # RISC-V inline assembly：排序、搜尋、linked-list cycle
├── PA2/  # Mel spectrogram / RVV 題目投影片
├── PA3/  # Cache simulator、矩陣乘法 RVV 最佳化、矩陣轉置片段
└── README.md
```

## PA1 — 從 C 走進 RISC-V 組語

PA1 要求在 C 程式中撰寫 RISC-V inline assembly，並在 Spike 模擬器上執行。三題分別對應陣列排序、二分搜尋與 linked list cycle detection；它們共同訓練的是把 C 的控制流程和資料存取，精準地映射為暫存器、load/store 與 branch 指令。

| 題目 | 實作 | 核心概念 |
| --- | --- | --- |
| Insertion sort | [1_insertion_sort.txt](PA1/1_insertion_sort.txt) | 浮點比較、元素搬移、索引轉位址 |
| Binary search | [2_binary_search.txt](PA1/2_binary_search.txt) | 有號整數分支、midpoint、搜尋區間收斂 |
| Linked-list cycle | [3_linked_list_cycle.txt](PA1/3_linked_list_cycle.txt) | 指標解參照、結構欄位 offset、Floyd tortoise-and-hare |

### 1. Insertion sort：用位址運算移動浮點元素

實作先以 `flw` 將目前的 key 載入浮點暫存器 `ft0`，再從右向左檢查已排序區間。`slli t1, t0, 2` 將索引乘以 `sizeof(float)`，配合 `add` 求出 `A[j]` 的實體位址；若 `flt.s` 判定 `key < A[j]`，便以 `fsw` 將元素右移，並累計 shift 次數。最後將 key 寫入停止位置。

這題讓我實際掌握：陣列下標不是硬體原生概念，編譯後必須成為「base pointer + byte offset」；整數暫存器負責位址與控制，浮點暫存器才負責單精度資料與比較。

### 2. Binary search：把迴圈不變量翻成 branch

二分搜尋維持 `[low, high]` 的候選區間，並以 `(low + high) >> 1` 計算中點。程式利用 `feq.s` 先判斷命中，再以 `flt.s` 決定 target 位於哪一側，對應更新 `low` 或 `high`；`blt high, low` 則是搜尋失敗的終止條件。

除了時間複雜度從線性降為 `O(log n)`，更重要的是理解每個 `if`、`while` 都會成為控制相依的 branch，而資料必須先經過 load 才能比較。浮點比較不直接產生旗標，而是將布林結果寫入一般整數暫存器，供後續 branch 使用。

### 3. Linked-list cycle：指標其實是記憶體位址

這題實作 Floyd’s cycle detection。`slow` 每輪走一步，`fast` 每輪走兩步；任一 `fast` 指標為空便代表無環，兩者相遇則代表偵測到環。程式以 `ld t?, 8(t?)` 取得下一個節點，反映目標結構中 `next` 欄位位於位移 8 bytes 的位置。

這讓我把 C 的 `node->next` 具體連到「以 pointer 為基底，加上欄位 offset 後 load」的機器操作，也練習在不破壞呼叫端狀態的前提下，正確宣告 inline assembly 的 input/output operands 與 clobber list。

### PA1 帶走的 RISC-V 技能

- 整數指令：`addi`、`add`、`slli`、`srai`、`li`、`mv`。
- 控制流程：`beq`、`blt` 與無條件跳躍 `j`，以及 label 的迴圈結構。
- 記憶體與浮點：`ld`、`flw`、`fsw`、`flt.s`、`feq.s`。
- GCC inline assembly：具名 operands、`+r` read-write constraint、`f` floating-point constraint，以及不可省略的 `memory` clobber。

## PA2 — 以 RVV 加速 Mel Spectrogram 的運算核心

PA2 的目標是實作 mel spectrogram 的三個核心函式：in-place FFT、power spectrum 與 mel filter bank，並以 RISC-V Vector（RVV）intrinsics 最佳化效能。題目將正確性與效能分開計分，因此它是第一次明確面對「正確的 C 程式不一定足夠快」的作業。

典型資料流如下：

```text
time-domain samples
        │ FFT
        ▼
complex frequency bins
        │ power spectrum: re² + im²
        ▼
per-bin energy
        │ mel filter bank（加權彙整）
        ▼
mel-frequency features
```

RVV 的關鍵並非假定固定向量寬度，而是每一輪用 `vsetvl`／對應 intrinsic 依剩餘元素取得可處理的 vector length（`vl`）。再以向量 load、逐元素算術和 reduction 或累加操作同時處理多個 sample/bin。這種 *vector-length agnostic* 的寫法能隨實際硬體可提供的向量長度調整，而不需要把演算法綁死在某個 SIMD 寬度。

本 repo 目前保留 [PA2 題目投影片](PA2/CO2026PA2.pptx)，未包含當時的 `src/main.c` 解答；因此此段聚焦作業的設計與所學，而不聲稱提供可重現的 PA2 實作或測量結果。

### PA2 帶走的實作觀念

- 將資料相依拆開：FFT 的 stage、power spectrum 的逐元素運算、filter bank 的加權歸約各有不同的平行化方式。
- 使用 RVV intrinsics 表達 load / arithmetic / store，同時保留 tail 元素的正確處理。
- 以連續記憶體存取、減少不必要的 scalar-vector 轉換與適當 loop blocking 降低資料搬移成本。
- 用 benchmark 驗證最佳化；效能是資料布局、指令級工作量與硬體資源共同作用的結果。

## PA3 — Cache 行為與資料布局導向的最佳化

PA3 把焦點放在記憶體階層：一部分是擴充 cache simulator 的替換與統計邏輯，另一部分則把理論回扣到矩陣轉置與矩陣乘法的存取模式。這份作業的核心提問是：**當 CPU 等記憶體時，演算法的 Big-O 還能說明多少效能？**

### Cache simulator：set-associative cache 與 tree-PLRU

[cachesim.cc](PA3/cachesim.cc) 與 [cachesim.h](PA3/cachesim.h) 實作可配置的 cache simulator。設定格式為 `sets:ways:blocksize`；例如 [dc_config.py](PA3/dc_config.py) 的 D-cache 是 `16 sets × 4 ways × 64 bytes`，總容量為 4 KiB。

每次 memory access 會先由位址拆出 block offset、set index 與 tag：

```text
address = [                 tag | set index | block offset ]
                                      ^              ^
                                 選一個 set       64 B line
```

實作的重點包括：

- tag 以 `VALID`、`DIRTY` bit 表示 cache line 狀態；store hit 將 line 標為 dirty。
- miss 時挑選 victim；若 victim 有效便累計 eviction，若同時 dirty 則 write back 到下一層。
- 每個 set 配置 `ways - 1` 個 bit，形成二元樹的 pseudo-LRU（PLRU）。hit 時 `update_tree_on_hit()` 把路徑標成較新使用的一側；需要替換時 `select()` 沿著反方向走到候選 victim。它只需 `O(ways)` 位元，而非精確 LRU 所需的完整順序資訊。
- simulator 額外輸出 read/write accesses、misses、writebacks 與 miss rate，讓最佳化能用觀察到的 cache 行為驗證。

### 矩陣轉置：避免 direct-mapped / set conflict 的陷阱

[snippet.c](PA3/snippet.c) 是 8×8 blocked transpose 的核心。程式先處理前四列，將右上角四個元素暫存；接著交換這些暫存值與左下角的資料，最後完成右下角。暫存器暫存可避免 A 與 B 對映到相同 cache set 時，讀寫交替造成不必要的 conflict miss。

這裡學到的不是「block 大小永遠選 8」，而是 block 大小必須配合元素大小、cache line 大小、set 數與 associativity；最佳策略來自工作集能否留在 cache 與位址是否互相衝突。

### 矩陣乘法：RVV、register blocking 與 K tiling

[matmul_improved.c](PA3/matmul_improved.c) 計算 row-major 的 `C = A × B`。對每個輸出位置區塊，它一次處理 C 的 8 列：八個向量 accumulator 留在 vector registers，對同一個 B 的連續向量載入執行八次 scalar-vector FMA。`__riscv_vsetvl_e32m2(N - j)` 讓 N 維度可依硬體 VL 與尾端長度自然切分；`KTILE = 64` 則分段掃描 K 維度。

這個設計同時結合三種層級的最佳化：

| 手法 | 程式中的做法 | 目的 |
| --- | --- | --- |
| Vectorization | `vle32`、`vfmacc`、`vse32` | 一條向量指令處理多個 N 維輸出 |
| Register blocking | 8 個 `a0`–`a7` accumulators | 提高同一份 B 向量的重用，減少 C 的中間讀寫 |
| Cache tiling | K 維以 64 為區塊 | 將工作集切小，改善資料重用與 cache locality |

尾端不足 8 列時，程式以 scalar row 的向量版本處理，因此不依賴 M 為 8 的倍數。這是效能最佳化不可省略的一環：fast path 與 boundary path 都必須正確。

## 整體收穫

完成三個 PA 後，我建立了從 source code 到硬體行為的連結：

1. **正確性先於最佳化。** PA1 迫使我仔細維持 loop boundary、指標有效性與 inline assembly 的資料契約；這些不變量也是之後所有最佳化的前提。
2. **RISC-V 是可組合的工具箱。** 基礎整數／浮點 ISA 負責精確控制；RVV 則以可變 `vl` 將資料平行化，避免將程式綁定於固定硬體寬度。
3. **資料移動往往比算術昂貴。** PA2 與 PA3 都顯示，向量化只有在存取連續、可重用且 cache 友善的資料時才能充分發揮效果。
4. **量測與模型要相互驗證。** Cache simulator 的 hit/miss/eviction 統計提供可解釋的模型；benchmark 則確認最佳化是否真的改善目標平台上的效能。

## 參考資料與限制

- 作業題目保留於 [PA1](PA1/CO2026PA1.pptx)、[PA2](PA2/CO2026PA2.pptx) 與 [PA3](PA3/Computer%20Organization%202026%20Programming%20Assessment%20III%20%281%29.pdf)。
- PA1 的程式以 inline-assembly 片段形式保存；PA3 提供的檔案是作業核心實作，而非獨立、可直接建置的完整專案。
- 本 README 不列出跨平台效能數字，因結果會受 RISC-V 模擬器／硬體、向量設定、編譯器選項與測資尺寸影響。
