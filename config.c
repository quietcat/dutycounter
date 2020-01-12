/////////////////////////////////////
//  Generated Initialization File  //
/////////////////////////////////////

#include "C8051F200.h"

// Peripheral specific initialization functions,
// Called from the Init_Device() function
void Reset_Sources_Init()
{
    WDTCN     = 0xDE;
    WDTCN     = 0xAD;
}

void Timer_Init()
{
    CKCON     = 0x10;
    TCON      = 0x40;
    TMOD      = 0x20;
    TH1       = 0xFD;
    T2CON     = 0x04;
    RCAP2H    = 0xEE;
    TH2       = 0xE0;
}

void UART_Init()
{
    SCON      = 0x50;
}

void Comparator_Init()
{
    int i = 0;
    CPT0CN    = 0x8F;
    for (i = 0; i < 60; i++);  // Wait 20us for initialization
    CPT0CN    &= ~0x30;
}

void Port_IO_Init()
{
    // P0.0  -  TX   (UART), Open-Drain  Digital
    // P0.1  -  RX   (UART), Open-Drain  Digital
    // P0.2  -  Unassigned,  Open-Drain  Digital
    // P0.3  -  Unassigned,  Open-Drain  Digital
    // P0.4  -  Unassigned,  Open-Drain  Digital
    // P0.5  -  Unassigned,  Open-Drain  Digital
    // P0.6  -  Unassigned,  Open-Drain  Digital
    // P0.7  -  Unassigned,  Open-Drain  Digital

    // P1.0  -  Unassigned,  Open-Drain  Analog
    // P1.1  -  Unassigned,  Open-Drain  Analog
    // P1.2  -  CP0  (Cmp0), Open-Drain  Digital
    // P1.3  -  Unassigned,  Open-Drain  Digital
    // P1.4  -  Unassigned,  Open-Drain  Digital
    // P1.5  -  Unassigned,  Open-Drain  Digital
    // P1.6  -  Unassigned,  Push-Pull   Digital
    // P1.7  -  Unassigned,  Push-Pull   Digital

    // P2.0  -  Unassigned,  Push-Pull   Digital
    // P2.1  -  Unassigned,  Push-Pull   Digital
    // P2.2  -  Unassigned,  Push-Pull   Digital
    // P2.3  -  Unassigned,  Push-Pull   Digital
    // P2.4  -  Unassigned,  Push-Pull   Digital
    // P2.5  -  Unassigned,  Push-Pull   Digital
    // P2.6  -  Unassigned,  Push-Pull   Digital
    // P2.7  -  Unassigned,  Push-Pull   Digital

    PRT0MX    = 0x01;
    PRT1MX    = 0x01;
    PRT1CF    = 0xC0;
    PRT2CF    = 0xFF;
    PRT3CF    = 0xFF;
    P1MODE    = 0xFC;
}

void Oscillator_Init()
{
    int i = 0;
    OSCXCN    = 0x67;
    for (i = 0; i < 3000; i++);  // Wait 1ms for initialization
    while ((OSCXCN & 0x80) == 0);
    OSCICN    = 0x08;
}

void Interrupts_Init()
{
    IE        = 0xB0;
    EIE1      = 0x10;
    EIP1      = 0x10;
}

// Initialization function for device,
// Call Init_Device() from your main program
void Init_Device(void)
{
    Reset_Sources_Init();
    Timer_Init();
    UART_Init();
    Comparator_Init();
    Port_IO_Init();
    Oscillator_Init();
    Interrupts_Init();
}
