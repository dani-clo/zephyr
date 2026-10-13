.. zephyr:code-sample:: dhcpv4-client
   :name: DHCPv4 client
   :relevant-api: dhcpv4 net_mgmt

   Start a DHCPv4 client to obtain an IPv4 address from a DHCPv4 server.

Overview
********

This application starts a DHCPv4 client, gets an IPv4 address from the
DHCPv4 server, and prints address, lease time, netmask and router
information to a serial console.

Requirements
************

- :ref:`networking_with_host`

Building and Running
********************

Validating deferred Ethernet initialization on Portenta H7
========================================================

Use the M7 target, an Ethernet connector/carrier connected to a network with a
DHCP server, and a 3.3 V UART adapter on USART1 (PA9 TX, PA10 RX, common ground).
The validation overlay selects USART1 for the console and shell at 115200 baud,
8N1. The PHY uses the board's 25 MHz crystal; no PWM reference clock is needed.
These builds use the Zephyr board configuration, not the Arduino core overlay.

Build the normal initialization variant from the Zephyr repository root:

.. code-block:: console

   west build -b arduino_portenta_h7/stm32h747xx/m7 \
     -d build/portenta-h7-eth-normal samples/net/dhcpv4_client -- \
     -DEXTRA_CONF_FILE=boards/portenta_h7_validation.conf \
     -DDTC_OVERLAY_FILE=boards/portenta_h7_validation.overlay

Build the deferred variant using the same configuration plus a second overlay:

.. code-block:: console

   west build -b arduino_portenta_h7/stm32h747xx/m7 \
     -d build/portenta-h7-eth-deferred samples/net/dhcpv4_client -- \
     -DEXTRA_CONF_FILE=boards/portenta_h7_validation.conf \
     '-DDTC_OVERLAY_FILE=boards/portenta_h7_validation.overlay;boards/portenta_h7_deferred.overlay'

Double-tap reset to enter the Arduino bootloader, then flash one variant:

.. code-block:: console

   west flash -d build/portenta-h7-eth-normal

Repeat for ``build/portenta-h7-eth-deferred``. The board revision defaults to
``1.0.0``; select the appropriate revision in the board target if needed.

The STM32-specific preparation in this sample prints device readiness before
and after initialization. In the normal build, MDIO, PHY and MAC should already
report ``ready=1`` when ``main()`` starts. In the deferred build they should all
report ``ready=0`` first. The sample waits five seconds, initializes MDIO, PHY
and MAC in that order, checks each return value, and explicitly calls
``net_if_up()`` before starting DHCP. ``device_init()`` does not initialize
dependencies or bring the network interface up automatically.

For both builds, check that:

* No HAL initialization errors, PHY readiness errors or assertions occur.
* The PHY reports link up and DHCP prints an address, netmask and router.
* ``ping <assigned-address>`` succeeds from a host on the same network.
* The shell's ``net iface`` shows the expected MAC address and interface state.
* Unplugging and reconnecting Ethernet restores the link and ping responses.

Test both with the cable connected before reset and with the cable connected
after initialization. For an additional comparison with only the MAC deferred,
remove the MDIO and PHY properties from a copy of the deferred overlay: those
devices should then report ``ready=1`` before the five-second delay.

The two configurations are also registered as Twister scenarios. To build both:

.. code-block:: console

   ./scripts/twister -T samples/net/dhcpv4_client \
     -p arduino_portenta_h7/stm32h747xx/m7 \
     -s sample.net.dhcpv4_client.portenta_h7.normal \
     -s sample.net.dhcpv4_client.portenta_h7.deferred \
     --build-only -O twister-out-portenta-eth

For runtime validation, run one scenario at a time with the UART adapter and
Ethernet connected, entering the bootloader before each run. For example:

.. code-block:: console

   ./scripts/twister -T samples/net/dhcpv4_client \
     -p arduino_portenta_h7/stm32h747xx/m7 \
     -s sample.net.dhcpv4_client.portenta_h7.deferred \
     --device-testing --device-serial /dev/ttyUSB0 \
     --device-serial-baud 115200 --west-flash \
     -O twister-out-portenta-eth-device

Replace the serial device with the UART adapter's path. The console harness
checks the initial readiness of all three devices, completion of MAC
initialization, and DHCP address assignment. The deferred scenario also checks
the delay marker. These checks require physical hardware and a DHCP server;
``--build-only`` does not validate Ethernet traffic.

Running DHCPv4 client in Linux Host
===================================

These are instructions for how to use this sample application using
QEMU on a Linux host to negotiate IP address from DHCPv4 server (kea) running
on Linux host.

To use QEMU for testing, follow the :ref:`networking_with_qemu` guide.

Here's a sample server configuration file '/etc/kea/kea-dhcp4.conf'
used to configure the DHCPv4 server:

.. code-block:: console

    {
        "Dhcp4": {
            "interfaces-config": {
                "interfaces": [ "tap0" ],
                "dhcp-socket-type": "raw"
            },

            "valid-lifetime": 7200,

            "subnet4": [
                {
                    "id": 1,
                    "subnet": "192.0.2.0/24",
                    "pools": [ { "pool": "192.0.2.10 - 192.0.2.100" } ],
                    "option-data": [
                        {
                            "name": "routers",
                            "data": "192.0.2.2"
                        },
                        {
                            "name": "domain-name-servers",
                            "data": "8.8.8.8"
                        }
                    ]
                }
            ]
        }
    }

Use another terminal window to start up a DHCPv4 server on the Linux host,
using this conf file:

.. code-block:: console

    $ sudo kea-dhcp4 -c /etc/kea/kea-dhcp4.conf

Run Zephyr samples/net/dhcpv4_client application in QEMU:

.. zephyr-app-commands::
   :zephyr-app: samples/net/dhcpv4_client
   :host-os: unix
   :board: qemu_x86
   :goals: run
   :compact:

Once DHCPv4 client address negotiation completed with server, details
are shown like this:

.. code-block:: console

    [00:00:00.000,000] <inf> net_dhcpv4_client_sample: Run dhcpv4 client
    [00:00:00.000,000] <inf> net_dhcpv4_client_sample: Start on slip: index=1
    [00:00:07.080,000] <inf> net_dhcpv4: Received: 192.0.2.10
    [00:00:07.080,000] <inf> net_dhcpv4_client_sample:    Address[1]: 192.0.2.10
    [00:00:07.080,000] <inf> net_dhcpv4_client_sample:     Subnet[1]: 255.255.255.0
    [00:00:07.080,000] <inf> net_dhcpv4_client_sample:     Router[1]: 192.0.2.2
    [00:00:07.080,000] <inf> net_dhcpv4_client_sample: Lease time[1]: 7200 seconds

To verify the Zephyr application client is running and has received
an ip address by typing:

.. code-block:: console

    $ ping -I tap0 192.0.2.10


FRDM_K64F
=========

These are instructions for how to use this sample application running on
:zephyr:board:`frdm_k64f` board to negotiate IP address from DHCPv4 server (kea) running
on Linux host.

Connect ethernet cable from :zephyr:board:`Freedom-K64F board <frdm_k64f>` to Linux host
machine and check for new interfaces:

.. code-block:: console

    $ ifconfig

Add ip address and routing information to interface:

.. code-block:: console

    $ sudo ip addr add 192.0.2.2 dev eth1
    $ sudo ip route add 192.0.2.0/24 dev eth1

Here's a sample server configuration file '/etc/kea/kea-dhcp4.conf'
used to configure the DHCPv4 server:

.. code-block:: console

    {
        "Dhcp4": {
            "interfaces-config": {
                "interfaces": [ "eth1" ],
                "dhcp-socket-type": "raw"
            },

            "valid-lifetime": 7200,

            "subnet4": [
                {
                    "id": 1,
                    "subnet": "192.0.2.0/24",
                    "pools": [ { "pool": "192.0.2.10 - 192.0.2.100" } ],
                    "option-data": [
                        {
                            "name": "routers",
                            "data": "192.0.2.2"
                        },
                        {
                            "name": "domain-name-servers",
                            "data": "8.8.8.8"
                        }
                    ]
                }
            ]
        }
    }

Use another terminal window to start up a DHCPv4 server on the Linux host,
using this conf file:

.. code-block:: console

    $ sudo kea-dhcp4 -c /etc/kea/kea-dhcp4.conf

Build Zephyr samples/net/dhcpv4_client application:

.. zephyr-app-commands::
   :zephyr-app: samples/net/dhcpv4_client
   :host-os: unix
   :board: frdm_k64f
   :goals: build flash
   :compact:

Once DHCPv4 client address negotiation completed with server, details
are shown like this:

.. code-block:: console

    $ sudo screen /dev/ttyACM0 115200
    [00:00:00.000,000] <inf> net_dhcpv4_client_sample: Run dhcpv4 client
    [00:00:00.000,000] <inf> net_dhcpv4_client_sample: Start on ethernet: index=1
    [00:00:07.080,000] <inf> net_dhcpv4: Received: 192.0.2.10
    [00:00:07.080,000] <inf> net_dhcpv4_client_sample:    Address[1]: 192.0.2.10
    [00:00:07.080,000] <inf> net_dhcpv4_client_sample:     Subnet[1]: 255.255.255.0
    [00:00:07.080,000] <inf> net_dhcpv4_client_sample:     Router[1]: 192.0.2.2
    [00:00:07.080,000] <inf> net_dhcpv4_client_sample: Lease time[1]: 7200 seconds

To verify the Zephyr application client is running and has received
an ip address by typing:

.. code-block:: console

    $ ping -I eth1 192.0.2.10


Arm FVP
========

* :zephyr:board:`fvp_baser_aemv8r`
* :zephyr:board:`fvp_base_revc_2xaem`

This sample application running on Arm FVP board can negotiate IP
address from DHCPv4 server running on Arm FVP, so there is no extra
configuration that needed to do. It can be built and run directly.

Build Zephyr samples/net/dhcpv4_client application:

.. zephyr-app-commands::
   :zephyr-app: samples/net/dhcpv4_client
   :host-os: unix
   :board: fvp_baser_aemv8r
   :goals: build run
   :compact:

Once DHCPv4 client address negotiation completed with server, details
are shown like this:

.. code-block:: console

    uart:~$
    [00:00:00.060,000] <inf> phy_mii: PHY (0) ID 16F840

    [00:00:00.170,000] <inf> phy_mii: PHY (0) Link speed 10 Mb, half duplex

    [00:00:00.170,000] <inf> eth_smsc91x: MAC 00:02:f7:ef:37:16
    *** Booting Zephyr OS build zephyr-v3.2.0-4300-g3e6505dba29e ***
    [00:00:00.170,000] <inf> net_dhcpv4_client_sample: Run dhcpv4 client
    [00:00:00.180,000] <inf> net_dhcpv4_client_sample: Start on ethernet@9a000000: index=1
    [00:00:07.180,000] <inf> net_dhcpv4: Received: 172.20.51.1
    [00:00:07.180,000] <inf> net_dhcpv4_client_sample:    Address[1]: 172.20.51.1
    [00:00:07.180,000] <inf> net_dhcpv4_client_sample:     Subnet[1]: 255.255.255.0
    [00:00:07.180,000] <inf> net_dhcpv4_client_sample:     Router[1]: 172.20.51.254
    [00:00:07.180,000] <inf> net_dhcpv4_client_sample: Lease time[1]: 86400 seconds
