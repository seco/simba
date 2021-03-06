Socket devices
==============

The Linux socket device drivers implementation allows an external
program to simulate the hardware. The external program communicates
with the Simba application using TCP sockets, one socket for each
device.

The Python script
:github-blob:`socket_device.py<bin/socket_device.py>` can be used to
monitor and send data to a device.

Arduino Mega example
--------------------

In this example :github-blob:`socket_device.py<bin/socket_device.py>`
is the hardware simulator (to the left in the image below), and
:github-tree:`socket_device<examples/socket_device>` is the Simba
application (to the right in the image below). The five horizontal
lines each represents input and output of one device.

.. image:: ../images/socket-devices.jpg
   :width: 80%
   :align: center
   :target: ../_images/socket-devices.jpg

First build and run the linux application with the Arduino Mega
pinout...

.. code-block:: text

   $ make BOARD=linux PINOUT=arduino_mega run

...and then, in a second terminal, monitor digital pin 2, ``d2``.

.. code-block:: text

   > socket_device.py pin d2
   Connecting to localhost:47000... done.
   Requesting pin device d2... done.
   $
   14:48:10.004512 pin(d2) RX: high
   14:48:52.535323 pin(d2) RX: high
   14:49:20.123124 pin(d2) RX: low

Alternatively, monitor all devices at the same time with the monitor
make target.

.. code-block:: text

   $ make BOARD=linux PINOUT=arduino_mega monitor
   socket_device.py monitor
   Connecting to localhost:47000... done.
   Requesting uart device 0... done.
   ...
   Connecting to localhost:47000... done.
   Requesting pin device 2... done.
   Connecting to localhost:47000... done.
   Requesting pin device 4... done.
   ...
   $
   14:51:50.531761 pin(2) RX: low
   14:51:50.541784 uart(0) RX: b'\n'
   14:51:51.178744 pin(4) RX: high

Python modules
--------------

There are two Python modules in the folder
:github-tree:`bin/socket_device<bin/socket_device>` in the Simba
repository. Both modules implements the same interface as the default
Python module/package with the same name, and can be used to
communicate over a socket device instead of using the hardware.

- :github-blob:`serial.py<bin/socket_device/serial.py>` implements the
  `pyserial`_ interface.

- :github-blob:`can.py<bin/socket_device/can.py>` implements the
  `python-can`_ interface.

Use the environment variable ``PYTHONPATH`` to import the socket
device modules instead of the default modules/packages.

.. code-block:: text

   > export PYTHONPATH=$(readlink -f ${SIMBA_ROOT}/bin)
   > export PYTHONPATH=${PYTHONPATH}:$(readlink -f ${SIMBA_ROOT}/bin/socket_device)
   > bpython3
   >>> import serial
   >>> serial
   <module 'serial' from '/home/erik/workspace/simba/bin/socket_device/serial.py'>
   >>> import can
   >>> can
   <module 'can' from '/home/erik/workspace/simba/bin/socket_device/can.py'>
   >>>

Protocol
--------

At startup the Simba application creates a socket and starts listening
for clients on TCP port 47000.

Devices
~~~~~~~

These drivers supports the socket device protocol at the moment. More
to be added when needed.

Uart
^^^^

The UART socket is equivalent to a serial port, it streams data to and
from the application.

Pin
^^^

Sends ``high`` or ``low`` when written to given device. Input is not
supported yet.

Pwm
^^^

Sends ``frequency=<value>`` and ``duty_cycle=<value>`` when set on
given device.

Can
^^^

Sends and receives frames on the format
``id=<id>,extended=<extended>,size=<size>,data=<data>``. ``<id>`` and
``<data>`` are hexadecimal numbers not prefixed with ``0x``. ``size``
and ``<extended>`` is a decimal integers.

.. code-block:: text

   > socket_device.py can 0
   Connecting to localhost:47000... done.
   Requesting can device 0... done.
   $ id=00000005,extended=1,size=2,data=0011<Enter>
   14:57:22.344321 can(0) TX: id=00000005,extended=1,size=2,data=0011
   14:57:22.346321 can(0) RX: id=00000006,extended=1,size=2,data=0112

I2c
^^^

Sends and receives data on the format
``address=<address>,size=<size>,data=<data>``. ``<address>`` is an
decimal integer, while ``<size>`` and ``<data>`` is a hexadecimal
numbers.

.. code-block:: text

   > socket_device.py i2c 0
   Connecting to localhost:47000... done.
   Requesting i2c device 0... done.
   $ 
   14:57:22.346321 i2c(0) RX: address=0006,size=0003,data=1a2b3c

Device request message
~~~~~~~~~~~~~~~~~~~~~~

This message is sent to the Simba application to request a device.

.. code-block:: text

   +---------+---------+----------------+
   | 4b type | 4b size | <size>b device |
   +---------+---------+----------------+

   `device` is the device name as a string without NULL termination.

   TYPE  SIZE  DESCRIPTION
   --------------------------------------
      1     n  Uart device request.
      3     n  Pin device request.
      5     n  Pwm device request.
      7     n  Can device request.
      9     n  I2c device request.
     11     n  Spi device request.

Device response message
~~~~~~~~~~~~~~~~~~~~~~~

This message is the response to the request message.

.. code-block:: text

   +---------+---------+-----------+
   | 4b type | 4b size | 4b result |
   +---------+---------+-----------+

   `result` is zero(0) on success, and otherwise a negative error
   code.

   Defined error codes are:

      ENODEV(19): No device found matching requested device name.

      EADDRINUSE(98): The requested device is already requested and in
                      use.

   TYPE  SIZE  DESCRIPTION
   --------------------------------------
      2     4  Uart device response.
      4     4  Pin device response.
      6     4  Pwm device response.
      8     4  Can device response.
     10     4  I2c device response.
     12     4  Spi device response.

.. _pyserial: https://pythonhosted.org/pyserial

.. _python-can: https://python-can.readthedocs.io
