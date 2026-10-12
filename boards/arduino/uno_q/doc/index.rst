.. zephyr:board:: arduino_uno_q

Overview
********

The Arduino UNO Q is a development board featuring a Qualcomm QRB2210
processor (Quad core ARM Cortex-A53) and an STM32U585 microcontroller.
The board is designed around the Arduino form factor and is compatible
with traditional Arduino shields and accessories.
This port targets the STM32U585 microcontroller on the board.

Hardware
********

- Qualcomm QRB2210 Processor (Quad core ARM Cortex-A53)
- STM32U585 Microcontroller (ARM Cortex-M33 at 160 MHz)
- 2 Mbyte of Flash memory and 786 Kbytes of RAM
- 2 RGB user LEDs
- One 13x8 LED Matrix
- Internal UART and SPI buses connected to the QRB2210
- Built-in CMSIS-DAP debug adapter (through QRB2210)

Supported Features
==================

.. zephyr:board-supported-hw::

CAN and USB
===========

FDCAN1 uses PB8 (D9) for RX and PB9 (D10) for TX, with PLL1 Q as its
kernel clock source. An external CAN transceiver is required. These pins
cannot be used for GPIO, SPI chip select or PWM while CAN is active.

The STM32 USB full-speed controller uses PA11 (D5, USB D-) and PA12
(D4, USB D+). A USB connection must be wired to these pins.

For :ref:`external_module_cannectivity`, ``counter0`` is a 32-bit TIM2
counter running at 1 MHz. An application overlay can reference it through
the CANnectivity node's ``timestamp-counter`` property and use ``fdcan1``
as its CAN controller.

When integrating with the Arduino sketch loader, its variant overlay must
preserve this CAN pin mapping and TIM2 prescaler, and disable TIM2 PWM.
PWM on D5, D9 and D10 must also be excluded because USB and CAN reserve
these pins. SPI2 must use software chip select because PB9 is CAN TX.

Programming and debugging
*************************

.. zephyr:board-supported-runners::

Debug adapter
=============

The QRB2210 microprocessor can act as an SWD debug adapter for the STM32U585.
This is supported by the ``openocd`` binary available in the board, and its
functionality can be made available to the computer via ``adb`` port forwarding
commands.

This interface is not yet integrated with the ``west flash`` command, but
debugging is supported.

Debugging
=========

Debugging can be done with the usual ``west debug`` command after starting the
debug server on the board. The following commands, run from an USB-connected
computer, allow to debug the :zephyr:code-sample:`blinky` application on the
Uno Q board:

.. code-block:: console

   adb forward tcp:3333 tcp:3333 && adb shell arduino-debug
   # in a different shell
   west build -b arduino_uno_q samples/basic/blinky
   west debug -r openocd

Restoring the Arduino sketch loader
===================================

The Arduino UNO Q comes with a pre-installed application that acts as a loader
for user sketches, and is shipped as part of the Arduino Zephyr cores. If you
overwrite this application, you can restore it later by issuing the following
command from an USB-connected computer:

.. code-block:: console

   adb shell arduino-cli burn-bootloader -b arduino:zephyr:unoq -P jlink

The same ``arduino-cli`` command can also be directly used on the board, when
in standalone mode:

.. code-block:: console

   arduino-cli burn-bootloader -b arduino:zephyr:unoq -P jlink
