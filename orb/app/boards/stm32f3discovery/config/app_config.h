//
// Created by Cyril on 08/07/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_STM32F3DISCOVERY_CONFIG_APP_CONFIG_H
#define WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_STM32F3DISCOVERY_CONFIG_APP_CONFIG_H

#define WATCHDOG_TIMEOUT_MS         200 //<! Watchdog reloaded from Idle task, so this is the amount of time without going back to idle task

#define COM_RX_BUFFER_SIZE          256 //<! UART RX buffer size in bytes
#define COM_TX_BUFFER_SIZE          256 //<! UART TX buffer size in bytes

#define SERIALIZER_QUEUE_SIZE       8
#define DESERIALIZER_QUEUE_SIZE     8

#define ACCEL_FIFO_SAMPLES_COUNT    16

#endif //WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_STM32F3DISCOVERY_CONFIG_APP_CONFIG_H
