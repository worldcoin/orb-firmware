CAN bus
======================

.. _can_speed:

Specifications
----------------

The bus speed can be pushed to accept up to 1Mbits/s for CAN and 8Mbits/s for FDCAN.

We are currently using the settings below:

.. code-block:: text

    CAN bus-speed: 1000000 bauds
    CAN FD bus-speed = 2000000 bauds

Addressing
~~~~~~~~~~~~~

.. code-block:: text

    MCU CAN address: 0x01
    Jetson CAN address: 0x80

ISO-TP
----------

.. code-block:: text

    Block-size: 8
    Separation time: 0


Tests
---------
