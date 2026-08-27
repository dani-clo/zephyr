/*
 * Copyright 2025 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sample_usbd.h>

#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/uart_bridge.h>
#include <zephyr/kernel.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cdc_acm_bridge, LOG_LEVEL_INF);

static const struct gpio_dt_spec esp32_boot_gpio =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), boot_gpios);
static const struct gpio_dt_spec esp32_reset_gpio =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), reset_gpios);

struct esp32_download_state {
	bool dtr;
	bool rts;
	bool initialized;
};

static struct esp32_download_state esp32_state;

const struct device *const uart_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

static struct usbd_context *sample_usbd;

#define DEVICE_DT_GET_COMMA(node_id) DEVICE_DT_GET(node_id),

const struct device *uart_bridges[] = {
	DT_FOREACH_STATUS_OKAY(zephyr_uart_bridge, DEVICE_DT_GET_COMMA)
};

static void esp32_apply_boot_reset_state(bool dtr, bool rts)
{
	/* ESP32-C3 GPIO0 and EN are active-low.  The USB CDC DTR/RTS state is
	 * interpreted as the reset/download sequence used by esptool:
	 *  - DTR=0, RTS=1 -> chip in reset (EN low, GPIO0 high)
	 *  - DTR=1, RTS=0 -> ROM download mode (GPIO0 low, EN high)
	 *  - DTR=0, RTS=0 -> idle / released
	 *
	 * The logical values passed to gpio_pin_set_dt() are the active-low values
	 * required by the board's GPIO_ACTIVE_LOW strap pins.
	 */
	bool boot_active = dtr;
	bool reset_active = rts;

	gpio_pin_set_dt(&esp32_boot_gpio, boot_active ? 1 : 0);
	gpio_pin_set_dt(&esp32_reset_gpio, reset_active ? 1 : 0);

	LOG_INF("ESP32 boot/reset: DTR=%d RTS=%d boot=%d reset=%d", dtr, rts,
		boot_active ? 1 : 0, reset_active ? 1 : 0);
}

static void esp32_set_boot_and_reset_from_line_state(const struct device *dev)
{
	uint32_t dtr = 0U;
	uint32_t rts = 0U;
	int ret;

	ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
	if (ret != 0) {
		LOG_WRN("Failed to get UART DTR state: %d", ret);
		return;
	}

	ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_RTS, &rts);
	if (ret != 0) {
		LOG_WRN("Failed to get UART RTS state: %d", ret);
		return;
	}

	if (!esp32_state.initialized ||
	    esp32_state.dtr != !!dtr ||
	    esp32_state.rts != !!rts) {
		esp32_state.dtr = !!dtr;
		esp32_state.rts = !!rts;
		esp32_state.initialized = true;

		/* Emulate the relevant esptool reset timing windows on the ESP strap
		 * pins so the ROM bootloader sees the same transitions the host is
		 * requesting.
		 */
		if (!esp32_state.dtr && esp32_state.rts) {
			esp32_apply_boot_reset_state(false, true);
			k_sleep(K_MSEC(100));
		} else if (esp32_state.dtr && !esp32_state.rts) {
			esp32_apply_boot_reset_state(true, false);
			k_sleep(K_MSEC(50));
		} else {
			esp32_apply_boot_reset_state(false, false);
		}
	}
}

static void sample_msg_cb(struct usbd_context *const ctx, const struct usbd_msg *msg)
{
	LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

	if (usbd_can_detect_vbus(ctx)) {
		if (msg->type == USBD_MSG_VBUS_READY) {
			if (usbd_enable(ctx)) {
				LOG_ERR("Failed to enable device support");
			}
		}

		if (msg->type == USBD_MSG_VBUS_REMOVED) {
			if (usbd_disable(ctx)) {
				LOG_ERR("Failed to disable device support");
			}
		}
	}

	if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING ||
	    msg->type == USBD_MSG_CDC_ACM_CONTROL_LINE_STATE) {
		for (uint8_t i = 0; i < ARRAY_SIZE(uart_bridges); i++) {
			/* update all bridges, non valid combinations are
			 * skipped automatically.
			 */
			uart_bridge_settings_update(msg->dev, uart_bridges[i]);
		}
	}

	if (msg->type == USBD_MSG_CDC_ACM_CONTROL_LINE_STATE) {
		esp32_set_boot_and_reset_from_line_state(msg->dev);
	}
}

int main(void)
{
	int err;

	if (!device_is_ready(esp32_boot_gpio.port) || !device_is_ready(esp32_reset_gpio.port)) {
		LOG_ERR("ESP32 boot/reset GPIO ports not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&esp32_boot_gpio, GPIO_OUTPUT_ACTIVE);
	if (err != 0) {
		LOG_ERR("Failed to configure ESP32 boot GPIO: %d", err);
		return err;
	}

	err = gpio_pin_configure_dt(&esp32_reset_gpio, GPIO_OUTPUT_ACTIVE);
	if (err != 0) {
		LOG_ERR("Failed to configure ESP32 reset GPIO: %d", err);
		return err;
	}

	esp32_state.dtr = false;
	esp32_state.rts = false;
	esp32_state.initialized = false;
	gpio_pin_set_dt(&esp32_boot_gpio, 0);
	gpio_pin_set_dt(&esp32_reset_gpio, 0);

	sample_usbd = sample_usbd_init_device(sample_msg_cb);
	if (sample_usbd == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	if (!usbd_can_detect_vbus(sample_usbd)) {
		err = usbd_enable(sample_usbd);
		if (err) {
			LOG_ERR("Failed to enable device support");
			return err;
		}
	}

	LOG_INF("USB device support enabled");

	k_sleep(K_FOREVER);

	return 0;
}
