#include "lora.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#define SX1278 "/dev/lora-0"
void LoRa_start(void)
{
	LoRa_gotoMode(RXCONTINUOUS_MODE);
}
int lora_transmit(uint8_t *data)
{
    int fd = open(SX1278, O_RDWR);
    if (-1 == fd)
        return -1;
    write(fd, data, strlen(data));
    close(fd);
    return 0;
}
int lora_receive(uint8_t *rx)
{
	int fd = open(SX1278, O_RDWR);
	if (-1 == fd)
		return -1;
	read(fd, rx, PACKET_SIZE);
	close(fd);
    	return 0;
}
int register_recv_signal_from_driver(void)
{
   	int fd;
	fd = open(SX1278, O_RDWR);
	if (-1 == fd)
        	return -1;
	if (ioctl(fd, RG_SIGNAL, NULL) < 0)
    	{
        	close(fd);
        	return -1;
    	}
	close(fd);
    return 0;
}
int unregister_recv_signal_from_driver(void)
{
    int fd;
	fd = open(SX1278, O_RDWR);
	if (-1 == fd)
		return -1;
	if (ioctl(fd, CTL_SIGNAL, NULL) < 0)
	{
		close(fd);
        return -1;
	}
	close(fd);
    return 0;
}
void format_pkt(struct LoRa_packet pkt, uint8_t *buff)
{
    int i = 0;
	memset(buff, 0, PACKET_SIZE);
    // Type
    buff[0] = pkt.pkt_type;
	// UID
	buff[1] = pkt.uid >> 24;
	buff[2] = (pkt.uid >> 16) & 0xFF;
	buff[3] = (pkt.uid >> 8) & 0xFF;
	buff[4] = pkt.uid & 0xFF;
	// Destination ID
	buff[5] = pkt.destination_id >> 24;
	buff[6] = (pkt.destination_id >> 16) & 0xFF;
	buff[7] = (pkt.destination_id >> 8) & 0xFF;
	buff[8] = pkt.destination_id & 0xFF;
	// Data length
	buff[9] = pkt.data_length;
	// Data
	for (i = 0; i < pkt.data_length; i++)
	{
		buff[BASE_DATA + i] = pkt.data[i];
	}
}
void LoRa_init(struct LoRa_node *LoRa, uint32_t new_id)
{
    LoRa->id                    = new_id;
    LoRa->current_mode          = MODE_AUTO;
    LoRa->current               = 0;
    LoRa->illuminance            = 0;
    LoRa->light_sensor_value    = 0;
    LoRa->voltage               = 0;
}
int LoRa_gotoMode(int mode)
{
    int fd;
    fd = open(SX1278, O_RDWR);
    if (-1 == fd)
        return -1;
	ioctl(fd, GOTO_MODE, &mode);
    close(fd);
    return 0;
}
int LoRa_setFrequency(int frequency)
{
	int fd;
	fd = open(SX1278, O_RDWR);
	if (-1 == fd)
		return -1;
	if (ioctl(fd, FREQUENCY, &frequency) < 0)
	{
		close(fd);
        return -1;
	}
	close(fd);
    return 0;
}
int LoRa_setSpreadingFactor(uint8_t SF)
{
	int fd;
	fd = open(SX1278, O_RDWR);
	if (-1 == fd)
		return -1;
	if (ioctl(fd, SPREADING_FACTOR, &SF) < 0)
	{
		close(fd);
        return -1;
	}
	close(fd);
    return 0;
}
int LoRa_setPower(uint8_t power)
{
	int fd;
	fd = open(SX1278, O_RDWR);
	if (-1 == fd)
		return -1;
	if (ioctl(fd, POWER, &power) < 0)
	{
		close(fd);
        return -1;
	}
	close(fd);
    return 0;
}
int LoRa_setBandWidth(uint8_t BW)
{
	int fd;
	fd = open(SX1278, O_RDWR);
	if (-1 == fd)
		return -1;
	if (ioctl(fd, BAND_WIDTH, &BW) < 0)
	{
		close(fd);
        return -1;
	}
	close(fd);
    return 0;
}
int LoRa_getRSSI(void)
{
	int fd;
	int rssi;
	fd = open(SX1278, O_RDWR);
	if (-1 == fd)
		return -1;
	if (ioctl(fd, GET_RSSI, &rssi) < 0)
	{
		close(fd);
        return -1;
	}
	close(fd);
    return rssi - 164;
}
int LoRa_setCodingRate(uint8_t cR)
{
	int fd;
	fd = open(SX1278, O_RDWR);
	if (-1 == fd)
		return -1;
	if (ioctl(fd, CODING_RATE, &cR) < 0)
	{
		close(fd);
        return -1;
	}
	close(fd);
    return 0;
}
int LoRa_setSyncWord(uint8_t syncword)
{
	int fd;
	fd = open(SX1278, O_RDWR);
	if (-1 == fd)
		return -1;
	if (ioctl(fd, SYNC_WORD, &syncword) < 0)
	{
		close(fd);
        return -1;
	}
	close(fd);
    return 0;
}
