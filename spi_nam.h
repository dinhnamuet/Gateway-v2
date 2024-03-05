#ifndef __SPI_NAM_H__
#define __SPI_NAM_H__

#define GPIO_BASE_ADDR	0xFE200000
#define GPIO_LENGTH	0xF0
#define GPIO_GPFSEL0_OFFSET 0x00
#define GPIO_GPFSEL1_OFFSET	0x04
#define GPSET0_OFFSET	0x1C
#define GPCLR0_OFFSET	0x28
#define GPLEV0_OFFSET	0x34
#define PULLUP1_OFFSET	0xE8

#define SPI0_BASE_ADDR		0xFE204000
#define SPI0_LENGTH		0x18
#define SPI0_CS_OFFSET		0x00
#define SPI0_FIFO_OFFSET	0x04
#define SPI0_CLK_OFFSET		0x08
#define SPI0_DLEN_OFFSET	0x0C
#define SPI0_LTOH_OFFSET	0x10
#define SPI0_DC_OFFSET		0x14

uint8_t spi_init(void);
void spi_free(void);
void spi_send_a_byte(uint8_t data);
uint8_t spi_receive(void);
void ss_enable(void);
void ss_disable(void);
void spi_burst_write(uint8_t *data);

#endif
