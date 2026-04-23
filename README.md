# I2S Minimal Example — nRF54L15

A stripped-down Zephyr application that demonstrates the minimum code required to stream audio continuously via the I2S peripheral on the nRF54L15.

For a more complete version with loopback verification and RX error counting, see the companion example: **[i2s_audio_example](https://github.com/kemegard/i2s_example)**.

This version contains no loopback verification — it simply generates a stereo sine wave and keeps the TX queue fed while silently discarding RX data.

## What it does

1. **Generates** two sine waves in memory:
   - Left channel: 440 Hz
   - Right channel: 880 Hz
   - 8 kHz sample rate, 16-bit stereo, 32 samples per block

2. **Streams continuously** using the I2S peripheral in master mode, feeding interleaved stereo blocks from a TX memory slab into the DMA pipeline.

3. **Discards RX data** silently — the RX path is active only because the nRF54L15 hardware requires it.

4. **Reports progress** every 1000 blocks:
   ```
   === I2S Minimal Example (nRF54L15) ===
   Streaming at 8000 Hz...
   Blocks sent: 1000
   Blocks sent: 2000
   ...
   ```

## Platform

| Item | Value |
|---|---|
| Board | `nrf54l15dk/nrf54l15/cpuapp` |
| SDK | nRF Connect SDK v3.3.0-rc2 |
| Zephyr | v4.3.99-22eb89901c98 |
| Toolchain | Zephyr SDK 0.17.0 (ARM GCC 12.2.0) |

## nRF54L15 I2S notes

- The nRF54L15 I2S peripheral **cannot run TX-only**. Both TX and RX must be active simultaneously (`I2S_DIR_BOTH`). A dummy RX memory slab is configured solely to satisfy this hardware requirement.
- TX and RX must be configured with **identical parameters** (word size, sample rate, block size, etc.).
- The hardware requires the trigger to start **both directions at once** via `I2S_TRIGGER_START` on `I2S_DIR_BOTH`.
- The TX queue must be **pre-filled with at least 2 blocks** before triggering to prevent DMA starvation on the first interrupt.

## Pin assignment

Configured in [boards/nrf54l15dk_nrf54l15_cpuapp.overlay](boards/nrf54l15dk_nrf54l15_cpuapp.overlay). Matches the official Zephyr I2S test suite overlay for this board.

| Signal | Pin |
|---|---|
| I2S_SCK_M | P1.11 |
| I2S_LRCK_M | P1.12 |
| I2S_SDOUT | P1.8 |
| I2S_SDIN | P1.9 (unused input) |

## Building and flashing

```
west build -b nrf54l15dk/nrf54l15/cpuapp --pristine
west flash
```

## Serial output

Connect to VCOM1 (e.g. COM90) at **115200 baud**. Expected output after reset:

```
=== I2S Minimal Example (nRF54L15) ===
Streaming at 8000 Hz...
Blocks sent: 1000
Blocks sent: 2000
Blocks sent: 3000
...
```

If a `TX error: -5` appears, the DMA TX queue stalled — this usually means the RX path was not started alongside TX.

Serial settings:
- Baud rate: 115200
- Data bits: 8
- Stop bits: 1
- Parity: None

## Project structure

```
i2s_minimal/
├── CMakeLists.txt
├── prj.conf
├── README.md
├── boards/
│   └── nrf54l15dk_nrf54l15_cpuapp.overlay
└── src/
    └── main.c
```

## How it works

1. **Sine generation** (`generate_sine`): fills two 32-sample lookup tables — 440 Hz left, 880 Hz right — using floating-point math (requires `CONFIG_FPU=y` and `CONFIG_NEWLIB_LIBC=y`).

2. **Block write** (`write_block`): interleaves the left/right tables into a single stereo buffer and calls `i2s_buf_write()`. Returns a negative errno on failure.

3. **RX drain** (`discard_rx`): calls `i2s_buf_read()` to release the RX slab entry back to the driver; return value is ignored.

4. **Main loop**: configures TX and RX with identical parameters → pre-fills 2 TX blocks → triggers `I2S_DIR_BOTH` → loops `write_block` + `discard_rx` indefinitely, printing a counter every 1000 iterations.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `ERROR: I2S device not ready` | Overlay not applied or wrong node label (`i2s20`) |
| `TX error: -5` (EIO) | DMA stall — TX queue starved or RX not draining |
| `TX error: -11` (ENOMEM) | TX slab exhausted — increase `NUM_BLOCKS` |
| No serial output | Wrong COM port or baud rate; try VCOM1 at 115200 |

## License

SPDX-License-Identifier: Apache-2.0
