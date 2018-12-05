/*
 Mikrovezérlök alkalmazásai
 BMEGEFOAMV1
 
 Projekt17: PS/2 billenytüzet beolvasása és LCD-re írása
 
 Készítette:
  Könnyü Máté
  Révész Levente
 
 2018-11-10
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
#define EEPROM_EN LATAbits.LATA2

#define ADATLAB PORTBbits.RB5  // SDI1 láb
#define CLKLAB PORTBbits.RB7   // SCK láb
#define CLKLAB_NO 7
#define KIMENET LATBbits.LATB6  // SDO1 láb

// RX - RB11    TX - RB10 
// SPI lábak
//      SDI1 - 14 - RB5
//      SDO1 - 15 - RB6
//      SCK  - 16 - RB7 - RP7

// delay
#define SYS_FREQ 79227500L
#define FCY SYS_FREQ/2 
#include <libpic30.h>

char * joe;                         // 101-es légió !
char * key;                         // billentyü neve stringben
int i, szamlalo;
uint8_t c;
uint64_t maszk64, data64, frame64;

// magyar ékezetes karakterek
const unsigned char hu_char[] = {
0x02,0x04,0x0E,0x01,0x0F,0x11,0x0F,0x00, // á
0x02,0x04,0x0E,0x11,0x1F,0x10,0x0E,0x00, // é
0x02,0x04,0x0C,0x04,0x04,0x04,0x0E,0x00, // í
0x02,0x04,0x0E,0x11,0x11,0x11,0x0E,0x00, // ó
0x02,0x04,0x11,0x11,0x11,0x13,0x0D,0x00, // ú
0x0A,0x00,0x11,0x11,0x11,0x13,0x0D,0x00, // ü
0x05,0x0A,0x11,0x11,0x11,0x13,0x0D,0x00, // üü
0x05,0x0A,0x0E,0x11,0x11,0x11,0x0E,0x00, // öö
};

void initLCD(void);
void refreshLCD(void);
uint8_t getOddParity(uint8_t p); 

void __attribute__((__interrupt__, __auto_psv__)) _INT1Interrupt (void)
{
    //KIMENET = !KIMENET;
    szamlalo++;
    if (ADATLAB) data64 = data64 | maszk64;
    if (maszk64 == 0x100000000) 
    {   // 33. bit
        frame64 = data64;
        maszk64 = 1;
        data64 = 0;
    }
    else maszk64 = maszk64 << 1;
    // IEC0bits.T1IE = 1;              // Enable T1 Interrupt
    // TMR1 = 0;
    IFS1bits.INT1IF = 0;
}

void __attribute__((__interrupt__, __auto_psv__)) _T1Interrupt (void)
{   // Timeout
  //Beérkezett adatok feldolgozása
    IFS0bits.T1IF = 0;
    IEC0bits.T1IE = 0;              // Disable T1 Interrupt
}

int main(void) 
{
  // oscillator init 80 Mhz
    CLKDIV=0x3000;
    PLLFBD=41;                      // 0=2, 1=3, 41=43
    while (! OSCCONbits.LOCK) ;
    RCONbits.SWDTEN=0;              // disable Watchdog Timer 
  
  // TRIS bits
    TRISB=0x0fff;                   // b12..b15 LCD output
    TRISBbits.TRISB6 = 0;           // SDO1 (KIMENET) out
    TRISBbits.TRISB5 = 1;           // SDI1 (ADATLAB) Input
    TRISBbits.TRISB7 = 1;           // SCK (CLOCKLAB)Input
    TRISAbits.TRISA2 = 0;           // EEPROM EN láb
 
  // az osccon-hoz unlock kell ! bit1: lposcen legyen 1, bit6: iolock legyen 0
  //PPSUnLock
    __builtin_write_OSCCONL(OSCCON & 0xbf); // bit6 -> 0
    __builtin_write_OSCCONL(OSCCON | 0x02); // bit1 -> 1
    // lábak átkonfigurálása ide jöhet
    RPINR0bits.INT1R = CLKLAB_NO;           // INT1 -> RP10 azaz TX
  //PPSLock
    __builtin_write_OSCCONL(OSCCON | 0x40); // bit6 -> 1
  
  // Analog funkciók tiltása
    AD1PCFGL = 0x1FFF;
  
  // EEPROM kikapcsolás
    EEPROM_EN = 1;                  // 1 = disable
    
  // INT1 inicializálás 
    IFS1bits.INT1IF = 0;            // IF törölve
    INTCON2bits.INT1EP = 1;         // 1 = Interrupt on negative edge
    IPC5bits.INT1IP = 0b111;        // 7-es prioritás
    IEC1bits.INT1IE = 1;            // INT0 IRQ Enable
    
  // Timer1
    // PR1=0x8000;                     // 32768, 1 sec: 32.768 khz
    // IPC0bits.T1IP=4;                // 4-es priortiás
    // IFS0bits.T1IF=0;                // töröld
    // IEC0bits.T1IE=0;                // engedélyezd az it-t  NEM
    // T1CONbits.TON=1;                // start timer
    // T1CONbits.TCS=1;                // timer1: külsö oszcillátor
    // T1CONbits.TCKPS=0;              // 1:1 prescaler
    // T1CONbits.TSIDL=0;              // mindig menjen
    
  // lcd - 2x16 karakter
    joe =(char *)malloc(33); // heap size-t a linkerben állitani >=44
    key =(char *)malloc(11); 
    if (!joe) // nem sikerült malloc-olni 
    {
        LED_R=1;                    // hiba.
        while (1) Sleep();          // vége.
    }
    memset(joe,0x20,32);            // feltöltés space-szel
    initLCD();
    strcpy(joe, "uMOGI v1.0 ready");
    refreshLCD();
    
    szamlalo = 0;
    maszk64 = 1, data64 = 0, frame64 = 0;
    c = 0;
    KIMENET = 0;

    while (1)
    {      
        memset(joe,0x20,32); // space-k
        c = (uint8_t)(frame64 >> 1);
        switch (c)
        {
        case 0x76: key = "ESC       ";	break;
        case 0x05: key = "F1        ";	break;
        case 0x06: key = "F2        ";	break;
        case 0x04: key = "F3        ";	break;
        case 0x0C: key = "F4        ";	break;
        case 0x03: key = "F5        ";	break;
        case 0x0B: key = "F6        ";	break;
        case 0x83: key = "F7        ";	break;
        case 0x0A: key = "F8        ";	break;
        case 0x01: key = "F9        ";	break;
        case 0x09: key = "F10       ";	break;
        case 0x78: key = "F11       ";	break;
        case 0x07: key = "F12       ";	break;
        case 0x7E: key = "ScrollLock";	break;
        case 0x0E: key = "`         ";	break;
        case 0x16: key = "1         ";	break;
        case 0x1E: key = "2         ";	break;
        case 0x26: key = "3         ";	break;
        case 0x25: key = "4         ";	break;
        case 0x2E: key = "5         ";	break;
        case 0x36: key = "6         ";	break;
        case 0x3D: key = "7         ";	break;
        case 0x3E: key = "8         ";	break;
        case 0x46: key = "9         ";	break;
        case 0x45: key = "0         ";	break;
        case 0x4E: key = "-         ";	break;
        case 0x55: key = "=         ";	break;
        case 0x66: key = "Backspace ";	break;
        case 0x0D: key = "Tab       ";	break;
        case 0x15: key = "Q         ";	break;
        case 0x1D: key = "W         ";	break;
        case 0x24: key = "E         ";	break;
        case 0x2D: key = "R         ";	break;
        case 0x2C: key = "T         ";	break;
        case 0x35: key = "Y         ";	break;
        case 0x3C: key = "U         ";	break;
        case 0x43: key = "I         ";	break;
        case 0x44: key = "O         ";	break;
        case 0x4D: key = "P         ";	break;
        case 0x54: key = "[         ";	break;
        case 0x5B: key = "]         ";	break;
        case 0x5D: key = "\\         ";	break;
        case 0x58: key = "Caps Lock ";	break;
        case 0x1C: key = "A         ";	break;
        case 0x1B: key = "S         ";	break;
        case 0x23: key = "D         ";	break;
        case 0x2B: key = "F         ";	break;
        case 0x34: key = "G         ";	break;
        case 0x33: key = "H         ";	break;
        case 0x3B: key = "J         ";	break;
        case 0x42: key = "K         ";	break;
        case 0x4B: key = "L         ";	break;
        case 0x4C: key = ";         ";	break;
        case 0x52: key = "'         ";	break;
        case 0x5A: key = "Enter     ";	break;
        case 0x12: key = "Shift (L) ";	break;
        case 0x1A: key = "Z         ";	break;
        case 0x22: key = "X         ";	break;
        case 0x21: key = "C         ";	break;
        case 0x2A: key = "V         ";	break;
        case 0x32: key = "B         ";	break;
        case 0x31: key = "N         ";	break;
        case 0x3A: key = "M         ";	break;
        case 0x41: key = ",         ";	break;
        case 0x49: key = ".         ";	break;
        case 0x4A: key = "/         ";	break;
        case 0x59: key = "Shift (R) ";	break;
        case 0x14: key = "Ctrl (L)  ";	break;
        case 0x11: key = "Alt (L)   ";	break;
        case 0x29: key = "Spacebar  ";	break;
        case 0x77: key = "Num Lock  ";	break;
        case 0x7C: key = "*         ";	break;
        case 0x7B: key = "-         ";	break;
        case 0x6C: key = "7         ";	break;
        case 0x75: key = "8         ";	break;
        case 0x7D: key = "9         ";	break;
        case 0x79: key = "+         ";	break;
        case 0x6B: key = "4         ";	break;
        case 0x73: key = "5         ";	break;
        case 0x74: key = "6         ";	break;
        case 0x69: key = "1         ";	break;
        case 0x72: key = "2         ";	break;
        case 0x7A: key = "3         ";	break;
        case 0x70: key = "0         ";	break;
        case 0x71: key = ".         ";	break;
        default:   key = "N/A       ";	break;
        }
        sprintf(joe, "0x%02x->%s ", c, key);
        //sprintf(joe+16, "%04x%04x%04x%04x", (uint16_t)(frame64>>48), (uint16_t)(frame64>>32),
        //        (uint16_t)(frame64>>16), (uint16_t)(frame64));
		sprintf(joe+16, "%I64x", frame64); // vagy "%llx"
        refreshLCD();
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

void lcd_clock_e(char b,char rs) 
{ 
  // Utasítás / adat fél-bájt kiírása LCD-re
  // A ledek is ugyanazokon a lábakon vannak, ez a kiírás felfüggeszti a
  //   ledek müködését.
  
  // b: data/instrzction byte
  // rs false: instruction, true:data
    int x=LATB;                     // led állapot mentés 
  // ha másra is használtuk, csak bitenként
    LATBbits.LATB12= b & 1;         // d4
    LATBbits.LATB13=(b & 2)>>1;     // d5
    LATBbits.LATB14=(b & 4)>>2;     // d6
    LATBbits.LATB15=(b & 8)>>3;     // d7
  // A lábakra (12,13,14,15) kirakjuk az adatot/utasítást
    LCD_RS = rs;                      // rs kint
  // clock kezdödik
    LCD_E = 1;                        // e:high
    Nop(); Nop(); Nop(); Nop(); // 220-300 ns kell.
  // clock végzödik
    LCD_E = 0;                        // e:low
    LATB = x;                         // ledek vissza
    __delay_us(40); // 40 us. 30-nál mágikus karakterek
}

void lcd_data(char b,char rs) 
{ 
  /* b bájtot elfelezi, és kiküldi az LCD-re a két felét. */
  // rs:false:instruction, true:data
  char x;
  x=(b & 0xf0)>>4;                  // upper half byte
  lcd_clock_e(x,rs);
  x=(b & 0x0f);                     // lower half 
  lcd_clock_e(x,rs);
}

void lcd_cgram(void)                
{
  /* Magyar karakterek definiálása */
  lcd_data(0x40,0);                 // set cgram parancs
  int i;
  for (i=0;i<64;i++) {              // magyar karakterek feltöltése
      lcd_data(hu_char[i],1);
  }
  lcd_data(0x80,0); // kurzor vissza a DDRAM elejére
}
void initLCD(void)
{
// rs és e outputra    
  TRISAbits.TRISA0=0;               // LCD RES
  TRISAbits.TRISA1=0;               // LCD E
  __delay_ms(50);                   // more than 40 msec
  lcd_clock_e(0x03,0);
  __delay_ms(5);                    // 4.1 msec
  lcd_clock_e(0x03,0);              // again
  __delay_us(100);                  // 100 usec
  lcd_clock_e(0x03,0);              // again
  lcd_clock_e(0x02,0);
  lcd_data(0x28,0);                 // 4 bit, 2 lines, font : 0: 5x8 dot
  lcd_data(0x0f,0);                 // 0x0f display, blink cursor on
  lcd_data(0x06,0);                 //  set increment addr
  lcd_data(0x01,0);                 //  clear display
  __delay_ms(32);                   // 32 msec
  lcd_cgram();
}

void refreshLCD(void)
{
  /* A joe[] tömb tartalmát kiírja az LCD-re */
  int i;
  lcd_data(0x80,0);                 // line 1 felsö sor
  for (i=0; i<16; i++) lcd_data(joe[i],1);
  lcd_data(0xC0,0);                 // line 2 alsó sor
  for (i=16; i<32; i++) lcd_data(joe[i],1);
}