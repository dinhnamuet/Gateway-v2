#ifndef __LORA_H__
#define __LORA_H__
#include <stdint.h>
#include "gateway.h"
#include "sx1278.h"

void LoRa_start(void);
int lora_transmit(uint8_t *data);
int lora_receive(uint8_t *rx);
int register_recv_signal_from_driver(void);
int unregister_recv_signal_from_driver(void);
void format_pkt(struct LoRa_packet pkt, uint8_t *buff);
void LoRa_init(struct LoRa_node *LoRa, uint32_t new_id);

int LoRa_gotoMode(int mode);
int LoRa_setFrequency(int frequency);
int LoRa_setSpreadingFactor(uint8_t SF);
int LoRa_setPower(uint8_t power);
int LoRa_setBandWidth(uint8_t BW);
int LoRa_getRSSI(void);
int LoRa_setCodingRate(uint8_t cR);
int LoRa_setSyncWord(uint8_t syncword);

#endif /* __LORA_H__ */
