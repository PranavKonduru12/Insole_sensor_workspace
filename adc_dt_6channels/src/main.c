/*
 * Six-channel ADC CSV logger
 *
 * Samples Feather pins A0 through A5 at 100 Hz per channel.
 *
 * CSV output format:
 *
 * time_ms,a0_raw,a1_raw,a2_raw,a3_raw,a4_raw,a5_raw
 *
 * Example:
 *
 * 10,1870,1879,1887,1257,1225,1208
 * 20,1872,1880,1885,1260,1227,1210
 */
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
/*
 * The zephyr,user node is defined in app.overlay.
 */
#define USER_NODE DT_PATH(zephyr_user)
/*
 * Six ADC inputs:
 *
 * A0, A1, A2, A3, A4, A5
 */
#define NUM_ADC_CHANNELS 6U
/*
 * Sampling period:
 *
 * 10 ms = 0.010 s
 *
 * Sampling frequency:
 *
 * 1 / 0.010 s = 100 Hz
 */
#define SAMPLE_PERIOD_MS 100U
#if !DT_NODE_EXISTS(USER_NODE) || \
    !DT_NODE_HAS_PROP(USER_NODE, io_channels)
#error "No suitable devicetree overlay specified"
#endif
/*
 * Confirm that the overlay contains exactly six ADC channels.
 */
BUILD_ASSERT(
    DT_PROP_LEN(USER_NODE, io_channels) == NUM_ADC_CHANNELS,
    "app.overlay must contain exactly six ADC channels"
);
/*
 * --------------------------------------------------------------------------
 * Direct USB CDC ACM output
 * --------------------------------------------------------------------------
 *
 * These are the same direct CDC output functions used in the working
 * program from yesterday.
 */
static const struct device *cdc_dev =
    DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
/*
 * Send one null-terminated string directly to the USB CDC device.
 */
static void cdc_puts(const char *msg)
{
    while (*msg != '\0') {
        uart_poll_out(cdc_dev, *msg);
        msg++;
    }
}
/*
 * Formatted output function.
 *
 * vsnprintf() converts the format string and arguments into text stored
 * in buf. cdc_puts() then sends that text through USB CDC.
 */
static void cdc_printf(const char *fmt, ...)
{
    char buf[160];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    cdc_puts(buf);
}
/*
 * Give Windows time to enumerate the USB device, then wait briefly for
 * the terminal or Python program to assert DTR.
 */
static void cdc_wait_for_terminal(void)
{
    uint32_t dtr = 0U;
    if (!device_is_ready(cdc_dev)) {
        return;
    }
    /*
     * Allow time for Windows to recognize the USB CDC device.
     */
    k_sleep(K_SECONDS(3));
    /*
     * Wait for DTR for up to approximately five seconds.
     */
    for (int i = 0; i < 50; i++) {
        (void)uart_line_ctrl_get(
            cdc_dev,
            UART_LINE_CTRL_DTR,
            &dtr
        );
        if (dtr != 0U) {
            break;
        }
        k_sleep(K_MSEC(100));
    }
}
/*
 * Redirect every printk() below this line to the direct CDC function.
 */
#define printk cdc_printf
/*
 * --------------------------------------------------------------------------
 * ADC channel definitions
 * --------------------------------------------------------------------------
 *
 * These correspond to the six entries in the app.overlay io-channels
 * property.
 *
 * Array index:
 *
 * 0 = A0
 * 1 = A1
 * 2 = A2
 * 3 = A3
 * 4 = A4
 * 5 = A5
 */
static const struct adc_dt_spec adc_channels[NUM_ADC_CHANNELS] = {
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 0),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 1),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 2),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 3),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 4),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 5),
};
/*
 * One signed 16-bit sample for each ADC input.
 *
 * After each complete scan:
 *
 * sample_buffer[0] = A0
 * sample_buffer[1] = A1
 * sample_buffer[2] = A2
 * sample_buffer[3] = A3
 * sample_buffer[4] = A4
 * sample_buffer[5] = A5
 */
static int16_t sample_buffer[NUM_ADC_CHANNELS];
/*
 * Periodic timer used to start one six-channel ADC scan every 10 ms.
 */
K_TIMER_DEFINE(sample_timer, NULL, NULL);
int main(void)
{
    int err;
    uint32_t start_time_ms;
    /*
     * Define one ADC sequence containing all six channels.
     *
     * The channel-selection bitmask is constructed during channel
     * initialization.
     */
    struct adc_sequence sequence = {
        .channels = 0U,
        .buffer = sample_buffer,
        .buffer_size = sizeof(sample_buffer),
        .resolution = 12,
        .oversampling = 0,
        .calibrate = false,
        .options = NULL,
    };
    /*
     * Wait for the USB CDC serial terminal.
     */
    cdc_wait_for_terminal();
    /*
     * Configure all six ADC channels before sampling.
     */
    for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
        /*
         * Verify that the ADC peripheral is ready.
         */
        if (!adc_is_ready_dt(&adc_channels[i])) {
            printk(
                "# ERROR: ADC device for A%u is not ready\r\n",
                (unsigned int)i
            );
            return 0;
        }
        /*
         * All six channels must belong to the same ADC peripheral.
         */
        if (adc_channels[i].dev != adc_channels[0].dev) {
            printk(
                "# ERROR: A%u uses a different ADC device\r\n",
                (unsigned int)i
            );
            return 0;
        }
        /*
         * Apply the channel configuration from app.overlay.
         */
        err = adc_channel_setup_dt(&adc_channels[i]);
        if (err < 0) {
            printk(
                "# ERROR: Could not configure A%u, error %d\r\n",
                (unsigned int)i,
                err
            );
            return 0;
        }
        /*
         * Add this logical ADC channel to the sequence bitmask.
         */
        sequence.channels |= BIT(adc_channels[i].channel_id);
    }
    /*
    * Print one CSV header line.
    *
    * timestamp : Time since the program started (ms)
    * p1-p6     : Raw ADC values from the six pressure sensors
    */
    printk(
        "timestamp,"
        "p1,"
        "p2,"
        "p3,"
        "p4,"
        "p5,"
        "p6\r\n"
    );
    /*
     * Save the starting uptime so that the CSV time begins near zero.
     */
    start_time_ms = k_uptime_get_32();
    /*
     * Start one six-channel scan every 10 ms.
     */
    k_timer_start(
        &sample_timer,
        K_MSEC(SAMPLE_PERIOD_MS),
        K_MSEC(SAMPLE_PERIOD_MS)
    );
    while (1) {
        uint32_t timestamp_ms;
        /*
         * Wait until the next 10 ms timer expiration.
         */
        (void)k_timer_status_sync(&sample_timer);
        /*
         * Record the actual elapsed time immediately before starting
         * the ADC scan.
         */
        timestamp_ms = k_uptime_get_32() - start_time_ms;
        /*
         * Perform one complete six-channel ADC scan.
         */
        err = adc_read(adc_channels[0].dev, &sequence);
        if (err < 0) {
            /*
             * Error lines begin with # so the Python logger can
             * recognize and ignore them.
             */
            printk(
                "# ERROR: ADC read failed, error %d\r\n",
                err
            );
            continue;
        }
        /*
         * Output one compact CSV row.
         *
         * One row is transmitted every 10 ms, giving 100 rows/s.
         */
        printk(
            "%" PRIu32 ","
            "%d,%d,%d,%d,%d,%d\r\n",
            timestamp_ms,
            (int)sample_buffer[0],
            (int)sample_buffer[1],
            (int)sample_buffer[2],
            (int)sample_buffer[3],
            (int)sample_buffer[4],
            (int)sample_buffer[5]
        );
    }
    return 0;
}