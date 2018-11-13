/*
 Mikrovez�rl�k alkalmaz�sai
 BMEGEFOAMV1
 
 Projekt17: PS/2 billenyt�zet beolvas�sa �s LCD-re �r�sa
 
 K�sz�tette:
  K�nny� M�t�
  R�v�sz Levente
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
#define ADATLAB PORTBbits.RB5
#define CLKLAB PORTBbits.RB7

// delay
#define SYS_FREQ 79227500L
#define FCY SYS_FREQ/2 
#include <libpic30.h>

// UART
#define BAUDRATE 9600
#define BRGVAL ((FCY/BAUDRATE)/16) - 1
char * joe; // 101-es l�gi� !
int i;
unsigned char readchar;
uint16_t maszk, data;
union {
    struct {
        uint16_t startbit:1;
        uint16_t data:8;
        uint16_t parity:1;
        uint16_t stopbit:1;
    } framebits;
    uint16_t frame;
} frameunion;

// magyar �kezetes karakterek
const unsigned char hu_char[] = {
0x02,0x04,0x0E,0x01,0x0F,0x11,0x0F,0x00, // �
0x02,0x04,0x0E,0x11,0x1F,0x10,0x0E,0x00, // �
0x02,0x04,0x0C,0x04,0x04,0x04,0x0E,0x00, // �
0x02,0x04,0x0E,0x11,0x11,0x11,0x0E,0x00, // �
0x02,0x04,0x11,0x11,0x11,0x13,0x0D,0x00, // �
0x0A,0x00,0x11,0x11,0x11,0x13,0x0D,0x00, // �
0x05,0x0A,0x11,0x11,0x11,0x13,0x0D,0x00, // ��
0x05,0x0A,0x0E,0x11,0x11,0x11,0x0E,0x00, // ��
};

void initUART1(void);
void initLCD(void);
void refreshLCD(void);
unsigned char getOddParity(unsigned char p); 

void __attribute__((__interrupt__, __auto_psv__)) _INT0Interrupt (void)
{
    if (ADATLAB) {
        data |= maszk;
    }
    if (!(maszk & 1<<10)){          // ha m�g nem k�sz�lt el a 11. bit
        maszk = maszk << 1;
    }
    else {                          // elk�sz�lt a frame
        frameunion.frame = data;
        maszk = 0;
        data = 0;
    }
    IFS0bits.INT0IF=0;
}

int main(void) 
{
  // oscillator init 80 Mhz
    CLKDIV=0x3000;
    PLLFBD=41;                      // 0=2, 1=3, 41=43
    while (! OSCCONbits.LOCK) ;
    RCONbits.SWDTEN=0;              // disable Watchdog Timer 
  
    TRISB=0x0fff;                   // b12..b15 LCD output
                                    // b11 RX bemenet
  // az osccon-hoz unlock kell ! bit1: lposcen legyen 1, bit6: iolock legyen 0
  //PPSUnLock
    __builtin_write_OSCCONL(OSCCON & 0xbf); // bit6 0
    // l�bak �tkonfigur�l�sa ide j�het
    
  //PPSLock
    __builtin_write_OSCCONL(OSCCON | 0x40); // bit6 1
    
  // INT0 interrupt
    IEC0bits.INT0IE = 1;            // INT0 IRQ Enable
    INTCON2bits.INT0EP = 1;         // Interrupt on negative edge
    IPC0bits.INT0IP = 0b111;        // 7-es priorit�s
    
  // lcd - 2x16 karakter
    joe =(char *)malloc(33); // heap size-t a linkerben �llitani >=44
    if (!joe) // nem siker�lt malloc-olni 
    {
        LED_R=1; // hiba.
        while (1) Sleep();          // v�ge.
    }
    memset(joe,0x20,32);            // felt�lt�s space-szel
    initLCD();
  //  LED_B=1;
    strcpy(joe, "uMOGI v1.0 ready");
    refreshLCD();
  // init k�sz, j�het a feldolgoz� v�gtelen cilkus  
    while (1)
    {      
        memset(joe,0x20,32); // space-k
        readchar = 'A';
        if (getOddParity(frameunion.framebits.data) == frameunion.framebits.parity) 
            readchar = frameunion.framebits.data;
        else 
            readchar = '?';         // Parit�sbit hib�s
        
//        switch(readchar) {          // magyar karakterek cser�je
//            case '�': readchar = 0x00; break;
//            case '�': readchar = 0x01; break;
//            case '�': readchar = 0x02; break;
//            case '�': readchar = 0x03; break;
//            case '�': readchar = 0x04; break;
//            case '�': readchar = 0x05; break;
//            case  ??: readchar = 0x06; break;
//            case  ??: readchar = 0x07; break;
//            case '�': readchar = 0xEF; break;
//        }

        sprintf(joe+16, "0x%x -> %c", readchar, readchar);
        joe[i]=0; i++; i&=0x0f;     // 0..15 k�zt valami szalad
        refreshLCD();               // mig az lcd-t irjuk, bejon az uj adat
        __delay_ms(250);
    }
    return 0;
}

// lcd. d4-d7 : port b12-b15. k�z�s l�b a ledekkel
// e: ra1: h valid data
// r/w:gnd
// rs: ra0 l:instruction, h:data
#define LCD_RS LATAbits.LATA0
#define LCD_E LATAbits.LATA1

void lcd_clock_e(char b,char rs) { 
  // Utas�t�s / adat f�l-b�jt ki�r�sa LCD-re
  // A ledek is ugyanazokon a l�bakon vannak, ez a ki�r�s felf�ggeszti a
  // ledek m�k�d�s�t.
  
  // b: data/instrzction byte
  // rs false: instruction, true:data
    int x=LATB;                     // led �llapot ment�s 
  // ha m�sra is haszn�ltuk, csak bitenk�nt
    LATBbits.LATB12= b & 1;         // d4
    LATBbits.LATB13=(b & 2)>>1;     // d5
    LATBbits.LATB14=(b & 4)>>2;     // d6
    LATBbits.LATB15=(b & 8)>>3;     // d7
  // A l�bakra (12,13,14,15) kirakjuk az adatot/utas�t�st
    LCD_RS=rs;                      // rs kint
  // clock kezd�dik
    LCD_E=1;                        // e:high
    Nop(); Nop(); Nop(); Nop(); // 220-300 ns kell.
  // clock v�gz�dik
    LCD_E=0;                        // e:low
    LATB=x;                         // ledek vissza
    __delay_us(40); // 40 us. 30-n�l m�gikus karakterek
}

void lcd_data(char b,char rs) 
{ 
  /* b b�jtot elfelezi, �s kik�ldi az LCD-re a k�t fel�t. */
  // rs:false:instruction, true:data
  char x;
  x=(b & 0xf0)>>4;                  // upper half byte
  lcd_clock_e(x,rs);
  x=(b & 0x0f);                     // lower half 
  lcd_clock_e(x,rs);
}

void lcd_cgram(void)                
{
  /* Magyar karakterek defini�l�sa */
  lcd_data(0x40,0);                 // set cgram parancs
  int i;
  for (i=0;i<64;i++) {              // magyar karakterek felt�lt�se
      lcd_data(hu_char[i],1);
  }
  lcd_data(0x80,0); // kurzor vissza a DDRAM elej�re
}
void initLCD(void)
{
// rs �s e outputra    
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
  /* A joe[] t�mb tartalm�t ki�rja az LCD-re */
  int i;
  lcd_data(0x80,0);                 // line 1 fels� sor
  for (i=0; i<16; i++) lcd_data(joe[i],1);
  lcd_data(0xC0,0);                 // line 2 als� sor
  for (i=16; i<32; i++) lcd_data(joe[i],1);
}

unsigned char getOddParity(unsigned char p) 
     { 
      p = p ^ (p >> 4 | p << 4); 
      p = p ^ (p >> 2); 
      p = p ^ (p >> 1); 
      return ~p & 1; 
     } 