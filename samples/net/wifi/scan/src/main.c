/*
 * Copyright (c) 2026 Arduino
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

static struct net_mgmt_event_callback scan_cb;
static struct net_if *scan_iface;
static uint32_t scan_result_count;

static void scan_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	ARG_UNUSED(iface);

	switch (mgmt_event) {
	case NET_EVENT_WIFI_SCAN_RESULT: {
		const struct wifi_scan_result *entry =
			(const struct wifi_scan_result *)cb->info;

		scan_result_count++;
		printk("%u: SSID: %-32.*s CH: %2u RSSI: %4d Security: %s\n",
		       scan_result_count, entry->ssid_length, entry->ssid, entry->channel,
		       entry->rssi, wifi_security_txt(entry->security));
		break;
	}
	case NET_EVENT_WIFI_SCAN_DONE:
		printk("Wi-Fi scan complete\n");
		break;
	default:
		break;
	}
}

int main(void)
{
	int ret;

	printk("Minimal Wi-Fi scan sample\n");

	scan_iface = net_if_get_wifi_sta();
	if (scan_iface == NULL) {
		printk("No Wi-Fi STA interface available\n");
		return 0;
	}

	if (!net_if_is_up(scan_iface)) {
		net_if_up(scan_iface);
	}

	net_mgmt_init_event_callback(&scan_cb, scan_event_handler,
				     NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE);
	net_mgmt_add_event_callback(&scan_cb);

	printk("Starting Wi-Fi scan...\n");
	ret = net_mgmt(NET_REQUEST_WIFI_SCAN, scan_iface, NULL, 0);
	if (ret < 0) {
		printk("Wi-Fi scan request failed: %d\n", ret);
		net_mgmt_del_event_callback(&scan_cb);
		return 0;
	}

	ret = net_mgmt_event_wait_on_iface(scan_iface, NET_EVENT_WIFI_SCAN_DONE, NULL, NULL, NULL,
					   K_SECONDS(15));
	if (ret == -ETIMEDOUT) {
		printk("Wi-Fi scan timed out\n");
	} else if (ret < 0) {
		printk("Wi-Fi scan wait failed: %d\n", ret);
	}

	net_mgmt_del_event_callback(&scan_cb);
	printk("Found %u network(s)\n", scan_result_count);
	return 0;
}
