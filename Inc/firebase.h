#ifndef __FIREBASE_H__
#define __FIREBASE_H__
#include <stdint.h>
#include <curl/curl.h>
#include "gateway.h"
#define firebaseKey "AIzaSyBIcYm19PAbZdMmmtm1lwk6yXQzUi9KK2E"
void db_add_node(struct LoRa_node LoRa);
void db_remove_node(struct LoRa_node LoRa);
void db_update_data(struct LoRa_node LoRa);
#endif
