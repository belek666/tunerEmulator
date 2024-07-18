
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "usart.h"
#include "can.h"

// -----------------------------------------------------------------------------
/** Set filters and masks.
 *
 * The filters are divided in two groups:
 *
 * Group 0: Filter 0 and 1 with corresponding mask 0.
 * Group 1: Filter 2, 3, 4 and 5 with corresponding mask 1.
 *
 * If a group mask is set to 0, the group will receive all messages.
 *
 * If you want to receive ONLY 11 bit identifiers, set your filters
 * and masks as follows:
 *
 *	uint8_t can_filter[] PROGMEM = {
 *		// Group 0
 *		MCP2515_FILTER(0),				// Filter 0
 *		MCP2515_FILTER(0),				// Filter 1
 *		
 *		// Group 1
 *		MCP2515_FILTER(0),				// Filter 2
 *		MCP2515_FILTER(0),				// Filter 3
 *		MCP2515_FILTER(0),				// Filter 4
 *		MCP2515_FILTER(0),				// Filter 5
 *		
 *		MCP2515_FILTER(0),				// Mask 0 (for group 0)
 *		MCP2515_FILTER(0),				// Mask 1 (for group 1)
 *	};
 *
 *
 * If you want to receive ONLY 29 bit identifiers, set your filters
 * and masks as follows:
 *
 * \code
 *	uint8_t can_filter[] PROGMEM = {
 *		// Group 0
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 0
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 1
 *		
 *		// Group 1
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 2
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 3
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 4
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 5
 *		
 *		MCP2515_FILTER_EXTENDED(0),		// Mask 0 (for group 0)
 *		MCP2515_FILTER_EXTENDED(0),		// Mask 1 (for group 1)
 *	};
 * \endcode
 *
 * If you want to receive both 11 and 29 bit identifiers, set your filters
 * and masks as follows:
 */
const uint8_t can_filter[] PROGMEM = 
{
	// Group 0
	MCP2515_FILTER(0),				// Filter 0
	MCP2515_FILTER(0),				// Filter 1
	
	// Group 1
	MCP2515_FILTER_EXTENDED(0),		// Filter 2
	MCP2515_FILTER_EXTENDED(0),		// Filter 3
	MCP2515_FILTER_EXTENDED(0),		// Filter 4
	MCP2515_FILTER_EXTENDED(0),		// Filter 5
	
	MCP2515_FILTER(0),				// Mask 0 (for group 0)
	MCP2515_FILTER_EXTENDED(0),		// Mask 1 (for group 1)
};
// You can receive 11 bit identifiers with either group 0 or 1.

enum {
	MODE_NONE = 0,
	MODE_TV = 1,
	MODE_CD = 2,
	MODE_RADIO = 3
} modes;

#define BAUD 19200
#define MYUBRR (F_CPU / 16 / BAUD - 1)

#define TUNER_ID 0x264
#define NAVI_ID 0x464
#define LCD_ID_A 0x341
#define LCD_ID_B 0x342
#define MODE_ID 0x35E

#define RASP_PWR PB0
#define MAX_NO_RESPONSE 3 //1min

static FILE mystdout = FDEV_SETUP_STREAM(usart_putchar_printf, NULL, _FDEV_SETUP_WRITE);

static const char tunerStrings[2][8] = { { 0x54, 0x56, 0x2F, 0x56, 0x49, 0x44, 0x45, 0x4F }, { 0x1C, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 } };
static char raspStrings[2][8] = { "RASPBERR", "Y 3B+   " };

static const char NAVI_LEFT[] = { 0x00, 0x00, 0x01, 0x01, 0x00 };
static const char NAVI_UP[] = { 0x00, 0x00, 0x01, 0x02, 0x00 };
static const char NAVI_RIGHT[] = { 0x00, 0x00, 0x01, 0x03, 0x00 };
static const char NAVI_DOWN[] = { 0x00, 0x00, 0x01, 0x20, 0x00 };
static const char NAVI_PLUS[] = { 0x00, 0x00, 0x01, 0x00, 0x20 };
static const char NAVI_MINUS[] = { 0x00, 0x00, 0x01, 0x00, 0x01 };
static const char NAVI_MODE[] = { 0x00, 0x00, 0x01, 0x00, 0x03 };
static const char NAVI_RETURN[] = { 0x00, 0x00, 0x01, 0x00, 0x50 };
static const char NAVI_SCROLL_RIGHT[] = { 0x00, 0x00, 0x01, 0x01, 0xFF };
static const char NAVI_SCROLL_LEFT[] = { 0x00, 0x00, 0x01, 0x00, 0xFF };
static const char NAVI_SCROLL_PRESS[] = { 0x00, 0x00, 0x01, 0x00, 0x05 };
static const char NAVI_AS[] = { 0x00, 0x00, 0x01, 0x04, 0x00 };

static bool tuner_init(void);
static bool manage_inputs(void);
static void lcd_strings(void);
static void manage_power(void);
static void power_off(void);
static void power_on(void)


static bool tunerInited = false;
static uint8_t mode;
static volatile bool powerOn = false;
static uint8_t no_resp = 0;
static volatile uint32_t lockCnt = 0;

ISR (TIMER0_OVF_vect)
{
	TCNT0 = 0; //32ms
	lockCnt++;
	if (lockCnt >= 18750) { //10min
		tunerInited = false;
		PORTB &= ~(1 << RASP_PWR); //low
		powerOn = false;
		lockCnt = 0;
	}
}

int main(void) {
	DDRB |= (1 << RASP_PWR); //output
	power_off();
	
	TCNT0 = 0; //32ms
	TCCR0 = (1 << CS00) | (1 << CS02); //1024 prescaler
	TIMSK = (1 << TOIE0); //enable interrupt
	sei();
	
    stdout = &mystdout;
    usart_init(MYUBRR);
	
	int start = 10;
	while (start--) {
		printf("*");
	}
	printf("Started!");
	
	// Initialize MCP2515
	can_init(BITRATE_100_KBPS);
	
	// Load filters and masks
	can_static_filter(can_filter);
	
	while (1) {
		lockCnt = 0;
		if (!tunerInited) {
			if (!tuner_init()) {
				printf("Failed to init tuner!\n");
				_delay_ms(3000);
				if (powerOn) {
					manage_power();
				}
			}
			else {
				printf("Tuner inited\n");
				tunerInited = true;
				mode = MODE_NONE;
				no_resp = 0;
				power_on();
			}
		}
		else {
			if (!manage_inputs()) {
				manage_power();
			}
			//lcd_strings();
		}
	}
	
	return 0;
}

static void power_off(void) {
	PORTB &= ~(1 << RASP_PWR); //low
	powerOn = false;
}

static void power_on(void) {
	PORTB |= (1 << RASP_PWR); //hi
	powerOn = true;
}

static void manage_power(void) {
	no_resp++;
	if (no_resp > MAX_NO_RESPONSE) {
		tunerInited = false;
		lockCnt = 0;
		for (uint8_t i = 0; i < 20; i++) { //2min to power off
			printf("NAVI_TURN_OFF\n");
			_delay_ms(6000);
		}
		PORTB &= ~(1 << RASP_PWR); //low
		powerOn = false;
		no_resp = 0;
	}
}

static bool recive_msg(can_t *msg) {
	int delay = 5;
	int timeout = 20 * 1000 / delay; //20s timeout
	memset(msg, 0, sizeof(can_t));
	while (timeout) {
		if (can_check_message() && can_get_message(msg)) {
			return true;
		}
		else {
			_delay_ms(delay);
			timeout--;
		}
	}
	return false;
}

static bool tuner_init(void) {
	can_t msg;
	
	uint8_t cnt = 0;
	
	msg.id = TUNER_ID;
	msg.flags.rtr = 0;
	msg.flags.extended = 0;
	
	msg.length = 2;
	msg.data[0] = 0xA1;
	msg.data[1] = 0x01;
	can_send_message(&msg);
	
	while (recive_msg(&msg)) {
		if (msg.id == NAVI_ID) {
			/*printf("DEBUG: id %x len %d length %d\n", msg.id, msg.length, msg.length);
			for (uint8_t i = 0; i < msg.length; i++) {
				printf("0x%02x ", msg.data[i]);
			}
			printf("\n");*/
			if (msg.length == 3 && msg.data[0] == 0xE0 && msg.data[1] == 0x01 && msg.data[2] == 0x00) {
				msg.id = TUNER_ID;
				msg.length = 2;
				msg.data[0] = 0xA1;
				msg.data[1] = 0x01;
				can_send_message(&msg);
				msg.length = 7;
				msg.data[0] = 0x10;
				msg.data[1] = 0x15;
				msg.data[2] = 0x01;
				msg.data[3] = 0x00;
				msg.data[4] = 0x01;
				msg.data[5] = 0x00;
				msg.data[6] = 0x00;
				can_send_message(&msg);
			}
			else if (msg.length == 3 && msg.data[0] == 0x10 && msg.data[1] == 0x00 && msg.data[2] == 0x01) {
				msg.id = TUNER_ID;
				msg.length = 1;
				msg.data[0] = 0xB1;
				can_send_message(&msg);
				msg.length = 3;
				msg.data[0] = 0x11;
				msg.data[1] = 0x01;
				msg.data[2] = 0x01;
				can_send_message(&msg);
			}
			else if (msg.length == 2 && msg.data[0] == 0x11 && msg.data[1] == 0x08) {
				msg.id = TUNER_ID;
				msg.length = 1;
				msg.data[0] = 0xB2;
				can_send_message(&msg);
				msg.length = 7;
				msg.data[0] = 0x12;
				msg.data[1] = 0x09;
				msg.data[2] = 0x01;
				msg.data[3] = 0x42;
				msg.data[4] = 0x50;
				msg.data[5] = 0x63;
				msg.data[6] = 0x1E;
				can_send_message(&msg);
			}
			else if (msg.length == 2 && msg.data[0] == 0x12 && msg.data[1] == 0x22) {
				msg.id = TUNER_ID;
				msg.length = 1;
				msg.data[0] = 0xB3;
				can_send_message(&msg);
				msg.length = 3;
				msg.data[0] = 0x13;
				msg.data[1] = 0x23;
				msg.data[2] = 0x00;
				can_send_message(&msg);
			}
			else if (msg.length == 3 && msg.data[0] == 0x13 && msg.data[1] == 0x26 && msg.data[2] == 0xFF) {
				msg.id = TUNER_ID;
				msg.length = 1;
				msg.data[0] = 0xB4;
				can_send_message(&msg);
			}
			else if (msg.length == 3 && msg.data[0] == 0x14 && msg.data[1] == 0x50 && msg.data[2] == 0x00) {
				msg.id = TUNER_ID;
				if (cnt == 0) {
					msg.length = 4;
					msg.data[0] = 0x14;
					msg.data[1] = 0x0B;
					msg.data[2] = 0x01;
					msg.data[3] = 0x26;
					can_send_message(&msg);
					cnt = 1;
				}
				else {
					msg.length = 1;
					msg.data[0] = 0xB5;
					can_send_message(&msg);
					msg.length = 4;
					msg.data[0] = 0x15;
					msg.data[1] = 0x0B;
					msg.data[2] = 0x01;
					msg.data[3] = 0x50;
					can_send_message(&msg);
				}
			}
			else if (msg.length == 1 && msg.data[0] == 0xB6) {
				return true;
			}
		}
	}
	return false;
}

static bool manage_inputs(void) {
	can_t msg;
	uint8_t byte;
	
	if (recive_msg(&msg)) {
		switch (msg.id) {
			case NAVI_ID:
					/*printf("DEBUG: id %x len %d length %d\n", msg.id, msg.length, msg.length);
					for (uint8_t i = 0; i < msg.length; i++) {
						printf("0x%02x ", msg.data[i]);
					}
					printf("\n");*/
				if (msg.length == 5) {
					uint8_t pressed = msg.data[2] == 1;
					byte = 0xB0 + (msg.data[0] & 0x0F) + 1;
					msg.data[0] = 0x00;
					msg.data[1] = 0x00;
					msg.data[2] = 0x01;
					if (!memcmp(msg.data, NAVI_LEFT, 5)) {
						printf("NAVI_LEFT %d\n", pressed);
					}
					else if (!memcmp(msg.data, NAVI_UP, 5)) {
						printf("NAVI_UP %d\n", pressed);
					}
					else if (!memcmp(msg.data, NAVI_RIGHT, 5)) {
						printf("NAVI_RIGHT %d\n", pressed);
					}
					else if (!memcmp(msg.data, NAVI_DOWN, 5)) {
						printf("NAVI_DOWN %d\n", pressed);
					}
					else if (!memcmp(msg.data, NAVI_PLUS, 5)) {
						printf("NAVI_PLUS %d\n", pressed);
					}
					else if (!memcmp(msg.data, NAVI_MINUS, 5)) {
						printf("NAVI_MINUS %d\n", pressed);
					}
					else if (!memcmp(msg.data, NAVI_MODE, 5)) {
						printf("NAVI_MODE %d\n", pressed);
					}
					else if (!memcmp(msg.data, NAVI_RETURN, 5)) {
						printf("NAVI_RETURN %d\n", pressed);
					}
					else if (!memcmp(msg.data, NAVI_SCROLL_RIGHT, 5)) {
						printf("NAVI_SCROLL_RIGHT\n");
					}
					else if (!memcmp(msg.data, NAVI_SCROLL_LEFT, 5)) {
						printf("NAVI_SCROLL_LEFT\n");
					}
					else if (!memcmp(msg.data, NAVI_SCROLL_PRESS, 5)) {
						printf("NAVI_SCROLL_PRESS %d\n", pressed);
					}
					else if (!memcmp(msg.data, NAVI_AS, 5)) {
						printf("NAVI_AS %d\n", pressed);
					}
					if (byte > 0xBF) {
						byte = 0xB0;
					}
					msg.id = TUNER_ID;
					msg.length = 1;
					msg.data[0] = byte;
					can_send_message(&msg);
				}
				else if (msg.length == 2 && msg.data[0] == 0xA3 && msg.data[1] == 0x00) {
					msg.id = TUNER_ID;
					msg.length = 2;
					msg.data[0] = 0xA1;
					msg.data[1] = 0x01;
					can_send_message(&msg);
				}
				else if (msg.length == 3 && msg.data[0] == 0xE0 && msg.data[1] == 0x01 && msg.data[2] == 0x00) {
					tunerInited = false;
				}
				else if (msg.length == 3 && (msg.data[0] & 0x10) && msg.data[1] == 0x00 && msg.data[2] == 0x02) {
					msg.id = TUNER_ID;
					byte = 0xB0 + (msg.data[0] & 0x0F);
					msg.length = 1;
					msg.data[0] = byte;
					can_send_message(&msg);
					msg.length = 3;
					msg.data[0] = 0x16;
					msg.data[1] = 0x01;
					msg.data[2] = 0x02;
					can_send_message(&msg);
					return false;
				}
				break;
			case LCD_ID_A:
				if (msg.length == 8 && mode == MODE_TV /*!memcmp(msg.data, tunerStrings[0], 8)*/) {
					memcpy(msg.data, raspStrings[0], 8);
					can_send_message(&msg);
				}
				break;
			case LCD_ID_B:
				if (msg.length == 8 && mode == MODE_TV /*!memcmp(msg.data, tunerStrings[1], 8)*/) {
					memcpy(msg.data, raspStrings[1], 8);
					can_send_message(&msg);
				}
				break;
			case MODE_ID:
				if (msg.length == 4 && msg.data[0] == 0x01 && msg.data[1] == 0x01 && msg.data[2] == 0x12) {
					switch (msg.data[3]) {
						case 0x37:
							if (mode != MODE_TV) {
								mode = MODE_TV;
								printf("NAVI_TV\n");
							}
							break;
						case 0x38:
							if (mode != MODE_CD) {
								mode = MODE_CD;
								printf("NAVI_CD\n");
							}
							break;
						case 0xA0:
							if (mode != MODE_RADIO) {
								mode = MODE_RADIO;
								printf("NAVI_RADIO\n");
							}
							break;
					}
				}
				break;
		}
		return true;
	}
	return false;
}

static void lcd_strings(void) {
	char str[20];
	if (usart_read_line(str, 20) >= 14) {
		if (!memcmp(str, "LCD_A:", 6)) {
			memcpy(raspStrings[0], str + 6, 8);
		}
		else if (!memcmp(str, "LCD_B:", 6)) {
			memcpy(raspStrings[1], str + 6, 8);
		}
	}
}




