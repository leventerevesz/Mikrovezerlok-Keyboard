#include <xc.h>

// cpu: pic24hj128gp502
// config bits
#pragma config BWRP = WRPROTECT_OFF,BSS = NO_FLASH,RBS = NO_RAM,SWRP = WRPROTECT_OFF
#pragma config SSS = NO_FLASH,RSS = NO_RAM,GWRP = OFF,GSS = OFF,FNOSC = FRCPLL,IESO = OFF
#pragma config POSCMD = NONE,OSCIOFNC = ON,IOL1WAY = ON,FCKSM = CSDCMD,WDTPOST = PS32768
#pragma config WDTPRE = PR128,WINDIS = OFF,FWDTEN = OFF,FPWRT = PWR128,ALTI2C = OFF
#pragma config ICS = PGD1,JTAGEN = OFF

#define SYS_FREQ 79227500L
#define FCY SYS_FREQ/2 
#include <libpic30.h>

#define R_led LATBbits.LATB13
#define G_led LATBbits.LATB12
#define B_led LATBbits.LATB14
#define SW1 !PORTAbits.RA3

//void __attribute__((__interrupt__, __auto_psv__)) _T2Interrupt (void)
//{
//     IFS0bits.T2IF=0;
//}

void sleep_1us()
{
    // Nop();//25.2 ns 40 db nop=1.01 us
    Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); // 8
    Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); // 16
    Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); // 24
    Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); // 32
    Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); // csak 39. a ret is idö
}

int main(void) 
{
    // oscillator init 80 Mhz
    CLKDIV=0x3000;
    PLLFBD=41; // 0=2, 1=3, 41=43
    while (! OSCCONbits.LOCK) ;
    RCONbits.SWDTEN=0;
  
  // gpio: rgb led + lcd az rb12..rb14-en,lcd az rb15-ön
    TRISB=0x0fff; // rb11..rb0 input
    
  // az osccon-hoz unlock kell ! bit1: lposcen legyen 1, bit6: iolock legyen 0
  //  __builtin_write_OSCCONL(OSCCON | ((1<<1) & ~(1<<6)));
  
   // init kész, jöhet a feldolgozó végtelen cilkus  
    while (1)
    {       
           
        Nop();
	__delay_ms(100);
    }
    
    return 0;
}

