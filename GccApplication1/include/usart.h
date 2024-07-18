/*
 * usart.h
 *
 * Created: 11.07.2021 09:45:12
 *  Author: ppien
 */ 


#ifndef USART_H_
#define USART_H_

void usart_init(uint16_t ubrr);
char usart_getchar( void );
void usart_putchar( char data );
void usart_pstr (char *s);
unsigned char usart_kbhit(void);
int usart_putchar_printf(char var, FILE *stream);
int usart_read_line(char *data, int size);

#endif /* USART_H_ */