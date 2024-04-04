#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include "oled.h"
#define OLED "/dev/oled"

int put_data_to_screen(struct node_info *node)
{
    int fd;
    fd = open(OLED, O_RDWR);
    if(-1 == fd)
        return -1;
    ioctl(fd, PUT_NODE_INFO, node);
    close(fd);
    return 0;
}
