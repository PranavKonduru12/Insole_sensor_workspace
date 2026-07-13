#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/services/nus.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include <string.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static bool ble_connected = false;
static struct bt_conn *current_conn = NULL;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

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
    printk("BLE connected\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    ble_connected = false;
    printk("BLE disconnected, reason %u\n", reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    /*
     * Restart advertising shortly after disconnect so the BLE stack has
     * time to clean up the previous connection before advertising again.
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
    uint32_t counter = 0;

    printk("Starting Adafruit Feather nRF52840 Sense BLE UART test\n");

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

    while (1) {
        char data_line[64];

        snprintf(data_line, sizeof(data_line),
                 "%u,%u\n",
                 k_uptime_get_32(),
                 counter++);

        printk("%s", data_line);

        if (ble_connected && current_conn) {
            err = bt_nus_send(current_conn,
                              data_line,
                              strlen(data_line));
            if (err) {
                printk("BLE send failed: %d\n", err);
            }
        }

        k_sleep(K_MSEC(1000));
    }
}