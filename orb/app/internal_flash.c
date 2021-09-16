//
// Created by Cyril on 09/09/2021.
//

#include <errors.h>
#include <logging.h>
#include "internal_flash.h"
#include "boards.h"

#define NB_PAGE_SECTOR_PER_ERASE  2U    /*!< Nb page erased per erase */
#define EXTERNAL_FLASH_ADDRESS    0x90000000U

#define GET_PAGE(ADDR)  (((uint32_t) (ADDR) - FLASH_BASE) / FLASH_PAGE_SIZE_128_BITS)

void
HAL_FLASH_EndOfOperationCallback(uint32_t ReturnValue)
{

}

static ret_code_t
clear_error(void)
{
    ret_code_t err_code = RET_ERROR_INTERNAL;

    if (HAL_FLASH_Unlock() == HAL_OK)
    {
        /* Clear all FLASH flags */
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

        /* Unlock the Program memory */
        if (HAL_FLASH_Lock() == HAL_OK)
        {
            err_code = RET_SUCCESS;
        }
        else
        {
            LOG_ERROR("Flash lock failure");
        }
    }
    else
    {
        LOG_ERROR("Flash unlock failure");
    }

    return err_code;
}

ret_code_t
int_flash_erase(void *start_addr, size_t length_bytes)
{
    // clear error flags raised during previous operation
    ret_code_t err_code = clear_error();

    if (err_code == RET_SUCCESS)
    {
        if (HAL_FLASH_Unlock() == HAL_OK)
        {
            uint32_t page_first = GET_PAGE(start_addr);
            uint32_t page_count = GET_PAGE(start_addr + length_bytes - 1) - page_first + 1;

            FLASH_EraseInitTypeDef x_erase_init = {0};
            x_erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
            x_erase_init.Banks = FLASH_BANK_1;
            x_erase_init.Page = page_first;
            x_erase_init.NbPages = page_count;

            if (HAL_FLASHEx_Erase_IT(&x_erase_init) != HAL_OK)
            {
                HAL_FLASH_GetError();
                err_code = RET_ERROR_INTERNAL;
            }
            else
            {
                err_code = RET_SUCCESS;
            }

            // Lock the Flash to disable the flash control register access
            // recommended to protect the FLASH memory against possible unwanted operation
            HAL_FLASH_Lock();
        }
        else
        {
            err_code = RET_ERROR_INVALID_STATE;
        }
    }

    return err_code;
}
