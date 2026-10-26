# stm32-dxl2

STM32 の HAL と UART の半二重モード（1 本の線）で、DYNAMIXEL を **Protocol 2.0** で動かす、小さな C のライブラリです。
ファイルは `dxl2.c` と `dxl2.h` の 2 つだけで、STM32CubeIDE（CubeMX）で作ったプロジェクトに、そのまま入れて使えます。

A small C library to drive DYNAMIXEL servos with **Protocol 2.0** from STM32 HAL, using the UART in single-wire half-duplex mode.
See [English](#english) below.

> このライブラリは非公式のものです。ROBOTIS とは関係がなく、ROBOTIS が認めたものでもありません。
> DYNAMIXEL は ROBOTIS Co., Ltd. の商標です。

---

## できること

| 関数 | 内容 |
| --- | --- |
| `dxl2_ping` | 返事があるか確かめ、モデル番号とファームウェアの版を受け取る |
| `dxl2_read` | 1 個のモーターから、コントロールテーブルを読む |
| `dxl2_write` / `dxl2_write1` / `dxl2_write2` / `dxl2_write4` | 1 個のモーターに書く（全員あて ID 0xFE も使える） |
| `dxl2_sync_write` | 複数のモーターの、同じアドレスに一度に書く |
| `dxl2_sync_read` | 複数のモーターの、同じアドレスから一度に読む |
| `dxl2_reboot` | モーターを再起動する（エラーを消す） |
| `dxl2_crc` | Protocol 2.0 の CRC-16 |

- 送るときと受け取るときの、**バイトスタッフィング**に対応しています
- 結果は `dxl2_result_t` で返ります（返事なし、CRC の不一致、モーターのエラー、など）
- X シリーズのよく使うアドレスを、`dxl2_xseries.h` に名前を付けて入れてあります

## 対応しているマイコン

STM32 の HAL の UART（`HAL_HalfDuplex_*`）だけを使っています。UART の作りには 2 つの世代があり、どちらにも対応するように書いています。

| シリーズ | UART の作り | 状態 |
| --- | --- | --- |
| **STM32F3**（NUCLEO-F303K8） | 新しい世代 | **実機で動作を確認**（XL330-M288 と XC330-T288、1 Mbps）※1 / コンパイルを確認 |
| STM32C0、F0、F7、G0、**G4**、H5、H7、L0、L4、L4+、L5、U0、U5、WB、WBA、WL | 新しい世代（F3 と同じ） | 動く見込み（未確認） |
| STM32F1、F2、F4、L1 | 古い世代 | 専用の処理を入れてあり、動く見込み（未確認） |

- **実機で動作を確認**：そのマイコンと DYNAMIXEL をつないで、実際に動かした
- **コンパイルを確認**：arm-none-eabi-gcc 14.3 で、`-Wall -Wextra -Wshadow -Wconversion` の警告なしにコンパイルできた（gnu11 と c99）
- **動く見込み**：同じ HAL の関数とマクロを使っているので動くはずだが、まだ確かめていない

※1 実機で確かめたのは、このライブラリのもとになった版です（Ping・Read・Write・Sync Write・Reboot）。名前をそろえて公開用に整えたこの版と、`dxl2_sync_read`、受け取るときのバイトスタッフィングは、まだ実機で確かめていません。

ほかのシリーズで動いた・動かなかった、という報告を歓迎します。

## 対応しているモーター

Protocol 2.0 に対応した DYNAMIXEL なら、どれでも使えるはずです（X シリーズ、MX シリーズの Protocol 2.0、XL320 など）。
`dxl2_xseries.h` のアドレスは X シリーズ用です。ほかのシリーズでは、e-Manual でアドレスを確かめてください。

---

## つなぎ方

### いちばん簡単な形（1 Mbps くらいまで）

```
 STM32 の TX ピン（例：PA9） ──┬──── DYNAMIXEL の DATA
                              │
                            [1 kΩ]
                              │
 3.3 V ───────────────────────┘

 STM32 の GND ─────────────────────── DYNAMIXEL の GND ─── モーター用電源の GND
 モーター用電源（＋） ──────────────── DYNAMIXEL の VDD   ※ STM32 の 5V ピンからは取らない
```

- TX ピンは **オープンドレイン**にして、外付けの抵抗でプルアップします
- そのピンが **5 V トレラント（FT）**か、データシートで確かめてください（DYNAMIXEL の中にもプルアップがあるため）
- **GND は必ず共通にしてください**。つながっていないと、DATA の信号を正しく読めません

### うまくいかないときに気をつけること

- **たくさんつなぐと、Low が浅くなる**：DYNAMIXEL の 1 個 1 個の中にプルアップがあるので、たくさんつなぐと、全体のプルアップがとても強くなります。ピンが Low に引っぱりきれなくなったら、外付けのプルアップを外すか大きくするか、下のバッファを使ってください
- **電圧の違うモーターを混ぜるとき**（例：XL330 は 6 V まで、XC330-T288 は 12 V）：デイジーチェーンのケーブルは VDD も通すので、**電源の＋どうしがつながらないように**分けてください（DATA と GND だけを共通にする）
- **速く通信したいとき**（2 Mbps 以上）や、モーターが多いとき：74LVC2G241 などのバッファ IC で、送るときと受け取るときの向きを切り替える回路がおすすめです。G4 や F3 の USART には、向きを自動で切り替える DE ピンがあります

---

## CubeMX の設定

| 項目 | 値 |
| --- | --- |
| USART | Mode：**Single Wire (Half-Duplex)** |
| Baud Rate | モーターと同じ（例：1000000） |
| Word Length / Parity / Stop Bits | 8 Bits / None / 1 |
| TX ピンの GPIO | **Alternate Function Open Drain**、内蔵プルアップなし（外付けを使う） |

クロックは、通信速度で割り切れる値にすると、誤差がなくなります（例：64 MHz ÷ 16 ÷ 1 Mbps ＝ 4、170 MHz ÷ 1 Mbps ＝ 170）。

## 入れ方

1. `include/` の `dxl2.h`・`dxl2_xseries.h` を、プロジェクトの `Core/Inc` に、`src/dxl2.c` を `Core/Src` にコピーする
2. `main.c` で `#include "dxl2.h"` して、UART の初期化のあとに `dxl2_init(&huart1, 2);` を呼ぶ

`dxl2.h` は、HAL を `main.h` から読み込みます。CubeMX を使っていないときは、`dxl2.h` を読み込む前に `#define DXL2_HAL_HEADER "stm32g4xx_hal.h"` のように指定してください。

## 使い方の例

[examples/basic/main_user_code.c](examples/basic/main_user_code.c) に、CubeMX の `main.c` の `USER CODE` に入れる形の例があります（ID の探索、トルク ON、Sync Write で動かし Sync Read で読む）。

```c
dxl2_init(&huart1, 2);

uint16_t model;
if (dxl2_ping(1, &model, NULL) == DXL2_OK) {
    dxl2_write1(1, DXL2_X_TORQUE_ENABLE, 1);
    dxl2_write4(1, DXL2_X_GOAL_POSITION, 2048);
}
```

---

## 気をつけること・できないこと

- **待つ方式**：HAL のポーリングで、返事が来るまで待ちます（DMA や割り込みは使いません）。割り込みの中から呼ばないでください
- **バス（UART）は 1 本だけ**：送受信のバッファを 1 組だけ持っています
- **Return Delay Time を 0 にする**のがおすすめです（初期値は 500 µs）。0 にすると、送信が終わってすぐに返事が来るので、受信への切り替えの間に、時間のかかる処理（printf など）を入れないでください
- **待ち時間**は HAL_GetTick（1 ms 刻み）で数えるので、2 ms 以上にしてください
- **`dxl2_sync_read`**：返事の中でバイトスタッフィングが起きると（まれ）、後ろのモーターがタイムアウトになることがあります
- **未対応**：Protocol 1.0、Bulk Read / Bulk Write、Fast Sync Read

## 参考にした資料

- ROBOTIS e-Manual：DYNAMIXEL Protocol 2.0（パケットの形、命令、バイトスタッフィング）
- ROBOTIS：CRC の計算（多項式 0x8005）

このライブラリは、上の公開されている仕様をもとに書いたものです。

このライブラリのコードは、AI（Anthropic の Claude）の手助けを受けて書きました。

## ライセンス

[MIT License](LICENSE)

---

## English

**stm32-dxl2** is a two-file C library (`dxl2.c`, `dxl2.h`) that drives DYNAMIXEL servos with Protocol 2.0 from STM32 HAL, using a UART in single-wire half-duplex mode.

**Features**: Ping, Read, Write (1/2/4 bytes, broadcast), Sync Write, Sync Read, Reboot, CRC-16, byte stuffing on transmit and receive, and named addresses for the X series (`dxl2_xseries.h`).

**Supported MCUs**

| Series | UART generation | Status |
| --- | --- | --- |
| **STM32F3** (NUCLEO-F303K8) | new | **tested on hardware** (XL330-M288, XC330-T288, 1 Mbit/s)\* / compiles cleanly |
| STM32C0, F0, F7, G0, **G4**, H5, H7, L0, L4, L4+, L5, U0, U5, WB, WBA, WL | new (same as F3) | expected to work, not tested |
| STM32F1, F2, F4, L1 | old | dedicated code path, expected to work, not tested |

\* Hardware tests were done with the version this library was derived from (Ping, Read, Write, Sync Write, Reboot). This cleaned-up version, `dxl2_sync_read`, and receive-side byte stuffing have not been tested on hardware yet. Reports for other series are welcome.

**Wiring**: TX pin as open-drain with an external pull-up (e.g. 1 kΩ to 3.3 V, use a 5 V tolerant pin), common GND, servo power from its own supply. With many servos the internal pull-ups add up; use a tri-state buffer (e.g. 74LVC2G241) for higher baud rates or long chains. When mixing servos with different supply voltages, do not connect their VDD lines.

**CubeMX**: USART in *Single Wire (Half-Duplex)* mode, 8N1, the servo's baud rate, TX pin as *Alternate Function Open Drain*.

**Usage**: copy the headers to `Core/Inc` and `dxl2.c` to `Core/Src`, then call `dxl2_init(&huart1, 2)` after the UART is initialised. See `examples/basic/main_user_code.c`.

**Limitations**: blocking (polling) API, one bus, not for use from interrupts, timeouts of 2 ms or more, no Protocol 1.0 / Bulk Read / Bulk Write / Fast Sync Read.

Written from the public ROBOTIS e-Manual (Protocol 2.0 and its CRC page), with help from an AI assistant (Anthropic's Claude).
Unofficial; not affiliated with ROBOTIS. DYNAMIXEL is a trademark of ROBOTIS Co., Ltd.

License: [MIT](LICENSE)
