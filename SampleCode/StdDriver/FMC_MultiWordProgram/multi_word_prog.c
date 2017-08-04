/******************************************************************************
 * @file     multi_word_prog.c
 * @version  V1.00
 * @brief    This sample run on SRAM to show FMC multi word program function.
 *
 *
 * @copyright (C) 2016 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/
#include <stdio.h>

#include "M480.h"

#define MULTI_WORD_PROG_LEN         512          /* The maximum length is 512. */


uint32_t    page_buff[FMC_FLASH_PAGE_SIZE/4];


void SYS_Init(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Enable HXT clock */
    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);

    /* Wait for HXT clock ready */
    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

    /* Switch HCLK clock source to HXT */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HXT,CLK_CLKDIV0_HCLK(1));

    /* Set core clock as PLL_CLOCK from PLL */
    CLK_SetCoreClock(FREQ_192MHZ);

    /* Set both PCLK0 and PCLK1 as HCLK/2 */
    CLK->PCLKDIV = CLK_PCLKDIV_PCLK0DIV2 | CLK_PCLKDIV_PCLK1DIV2;

    /* Enable UART module clock */
    CLK_EnableModuleClock(UART0_MODULE);

    /* Select UART module clock source as HXT and UART module clock divider as 1 */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HXT, CLK_CLKDIV0_UART0(1));

    /* Update System Core Clock */
    SystemCoreClockUpdate();

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init UART0 multi-function pins, RXD(PD.2) and TXD(PD.3) */
    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD2MFP_Msk | SYS_GPD_MFPL_PD3MFP_Msk);
    SYS->GPD_MFPL |= (SYS_GPD_MFPL_PD2MFP_UART0_RXD | SYS_GPD_MFPL_PD3MFP_UART0_TXD);

    /* Lock protected registers */
    SYS_LockReg();
}

void UART0_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init UART                                                                                               */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Configure UART0 and set UART0 baud rate */
    UART_Open(UART0, 115200);
}

int  multi_word_program(uint32_t start_addr)
{
    uint32_t    i;

    printf("    program address 0x%x\n", start_addr);

    FMC->ISPADDR = start_addr;
    FMC->MPDAT0  = page_buff[0];
    FMC->MPDAT1  = page_buff[1];
    FMC->MPDAT2  = page_buff[2];
    FMC->MPDAT3  = page_buff[3];
    FMC->ISPCMD  = FMC_ISPCMD_PROGRAM_MUL;
    FMC->ISPTRG  = FMC_ISPTRG_ISPGO_Msk;

    for (i = 4; i < MULTI_WORD_PROG_LEN/4; ) {
        while (FMC->MPSTS & (FMC_MPSTS_D0_Msk | FMC_MPSTS_D1_Msk))
            ;

        if (!(FMC->MPSTS & FMC_MPSTS_MPBUSY_Msk)) {
            printf("    [WARNING] busy cleared after D0D1 cleared!\n");
            i += 2;
            break;
        }

        FMC->MPDAT0 = page_buff[i++];
        FMC->MPDAT1 = page_buff[i++];

        if (i == MULTI_WORD_PROG_LEN/4)
            return 0;           // done

        while (FMC->MPSTS & (FMC_MPSTS_D2_Msk | FMC_MPSTS_D3_Msk))
            ;

        if (!(FMC->MPSTS & FMC_MPSTS_MPBUSY_Msk)) {
            printf("    [WARNING] busy cleared after D2D3 cleared!\n");
            i += 2;
            break;
        }

        FMC->MPDAT2 = page_buff[i++];
        FMC->MPDAT3 = page_buff[i++];
    }

    if (i != MULTI_WORD_PROG_LEN/4) {
        printf("    [WARNING] Multi-word program interrupted at 0x%x !!\n", i);
        return -1;
    }

    while (FMC->MPSTS & FMC_MPSTS_MPBUSY_Msk) ;

    return 0;
}

int main()
{
    uint32_t  i, addr, maddr;          /* temporary variables */

    SYS_Init();                        /* Init System, IP clock and multi-function I/O */

    UART0_Init();                      /* Initialize UART0 */

    /*---------------------------------------------------------------------------------------------------------*/
    /* SAMPLE CODE                                                                                             */
    /*---------------------------------------------------------------------------------------------------------*/

    printf("\n\n");
    printf("+-------------------------------------+\n");
    printf("|    M480 Multi-word Program Sample   |\n");
    printf("+-------------------------------------+\n");

    SYS_UnlockReg();                   /* Unlock protected registers */

    FMC_Open();                        /* Enable FMC ISP function */

    FMC_ENABLE_AP_UPDATE();            /* Enable APROM erase/program */

    for (addr = 0; addr < 0x20000; addr += FMC_FLASH_PAGE_SIZE) {
        printf("Multiword program APROM page 0x%x =>\n", addr);

        if (FMC_Erase(addr) < 0) {
            printf("    Erase failed!!\n");
            goto err_out;
        }

        printf("    Program...");

        for (maddr = addr; maddr < addr + FMC_FLASH_PAGE_SIZE; maddr += MULTI_WORD_PROG_LEN) {
            /* Prepare test pattern */
            for (i = 0; i < MULTI_WORD_PROG_LEN; i+=4)
                page_buff[i/4] = maddr + i;

            /* execute multi-word program */
            if (multi_word_program(maddr) < 0)
                goto err_out;
        }
        printf("    [OK]\n");

        printf("    Verify...");

        for (i = 0; i < FMC_FLASH_PAGE_SIZE; i+=4)
            page_buff[i/4] = addr + i;

        for (i = 0; i < FMC_FLASH_PAGE_SIZE; i+=4) {
            if (FMC_Read(addr+i) != page_buff[i/4]) {
                printf("\n[FAILED] Data mismatch at address 0x%x, expect: 0x%x, read: 0x%x!\n", addr+i, page_buff[i/4], FMC_Read(addr+i));
                goto err_out;
            }
        }
        printf("[OK]\n");
    }

    printf("\n\nMulti-word program demo done.\n");
    while (1);

err_out:
    printf("\n\nERROR!\n");
    while (1);
}

/*** (C) COPYRIGHT 2016 Nuvoton Technology Corp. ***/