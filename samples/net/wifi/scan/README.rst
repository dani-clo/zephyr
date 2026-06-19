.. zephyr:code-sample:: wifi_scan
   :name: Wi-Fi scan
   :relevant-api: net_mgmt net_mgmt_event_wait_on_iface wifi_mgmt

   Minimal Wi-Fi scan sample using net_mgmt and net_mgmt_event_wait_on_iface.

Build it from the ArduinoCore-zephyr workspace with::

   west build -p -b arduino_portenta_h7/stm32h747xx/m7 ../zephyr/samples/net/wifi/scan
