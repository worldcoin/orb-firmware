CAN bus
======================

.. _can_speed:

Bus Speed
-----------

The bus speed can be pushed to accept up to 1Mbits/s for CAN and 8Mbits/s for FDCAN.

.. code-block:: devicetree

    &can2
    {
        [...]
    	bus-speed = <1000000>;
    	bus-speed-data = <2000000>;
    }

Tests
---------

