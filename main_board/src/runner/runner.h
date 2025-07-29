#pragma once

#include <can_messaging.h>
#include <errors.h>
#include <main.pb.h>
#include <stdint.h>

/**
 * @brief Get the number of successfully processed jobs
 * @return number of successfully processed jobs
 */
uint32_t
runner_successful_jobs_count(void);

/**
 * Queue new message to be processed, from CAN bus
 *
 * @note The function blocks 5ms if the queue is full.
 *
 * @param msg CAN message
 * @retval RET_SUCCESS if the message is queued
 * @retval RET_ERROR_INVALID_STATE if the runner is not initialized
 * @retval RET_ERROR_BUSY if the queue is still full after 5ms or message
 * decoder is not available after 5ms
 * @retval RET_ERROR_INVALID_PARAM message cannot be decoded
 */
ret_code_t
runner_handle_new_can(can_message_t *msg);

#if CONFIG_ORB_LIB_UART_MESSAGING
#include <uart_messaging.h>

/**
 * Queue new message to be processed, from UART
 *
 * @note The function blocks 5ms if the queue is full.
 *
 * @param msg CAN message
 * @retval RET_SUCCESS if the message is queued
 * @retval RET_ERROR_INVALID_STATE if the runner is not initialized
 * @retval RET_ERROR_BUSY if the queue is still full after 5ms or message
 * decoder is not available after 5ms
 * @retval RET_ERROR_INVALID_PARAM message cannot be decoded
 * @retval RET_ERROR_INVALID_ADDR message is not addressed to this device
 */
ret_code_t
runner_handle_new_uart(uart_message_t *msg);
#endif

ret_code_t
runner_handle_new_cli(const orb_mcu_main_JetsonToMcu *const message);

/**
 * @brief Initialize the runner
 */
void
runner_init(void);
