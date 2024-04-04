#ifndef __OLED_H__
#define __OLED_H__
#include <stdint.h>
typedef enum {
	AUTO,
	MANUAL
} lmode_t;
struct node_info {
	uint32_t id;
	lmode_t mode;
	uint32_t node_count;
	uint8_t illuminance;
};

#define PUT_NODE_INFO _IOW('b', '1', struct node_info *)

int put_data_to_screen(struct node_info *node);

#endif /* __OLED_H__ */