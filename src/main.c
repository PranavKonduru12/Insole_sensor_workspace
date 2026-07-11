#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "No led0 alias found in devicetree"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static void uart_print(const struct device *dev, const char *msg)
{
	while (*msg != '\0') {
		uart_poll_out(dev, *msg);
		msg++;
	}
}

int main(void)
{
	const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

	k_sleep(K_SECONDS(3));

	while (1) {
		gpio_pin_toggle_dt(&led);

		if (device_is_ready(uart_dev)) {
			uart_print(uart_dev, "LED toggled. Main is running.\r\n");
		}

		k_sleep(K_SECONDS(1));
	}

	return 0;
}