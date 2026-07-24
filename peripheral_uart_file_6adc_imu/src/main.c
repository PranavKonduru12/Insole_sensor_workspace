#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/services/nus.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define USER_NODE DT_PATH(zephyr_user)
#define NUM_ADC_CHANNELS 6U
#define SAMPLE_PERIOD_MS 10U

#if !DT_NODE_EXISTS(USER_NODE) || !DT_NODE_HAS_PROP(USER_NODE, io_channels)
#error "No suitable devicetree overlay specified"
#endif

BUILD_ASSERT(DT_PROP_LEN(USER_NODE, io_channels) == NUM_ADC_CHANNELS,
             "Overlay must contain exactly six ADC channels");

static bool ble_connected = false;
static bool header_sent = false;
static struct bt_conn *current_conn = NULL;

static const char csv_header[] =
    "timestamp,p1,p2,p3,p4,p5,p6\n";

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

static const struct adc_dt_spec adc_channels[NUM_ADC_CHANNELS] = {
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 0),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 1),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 2),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 3),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 4),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 5),
};

static int16_t sample_buffer[NUM_ADC_CHANNELS];

static void restart_advertising_work_handler(struct k_work *work)
{
    int err;

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2,
                          ad, ARRAY_SIZE(ad),
                          sd, ARRAY_SIZE(sd));

    if (err) {
        printk("Advertising restart failed: %d\n", err);
    } else {
        printk("Advertising restarted\n");
    }
}

K_WORK_DELAYABLE_DEFINE(restart_advertising_work,
                        restart_advertising_work_handler);

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("BLE connection failed, error %u\n", err);
        return;
    }

    current_conn = bt_conn_ref(conn);
    ble_connected = true;
    header_sent = false;

    printk("BLE connected\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    ble_connected = false;
    header_sent = false;

    printk("BLE disconnected, reason %u\n", reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    /*
     * Restart advertising after disconnect so the device becomes
     * discoverable again without requiring a reset.
     */
    k_work_schedule(&restart_advertising_work, K_MSEC(500));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

int main(void)
{
    int err;
    uint32_t start_time_ms;

    struct adc_sequence sequence = {
        .channels = 0U,
        .buffer = sample_buffer,
        .buffer_size = sizeof(sample_buffer),
        .resolution = 12,
        .oversampling = 0,
        .calibrate = false,
        .options = NULL,
    };

    printk("Starting BLE + 6-channel ADC test\n");

    for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
        if (!adc_is_ready_dt(&adc_channels[i])) {
            printk("ADC device for channel %u is not ready\n",
                   (unsigned int)i);
            return 0;
        }

        if (adc_channels[i].dev != adc_channels[0].dev) {
            printk("ADC channel %u uses a different ADC device\n",
                   (unsigned int)i);
            return 0;
        }

        err = adc_channel_setup_dt(&adc_channels[i]);
        if (err < 0) {
            printk("Could not configure ADC channel %u, error %d\n",
                   (unsigned int)i, err);
            return 0;
        }

        sequence.channels |= BIT(adc_channels[i].channel_id);
    }

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed: %d\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");

    err = bt_nus_init(NULL);
    if (err) {
        printk("Nordic UART Service init failed: %d\n", err);
        return 0;
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2,
                          ad, ARRAY_SIZE(ad),
                          sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Advertising failed to start: %d\n", err);
        return 0;
    }

    printk("Advertising as %s\n", DEVICE_NAME);
    printk("%s", csv_header);

    start_time_ms = k_uptime_get_32();

    while (1) {
        char data_line[128];
        uint32_t timestamp_ms;

        timestamp_ms = k_uptime_get_32() - start_time_ms;

        err = adc_read(adc_channels[0].dev, &sequence);
        if (err < 0) {
            printk("# ERROR: ADC read failed, error %d\n", err);
            k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
            continue;
        }

        snprintf(data_line, sizeof(data_line),
                 "%" PRIu32 ",%d,%d,%d,%d,%d,%d\n",
                 timestamp_ms,
                 (int)sample_buffer[0],
                 (int)sample_buffer[1],
                 (int)sample_buffer[2],
                 (int)sample_buffer[3],
                 (int)sample_buffer[4],
                 (int)sample_buffer[5]);

        printk("%s", data_line);

        if (ble_connected && current_conn) {
            if (!header_sent) {
                err = bt_nus_send(current_conn,
                                  csv_header,
                                  strlen(csv_header));

                if (err == 0) {
                    header_sent = true;
                    printk("CSV header sent\n");

                    err = bt_nus_send(current_conn,
                                      data_line,
                                      strlen(data_line));
                    if (err) {
                        printk("BLE send failed: %d\n", err);
                    }
                } else {
                    printk("Header send failed: %d\n", err);
                }
            } else {
                err = bt_nus_send(current_conn,
                                  data_line,
                                  strlen(data_line));
                if (err) {
                    printk("BLE send failed: %d\n", err);
                }
            }
        }

        k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
    }

    return 0;
}