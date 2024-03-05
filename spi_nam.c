#include "spi_nam.h"
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#define set(reg,bit) reg|=(1UL<<bit);
#define reset(reg,bit) reg&=~(1UL<<bit);

uint32_t __iomem *spi_base = NULL;
uint32_t __iomem *gpio_base = NULL;

uint8_t spi_init(void)
{
	uint32_t mid_val;
	//alloc virtual memory
	spi_base = (uint32_t *)ioremap(SPI0_BASE_ADDR, SPI0_LENGTH);
	gpio_base = (uint32_t *)ioremap(GPIO_BASE_ADDR, GPIO_LENGTH);
	if(spi_base == NULL || gpio_base == NULL)
	{
		printk(KERN_ERR "Mapping failure!\n");
		return -1;
	}
	//config chip select
	mid_val = ioread32(gpio_base + GPIO_GPFSEL0_OFFSET/4);
	set(mid_val, 29);
	reset(mid_val, 28);
	reset(mid_val, 27); // MISO
	
	set(mid_val, 26);
	reset(mid_val, 25);
	reset(mid_val, 24); //CS
	iowrite32(mid_val, (gpio_base + GPIO_GPFSEL0_OFFSET/4));
	
	mid_val = ioread32(gpio_base + GPIO_GPFSEL1_OFFSET/4);
	set(mid_val, 2);
	reset(mid_val, 1);
	reset(mid_val, 0);
	
	set(mid_val, 5);
	reset(mid_val, 4);
	reset(mid_val, 3);
	iowrite32(mid_val, (gpio_base + GPIO_GPFSEL1_OFFSET/4));
	// config CS Register
	mid_val = ioread32(spi_base + SPI0_CS_OFFSET/4);
	reset(mid_val, 25); // single byte
	reset(mid_val, 21); //CS active low
	reset(mid_val, 13); //spi master mode
	reset(mid_val, 10); //no int
	reset(mid_val, 9); //no int
	reset(mid_val, 8); //no DMA
	reset(mid_val, 3); //CPOL = 0
	reset(mid_val, 2); //CPHA = 0
	reset(mid_val, 1);
	reset(mid_val, 0);
	iowrite32(mid_val, (spi_base + SPI0_CS_OFFSET/4));
	//config CLK Register
	mid_val = 200;
	iowrite32(mid_val, (spi_base + SPI0_CLK_OFFSET/4));
	return 0;
}
void spi_free(void)
{
	iounmap(spi_base);
	iounmap(gpio_base);
}
void ss_enable(void)
{
	uint32_t mid_val;
	mid_val = ioread32(spi_base + SPI0_CS_OFFSET/4);
	set(mid_val, 7);
	iowrite32(mid_val, (spi_base + SPI0_CS_OFFSET/4));
}
void ss_disable(void)
{
	uint32_t mid_val;
        mid_val = ioread32(spi_base + SPI0_CS_OFFSET/4);
        reset(mid_val, 7);
        iowrite32(mid_val, (spi_base + SPI0_CS_OFFSET/4));
}
void spi_send_a_byte(uint8_t data)
{
	while(!(ioread32(spi_base + SPI0_CS_OFFSET/4) & (1UL << 18)));
	iowrite8(data, (spi_base + SPI0_FIFO_OFFSET/4));
	while(!(ioread32(spi_base + SPI0_CS_OFFSET/4) & (1UL << 16)));
}
void spi_burst_write(uint8_t *data)
{
	while(*data)
	{
		spi_send_a_byte(*data++);
		spi_receive();
	}
}
uint8_t spi_receive(void)
{
	while(!(ioread32(spi_base + SPI0_CS_OFFSET/4) & (1UL << 17)));
	return ioread8(spi_base + SPI0_FIFO_OFFSET/4);
}

