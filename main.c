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
#define ADATLAB PORTBbits.RB11  // RX láb
#define CLKLAB PORTBbits.RB12   // TX láb
#define KIMENET LATBbits.LATB6  // SDO1 láb

// delay
#define SYS_FREQ 79227500L
#define FCY SYS_FREQ/2 
#include <libpic30.h>

// UART
#define BAUDRATE 9600
#define BRGVAL ((FCY/BAUDRATE)/16) - 1
char * lcd_buffer; // 101-es légió !
int i, szamlalo, szamlalo2;
char readchar;
uint16_t maszk, data;
uint64_t maszk64, data64, frame64;
#define TMERET 33
int data_tomb[TMERET], frame_tomb[TMERET];

union {
    struct {
        uint16_t startbit:1;
        uint16_t data:8;
        uint16_t parity:1;
        uint16_t stopbit:1;
    } framebits;
    uint16_t frame;
} frameunion;

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
    KIMENET = !KIMENET;
    szamlalo++;
    if (ADATLAB) data64 |= maszk64;
    if (maszk64 == 0x100000000) 
    {   // 33. bit
        frame64 = data64;
        maszk64 = 1;
        data64 = 0;
    }
    else maszk64 = maszk64 << 1;
    
    data_tomb[szamlalo2++] = ADATLAB;
    if (szamlalo2 == TMERET)
    {
        szamlalo2 = 0;
        memset(frame_tomb, 0, TMERET);
        for (i=0; i<TMERET; i++)
            frame_tomb[i] = data_tomb[i];
        memset(data_tomb, 0, TMERET);
    }
    IFS1bits.INT1IF = 0;
}

int main(void) 
{
  // oscillator init 80 Mhz
    CLKDIV=0x3000;
    PLLFBD=41;                      // 0=2, 1=3, 41=43
    while (! OSCCONbits.LOCK) ;
    RCONbits.SWDTEN=0;              // disable Watchdog Timer 
  
    TRISB=0x0fff;                   // b12..b15 LCD output
    TRISBbits.TRISB6 = 0;           // SDO1 (KIMENET) out
    TRISBbits.TRISB11 = 1;          // RX (ADATLAB) Input
    TRISBbits.TRISB12 = 1;          // TX (CLOCKLAB)Input
 
  // az osccon-hoz unlock kell ! bit1: lposcen legyen 1, bit6: iolock legyen 0
  //PPSUnLock
    __builtin_write_OSCCONL(OSCCON & 0xbf); // bit6 -> 0
    __builtin_write_OSCCONL(OSCCON | 0x02); // bit1 -> 1
    // lábak átkonfigurálása ide jöhet
    RPINR0bits.INT1R = 8; // INT1 -> RP8 azaz 17. láb, SCL
  //PPSLock
    __builtin_write_OSCCONL(OSCCON | 0x40); // bit6 -> 1
  
  // Analog funkciók tiltása
    AD1PCFGL = 0x1FFF;
  
  // INT1 inicializálás 
    IFS1bits.INT1IF = 0;            // IF törölve
    INTCON2bits.INT1EP = 1;         // 1 = Interrupt on negative edge
    IPC5bits.INT1IP = 0b111;        // 7-es prioritás
    IEC1bits.INT1IE = 1;            // INT0 IRQ Enable
    
  // lcd - 2x16 karakter
    lcd_buffer =(char *)malloc(33); // heap size-t a linkerben állitani >=44
    if (!lcd_buffer) // nem sikerült malloc-olni 
    {
        LED_R=1;                    // hiba.
        while (1) Sleep();          // vége.
    }
    memset(lcd_buffer,0x20,32);            // feltöltés space-szel
    initLCD();
    strcpy(lcd_buffer, "uMOGI v1.0 ready");
    refreshLCD();
    
    szamlalo = 0, szamlalo2 = 0;
    maszk = 1, data = 0;
    maszk64 = 1, data64 = 0, frame64 = 0;
//    memset(data_tomb, 0, 33);
//    memset(frame_tomb, 0, 33);
    int data_tomb[33] = {0};
    int frame_tomb[33] = {0};
    KIMENET = 0;

    while (1)
    {      
        memset(lcd_buffer,0x20,32); // space-k
        
//        switch(readchar) {          // magyar karakterek cseréje
//            case 'á': readchar = 0x00; break;
//            case 'é': readchar = 0x01; break;
//            case 'í': readchar = 0x02; break;
//            case 'ó': readchar = 0x03; break;
//            case 'ú': readchar = 0x04; break;
//            case 'ü': readchar = 0x05; break;
//            case  ??: readchar = 0x06; break;
//            case  ??: readchar = 0x07; break;
//            case 'ö': readchar = 0xEF; break;
//        }
        sprintf(lcd_buffer, "sz:%i    ", szamlalo);
        sprintf(lcd_buffer+16, "0x%x     ", frame64);

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

void lcd_clock_e(char b,char rs) { 
  // Utasítás / adat fél-bájt kiírása LCD-re
  // A ledek is ugyanazokon a lábakon vannak, ez a kiírás felfüggeszti a
  // ledek müködését.
  
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
  for (i=0; i<16; i++) lcd_data(lcd_buffer[i],1);
  lcd_data(0xC0,0);                 // line 2 alsó sor
  for (i=16; i<32; i++) lcd_data(lcd_buffer[i],1);
}

uint8_t getOddParity(uint8_t p) 
{ 
  p = p ^ (p >> 4 | p << 4); 
  p = p ^ (p >> 2); 
  p = p ^ (p >> 1); 
  return ~p & 1; 
} 

//void __attribute__((__interrupt__, __auto_psv__)) _INT1Interrupt (void)
//{
//    KIMENET = !KIMENET;
//    maszk = maszk<<1;
//    szamlalo++;
//    if (ADATLAB) data |= maszk;
//    if (maszk == 0x2000) {
//        frameunion.frame = data;
//        maszk = 1;
//        data = 0;
//    }
//    IFS1bits.INT1IF = 0;
//}