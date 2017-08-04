/******************************************************************************
 * @file     usbd_audio.c
 * @brief    December series USBD driver Sample file
 * @version  1.0.0
 * @date     01, April, 2017
 *
 * @note
 * Copyright (C) 2017 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/

/*!<Includes */
#include <string.h>
#include "M480.h"
#include "usbd_audio.h"

uint32_t g_usbd_SampleRate = AUDIO_RATE;
/*--------------------------------------------------------------------------*/
static volatile uint8_t bPlayVolumeLAdjust = FALSE;
static volatile uint8_t bPlayVolumeRAdjust = FALSE;
static volatile uint8_t bIsI2CIdle = TRUE;


/*--------------------------------------------------------------------------*/
/* Global variables for Audio class */
uint32_t g_usbd_UsbAudioState = 0;

uint8_t  g_usbd_RecMute = 0x01;
int16_t  g_usbd_RecVolume = 0x1000;
int16_t  g_usbd_RecMaxVolume = 0x7FFF;
int16_t  g_usbd_RecMinVolume = 0x8000;
int16_t  g_usbd_RecResVolume = 0x400;

uint8_t  g_usbd_PlayMute = 0x01;
int16_t  g_usbd_PlayVolumeL = 0x1000;
int16_t  g_usbd_PlayVolumeR = 0x1000;
int16_t  g_usbd_PlayMaxVolume = 0x7FFF;
int16_t  g_usbd_PlayMinVolume = 0x8000;
int16_t  g_usbd_PlayResVolume = 0x400;

uint8_t volatile u8PlayEn = 0;
uint8_t volatile u8AudioPlaying=0;
uint8_t volatile u8TxDataCntInBuffer=0;
uint8_t volatile u8PDMATxIdx=0;

uint8_t volatile u8RecEn = 0;
uint8_t volatile u8RxDataCntInBuffer=0;
uint8_t volatile u8PDMARxIdx=0;

uint32_t volatile u32BuffLen = 0, u32RxBuffLen = 0;

/* Player Buffer and its pointer */
#ifdef __ICCARM__
#pragma data_alignment=4
uint32_t PcmPlayBuff[PDMA_TXBUFFER_CNT][BUFF_LEN] = {0};
uint32_t PcmPlayBuffLen[PDMA_TXBUFFER_CNT] = {0};

uint8_t PcmRecBuff[PDMA_RXBUFFER_CNT][BUFF_LEN] = {0};
uint8_t u8PcmRxBufFull[PDMA_RXBUFFER_CNT] = {0};
#else
__align(4) uint32_t PcmPlayBuff[PDMA_TXBUFFER_CNT][BUFF_LEN] = {0};
__align(4) uint32_t PcmPlayBuffLen[PDMA_TXBUFFER_CNT] = {0};

__align(4) uint8_t PcmRecBuff[PDMA_RXBUFFER_CNT][BUFF_LEN] = {0};
__align(4) uint8_t u8PcmRxBufFull[PDMA_RXBUFFER_CNT] = {0};
#endif

volatile uint32_t u32BufPlayIdx = 0;
volatile uint32_t u32PlayBufPos=0;
volatile uint32_t u32BufRecIdx = 0;


/*--------------------------------------------------------------------------*/


/**
 * @brief       USBD Interrupt Service Routine
 *
 * @param[in]   None
 *
 * @return      None
 *
 * @details     This function is the USBD ISR
 */
void USBD_IRQHandler(void)
{
    uint32_t u32IntSts = USBD_GET_INT_FLAG();
    uint32_t u32State = USBD_GET_BUS_STATE();

//------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_FLDET) {
        // Floating detect
        USBD_CLR_INT_FLAG(USBD_INTSTS_FLDET);

        if (USBD_IS_ATTACHED()) {
            /* USB Plug In */
            USBD_ENABLE_USB();
        } else {
            /* USB Un-plug */
            USBD_DISABLE_USB();
        }
    }

//------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_BUS) {
        /* Clear event flag */
        USBD_CLR_INT_FLAG(USBD_INTSTS_BUS);

        if(u32State & USBD_STATE_USBRST) {
            /* Bus reset */
            USBD_ENABLE_USB();
            USBD_SwReset();
        }
        if(u32State & USBD_STATE_SUSPEND) {
            /* Enable USB but disable PHY */
            USBD_DISABLE_PHY();
        }
        if(u32State & USBD_STATE_RESUME) {
            /* Enable USB and enable PHY */
            USBD_ENABLE_USB();
        }
    }

//------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_USB) {
        // USB event
        if(u32IntSts & USBD_INTSTS_SETUP) {
            // Setup packet
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_SETUP);

            /* Clear the data IN/OUT ready flag of control end-points */
            USBD_STOP_TRANSACTION(EP0);
            USBD_STOP_TRANSACTION(EP1);

            USBD_ProcessSetupPacket();
        }

        // EP events
        if(u32IntSts & USBD_INTSTS_EP0) {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP0);

            // control IN
            USBD_CtrlIn();
        }

        if(u32IntSts & USBD_INTSTS_EP1) {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP1);

            // control OUT
            USBD_CtrlOut();
        }

        if(u32IntSts & USBD_INTSTS_EP2) {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP2);

            // Isochronous IN
            EP2_Handler();
        }

        if(u32IntSts & USBD_INTSTS_EP3) {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP3);

            // Isochronous OUT
            EP3_Handler();
        }

        if(u32IntSts & USBD_INTSTS_EP4) {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP4);
        }

        if(u32IntSts & USBD_INTSTS_EP5) {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP5);
        }

        if(u32IntSts & USBD_INTSTS_EP6) {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP6);
        }

        if(u32IntSts & USBD_INTSTS_EP7) {
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP7);
        }
    }
}

/**
 * @brief       EP2 Handler
 *
 * @param[in]   None
 *
 * @return      None
 *
 * @details     This function is used to process EP2 event
 */
/* Record */
void EP2_Handler(void)
{
    /* ISO IN transfer ACK */
    if (g_usbd_UsbAudioState == UAC_START_AUDIO_RECORD)
        UAC_SendRecData();
}

/**
 * @brief       EP3 Handler
 *
 * @param[in]   None
 *
 * @return      None
 *
 * @details     This function is used to process EP3 event
 */
/* Play */
void EP3_Handler(void)
{
    /* ISO OUT transfer ACK */
    UAC_GetPlayData((uint8_t *)((uint32_t)USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP3)), (uint32_t)USBD_GET_PAYLOAD_LEN(EP3));

    USBD_SET_PAYLOAD_LEN(EP3, EP3_MAX_PKT_SIZE);
}



/*--------------------------------------------------------------------------*/
/**
 * @brief       UAC Class Initial
 *
 * @param[in]   None
 *
 * @return      None
 *
 * @details     This function is used to configure endpoints for UAC class
 */
void UAC_Init(void)
{
    if ((g_usbd_SampleRate % 8000) == 0) {
        u32BuffLen = 768;
        u32RxBuffLen = (g_usbd_SampleRate / 1000);
    } else {
        u32BuffLen = 441;
        u32RxBuffLen = (g_usbd_SampleRate / 1000);
    }

    /* Init setup packet buffer */
    /* Buffer for setup packet -> [0 ~ 0x7] */
    USBD->STBUFSEG = SETUP_BUF_BASE;

    /*****************************************************/
    /* EP0 ==> control IN endpoint, address 0 */
    USBD_CONFIG_EP(EP0, USBD_CFG_CSTALL | USBD_CFG_EPMODE_IN | 0);
    /* Buffer range for EP0 */
    USBD_SET_EP_BUF_ADDR(EP0, EP0_BUF_BASE);

    /* EP1 ==> control OUT endpoint, address 0 */
    USBD_CONFIG_EP(EP1, USBD_CFG_CSTALL | USBD_CFG_EPMODE_OUT | 0);
    /* Buffer range for EP1 */
    USBD_SET_EP_BUF_ADDR(EP1, EP1_BUF_BASE);

    /*****************************************************/
    /* EP2 ==> Isochronous IN endpoint, address 1 */
    USBD_CONFIG_EP(EP2, USBD_CFG_EPMODE_IN | ISO_IN_EP_NUM | USBD_CFG_TYPE_ISO);
    /* Buffer offset for EP2 */
    USBD_SET_EP_BUF_ADDR(EP2, EP2_BUF_BASE);

    /*****************************************************/
    /* EP3 ==> Isochronous OUT endpoint, address 2 */
    USBD_CONFIG_EP(EP3, USBD_CFG_EPMODE_OUT | ISO_OUT_EP_NUM | USBD_CFG_TYPE_ISO);
    /* Buffer offset for EP3 */
    USBD_SET_EP_BUF_ADDR(EP3, EP3_BUF_BASE);
    /* trigger receive OUT data */
    USBD_SET_PAYLOAD_LEN(EP3, EP3_MAX_PKT_SIZE);
}


/**
 * @brief       UAC class request
 *
 * @param[in]   None
 *
 * @return      None
 *
 * @details     This function is used to process UAC class requests
 */
void UAC_ClassRequest(void)
{
    uint8_t buf[8];

    USBD_GetSetupPacket(buf);

    if (buf[0] & 0x80) {
        /* request data transfer direction */
        // Device to host
        switch (buf[1]) {
        case UAC_GET_CUR: {
            if ((buf[0] & 0x0f) == 0x2) {
                M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_SampleRate;
                M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0) + 1) = g_usbd_SampleRate >> 8;
                M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0) + 2) = g_usbd_SampleRate >> 16;
                /* Data stage */
                USBD_SET_DATA1(EP0);
                USBD_SET_PAYLOAD_LEN(EP0, buf[6]);
            } else {
                switch (buf[3]) {
                case MUTE_CONTROL: {
                    if (REC_FEATURE_UNITID == buf[5])
                        M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_RecMute;
                    else if (PLAY_FEATURE_UNITID == buf[5])
                        M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_PlayMute;

                    /* Data stage */
                    USBD_SET_DATA1(EP0);
                    USBD_SET_PAYLOAD_LEN(EP0, 1);
                    break;
                }
                case VOLUME_CONTROL: {
                    if (REC_FEATURE_UNITID == buf[5]) {
                        M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_RecVolume;
                        M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0) + 1) = g_usbd_RecVolume >> 8;
                    } else if (PLAY_FEATURE_UNITID == buf[5]) {
                        if(buf[2] == 1) {
                            M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_PlayVolumeL;
                            M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0) + 1) = g_usbd_PlayVolumeL >> 8;
                        } else {
                            M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_PlayVolumeR;
                            M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0) + 1) = g_usbd_PlayVolumeR >> 8;
                        }
                    }
                    /* Data stage */
                    USBD_SET_DATA1(EP0);
                    USBD_SET_PAYLOAD_LEN(EP0, 2);
                    break;
                }
                default: {
                    /* Setup error, stall the device */
                    USBD_SetStall(0);
                }
                }
            }
            // Trigger next Control Out DATA1 Transaction.
            /* Status stage */
            USBD_PrepareCtrlOut(0,0);
            break;
        }

        case UAC_GET_MIN: {
            switch (buf[3]) {
            case VOLUME_CONTROL: {
                if (REC_FEATURE_UNITID == buf[5]) {
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_RecMinVolume;
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0) + 1) = g_usbd_RecMinVolume >> 8;
                } else if (PLAY_FEATURE_UNITID == buf[5]) {
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_PlayMinVolume;
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0) + 1) = g_usbd_PlayMinVolume >> 8;
                }
                /* Data stage */
                USBD_SET_DATA1(EP0);
                USBD_SET_PAYLOAD_LEN(EP0, 2);
                break;
            }
            default:
                /* STALL control pipe */
                USBD_SetStall(0);
            }
            // Trigger next Control Out DATA1 Transaction.
            /* Status stage */
            USBD_PrepareCtrlOut(0,0);
            break;
        }

        case UAC_GET_MAX: {
            switch (buf[3]) {
            case VOLUME_CONTROL: {
                if (REC_FEATURE_UNITID == buf[5]) {
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_RecMaxVolume;
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0) + 1) = g_usbd_RecMaxVolume >> 8;
                } else if (PLAY_FEATURE_UNITID == buf[5]) {
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_PlayMaxVolume;
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0) + 1) = g_usbd_PlayMaxVolume >> 8;
                }
                /* Data stage */
                USBD_SET_DATA1(EP0);
                USBD_SET_PAYLOAD_LEN(EP0, 2);
                break;
            }
            default:
                /* STALL control pipe */
                USBD_SetStall(0);
            }
            // Trigger next Control Out DATA1 Transaction.
            /* Status stage */
            USBD_PrepareCtrlOut(0,0);
            break;
        }

        case UAC_GET_RES: {
            switch (buf[3]) {
            case VOLUME_CONTROL: {
                if (REC_FEATURE_UNITID == buf[5]) {
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_RecResVolume;
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0) + 1) = g_usbd_RecResVolume >> 8;
                } else if (PLAY_FEATURE_UNITID == buf[5]) {
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0)) = g_usbd_PlayResVolume;
                    M8(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP0) + 1) = g_usbd_PlayResVolume >> 8;
                }
                /* Data stage */
                USBD_SET_DATA1(EP0);
                USBD_SET_PAYLOAD_LEN(EP0, 2);
                break;
            }
            default:
                /* STALL control pipe */
                USBD_SetStall(0);
            }
            // Trigger next Control Out DATA1 Transaction.
            /* Status stage */
            USBD_PrepareCtrlOut(0,0);
            break;
        }

        default: {
            /* Setup error, stall the device */
            USBD_SetStall(0);
        }
        }
    } else {
        // Host to device
        switch (buf[1]) {
        case UAC_SET_CUR: {
            if ((buf[0] & 0x0f) == 0x02) {
                USBD_PrepareCtrlOut((uint8_t *)&g_usbd_SampleRate, buf[6]);
                if (u8RecEn) {
                    UAC_DeviceDisable(0);
                    AudioStartRecord(g_usbd_SampleRate);
                }
                /* Status stage */
                USBD_SET_DATA1(EP0);
                USBD_SET_PAYLOAD_LEN(EP0, 0);
                break;
            } else {
                switch (buf[3]) {
                case MUTE_CONTROL:
                    if (REC_FEATURE_UNITID == buf[5])
                        USBD_PrepareCtrlOut((uint8_t *)&g_usbd_RecMute, buf[6]);
                    else if (PLAY_FEATURE_UNITID == buf[5])
                        USBD_PrepareCtrlOut((uint8_t *)&g_usbd_PlayMute, buf[6]);
                    /* Status stage */
                    USBD_SET_DATA1(EP0);
                    USBD_SET_PAYLOAD_LEN(EP0, 0);
                    break;

                case VOLUME_CONTROL:
                    if (REC_FEATURE_UNITID == buf[5])
                        USBD_PrepareCtrlOut((uint8_t *)&g_usbd_RecVolume, buf[6]);
                    else if (PLAY_FEATURE_UNITID == buf[5]) {
                        if (buf[2] == 1) {
                            USBD_PrepareCtrlOut((uint8_t *)&g_usbd_PlayVolumeL, buf[6]);
                            if(g_usbd_PlayVolumeL & 0x8000)
                                g_usbd_PlayVolumeL = (g_usbd_PlayVolumeL & 0x7FFF) >> 8;
                            else
                                g_usbd_PlayVolumeL = (g_usbd_PlayVolumeL >> 7);
                            bPlayVolumeLAdjust = TRUE; //ATOM_I2C_WriteWAU8822(11, i16PlayVolumeL | 0x100);   /* Set left DAC volume */
                        } else {
                            USBD_PrepareCtrlOut((uint8_t *)&g_usbd_PlayVolumeR, buf[6]);
                            if(g_usbd_PlayVolumeR & 0x8000)
                                g_usbd_PlayVolumeR = (g_usbd_PlayVolumeR & 0x7FFF) >> 8;
                            else
                                g_usbd_PlayVolumeR = (g_usbd_PlayVolumeR >> 7);
                            bPlayVolumeRAdjust = TRUE; //ATOM_I2C_WriteWAU8822(12, i16PlayVolumeR | 0x100);   /* Set right DAC volume */
                        }
                    }
                    /* Status stage */
                    USBD_SET_DATA1(EP0);
                    USBD_SET_PAYLOAD_LEN(EP0, 0);
                    break;

                default:
                    /* STALL control pipe */
                    USBD_SetStall(0);
                    break;
                }
                break;
            }
        }
        default: {
            /* Setup error, stall the device */
            USBD_SetStall(0);
            break;
        }
        }
    }
}

/**
 * @brief       Set Interface standard request
 *
 * @param[in]   u32AltInterface Interface
 *
 * @return      None
 *
 * @details     This function is used to set UAC Class relative setting
 */
void UAC_SetInterface(uint32_t u32AltInterface)
{
    uint8_t buf[8];

    USBD_GetSetupPacket(buf);

    if (buf[4] == 1) {
        /* Audio Iso IN interface */
        if (u32AltInterface == 1) {
            g_usbd_UsbAudioState = UAC_START_AUDIO_RECORD;
            USBD_SET_DATA1(EP2);
            USBD_SET_PAYLOAD_LEN(EP2, 0);
            UAC_DeviceEnable(0);
        } else if (u32AltInterface == 0) {
            UAC_DeviceDisable(0);
            USBD_SET_DATA1(EP2);
            USBD_SET_PAYLOAD_LEN(EP2, 0);
            g_usbd_UsbAudioState = UAC_STOP_AUDIO_RECORD;
        }
    } else if (buf[4] == 2) {
        /* Audio Iso OUT interface */
        if (u32AltInterface == 1) {
            USBD_SET_PAYLOAD_LEN(EP3, EP3_MAX_PKT_SIZE);
            UAC_DeviceEnable(1);
        } else
            UAC_DeviceDisable(1);
    }
}

/*******************************************************************/
/* For I2C transfer */
__IO uint32_t EndFlag0 = 0;
uint8_t Device_Addr0 = 0x1A;                /* WAU8822 Device ID */

typedef enum {
    E_RS_NONE,          // no re-sampling
    E_RS_UP,            // up sampling
    E_RS_DOWN           // down sampling
} RESAMPLE_STATE_T;


void RecoveryFromArbLost(void)
{
    I2C2->CTL0 &= ~I2C_CTL0_I2CEN_Msk;
    I2C2->CTL0 |= I2C_CTL0_I2CEN_Msk;
}

uint8_t I2cWrite_MultiByteforNAU88L25(uint8_t chipadd,uint16_t subaddr, const uint8_t *p,uint32_t len)
{
    /* Send START */
    I2C_START(I2C2);
    I2C_WAIT_READY(I2C2);

    /* Send device address */
    I2C_SET_DATA(I2C2, chipadd);
    I2C_SET_CONTROL_REG(I2C2, I2C_CTL_SI);
    I2C_WAIT_READY(I2C2);

    /* Send register number and MSB of data */
    I2C_SET_DATA(I2C2, (uint8_t)(subaddr>>8));
    I2C_SET_CONTROL_REG(I2C2, I2C_CTL_SI);
    I2C_WAIT_READY(I2C2);

    /* Send register number and MSB of data */
    I2C_SET_DATA(I2C2, (uint8_t)(subaddr));
    I2C_SET_CONTROL_REG(I2C2, I2C_CTL_SI);
    I2C_WAIT_READY(I2C2);

    /* Send data */
    I2C_SET_DATA(I2C2, p[0]);
    I2C_SET_CONTROL_REG(I2C2, I2C_CTL_SI);
    I2C_WAIT_READY(I2C2);

    /* Send data */
    I2C_SET_DATA(I2C2, p[1]);
    I2C_SET_CONTROL_REG(I2C2, I2C_CTL_SI);
    I2C_WAIT_READY(I2C2);

    /* Send STOP */
    I2C_STOP(I2C2);

    return  0;
}


uint8_t I2C_WriteNAU88L25(uint16_t addr,uint16_t dat)
{
    uint8_t Tx_Data0[2];

    Tx_Data0[0] = (uint8_t)(dat >> 8);
    Tx_Data0[1] = (uint8_t)(dat & 0x00FF);

    return ( I2cWrite_MultiByteforNAU88L25(0x1A << 1,addr,&Tx_Data0[0],2) );
}

uint32_t u32OldSampleRate=0;

/* config play sampling rate */
void NAU88L25_ConfigSampleRate(uint32_t u32SampleRate)
{
    if(u32SampleRate == u32OldSampleRate)
        return;

    u32OldSampleRate = u32SampleRate;

    printf("[NAU88L25] Configure Sampling Rate to %d\n", u32SampleRate);

    if((u32SampleRate % 8) == 0) {
        I2C_WriteNAU88L25(0x0005, 0x3126); //12.288Mhz
        I2C_WriteNAU88L25(0x0006, 0x0008);
    } else {
        I2C_WriteNAU88L25(0x0005, 0x86C2); //11.2896Mhz
        I2C_WriteNAU88L25(0x0006, 0x0007);
    }

    switch (u32SampleRate) {
    case 44100:
        I2C_WriteNAU88L25(0x001D,  0x301A); //301A:Master, BCLK_DIV=11.2896M/8=1.4112M, LRC_DIV=1.4112M/32=44.1K
        I2C_WriteNAU88L25(0x002B,  0x0012);
        I2C_WriteNAU88L25(0x002C,  0x0082);
        u32BuffLen = 441;
        u32RxBuffLen = 444;
        break;

    case 48000:
        I2C_WriteNAU88L25(0x001D,  0x301A); //301A:Master, BCLK_DIV=12.288M/8=1.536M, LRC_DIV=1.536M/32=48K
        I2C_WriteNAU88L25(0x002B,  0x0012);
        I2C_WriteNAU88L25(0x002C,  0x0082);
        u32BuffLen = 768;
        u32RxBuffLen = 768;
        break;

    case 96000:
        I2C_WriteNAU88L25(0x0003,  0x80A2); //MCLK = SYSCLK_SRC/2
        I2C_WriteNAU88L25(0x0004,  0x1801);
        I2C_WriteNAU88L25(0x0005,  0x3126); //MCLK = 24.576MHz
        I2C_WriteNAU88L25(0x0006,  0xF008);
        I2C_WriteNAU88L25(0x001D,  0x301A); //3019:Master, BCLK_DIV=MCLK/8=3.072M, LRC_DIV=3.072M/32=96K
        I2C_WriteNAU88L25(0x002B,  0x0001);
        I2C_WriteNAU88L25(0x002C,  0x0080);
        u32BuffLen = 768;
        u32RxBuffLen = 768;
        break;

    default:
        printf("do not support %d sampling rate\n", u32SampleRate);
    }
}


void NAU88L25_Reset(void)
{
    I2C_WriteNAU88L25(0,  0x1);
    I2C_WriteNAU88L25(0,  0);   // Reset all registers
    CLK_SysTickDelay(10000);

    printf("NAU88L25 Software Reset.\n");
}


void NAU88L25_Setup(void)
{
    I2C_WriteNAU88L25(0x0003,  0x80A2);
    I2C_WriteNAU88L25(0x0004,  0x0001);
    I2C_WriteNAU88L25(0x0005,  0x3126);
    I2C_WriteNAU88L25(0x0006,  0x0008);
    I2C_WriteNAU88L25(0x0007,  0x0010);
    I2C_WriteNAU88L25(0x0008,  0xC000);
    I2C_WriteNAU88L25(0x0009,  0x6000);
    I2C_WriteNAU88L25(0x000A,  0xF13C);
    I2C_WriteNAU88L25(0x000C,  0x0048);
    I2C_WriteNAU88L25(0x000D,  0x0000);
    I2C_WriteNAU88L25(0x000F,  0x0000);
    I2C_WriteNAU88L25(0x0010,  0x0000);
    I2C_WriteNAU88L25(0x0011,  0x0000);
    I2C_WriteNAU88L25(0x0012,  0xFFFF);
    I2C_WriteNAU88L25(0x0013,  0x0015);
    I2C_WriteNAU88L25(0x0014,  0x0110);
    I2C_WriteNAU88L25(0x0015,  0x0000);
    I2C_WriteNAU88L25(0x0016,  0x0000);
    I2C_WriteNAU88L25(0x0017,  0x0000);
    I2C_WriteNAU88L25(0x0018,  0x0000);
    I2C_WriteNAU88L25(0x0019,  0x0000);
    I2C_WriteNAU88L25(0x001A,  0x0000);
    I2C_WriteNAU88L25(0x001B,  0x0000);
    I2C_WriteNAU88L25(0x001C,  0x0002);
    I2C_WriteNAU88L25(0x001D,  0x301a);   //301A:Master, BCLK_DIV=12.288M/8=1.536M, LRC_DIV=1.536M/32=48K
    I2C_WriteNAU88L25(0x001E,  0x0000);
    I2C_WriteNAU88L25(0x001F,  0x0000);
    I2C_WriteNAU88L25(0x0020,  0x0000);
    I2C_WriteNAU88L25(0x0021,  0x0000);
    I2C_WriteNAU88L25(0x0022,  0x0000);
    I2C_WriteNAU88L25(0x0023,  0x0000);
    I2C_WriteNAU88L25(0x0024,  0x0000);
    I2C_WriteNAU88L25(0x0025,  0x0000);
    I2C_WriteNAU88L25(0x0026,  0x0000);
    I2C_WriteNAU88L25(0x0027,  0x0000);
    I2C_WriteNAU88L25(0x0028,  0x0000);
    I2C_WriteNAU88L25(0x0029,  0x0000);
    I2C_WriteNAU88L25(0x002A,  0x0000);
    I2C_WriteNAU88L25(0x002B,  0x0012);
    I2C_WriteNAU88L25(0x002C,  0x0082);
    I2C_WriteNAU88L25(0x002D,  0x0000);
    I2C_WriteNAU88L25(0x0030,  0x00CF);
    I2C_WriteNAU88L25(0x0031,  0x0000);
    I2C_WriteNAU88L25(0x0032,  0x0000);
    I2C_WriteNAU88L25(0x0033,  0x009E);
    I2C_WriteNAU88L25(0x0034,  0x029E);
    I2C_WriteNAU88L25(0x0038,  0x1486);
    I2C_WriteNAU88L25(0x0039,  0x0F12);
    I2C_WriteNAU88L25(0x003A,  0x25FF);
    I2C_WriteNAU88L25(0x003B,  0x3457);
    I2C_WriteNAU88L25(0x0045,  0x1486);
    I2C_WriteNAU88L25(0x0046,  0x0F12);
    I2C_WriteNAU88L25(0x0047,  0x25F9);
    I2C_WriteNAU88L25(0x0048,  0x3457);
    I2C_WriteNAU88L25(0x004C,  0x0000);
    I2C_WriteNAU88L25(0x004D,  0x0000);
    I2C_WriteNAU88L25(0x004E,  0x0000);
    I2C_WriteNAU88L25(0x0050,  0x2007);
    I2C_WriteNAU88L25(0x0051,  0x0000);
    I2C_WriteNAU88L25(0x0053,  0xC201);
    I2C_WriteNAU88L25(0x0054,  0x0C95);
    I2C_WriteNAU88L25(0x0055,  0x0000);
    I2C_WriteNAU88L25(0x0058,  0x1A14);
    I2C_WriteNAU88L25(0x0059,  0x00FF);
    I2C_WriteNAU88L25(0x0066,  0x0060);
    I2C_WriteNAU88L25(0x0068,  0xC300);
    I2C_WriteNAU88L25(0x0069,  0x0000);
    I2C_WriteNAU88L25(0x006A,  0x0083);
    I2C_WriteNAU88L25(0x0071,  0x0011);
    I2C_WriteNAU88L25(0x0072,  0x0260);
    I2C_WriteNAU88L25(0x0073,  0x332C);
    I2C_WriteNAU88L25(0x0074,  0x4502);
    I2C_WriteNAU88L25(0x0076,  0x3140);
    I2C_WriteNAU88L25(0x0077,  0x0000);
    I2C_WriteNAU88L25(0x007F,  0x553F);
    I2C_WriteNAU88L25(0x0080,  0x0420);
    I2C_WriteNAU88L25(0x0001,  0x07D4);

    printf("NAU88L25 Configured done.\n");
}


/**
  * @brief  UAC_DeviceEnable. To enable the device to play or record audio data.
  * @param  None.
  * @retval None.
  */
void UAC_DeviceEnable(uint32_t bIsPlay)
{
    if (bIsPlay) {
        /* Enable play hardware */
        u8PlayEn = 1;
    } else {
        /* Enable record hardware */
        if(!u8RecEn)
            AudioStartRecord(g_usbd_SampleRate);

        u8RecEn = 1;
    }
    TIMER_Start(TIMER0);
}


/**
  * @brief  UAC_DeviceDisable. To disable the device to play or record audio data.
  * @param  None.
  * @retval None.
  */
void UAC_DeviceDisable(uint32_t bIsPlay)
{
    if (bIsPlay) {
        /* Disable play hardware/stop play */
        u8PlayEn = 0;
        /* Disable I2S Tx function */
        I2S_DISABLE_TXDMA(I2S0);
        I2S_DISABLE_TX(I2S0);

        /* Disable PDMA channel */
        PDMA->PAUSE |= (1 << PDMA_I2S_TX_CH);
        printf("Stop Play ...\n");

        /* Reset some variables */
        u32BufPlayIdx = 0;
        u32PlayBufPos = 0;
        u8PDMATxIdx = 0;
        u8AudioPlaying = 0;
        u8TxDataCntInBuffer = 0;

        /* flush PCM buffer */
        memset(PcmPlayBuff, 0, sizeof(PcmPlayBuff));
        TIMER0->CNT = 0x7657;

    } else {
        /* Disable record hardware/stop record */
        u8RecEn = 0;
        /* Disable I2S Rx function */
        I2S_DISABLE_RXDMA(I2S0);
        I2S_DISABLE_RX(I2S0);

        /* Disable PDMA channel */
        PDMA->PAUSE |= (1 << PDMA_I2S_RX_CH);
        printf("Stop Record ..\n");

        /* Reset some variables */
        u32BufRecIdx = 0;
        u8PDMARxIdx = 0;

        /* flush PCM buffer */
        memset(u8PcmRxBufFull, 0, sizeof(u8PcmRxBufFull));
    }
}


/**
  * @brief  GetPlayData, To get data from ISO OUT to play buffer.
  * @param  pi16src: The data buffer of ISO OUT.
  *         i16Samples: The sample number in data buffer.
  * @retval None.
  */
void UAC_GetPlayData(uint8_t *psrc, uint32_t u32Samples)
{
    uint32_t i, u32len;
    uint32_t *pBuff;
    uint32_t *pSrc;

    /* if buffer has enough data, play it!! */
    if (!u8AudioPlaying && (u8TxDataCntInBuffer >= (PDMA_TXBUFFER_CNT/2+1))) {
        AudioStartPlay(g_usbd_SampleRate);
        u8AudioPlaying = 1;
    }

    u32len = u32Samples/4;
    /* Ring buffer check */
    if ((u32PlayBufPos + u32len) > u32BuffLen) {
        PcmPlayBuffLen[u32BufPlayIdx] = u32PlayBufPos;
        u32PlayBufPos = 0;
        u32BufPlayIdx ++;

        /* change buffer index */
        if(u32BufPlayIdx >= PDMA_TXBUFFER_CNT)
            u32BufPlayIdx=0;

        /* increase data count in buffer */
        u8TxDataCntInBuffer ++;
    }

    pBuff = &PcmPlayBuff[u32BufPlayIdx][u32PlayBufPos];
    pSrc = (uint32_t *) psrc;
    for ( i = 0; i < u32len; i++ ) {
        pBuff[i] = pSrc[i];
    }

    u32PlayBufPos += u32len;
}


/* adjust codec PLL */
void AdjustCodecPll(RESAMPLE_STATE_T r)
{
    /* Sample rate = 48KH, 96KHz */
    /* 8.192, 8.192*1.005 = 8.23296, 8.192*0.995 = 8.15104 */
    /* 0.192   * 2^16 = 0x3126 */
    /* 0.23296 * 2^16 = 0x3BA3 */
    /* 0.15104 * 2^16 = 0x26AB */
    static uint16_t tb0[3] = {0x3216, 0x3BA3, 0x26AB};

    /* Sample rate = 44.1KHz only */
    /* 7.5264, 7.5264*1.005 = 7.5640, 7.5264*0.995 = 7.4887 */
    /* 0.5264  * 2^16 = 0x86C2 */
    /* 0.5640  * 2^16 = 0x9062 */
    /* 0.4887  * 2^16 = 0x7D1B */
    static uint16_t tb1[3] = {0x86C2, 0x9062, 0x7D1B};

    static RESAMPLE_STATE_T current = E_RS_NONE;
    int s;

    if(r == current)
        return;
    else
        current = r;
    switch(r) {
    case E_RS_UP:
        s = 1;
        break;
    case E_RS_DOWN:
        s = 2;
        break;
    case E_RS_NONE:
    default:
        s = 0;
    }

    if((g_usbd_SampleRate % 8) == 0) {
        I2C_WriteNAU88L25(0x0005, tb0[s]);
        I2C_WriteNAU88L25(0x0006, 0x0008);
    } else {
        I2C_WriteNAU88L25(0x0005, tb1[s]);
        I2C_WriteNAU88L25(0x0006, 0x0007);
    }
}


void AudioStartPlay(uint32_t u32SampleRate)
{
    UAC_DeviceEnable(1);

    /* Configure TX PDMA SG table */
    PDMA_WriteTxSGTable();

    /* Configure NAU88L25 to specific sample rate */
    NAU88L25_ConfigSampleRate(u32SampleRate);

    /* Enable I2S Tx function */
    I2S_ENABLE_TXDMA(I2S0);
    I2S_ENABLE_TX(I2S0);

    /* Enable PDMA channel */
    PDMA->CHCTL |= (1 << PDMA_I2S_TX_CH);
    printf("Start Play ... \n");

    // workaround for PDMA suspend
    PDMA->DSCT[PDMA_I2S_TX_CH].CTL = 0;
    PDMA->DSCT[PDMA_I2S_TX_CH].CTL = 2;
}


/**
  * @brief  SendRecData, prepare the record data for next ISO transfer.
  * @param  None.
  * @retval None.
  */
void UAC_SendRecData(void)
{
    uint32_t *pBuff;

    /* when record buffer full, send data to host */
    if(u8PcmRxBufFull[u32BufRecIdx]) {

        /* Set empty flag */
        u8PcmRxBufFull[u32BufRecIdx] = 0;

        pBuff = (uint32_t *)&PcmRecBuff[u32BufRecIdx][0];
        USBD_MemCopy((uint8_t *)((uint32_t)USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP2)), (void *)pBuff, u32RxBuffLen<<2);
        USBD_SET_PAYLOAD_LEN(EP2, u32RxBuffLen<<2);

        /* Change to next PCM buffer */
        u32BufRecIdx ++;
        if(u32BufRecIdx >= PDMA_RXBUFFER_CNT)
            u32BufRecIdx=0;

    } else {
        USBD_SET_PAYLOAD_LEN(EP2, 0);
    }
}

void AudioStartRecord(uint32_t u32SampleRate)
{
    /* Configure RX PDMA SG table */
    PDMA_WriteRxSGTable();

    /* Configure NAU88L25 to specific sample rate */
    NAU88L25_ConfigSampleRate(u32SampleRate);

    /* Enable I2S Tx function */
    I2S_ENABLE_RXDMA(I2S0);
    I2S_ENABLE_RX(I2S0);

    /* Enable PDMA channel */
    PDMA->CHCTL |= (1 << PDMA_I2S_RX_CH);
    printf("Start Record ... \n");

    PDMA->DSCT[PDMA_I2S_RX_CH].CTL = 0;
    PDMA->DSCT[PDMA_I2S_RX_CH].CTL = 2;
}


//======================================================
void TMR0_IRQHandler(void)
{
    TIMER_ClearIntFlag(TIMER0);

    if(u8PlayEn) {
        if((u8TxDataCntInBuffer >= (PDMA_TXBUFFER_CNT/2)) && (u8TxDataCntInBuffer <= (PDMA_TXBUFFER_CNT/2+1)))
            AdjustCodecPll(E_RS_NONE);
        else if(u8TxDataCntInBuffer >= (PDMA_TXBUFFER_CNT-2))
            AdjustCodecPll(E_RS_UP);
        else
            AdjustCodecPll(E_RS_DOWN);
    }
}



