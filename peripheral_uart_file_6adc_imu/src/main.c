#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/services/nus.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define USER_NODE DT_PATH(zephyr_user)

#define NUM_ADC_CHANNELS 6U

/*
 * One combined ADC + IMU sample approximately every 10 ms.
 *
 * 10 ms corresponds to a target software sampling rate of 100 Hz.
 */
#define SAMPLE_PERIOD_MS 10U

/*
 * The LSM6DSL family supports 104 Hz as the closest hardware ODR
 * to the requested 100 Hz.
 */
#define IMU_ODR_HZ 104

/*
 * Repeat the CSV header over BLE after this many successfully
 * transmitted data rows.
 */
#define CSV_HEADER_INTERVAL_ROWS 500U

#if !DT_NODE_EXISTS(USER_NODE) || \
    !DT_NODE_HAS_PROP(USER_NODE, io_channels)
#error "No suitable devicetree overlay specified"
#endif

BUILD_ASSERT(
    DT_PROP_LEN(USER_NODE, io_channels) == NUM_ADC_CHANNELS,
    "Overlay must contain exactly six ADC channels"
);

static uint32_t serial_rows_since_header =
    CSV_HEADER_INTERVAL_ROWS;

/*
 * --------------------------------------------------------------------------
 * BLE state
 * --------------------------------------------------------------------------
 */

static bool ble_connected;
static bool header_sent;
static uint32_t rows_since_header;
static struct bt_conn *current_conn;

/*
 * IMU values use integer micro-units:
 *
 * ax, ay, az:
 *     micrometers per second squared
 *
 * gx, gy, gz:
 *     microradians per second
 *
 * Divide by 1,000,000 on the phone or computer to recover:
 *
 *     acceleration in m/s^2
 *     angular velocity in rad/s
 */
static const char csv_header[] =
    "timestamp,p1,p2,p3,p4,p5,p6,"
    "ax,ay,az,gx,gy,gz\n";

static const struct bt_data ad[] = {
    BT_DATA_BYTES(
        BT_DATA_FLAGS,
        BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR
    ),
    BT_DATA(
        BT_DATA_NAME_COMPLETE,
        DEVICE_NAME,
        DEVICE_NAME_LEN
    ),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(
        BT_DATA_UUID128_ALL,
        BT_UUID_NUS_VAL
    ),
};

/*
 * --------------------------------------------------------------------------
 * ADC definitions
 * --------------------------------------------------------------------------
 */

static const struct adc_dt_spec adc_channels[NUM_ADC_CHANNELS] = {
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 0),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 1),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 2),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 3),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 4),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 5),
};

static int16_t adc_sample_buffer[NUM_ADC_CHANNELS];

/*
 * --------------------------------------------------------------------------
 * IMU definition
 * --------------------------------------------------------------------------
 */

static const struct device *const imu_dev =
    DEVICE_DT_GET_ONE(st_lsm6dsl);

/*
 * --------------------------------------------------------------------------
 * BLE callbacks
 * --------------------------------------------------------------------------
 */

static void restart_advertising_work_handler(struct k_work *work)
{
    int err;

    ARG_UNUSED(work);

    err = bt_le_adv_start(
        BT_LE_ADV_CONN_FAST_2,
        ad,
        ARRAY_SIZE(ad),
        sd,
        ARRAY_SIZE(sd)
    );

    if (err) {
        printk("Advertising restart failed: %d\n", err);
    } else {
        printk("Advertising restarted\n");
    }
}

K_WORK_DELAYABLE_DEFINE(
    restart_advertising_work,
    restart_advertising_work_handler
);

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("BLE connection failed, error %u\n", err);
        return;
    }

    /*
     * Defensive cleanup in case a previous reference still exists.
     */
    if (current_conn != NULL) {
        bt_conn_unref(current_conn);
    }

    current_conn = bt_conn_ref(conn);
    ble_connected = true;
    header_sent = false;
    rows_since_header = 0U;

    printk("BLE connected\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);

    ble_connected = false;
    header_sent = false;
    rows_since_header = 0U;

    printk("BLE disconnected, reason %u\n", reason);

    if (current_conn != NULL) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    /*
     * Restart advertising so the board becomes discoverable again.
     */
    k_work_schedule(
        &restart_advertising_work,
        K_MSEC(500)
    );
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/*
 * --------------------------------------------------------------------------
 * ADC initialization
 * --------------------------------------------------------------------------
 */

static int initialize_adc(struct adc_sequence *sequence)
{
    int err;

    for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
        if (!adc_is_ready_dt(&adc_channels[i])) {
            printk(
                "ADC device for channel %u is not ready\n",
                (unsigned int)i
            );
            return -ENODEV;
        }

        /*
         * All six logical channels must use the same ADC peripheral.
         */
        if (adc_channels[i].dev != adc_channels[0].dev) {
            printk(
                "ADC channel %u uses a different ADC device\n",
                (unsigned int)i
            );
            return -EINVAL;
        }

        err = adc_channel_setup_dt(&adc_channels[i]);
        if (err < 0) {
            printk(
                "Could not configure ADC channel %u, error %d\n",
                (unsigned int)i,
                err
            );
            return err;
        }

        sequence->channels |= BIT(adc_channels[i].channel_id);
    }

    printk("Six ADC channels initialized\n");

    return 0;
}

/*
 * --------------------------------------------------------------------------
 * IMU initialization
 * --------------------------------------------------------------------------
 */

static int initialize_imu(void)
{
    int err;

    struct sensor_value odr = {
        .val1 = IMU_ODR_HZ,
        .val2 = 0,
    };

    if (!device_is_ready(imu_dev)) {
        printk("IMU device is not ready\n");
        return -ENODEV;
    }

    err = sensor_attr_set(
        imu_dev,
        SENSOR_CHAN_ACCEL_XYZ,
        SENSOR_ATTR_SAMPLING_FREQUENCY,
        &odr
    );

    if (err < 0) {
        printk(
            "Cannot set accelerometer ODR, error %d\n",
            err
        );
        return err;
    }

    err = sensor_attr_set(
        imu_dev,
        SENSOR_CHAN_GYRO_XYZ,
        SENSOR_ATTR_SAMPLING_FREQUENCY,
        &odr
    );

    if (err < 0) {
        printk(
            "Cannot set gyroscope ODR, error %d\n",
            err
        );
        return err;
    }

    printk("IMU initialized at %u Hz\n", IMU_ODR_HZ);

    return 0;
}

/*
 * --------------------------------------------------------------------------
 * BLE initialization
 * --------------------------------------------------------------------------
 */

static int initialize_ble(void)
{
    int err;

    err = bt_enable(NULL);
    if (err < 0) {
        printk("Bluetooth initialization failed: %d\n", err);
        return err;
    }

    printk("Bluetooth initialized\n");

    err = bt_nus_init(NULL);
    if (err < 0) {
        printk(
            "Nordic UART Service initialization failed: %d\n",
            err
        );
        return err;
    }

    err = bt_le_adv_start(
        BT_LE_ADV_CONN_FAST_2,
        ad,
        ARRAY_SIZE(ad),
        sd,
        ARRAY_SIZE(sd)
    );

    if (err < 0) {
        printk("Advertising failed to start: %d\n", err);
        return err;
    }

    printk("Advertising as %s\n", DEVICE_NAME);

    return 0;
}

/*
 * --------------------------------------------------------------------------
 * Send one CSV row over BLE
 * --------------------------------------------------------------------------
 */

static void send_csv_over_ble(
    const char *data_line,
    size_t data_length
)
{
    int err;

    if (!ble_connected || current_conn == NULL) {
        return;
    }

    /*
     * Send the header after each connection and periodically afterward.
     */
    if (!header_sent ||
        rows_since_header >= CSV_HEADER_INTERVAL_ROWS) {

        err = bt_nus_send(
            current_conn,
            csv_header,
            strlen(csv_header)
        );

        if (err < 0) {
            printk("Header send failed: %d\n", err);
            return;
        }

        header_sent = true;
        rows_since_header = 0U;
    }

    err = bt_nus_send(
        current_conn,
        data_line,
        data_length
    );

    if (err < 0) {
        printk("BLE send failed: %d\n", err);
        return;
    }

    rows_since_header++;
}

/*
 * --------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------
 */

int main(void)
{
    int err;
    uint32_t start_time_ms;

    struct adc_sequence adc_sequence = {
        .channels = 0U,
        .buffer = adc_sample_buffer,
        .buffer_size = sizeof(adc_sample_buffer),
        .resolution = 12,
        .oversampling = 0,
        .calibrate = false,
        .options = NULL,
    };

    printk("Starting BLE + ADC + IMU application\n");

    err = initialize_adc(&adc_sequence);
    if (err < 0) {
        return 0;
    }

    err = initialize_imu();
    if (err < 0) {
        return 0;
    }

    err = initialize_ble();
    if (err < 0) {
        return 0;
    }

    k_sleep(K_SECONDS(2));

    start_time_ms = k_uptime_get_32();

    while (1) {
        char data_line[256];

        uint32_t timestamp_ms;

        struct sensor_value accel[3];
        struct sensor_value gyro[3];

        int64_t ax;
        int64_t ay;
        int64_t az;

        int64_t gx;
        int64_t gy;
        int64_t gz;

        int line_length;

        /*
         * Timestamp represents elapsed time since sampling started.
         */
        timestamp_ms =
            k_uptime_get_32() - start_time_ms;

        /*
         * Read all six ADC channels.
         */
        err = adc_read(
            adc_channels[0].dev,
            &adc_sequence
        );

        if (err < 0) {
            printk(
                "# ERROR: ADC read failed, error %d\n",
                err
            );

            k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
            continue;
        }

        /*
         * Fetch the latest accelerometer and gyroscope sample.
         */
        err = sensor_sample_fetch(imu_dev);
        if (err < 0) {
            printk(
                "# ERROR: IMU sample fetch failed, error %d\n",
                err
            );

            k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
            continue;
        }

        err = sensor_channel_get(
            imu_dev,
            SENSOR_CHAN_ACCEL_XYZ,
            accel
        );

        if (err < 0) {
            printk(
                "# ERROR: Accelerometer read failed, error %d\n",
                err
            );

            k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
            continue;
        }

        err = sensor_channel_get(
            imu_dev,
            SENSOR_CHAN_GYRO_XYZ,
            gyro
        );

        if (err < 0) {
            printk(
                "# ERROR: Gyroscope read failed, error %d\n",
                err
            );

            k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
            continue;
        }

        /*
         * Convert Zephyr sensor_value objects to integer micro-units.
         */
        ax = sensor_value_to_micro(&accel[0]);
        ay = sensor_value_to_micro(&accel[1]);
        az = sensor_value_to_micro(&accel[2]);

        gx = sensor_value_to_micro(&gyro[0]);
        gy = sensor_value_to_micro(&gyro[1]);
        gz = sensor_value_to_micro(&gyro[2]);

        /*
         * Create one synchronized ADC + IMU CSV record.
         */
        line_length = snprintf(
            data_line,
            sizeof(data_line),
            "%" PRIu32 ","
            "%d,%d,%d,%d,%d,%d,"
            "%" PRId64 ",%" PRId64 ",%" PRId64 ","
            "%" PRId64 ",%" PRId64 ",%" PRId64 "\n",
            timestamp_ms,
            (int)adc_sample_buffer[0],
            (int)adc_sample_buffer[1],
            (int)adc_sample_buffer[2],
            (int)adc_sample_buffer[3],
            (int)adc_sample_buffer[4],
            (int)adc_sample_buffer[5],
            ax,
            ay,
            az,
            gx,
            gy,
            gz
        );

        if (line_length < 0 ||
            line_length >= (int)sizeof(data_line)) {

            printk(
                "# ERROR: CSV row formatting failed or was truncated\n"
            );

            k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
            continue;
        }

        /*
        * Repeat the header periodically on the serial console.
        * Initializing the counter to CSV_HEADER_INTERVAL_ROWS causes
        * the header to be printed immediately before the first row.
        */
        if (serial_rows_since_header >= CSV_HEADER_INTERVAL_ROWS) {
            printk("%s", csv_header);
            serial_rows_since_header = 0U;
        }

        printk("%s", data_line);
        serial_rows_since_header++;

        /*
         * Send the same row over BLE when connected.
         */
        send_csv_over_ble(
            data_line,
            (size_t)line_length
        );

        k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
    }

    return 0;
}