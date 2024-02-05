#pragma once

/**
 * Initialize CAN TX handling
 * @retval RET_ERROR_NOT_FOUND if CAN device not found (no definition in device
 * tree)
 * @retval RET_SUCCESS on success
 */
ret_code_t
canbus_tx_init(void);

/**
 * Initialize CAN ISO-TP TX handling
 * @retval RET_ERROR_NOT_FOUND if CAN device not found (no definition in device
 * tree)
 * @retval RET_SUCCESS on success
 */
ret_code_t
canbus_isotp_tx_init(void);
