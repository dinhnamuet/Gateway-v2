#include "lora.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#define SX1278 "/dev/sx-1278"
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
    //memset(LoRa->working_time, 0, sizeof(struct time_set));
}
int LoRa_gotoMode(int mode)
{
    int fd;
    fd = open(SX1278, O_RDWR);
    if (-1 == fd)
        return -1;
    switch(mode)
    {
	case SLEEP:
		ioctl(fd, SLEEP_SET, NULL);
		break;
	case STANDBY:
		ioctl(fd, STANDBY_SET, NULL);
		break;
	case TRANSMIT:
		ioctl(fd, TRANSMIT_SET, NULL);
		break;
	case RXCONTINOUS:
		ioctl(fd, RXCONTINOUS_SET, NULL);
		break;
	case RXSINGLE:
		ioctl(fd, RXSINGLE_SET, NULL);
		break;
	default:
		break;
    }
    close(fd);
    return 0;
}

