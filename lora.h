#ifndef __LORA_H__
#define __LORA_H__
#include <stdint.h>
#include "gateway.h"
int lora_transmit(uint8_t *data);
int lora_receive(uint8_t *rx);
int register_recv_signal_from_driver(void);
int unregister_recv_signal_from_driver(void);
void format_pkt(struct LoRa_packet pkt, uint8_t *buff);
void LoRa_init(struct LoRa_node *LoRa, uint32_t new_id);
int LoRa_gotoMode(int mode);
#endif
