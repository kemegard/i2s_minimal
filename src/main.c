/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal I2S example for nRF54L15.
 *
 * Streams a stereo sine wave continuously using the I2S peripheral.
 *
 * nRF54L15 I2S hardware requires both TX and RX to be active at the same
 * time (I2S_DIR_BOTH). A dummy RX slab is configured solely to satisfy
 * this hardware requirement; received data is discarded.
 *
 * Pins (nrf54l15dk expansion header):
 *   P1.11  SCK_M
 *   P1.12  LRCK_M
 *   P1.8   SDOUT
 *   P1.9   SDIN  (unused input, required by hardware)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/printk.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── I2S configuration ────────────────────────────────────────────────── */

#define I2S_NODE         DT_NODELABEL(i2s20)
#define SAMPLE_FREQ      8000           /* Hz                              */
#define SAMPLE_NO        32             /* samples per block per channel   */
#define BLOCK_SIZE       (2 * SAMPLE_NO * sizeof(int16_t))  /* stereo      */
#define NUM_BLOCKS       4              /* slab depth                      */
#define TIMEOUT_MS       2000

K_MEM_SLAB_DEFINE(tx_mem_slab, BLOCK_SIZE, NUM_BLOCKS, 32);
K_MEM_SLAB_DEFINE(rx_mem_slab, BLOCK_SIZE, NUM_BLOCKS, 32);

/* ── Sine wave data ───────────────────────────────────────────────────── */

static int16_t sine_l[SAMPLE_NO];   /* 440 Hz left  channel */
static int16_t sine_r[SAMPLE_NO];   /* 880 Hz right channel */

static void generate_sine(void)
{
	const double amp = 16000.0;

	for (int i = 0; i < SAMPLE_NO; i++) {
		sine_l[i] = (int16_t)(amp * sin(2.0 * M_PI * 440.0 * i / SAMPLE_FREQ));
		sine_r[i] = (int16_t)(amp * sin(2.0 * M_PI * 880.0 * i / SAMPLE_FREQ));
	}
}

/* ── I2S helpers ──────────────────────────────────────────────────────── */

/*
 * Write one stereo block to the TX queue.
 * Returns 0 on success, negative errno on failure.
 */
static int write_block(const struct device *dev)
{
	int16_t buf[SAMPLE_NO * 2];

	for (int i = 0; i < SAMPLE_NO; i++) {
		buf[2 * i]     = sine_l[i];
		buf[2 * i + 1] = sine_r[i];
	}

	return i2s_buf_write(dev, buf, BLOCK_SIZE);
}

/*
 * Drain one block from the RX queue.
 * The nRF54L15 I2S peripheral requires RX to run alongside TX; received
 * data is not used here and is silently discarded.
 */
static void discard_rx(const struct device *dev)
{
	char buf[BLOCK_SIZE];
	size_t sz;

	i2s_buf_read(dev, buf, &sz);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
	const struct device *dev;
	struct i2s_config cfg;
	int ret;
	uint32_t count = 0;

	printk("=== I2S Minimal Example (nRF54L15) ===\n");

	dev = DEVICE_DT_GET(I2S_NODE);
	if (!device_is_ready(dev)) {
		printk("ERROR: I2S device not ready\n");
		return -ENODEV;
	}

	generate_sine();

	/* Both TX and RX must use identical configuration on nRF54L15. */
	cfg.word_size      = 16;
	cfg.channels       = 2;
	cfg.format         = I2S_FMT_DATA_FORMAT_I2S;
	cfg.frame_clk_freq = SAMPLE_FREQ;
	cfg.block_size     = BLOCK_SIZE;
	cfg.timeout        = TIMEOUT_MS;
	cfg.options        = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER;

	cfg.mem_slab = &tx_mem_slab;
	ret = i2s_configure(dev, I2S_DIR_TX, &cfg);
	if (ret < 0) {
		printk("ERROR: i2s_configure TX: %d\n", ret);
		return ret;
	}

	cfg.mem_slab = &rx_mem_slab;
	ret = i2s_configure(dev, I2S_DIR_RX, &cfg);
	if (ret < 0) {
		printk("ERROR: i2s_configure RX: %d\n", ret);
		return ret;
	}

	/* Pre-fill the TX queue before starting so the DMA pipeline never
	 * starves during the first trigger.
	 */
	ret = write_block(dev);
	if (ret < 0) {
		return ret;
	}
	ret = write_block(dev);
	if (ret < 0) {
		return ret;
	}

	/* Start TX and RX simultaneously (required on nRF54L15). */
	ret = i2s_trigger(dev, I2S_DIR_BOTH, I2S_TRIGGER_START);
	if (ret < 0) {
		printk("ERROR: I2S START: %d\n", ret);
		return ret;
	}

	printk("Streaming at %d Hz...\n", SAMPLE_FREQ);

	/* Keep the TX queue fed and drain the RX queue. */
	while (true) {
		ret = write_block(dev);
		if (ret < 0) {
			printk("TX error: %d\n", ret);
			break;
		}

		discard_rx(dev);

		count++;
		if (count % 1000 == 0) {
			printk("Blocks sent: %u\n", count);
		}
	}

	i2s_trigger(dev, I2S_DIR_BOTH, I2S_TRIGGER_DROP);
	printk("Stopped.\n");
	return 0;
}
