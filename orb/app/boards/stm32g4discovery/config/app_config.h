//
// Created by Cyril on 08/07/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_STM32F3DISCOVERY_CONFIG_APP_CONFIG_H
#define WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_STM32F3DISCOVERY_CONFIG_APP_CONFIG_H

#ifndef DEBUG_UART_TX_BUFFER_SIZE
#define DEBUG_UART_TX_BUFFER_SIZE   ((uint16_t) 512)
#endif

#define WATCHDOG_TIMEOUT_MS         200 //<! Watchdog reloaded from Idle task, so this is the amount of time without going back to idle task

#define COM_RX_BUFFER_SIZE          256 //<! UART RX buffer size in bytes
#define COM_TX_BUFFER_SIZE          256 //<! UART TX buffer size in bytes

#define SERIALIZER_QUEUE_SIZE       8
#define DESERIALIZER_QUEUE_SIZE     8

#define PROTOBUF_DATA_MAX_SIZE      128 //<! Maximum size for protobuf-encoded data

#define ACCEL_FIFO_SAMPLES_COUNT    16

#endif //WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_STM32F3DISCOVERY_CONFIG_APP_CONFIG_H
