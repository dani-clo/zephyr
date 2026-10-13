/* Networking DHCPv4 client */

/*
 * Copyright (c) 2017 ARM Ltd.
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_dhcpv4_client_sample, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/linker/sections.h>
#include <errno.h>
#include <stdio.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>

#if DT_HAS_COMPAT_STATUS_OKAY(st_stm32_ethernet)
#define STM32_ETH_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(st_stm32_ethernet)
#define STM32_PHY_NODE DT_PHANDLE(STM32_ETH_NODE, phy_handle)

static int prepare_stm32_ethernet(void)
{
	const struct device *const devices[] = {
		DEVICE_DT_GET(DT_BUS(STM32_PHY_NODE)),
		DEVICE_DT_GET(STM32_PHY_NODE),
		DEVICE_DT_GET(STM32_ETH_NODE),
	};
	struct net_if *iface;
	int ret;

	for (size_t i = 0; i < ARRAY_SIZE(devices); i++) {
		LOG_INF("Before init: %s ready=%d", devices[i]->name,
			device_is_ready(devices[i]));
	}

	if (DT_PROP_OR(STM32_ETH_NODE, zephyr_deferred_init, false)) {
		LOG_INF("Waiting 5 seconds before Ethernet device initialization");
		k_sleep(K_SECONDS(5));
	}

	/* device_init() does not initialize dependencies automatically. */
	for (size_t i = 0; i < ARRAY_SIZE(devices); i++) {
		if (!device_is_ready(devices[i])) {
			ret = device_init(devices[i]);
			if (ret < 0) {
				LOG_ERR("Cannot initialize %s: %d", devices[i]->name, ret);
				return ret;
			}
		}
		LOG_INF("After init: %s ready=%d", devices[i]->name,
			device_is_ready(devices[i]));
	}

	iface = net_if_lookup_by_dev(devices[ARRAY_SIZE(devices) - 1]);
	if (iface == NULL) {
		return -ENODEV;
	}

	/* A deferred interface is not automatically brought up after device_init(). */
	ret = net_if_up(iface);
	return ret == -EALREADY ? 0 : ret;
}
#endif

#define DHCP_OPTION_NTP (42)

static uint8_t ntp_server[4];

static struct net_mgmt_event_callback mgmt_cb;

static struct net_dhcpv4_option_callback dhcp_cb;

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("Start on %s: index=%d", net_if_get_device(iface)->name,
		net_if_get_by_iface(iface));
	net_dhcpv4_start(iface);
}

static void handler(struct net_mgmt_event_callback *cb,
		    uint64_t mgmt_event,
		    struct net_if *iface)
{
	int i = 0;

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type !=
							NET_ADDR_DHCP) {
			continue;
		}

		LOG_INF("   Address[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(NET_AF_INET,
			    &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
						  buf, sizeof(buf)));
		LOG_INF("    Subnet[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(NET_AF_INET,
				       &iface->config.ip.ipv4->unicast[i].netmask,
				       buf, sizeof(buf)));
		LOG_INF("    Router[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(NET_AF_INET,
						 &iface->config.ip.ipv4->gw,
						 buf, sizeof(buf)));
		LOG_INF("Lease time[%d]: %u seconds", net_if_get_by_iface(iface),
			iface->config.dhcpv4.lease_time);
	}
}

static void option_handler(struct net_dhcpv4_option_callback *cb,
			   size_t length,
			   enum net_dhcpv4_msg_type msg_type,
			   struct net_if *iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	LOG_INF("DHCP Option %d: %s", cb->option,
		net_addr_ntop(NET_AF_INET, cb->data, buf, sizeof(buf)));
}

int main(void)
{
	LOG_INF("Run dhcpv4 client");

#if DT_HAS_COMPAT_STATUS_OKAY(st_stm32_ethernet)
	int ret = prepare_stm32_ethernet();

	if (ret < 0) {
		LOG_ERR("Ethernet preparation failed: %d", ret);
		return ret;
	}
#endif

	net_mgmt_init_event_callback(&mgmt_cb, handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);

	net_dhcpv4_init_option_callback(&dhcp_cb, option_handler,
					DHCP_OPTION_NTP, ntp_server,
					sizeof(ntp_server));

	net_dhcpv4_add_option_callback(&dhcp_cb);

	net_if_foreach(start_dhcpv4_client, NULL);
	return 0;
}
