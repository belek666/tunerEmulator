/*
 * i2c.c
 *
 * Created: 11.07.2021 09:46:57
 *  Author: ppien
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void I2C_start(void) {
     TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTA);
     while (!(TWCR & (1 << TWINT)));
}

void I2C_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
    while (!(TWCR & (1 << TWSTO)));
}

void I2C_write(uint8_t data) {
     TWDR = data;
     TWCR = (1 << TWINT) | (1 << TWEN);
     while (!(TWCR & (1 << TWINT)));
}

uint8_t I2C_read(uint8_t ACK) {
    TWCR = (1 << TWINT) | (ACK << TWEA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
    return TWDR;
}

uint8_t IC2ReadByte(uint8_t address, uint8_t subaddress, uint8_t *data) {
	I2C_start();
	if ((TWSR & 0xF8) != 0x08) {
		printf("IC2ReadByte: start error 0x%x\n\r", (TWSR & 0xF8));
		return 0;
	}
	I2C_write((address << 1) | 0); //write subaddress
	if ((TWSR & 0xF8) != 0x18) {
		printf("IC2ReadByte: write address error 0x%x\n\r", (TWSR & 0xF8));
		return 0;
	}
	I2C_write(subaddress);
	if ((TWSR & 0xF8) != 0x28) {
		printf("IC2ReadByte: write subaddress error 0x%x\n\r", (TWSR & 0xF8));
		return 0;
	}
	I2C_start();
	I2C_write((address << 1) | 1); //read data from subaddress
	if ((TWSR & 0xF8) != 0x40) {
		printf("IC2ReadByte: write address2 error 0x%x\n\r", (TWSR & 0xF8));
		return 0;
	}
	if (data != NULL) {
		*data = I2C_read(1);
		if ((TWSR & 0xF8) != 0x50) {
			printf("IC2ReadByte: read data error 0x%x\n\r", (TWSR & 0xF8));
			return 0;
		}
		I2C_stop();
		return 1;
	}
	return 0;
}

uint8_t IC2WriteByte(uint8_t address, uint8_t subaddress, uint8_t data) {
	I2C_start();
	if ((TWSR & 0xF8) != 0x08) {
		printf("IC2WriteByte: start error 0x%x\n\r", (TWSR & 0xF8));
		return 0;
	}
	I2C_write((address << 1) | 0); //write
	if ((TWSR & 0xF8) != 0x18) {
		printf("IC2WriteByte: write address error 0x%x\n\r", (TWSR & 0xF8));
		return 0;
	}
	I2C_write(subaddress);
	if ((TWSR & 0xF8) != 0x28) {
		printf("IC2WriteByte: write subaddress error 0x%x\n\r", (TWSR & 0xF8));
		return 0;
	}
	I2C_write(data);
	if ((TWSR & 0xF8) != 0x28) {
		printf("IC2WriteByte: write data error 0x%x\n\r", (TWSR & 0xF8));
		return 0;
	}
	I2C_stop();
	return 1;
}
