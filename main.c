/*
 Mikrovezérl?k alkalmazásai
 BMEGEFOAMV1
 
 Projekt17: PS/2 billenyt?zet beolvasása és LCD-re írása
 
 Készítette:
  Könny? Máté
  Révész Levente
 */

#include <xc.h>
#include <stdlib.h> // malloc
#include <stdio.h> // sprintf
#include <string.h> // char *

// cpu: pic24hj128gp502
// config bits
#pragma config BWRP = WRPROTECT_OFF,BSS = NO_FLASH,RBS = NO_RAM,SWRP = WRPROTECT_OFF
#pragma config SSS = NO_FLASH,RSS = NO_RAM,GWRP = OFF,GSS = OFF,FNOSC = FRCPLL,IESO = OFF
#pragma config POSCMD = NONE,OSCIOFNC = ON,IOL1WAY = ON,FCKSM = CSDCMD,WDTPOST = PS32768
#pragma config WDTPRE = PR128,WINDIS = OFF,FWDTEN = OFF,FPWRT = PWR128,ALTI2C = OFF
#pragma config ICS = PGD1,JTAGEN = OFF

#define LED_R LATBbits.LATB13
#define LED_G LATBbits.LATB12
#define LED_B LATBbits.LATB14
#define SW1 !PORTAbits.RA3

// delay
#define SYS_FREQ 79227500L
#define FCY SYS_FREQ/2 
#include <libpic30.h>

// UART
#define BAUDRATE 9600
#define BRGVAL ((FCY/BAUDRATE)/16) - 1
char * joe; // 101-es légió !
int i;
char readchar;
// magyar ékezetes karakterek
const unsigned char hu_char[] = {
0x02,0x04,0x0E,0x01,0x0F,0x11,0x0F,0x00, // á
0x02,0x04,0x0E,0x11,0x1F,0x10,0x0E,0x00, // é
0x02,0x04,0x0C,0x04,0x04,0x04,0x0E,0x00, // í
0x02,0x04,0x0E,0x11,0x11,0x11,0x0E,0x00, // ó
0x02,0x04,0x11,0x11,0x11,0x13,0x0D,0x00, // ú
0x0A,0x00,0x11,0x11,0x11,0x13,0x0D,0x00, // ü
0x05,0x0A,0x11,0x11,0x11,0x13,0x0D,0x00, // ?
0x05,0x0A,0x0E,0x11,0x11,0x11,0x0E,0x00, // ?
};

void initUART1(void);
void initLCD(void);
void refreshLCD(void);

//void __attribute__((__interrupt__, __auto_psv__)) _ADC1Interrupt (void)
//{
//     IFS0bits.AD1IF=0;
//}

int main(void) 
{
    // oscillator init 80 Mhz
    CLKDIV=0x3000;
    PLLFBD=41; // 0=2, 1=3, 41=43
    while (! OSCCONbits.LOCK) ;
    RCONbits.SWDTEN=0;
  
    TRISB=0x0fff;   // b12..b15 LCD output
                    // b11 RX bemenet
  // az osccon-hoz unlock kell ! bit1: lposcen legyen 1, bit6: iolock legyen 0
    //PPSUnLock
    __builtin_write_OSCCONL(OSCCON & 0xbf); // bit6 0
    //UART
    RPOR5bits.RP10R=3;      //21-es láb TX
    RPINR18bits.U1RXR=11;    //22-es láb RX
    //PPSLock
    __builtin_write_OSCCONL(OSCCON | 0x40); // bit6 1
    
    //UART1_init();
    
  // lcd - 2x16 karakter
    joe =(char *)malloc(33); // heap size-t a linkerben állitani >=44
    if (!joe) // nem sikerült malloc-olni 
    {
        LED_R=1; // hiba.
        while (1) Sleep(); // vége.
    }
    memset(joe,0x20,32); // space
    initLCD();
  //  LED_B=1;
    strcpy(joe, "uMOGI v1.0 ready");
    refreshLCD();
  // init kész, jöhet a feldolgozó végtelen cilkus  
    while (1)
    {      
        //if (U1STAbits.URXDA) {}
        memset(joe,0x20,32); // space-k
        readchar = 'A';
        //readchar = getUART1();
        switch(readchar) { // magyar karakterek cseréje
            case 'á': readchar = 0x00; break;
            case 'é': readchar = 0x01; break;
            case 'í': readchar = 0x02; break;
            case 'ó': readchar = 0x03; break;
            case 'ú': readchar = 0x04; break;
            case 'ü': readchar = 0x05; break;
            case '?': readchar = 0x06; break;
            case '?': readchar = 0x07; break;
            case 'ö': readchar = 0xEF; break;
        }

        sprintf(joe+16, "0xAB -> %c", readchar);
        joe[i]=0; i++; i&=0x0f; // 0..15
        AD1CHS0bits.CH0SA = SW1 ? 5 : 4; //AN4: poti AN5:szenzor
        AD1CON1bits.SAMP = 1; // konverzio inditasa, ha asam=0
        // while(!AD1CON1bits.DONE); // polling: konverzio vege
        refreshLCD(); // mig az lcd-t irjuk, bejon az uj adat
        __delay_ms(250);
    }
    return 0;
}

// lcd. d4-d7 : port b12-b15. közös láb a ledekkel
// e: ra1: h valid data
// r/w:gnd
// rs: ra0 l:instruction, h:data
#define LCD_RS LATAbits.LATA0
#define LCD_E LATAbits.LATA1

void lcd_clock_e(char b,char rs) { 
  // Utasítás / adat fél-bájt kiírása LCD-re
  // A ledek is ugyanazokon a lábakon vannak, ez a kiírás felfüggeszti a
  // ledek m?ködését.
  
  // b: data/instrzction byte
  // rs false: instruction, true:data
    int x=LATB;// led állapot mentés 
  // ha másra is használtuk, csak bitenként
    // LATBbits.LATB12=b & 1; // d4
    // LATBbits.LATB13=(b & 2)>>1; // d5
    // LATBbits.LATB14=(b & 4)>>2; // d6
    // LATBbits.LATB15=(b & 8)>>3; // d7
  // A lábakra (12,13,14,15) kirakjuk az adatot/utasítást
    LATB = b<<12; // a 4 bitet feltolja LATB tetejére
    LCD_RS=rs; // rs kint
  // clock kezd?dik
    LCD_E=1;  // e:h
    Nop(); Nop(); Nop(); Nop(); // 220-300 ns kell.
  // clock végz?dik
    LCD_E=0; // e:l
    LATB=x; // ledek vissza
    __delay_us(40); // 40 us. 30-nál mágikus karakterek
}

void lcd_data(char b,char rs) { 
  // b bájtot elfelezi, és kiküldi az LCD-re a két felét.
  // rs:false:instruction, true:data
  char x;
  x=(b & 0xf0)>>4; // upper half byte
  lcd_clock_e(x,rs);
  x=(b & 0x0f); // lower half 
  lcd_clock_e(x,rs);
}

void lcd_cgram(void) // Magyar karakterek definiálása
{
  lcd_data(0x40,0); // set cgram
  int i;
  for (i=0;i<64;i++) {
      lcd_data(hu_char[i],1);
  }
  lcd_data(0x80,0); // kurzor vissza a DDRAM elejére
}
void initLCD(void)
{
// rs és e outputra    
  TRISAbits.TRISA0=0;   // LCD RES
  TRISAbits.TRISA1=0;   // LCD E
  __delay_ms(50); // more than 40 msec
  lcd_clock_e(0x03,0);
  __delay_ms(5); // 4.1 msec
  lcd_clock_e(0x03,0); // again
  __delay_us(100); // 100 usec
  lcd_clock_e(0x03,0); // again
  lcd_clock_e(0x02,0);
  lcd_data(0x28,0);  // 4 bit, 2 lines, font : 0: 5x8 dot
  lcd_data(0x0f,0); // 0x0f display, blink cursor on
  lcd_data(0x06,0); //  set increment addr
  lcd_data(0x01,0); //  clear display
  __delay_ms(32); // 32 msec
  lcd_cgram();
}

void refreshLCD(void)
{
  int i;
  lcd_data(0x80,0);// line 1 fels? sor
  for (i=0; i<16; i++) lcd_data(joe[i],1);
  lcd_data(0xC0,0); // line 2 alsó sor
  for (i=16; i<32; i++) lcd_data(joe[i],1);
}

void initUART1(void)
{
    U1MODE = 0; // módregiszter
    U1STA = 0; // státuszregiszter
    U1BRG = BRGVAL; // Baudrate beállítása
    
    U1MODEbits.UARTEN = 1; //enable
    U1MODEbits.PDSEL = 0b10; // páros paritásbit
    U1STAbits.UTXEN = 1; // TX enable
}
char getUART1(void)
{
    while (!U1STAbits.URXDA); // várakozás új karakterre
    return U1RXREG;
}