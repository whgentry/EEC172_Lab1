//*****************************************************************************
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

//*****************************************************************************
//
// Application Name     - Lab3
// Application Details  - William Gentry
//
//*****************************************************************************

#include "project.h"

#define APPLICATION_VERSION     "3.1"

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

// My variables
int8_t code_current = 0;
int8_t code_prev = 0;
char out_str[128] = "";
char in_str[128] = "";

unsigned long adc_buffer[ADC_BUFFER_SIZE];
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

// Sends message over UART_TEXT
void
Send_Text(const char *str)
{
    if(str != NULL)
    {
        while(*str!='\0')
        {
            MAP_UARTCharPut(UART_T_BASE,*str++);
        }
    }
}

//*****************************************************************************
//
//! \UART_T_IntHandler
//!
//! Handles interrupts for UART_T,
//!
//! \param None.
//!
//! \return None.
//
//*****************************************************************************
static void
UART_T_IntHandler(void)
{
    MAP_UARTIntClear(UART_T_BASE, UART_INT_RX);

    static int iLen = 0;

    // Collect all characters on UART
    while(MAP_UARTCharsAvail(UART_T_BASE)) {
        if (iLen == 128) {break;}
        in_str[iLen++] = MAP_UARTCharGetNonBlocking(UART_T_BASE);
        in_str[iLen] = '\0';
    }

//    MAP_UARTCharPut(UART_T_BASE, char_current);

    if (in_str[iLen-1] == '\n') {
        in_str[iLen-1] = '\0';
        iLen = 0;
        setCursor(0,64);
        setTextColor(RED,BLACK);
        fillRect(0,64,128,64,BLACK);
        Outstr(in_str);
    }

    return;
}
//*****************************************************************************
//
//! \GPIOA1IntHandler
//!
//! Handles interrupts for GPIO A1,
//!
//! \param None.
//!
//! \return None.
//
//*****************************************************************************
static void
TimerA0_IntHandler(void)
{
    unsigned long adc_msb = 0;
    unsigned long adc_lsb = 0;

    Timer_IF_InterruptClear(TIMERA0_BASE);

    GPIOPinWrite(GPIOA1_BASE, GPIO_PIN_5, 0x00);

    MAP_SPIDataPut(GSPI_BASE, 0x00);
    MAP_SPIDataGet(GSPI_BASE, &adc_msb);
    MAP_SPIDataPut(GSPI_BASE, 0x00);
    MAP_SPIDataGet(GSPI_BASE, &adc_lsb);

    GPIOPinWrite(GPIOA1_BASE, GPIO_PIN_5, GPIO_PIN_5);

    unsigned long temp = ((adc_msb & ADC_MSB_MASK) << ADC_MSB_SHIFT) | ((adc_lsb & ADC_LSB_MASK) >> ADC_LSB_SHIFT);

    adc_add_sample(temp);

    return;
}
//*****************************************************************************
//
//! Initialize Interupts for IRReceiver
//!
//! \param None.
//!
//! \return None.
//
//*****************************************************************************
static void
Interrupt_Init(void)
{
    // Setup UART interrupt
    MAP_UARTConfigSetExpClk(UART_T_BASE,MAP_PRCMPeripheralClockGet(UART_T_PERIPH),
                      UART_BAUD_RATE, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                       UART_CONFIG_PAR_NONE));
    MAP_UARTFIFODisable(UART_T_BASE);
    MAP_IntPrioritySet(INT_UART_T, INT_PRIORITY_LVL_1);
    MAP_UARTIntRegister(UART_T_BASE,UART_T_IntHandler);
    MAP_UARTIntClear(UART_T_BASE, UART_INT_RX);
    MAP_UARTIntEnable(UART_T_BASE, UART_INT_RX);

    // Setup Configure Timer For ADC sampling rate 16KHz
    Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_IntSetup(TIMERA0_BASE, TIMER_A, TimerA0_IntHandler);
    Timer_IF_Start(TIMERA0_BASE, TIMER_A, ADC_TIMER_VALUE);

    // Setup Timer for character select timeout
    Timer_IF_Init(PRCM_TIMERA1, TIMERA1_BASE, TIMER_CFG_ONE_SHOT_UP, TIMER_A, 0);
    Timer_IF_Start(TIMERA1_BASE, TIMER_A, 0xFFFFFFFF);
}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

//*****************************************************************************
//
//! Main function for spi demo application
//!
//! \param none
//!
//! \return None.
//
//*****************************************************************************
void main()
{
    // Initialize Board configurations
    BoardInit();

    // Muxing UART and SPI lines.
    PinMuxConfig();

    // Enable the SPI module clock
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);

    // Initialising the Terminal.
//    InitTerm();

    // Clearing the Terminal.
//    ClearTerm();

    // Display the Banner
//    Message("\n\n\n\r");
//    Message("\t\t   ********************************************\n\r");
//    Message("\t\t        CC3200 Lab 3 Application  \n\r");
//    Message("\t\t   ********************************************\n\r");
//    Message("\n\n\n\r");

    // Reset the peripheral
    MAP_PRCMPeripheralReset(PRCM_GSPI);

    // Initialize dtmf
    dtmf_coeff_Init();

    // Initialize Display
    Adafruit_Init();
    fillScreen(BLACK);
    drawLine(0,63,127,63,WHITE);

    // Set up interrupts
    Interrupt_Init();

    // Text Setup
    setTextWrap(1);
    setTextSize(1);
    setTextColor(RED,BLACK);

    char char_current;
    uint32_t char_delay;

    MAP_IntMasterDisable();
    setCursor(0,0);
    Outstr("tone_not_ready");
    MAP_IntMasterEnable();

    while(1)
    {
        // This Function call clears the button ready value //
        char_delay = TICKS_TO_MILLISECONDS(TimerValueGet(TIMERA1_BASE, TIMER_A));

        if (dtmf_tone_ready()) {
            dtmf_stop_sampling();

            TimerValueSet(TIMERA1_BASE, TIMER_A, 0);
            code_current = dtmf_tone_get();
            char_current = button_char_get(code_current);
            int len = strlen(out_str);

            switch(char_current)
            {
            case 0xFF:  // Caps or bad code
                break;
            case 0x7F:  // Delete
                if (len) {
                    out_str[len-1] = '\0';
                }
                break;
            default:
                if ( (len == 0) ||
                     (code_current != code_prev))
                {
                    out_str[len] = char_current;
                    out_str[len+1] = '\0';
                } else {
                    out_str[len-1] = char_current;
                }
                break;
            }

            if (char_current == '\n') {
                // send the out_str over uart
                MAP_UARTIntDisable(UART_T_BASE, UART_INT_RX);
                fillRect(0,0,128,63,BLACK);
                MAP_UARTIntEnable(UART_T_BASE, UART_INT_RX);
                // Reset text screen
                Send_Text(out_str);
                out_str[0] = '\0';
            } else {
                setCursor(0,0);
                setTextColor(GREEN,BLACK);
                fillRect(0,0,128,63,BLACK);
                Outstr(out_str);
            }

            code_prev = code_current;

            dtmf_start_sampling();
        } else if (char_delay > CHAR_TIMEOUT) {
            code_prev = -1;
        }
    }
}
