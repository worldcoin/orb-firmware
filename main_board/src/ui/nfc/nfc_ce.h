#pragma once

#include "platform.h"

void
nfc_ce_init(uint8_t *nfcfNfcid);

uint16_t
nfc_ce_t3t(uint8_t *rxData, uint16_t rxDataLen, uint8_t *txBuf,
           uint16_t txBufLen);

uint16_t
nfc_ce_t4t(uint8_t *rxData, uint16_t rxDataLen, uint8_t *txBuf,
           uint16_t txBufLen);
