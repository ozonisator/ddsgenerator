/*
Pinbelegung AD9850
	duino	avr
RST	D11	B3
FQ	D10	B2
CLK	D9	B1
DATA	D8	B0


*/




#include "./u8glib/src/u8g.h"
#include <util/delay.h>
#include <avr/io.h>
#include "./AtmelLib/global.h"
#include <stdlib.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <avr/pgmspace.h>

#define BAUD 9600				//Baudrate
#define BAUDRATE ((F_CPU)/(BAUD*16UL)-1)	//Umrechnung für UBBR
#define XTAL	16e6				//16MHz
#define PHASE_A		(PIND & 1<<PD3)		// an Pinbelegung anpassen
#define PHASE_B		(PIND & 1<<PD2)		// an Pinbelegung anpassen

volatile int8_t enc_delta;			// Drehgeberbewegung zwischen
static int8_t last;				// zwei Auslesungen im Hauptprogramm
uint8_t val = 0;

/*
#define MENU_ITEMS 4
char *menu_strings[MENU_ITEMS] = { "First Line", "Second Item", "3333333", "abcdefg" };
uint8_t menu_current = 0;
*/

void uart_init (void)
{
	UBRR0H  = (BAUDRATE>>8);		// Baudrate setzen
	UBRR0L  = BAUDRATE;			// Baudrate setzen
	UCSR0B |= 1<<TXEN0 | 1<<RXEN0;		// Senden/Empfangen enablen
	UCSR0C |= 1<<UCSZ00 | 1<<UCSZ01;	// 8n1 Datenformat
}
void uart_putc(unsigned char c)
{
	while (!(UCSR0A & (1<<UDRE0)));		// warten bis UART frei
	UDR0 = c;				// Zeichen senden
}


void uart_puts (char *s)
{
	while (*s)
		{
		uart_putc(*s);
		s++;
		}
}



 
ISR( TIMER0_COMPA_vect )			// 1ms fuer manuelle Eingabe
{
	int8_t new, diff;
	new = 0;
	if( PHASE_A )
		new = 3;
	if( PHASE_B )
			new ^= 1;		// convert gray to binary
	diff = last - new;			// difference last - new
	if( diff & 1 ){				// bit 0 = value (1)
		last = new;			// store new as next last
		enc_delta += (diff & 2) - 1;	// bit 1 = direction (+/-)
			}
}

void encode_init( void )			// nur Timer 0 initialisieren
{

	int8_t new;
	new = 0;
	if( PHASE_A )
		new = 3;
	if( PHASE_B )
		new ^= 1;					// convert gray to binary
	last = new;						// power on state
	enc_delta = 0;
	TCCR0A = (1<<WGM01);
	TCCR0B = (1<<CS01) | (1<<CS00);				// CTC, XTAL / 64
	OCR0A = (uint8_t)(XTAL / 64.0 * 1e-3 - 0.5);		// 1ms
	TIMSK0 |= 1<<OCIE0A;
}
 
int8_t encode_read( void )					// Encoder auslesen
{
	int8_t val;
	cli();
	val = enc_delta;
	enc_delta = val & 3;
	sei();
	return val >> 2; 
}

u8g_t u8g;

void RST(void) {
	sbi(PORTB, PB3);
	delayus(1);
	cbi(PORTB, PB3);
	delayus(1);
}

void FQ(void) {
	sbi(PORTB, PB2);
	delayus(1);
	cbi(PORTB, PB2);
	delayus(1);
}

void CLK(void) {
	delayus(1);
	sbi(PORTB, PB1);
	delayus(1);
	cbi(PORTB, PB1);

}

void sendAD9850(uint32_t freq, uint8_t phase){
uint32_t word = ((uint64_t)freq<<26) / 1953125;

	for (uint8_t i=0; i<32; i++) {			// frequenz tuning wort senden
		if ((word>>i)&0x01) {
			sbi(PORTB, PB0);
			}
		 else {
			cbi(PORTB, PB0);
			}
	CLK();
	}
	cbi(PORTB, PB0);				// CONTROL Bits 0, pwdwn ebenfalls 0
	CLK();
	CLK();
	CLK();

	for (uint8_t i=0; i<5; i++) {			// phasen tuning wort senden
		if ((phase>>i)&0x01) {
			sbi(PORTB, PB0);
			}
		 else {
			cbi(PORTB, PB0);
			}
	CLK();
	}
	
	FQ();						// frequenz update impuls
}


void AD9850reset(){
	RST();
	CLK();
	cbi(PORTB, PB0);
	FQ();
}


//displaytest)
void draw(void)
{
	u8g_SetFont(&u8g, u8g_font_gdr17);
	val += (encode_read());
	u8g_DrawStr(&u8g, 40, 25, u8g_u16toa(val,3));
	
}



/*
//menü test analog beispiel u8glib
void draw_menu(void) {
	uint8_t i, h;
	u8g_uint_t w, d;
	u8g_SetFont(&u8g, u8g_font_6x13);
	u8g_SetFontRefHeightText(&u8g);
	u8g_SetFontPosTop(&u8g);
	h = u8g_GetFontAscent(&u8g)-u8g_GetFontDescent(&u8g);
	w = u8g_GetWidth(&u8g);
	for( i = 0; i < MENU_ITEMS; i++ ) {        // draw all menu items
		d = (w-u8g_GetStrWidth(&u8g, menu_strings[i]))/2;
		u8g_SetDefaultForegroundColor(&u8g);
		if ( i == menu_current ) {               // current selected menu item
		u8g_DrawBox(&u8g, 0, i*h+1, w, h);     // draw cursor bar
		u8g_SetDefaultBackgroundColor(&u8g);
			}
		u8g_DrawStr(&u8g, d, i*h, menu_strings[i]);
		}
}
*/


int main (void) {
	DDRB = 1<<PB0 | 1<<PB1 | 1<<PB2 | 1<<PB3;
	DDRD = 0xff;
	char s[7];
	delayms(1);
	AD9850reset();
	delayms(1);
	sendAD9850(40000, 0);				
	encode_init();
	uart_init();
	sei();

	u8g_InitI2C(&u8g, &u8g_dev_ssd1306_128x64_i2c, U8G_I2C_OPT_NONE);
while(1) {
	for(;;)
		{  
		u8g_FirstPage(&u8g);
		do
		{
		//draw_menu();   
		draw();
		}
		while ( u8g_NextPage(&u8g) );
			//menu_current = val;
			uart_puts( itoa( val, s, 10 ) );
			uart_putc('\n');			// \n line feed
			uart_putc('\r');
		}
}
}
