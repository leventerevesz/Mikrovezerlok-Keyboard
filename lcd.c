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

char * joe; // 101-es légió !
int i,poti;
double volt;  // ==float. gcc opció: 64 bit double

void initLCD(void);
void refreshLCD(void);

void __attribute__((__interrupt__, __auto_psv__)) _ADC1Interrupt (void)
{
     poti= ADC1BUF0; // az A/D kimenete
     volt=(double)poti/1023*3.3; // 10 bit=3.3v
     IFS0bits.AD1IF=0;
}

int main(void) 
{
    // oscillator init 80 Mhz
    CLKDIV=0x3000;
    PLLFBD=41; // 0=2, 1=3, 41=43
    while (! OSCCONbits.LOCK) ;
    RCONbits.SWDTEN=0;
  
  // gpio: rgb led az rb12..rb14-en,lcd az rb15-ön
    TRISB=0x0fff; // b12..b15 output
  // az osccon-hoz unlock kell ! bit1: lposcen legyen 1, bit6: iolock legyen 0
    __builtin_write_OSCCONL(OSCCON | ((1<<1) & ~(1<<6))); 
    
     // RPOR7bits.RP14R=0x01; //25-os lab C1OUT: kek led
     __builtin_write_OSCCONL(OSCCON | (1<<6)); // iolock 1. soha többé ...
    // analóg komparátor. 
    // b14-en LED-et, ha az optokapu értéke kisebb, mint a potméter/sw2
    // jumpert analógra kapcsolni (jobb oldal)! 
    // poti/sw2: c1in-, optokapu/sw3: c1in+
    //CMCONbits.C1OUTEN=1; // output enable
    //CMCONbits.C1INV=1; // inverted, vin+ < vin-
    //CMCONbits.C1POS=1; // vin+
    //CMCONbits.C1NEG=0; // vin-
    //CMCONbits.C1EN=1; // start ! a komparátor kimenete a led. nem kell program
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
    // A/D
    AD1PCFGL=0xffcf;  // AN4 es AN5 analog labak
    AD1CON2bits.VCFG = 0b000;// ref+=VDD, ref-=VSS
    AD1CON3bits.ADRC= 0;// CPU orajel
    // ADC konverzios idoegysege
    AD1CON3bits.ADCS = 3; //Tad= 4 x Tcy= 100ns >76ns
    // mintavételezési id?
    AD1CON3bits.SAMC = 31; //31Tad
    AD1CON1bits.SSRC=0b111; //Automata konverzió
    AD1CON1bits.AD12B =0; //10 bites felbontás
    AD1CON1bits.ADON = 1; //ADC bekapcs 
    IFS0bits.AD1IF=0;
    IPC3bits.AD1IP=1; // prioritás
    IEC0bits.AD1IE=1; // enable adc1 int
    i=0; poti=0; volt=0;
    __delay_ms(500);
   // init kész, jöhet a feldolgozó végtelen cilkus  
    while (1)
    {       
        memset(joe,0x20,32); // space-k
        sprintf(joe+16, "AN%i=%4i U=%4.2fV",
                AD1CHS0bits.CH0SA,poti, volt);
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

void lcd_cgram(void) // Egy <3 ikont definiál az LCD kijelz?n
{
  lcd_data(0x40,0); // set cgram, and define char 0
  lcd_data(0,1); // 0 8x5
  lcd_data(0x0a,1); // 1
  lcd_data(0x15,1); // 2 
  lcd_data(0x11,1); // 3 
  lcd_data(0x0a,1); // 4 
  lcd_data(0x04,1); // 5 
  lcd_data(0,1); // 6 
  lcd_data(0,1); // 7 
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

