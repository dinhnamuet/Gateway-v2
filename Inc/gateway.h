#ifndef __GATEWAY_H__
#define __GATEWAY_H__
#include <stdint.h>

#define BASE_DATA	10
#define PACKET_SIZE	60
#define GATEWAY_ID	20021163
#define SCAN_DURATION	6000
#define ADD_DEVICE	0
#define DATA_AVAILABLE 	1
#define GOT_SOCK 	2
#define CTL_SOCK 	3
#define REMOVE_DEVICE 	4
#define DISCONNECTED 	5
#define NOT_DONE 	0
#define HANDLED 	1

#define GLOBALIP 	282002
#define MODE_MANUAL 	6
#define MODE_AUTO 	7
#define FIND_DATA	8
#define REQUEST_DATA 	2
#define RESPONSE_DATA 	3

#define	SLEEP		0
#define	STANDBY		1
#define	TRANSMIT	2
#define	RXCONTINOUS	3
#define RXSINGLE	4

#pragma pack(1)

struct time_set
{
	int h_start_0;
	int h_stop_0;
	int m_start_0;
	int m_stop_0;

	int h_start_50;
	int h_stop_50;
	int m_start_50;
	int m_stop_50;

	int h_start_75;
	int h_stop_75;
	int m_start_75;
	int m_stop_75;

	int h_start_100;
	int h_stop_100;
	int m_start_100;
	int m_stop_100;
};

struct device_command
{
	uint8_t request;
	uint8_t ack;
	char data_to_send[1024];
	uint32_t id_to_handler;
	struct time_set user_set;
};

struct handling
{
	uint32_t id_handling;
	uint8_t status;
};

struct LoRa_packet
{
	uint32_t uid;
	uint32_t destination_id;
	uint8_t pkt_type;
	uint8_t data_length;
	char data[PACKET_SIZE - BASE_DATA];
};

struct LoRa_node
{
	uint32_t current_mode;
	uint32_t id;
	uint32_t light_sensor_value;
	uint32_t illuminance;
	float voltage;
	float current;
	struct time_set working_time;
};
#pragma pack()
#endif
/* RESPONSE PACKET DATA:	light intensity +  illuminance + voltage + current */
/* REQUEST PACKET DATA:		HOURS + MIN + SEC */
/* SET MANUAL:				hstart0 mstart0 hstop0 mstop0 | hstart50 mstart50 hstop50 mstop50 | hstart75 mstart75 hstop75 mstop75 |
hstart100 mstart100 hstop100 mstop100*/
/* SET AUTO:				HOURS + MIN + SEC */
