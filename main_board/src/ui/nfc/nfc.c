#include "nfc.h"
#include "app_config.h"
#include "logs_can.h"
#include "nfc_ce.h"
#include <rfal_nfc.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(nfc, CONFIG_NFC_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(nfc_stack_area, THREAD_STACK_SIZE_NFC);
static struct k_thread nfc_thread_data;

enum nfc_state {
    NFC_NO_INIT,
    NFC_RESTART_DISCOVERY,
    NFC_DISCOVERY,
};

static rfalNfcDiscoverParam nfc_discovery_param = {0};
static uint8_t state = NFC_NO_INIT;
static bool multiple_devices;

#define NFC_DISCOVERY_TIMEOUT_MS 3000

#if RFAL_SUPPORT_CE && RFAL_FEATURE_LISTEN_MODE
#if RFAL_SUPPORT_MODE_LISTEN_NFCA
/* NFC-A CE config */
/* 4-byte UIDs with first byte 0x08 would need random number for the subsequent
 * 3 bytes. 4-byte UIDs with first byte 0x*F are Fixed number, not unique, use
 * for this demo 7-byte UIDs need a manufacturer ID and need to assure
 * uniqueness of the rest.*/
static uint8_t ceNFCA_NFCID[] = {
    0x5F, 'S', 'T', 'M'}; /* =_STM, 5F 53 54 4D NFCID1 / UID (4 bytes) */
static uint8_t ceNFCA_SENS_RES[] = {0x02,
                                    0x00}; /* SENS_RES / ATQA for 4-byte UID */
static uint8_t ceNFCA_SEL_RES = 0x20;      /* SEL_RES / SAK      */
#endif                                     /* RFAL_SUPPORT_MODE_LISTEN_NFCA */

static uint8_t ceNFCF_nfcid2[] = {0x02, 0xFE, 0x11, 0x22,
                                  0x33, 0x44, 0x55, 0x66};

#if RFAL_SUPPORT_MODE_LISTEN_NFCF
/* NFC-F CE config */
static uint8_t ceNFCF_SC[] = {0x12, 0xFC};
static uint8_t ceNFCF_SENSF_RES[] = {
    0x01,                   /* SENSF_RES  */
    0x02, 0xFE, 0x11, 0x22, /* NFCID2 */
    0x33, 0x44, 0x55, 0x66, /* NFCID2 */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x7F, 0x7F, 0x00, /* PAD0, PAD01, MRTIcheck, MRTIupdate, PAD2 */
    0x00, 0x00};            /* RD                                       */
#endif                      /* RFAL_SUPPORT_MODE_LISTEN_NFCF */
#endif                      /* RFAL_SUPPORT_CE && RFAL_FEATURE_LISTEN_MODE */

/*!
 *****************************************************************************
 * \brief Demo Blocking Transceive
 *
 * Helper function to send data in a blocking manner via the rfalNfc module
 *
 * \warning A protocol transceive handles long timeouts (several seconds),
 * transmission errors and retransmissions which may lead to a long period of
 * time where the MCU/CPU is blocked in this method.
 * This is a demo implementation, for a non-blocking usage example please
 * refer to the Examples available with RFAL
 *
 * \param[in]  txBuf      : data to be transmitted
 * \param[in]  txBufSize  : size of the data to be transmited
 * \param[out] rxData     : location where the received data has been placed
 * \param[out] rcvLen     : number of data bytes received
 * \param[in]  fwt        : FWT to be used (only for RF frame interface,
 *                                          otherwise use RFAL_FWT_NONE)
 *
 *
 *  \return ERR_PARAM     : Invalid parameters
 *  \return ERR_TIMEOUT   : Timeout error
 *  \return ERR_FRAMING   : Framing error detected
 *  \return ERR_PROTO     : Protocol error detected
 *  \return ERR_NONE      : No error, activation successful
 *
 *****************************************************************************
 */
static ret_code_t
tx_blocking(uint8_t *txBuf, uint16_t txBufSize, uint8_t **rxData,
            uint16_t **rcvLen, uint32_t fwt)
{
    int err;

    err = rfalNfcDataExchangeStart(txBuf, txBufSize, rxData, rcvLen, fwt);
    if (err == ERR_NONE) {
        do {
            rfalNfcWorker();
            err = rfalNfcDataExchangeGetStatus();
        } while (err == ERR_BUSY);
    }

    if (err == ERR_LINK_LOSS) {
        LOG_DBG("Device removed");
    } else if (err == ERR_SLEEP_REQ) {
        LOG_DBG("Sleep requested");
    } else if (err != ERR_NONE) {
        LOG_ERR("tx_blocking failed: %d", err);
    }

    return err == ERR_NONE ? RET_SUCCESS : RET_ERROR_INTERNAL;
}

/** @defgroup PTD_Demo_Private_Functions
 * @{
 */
/*!
 *****************************************************************************
 * \brief Demo Notification
 *
 *  This function receives the event notifications from RFAL
 *****************************************************************************
 */
static void
nfc_event_cb(rfalNfcState nfc_state)
{
    uint8_t device_count;
    rfalNfcDevice *nfc_dev;

    LOG_DBG("state: %u", nfc_state);

    if (nfc_state == RFAL_NFC_STATE_WAKEUP_MODE) {
        LOG_INF("Wake Up mode started");
    } else if (nfc_state == RFAL_NFC_STATE_POLL_TECHDETECT) {
        if (nfc_discovery_param.wakeupEnabled) {
            LOG_INF("Wake Up mode terminated. Polling for devices");
        }
    } else if (nfc_state == RFAL_NFC_STATE_POLL_SELECT) {
        /* Check if in case of multiple devices, selection is already attempted
         */
        if ((!multiple_devices)) {
            multiple_devices = true;
            /* Multiple devices were found, activate first of them */
            rfalNfcGetDevicesFound(&nfc_dev, &device_count);
            rfalNfcSelect(0);

            LOG_INF("Multiple Tags detected: %d", device_count);
        } else {
            rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_DISCOVERY);
        }
    } else if (nfc_state == RFAL_NFC_STATE_START_DISCOVERY) {
        /* Clear multiple device selection flag */
        multiple_devices = false;
    }
}

static void
nfc_ce_handle(rfalNfcDevice *nfcDev)
{
    ReturnCode err = ERR_NONE;
    uint8_t *rx_data;
    uint16_t *rcv_len;
    uint8_t tx_buf[150];
    uint16_t tx_len;

    do {
        rfalNfcWorker();

        switch (rfalNfcGetState()) {
        case RFAL_NFC_STATE_ACTIVATED:
            err = tx_blocking(NULL, 0, &rx_data, &rcv_len, 0);
            break;

        case RFAL_NFC_STATE_DATAEXCHANGE:
        case RFAL_NFC_STATE_DATAEXCHANGE_DONE:
            tx_len =
                ((nfcDev->type == RFAL_NFC_POLL_TYPE_NFCA)
                     ? nfc_ce_t4t(rx_data, *rcv_len, tx_buf, sizeof(tx_buf))
                     : rfalConvBytesToBits(
                           nfc_ce_t3t(rx_data, rfalConvBitsToBytes(*rcv_len),
                                      tx_buf, sizeof(tx_buf))));
            err =
                tx_blocking(tx_buf, tx_len, &rx_data, &rcv_len, RFAL_FWT_NONE);
            break;

        case RFAL_NFC_STATE_START_DISCOVERY:
            return;

        case RFAL_NFC_STATE_LISTEN_SLEEP:
        default:
            break;
        }
    } while ((err == ERR_NONE) || (err == ERR_SLEEP_REQ));
}

/**
 *
 * @return ReturnCode, see \c ERR_NONE
 */
static int
rfal_init(void)
{
    ReturnCode err;

    err = rfalNfcInitialize();
    if (err == ERR_NONE) {
        rfalNfcDefaultDiscParams(&nfc_discovery_param);
        nfc_discovery_param.notifyCb = nfc_event_cb;
        nfc_discovery_param.totalDuration = NFC_DISCOVERY_TIMEOUT_MS;
        nfc_discovery_param.techs2Find = RFAL_NFC_TECH_NONE;

        nfc_ce_init(ceNFCF_nfcid2);

#if RFAL_SUPPORT_MODE_LISTEN_NFCA
        /* Set configuration for NFC-A CE */
        memcpy(nfc_discovery_param.lmConfigPA.SENS_RES, ceNFCA_SENS_RES,
               RFAL_LM_SENS_RES_LEN); /* Set SENS_RES / ATQA */
        memcpy(nfc_discovery_param.lmConfigPA.nfcid, ceNFCA_NFCID,
               RFAL_LM_NFCID_LEN_04); /* Set NFCID / UID */
        nfc_discovery_param.lmConfigPA.nfcidLen =
            RFAL_LM_NFCID_LEN_04; /* Set NFCID length to 7 bytes */
        nfc_discovery_param.lmConfigPA.SEL_RES =
            ceNFCA_SEL_RES; /* Set SEL_RES / SAK */

        nfc_discovery_param.techs2Find |= RFAL_NFC_LISTEN_TECH_A;
#endif /* RFAL_SUPPORT_MODE_LISTEN_NFCA */

#if RFAL_SUPPORT_MODE_LISTEN_NFCF
        /* Set configuration for NFC-F CE */
        memcpy(nfc_discovery_param.lmConfigPF.SC, ceNFCF_SC,
               RFAL_LM_SENSF_SC_LEN); /* Set System Code */
        memcpy(&ceNFCF_SENSF_RES[RFAL_NFCF_CMD_LEN], ceNFCF_nfcid2,
               RFAL_NFCID2_LEN); /* Load NFCID2 on SENSF_RES */
        memcpy(nfc_discovery_param.lmConfigPF.SENSF_RES, ceNFCF_SENSF_RES,
               RFAL_LM_SENSF_RES_LEN); /* Set SENSF_RES / Poll Response */

        nfc_discovery_param.techs2Find |= RFAL_NFC_LISTEN_TECH_F;
#endif /* RFAL_SUPPORT_MODE_LISTEN_NFCF */

        /* Check for valid configuration by calling Discover once */
        err = rfalNfcDiscover(&nfc_discovery_param);
        rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_IDLE);

        if (err != ERR_NONE) {
            LOG_ERR("rfalNfcDiscover failed: %d", err);
            return err;
        }

        state = NFC_RESTART_DISCOVERY;
        return ERR_NONE;
    }

    return err;
}

_Noreturn void
nfc_thread(void)
{
    static rfalNfcDevice *nfc_device;

    LOG_INF("NFC started");

    while (1) {

        rfalNfcWorker(); /* Run RFAL worker periodically */

        switch (state) {
        case NFC_RESTART_DISCOVERY:
            rfalNfcDeactivate(RFAL_NFC_DEACTIVATE_IDLE);
            rfalNfcDiscover(&nfc_discovery_param);

            multiple_devices = false;
            state = NFC_DISCOVERY;
            break;

        case NFC_DISCOVERY:
            if (rfalNfcIsDevActivated(rfalNfcGetState())) {
                rfalNfcGetActiveDevice(&nfc_device);
                switch (nfc_device->type) {
                case RFAL_NFC_LISTEN_TYPE_NFCA:
                    switch (nfc_device->dev.nfca.type) {
                    case RFAL_NFCA_T1T:
                        LOG_INF("ISO14443A/Topaz (NFC-A T1T) TAG found");
                        break;

                    case RFAL_NFCA_T4T:
                        LOG_INF("NFCA Passive ISO-DEP device found");
                        break;

                    case RFAL_NFCA_T4T_NFCDEP:
                    case RFAL_NFCA_NFCDEP:
                        LOG_INF("NFCA Passive P2P device found");
                        break;

                    default:
                        LOG_INF("ISO14443A/NFC-A card found");
                        break;
                    }
                    break;

                case RFAL_NFC_POLL_TYPE_NFCA:
                case RFAL_NFC_POLL_TYPE_NFCF:
                    LOG_INF("Activated in CE %s mode.",
                            (nfc_device->type == RFAL_NFC_POLL_TYPE_NFCA)
                                ? "NFC-A"
                                : "NFC-F");

                    nfc_ce_handle(nfc_device);
                    break;

                default:
                    LOG_ERR("Type not supported: %u", nfc_device->type);
                    break;
                }

                state = NFC_RESTART_DISCOVERY;
            }
            break;

        case NFC_NO_INIT:
        default:
            break;
        }

        k_msleep(4);
    }
}

ret_code_t
nfc_init(void)
{
    int err = rfal_init();
    if (err) {
        LOG_ERR("rfal_init failed: %d", err);
        return RET_ERROR_NOT_INITIALIZED;
    }

    k_tid_t tid = k_thread_create(&nfc_thread_data, nfc_stack_area,
                                  K_THREAD_STACK_SIZEOF(nfc_stack_area),
                                  (k_thread_entry_t)nfc_thread, NULL, NULL,
                                  NULL, THREAD_PRIORITY_NFC, 0, K_NO_WAIT);
    k_thread_name_set(tid, "nfc");

    return RET_SUCCESS;
}
