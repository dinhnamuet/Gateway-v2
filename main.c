#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include "firebase.h"
#include "gateway.h"
#include "lora.h"
#define shared "foo"
sigset_t newmask;
volatile sig_atomic_t rx_sig = 0;
volatile sig_atomic_t socket_flag = 0;
volatile sig_atomic_t ready_flag = 0;
uint8_t sock_status = 0;
char buff[PACKET_SIZE];
pthread_t recv_t, snd;
pid_t pid;
pid_t ppid;
struct LoRa_node *newLoRa = NULL;
volatile uint32_t node_count = 0;
int communication;

static int add_device(uint32_t new_id);
static int remove_device(uint32_t device_id);
static int handler_rx_data(uint8_t *buff, struct device_command *dev, struct handling *node);
static int get_data_from_node(struct device_command *dev, struct handling *node);
void sig_chld(int num);
void sig_int(int num);
void sig_usr(int num);
static void scan_node_list(struct device_command *dev);
static void block_sigusr1(void);
static void unblock_sigusr1(void);

void *send_msg(void *args)
{
	struct device_command *new = (struct device_command *)args;
	while (1)
	{
		if (ready_flag)
		{
			write(communication, new->data_to_send, strlen(new->data_to_send));
			ready_flag = 0;
			sleep(1);
			new->ack = 1;
		}
		else
		{
			sleep(0.1);
		}
	}
}
void *recv_msg(void *args)
{
	char RxD_Buff[1024];
	int type;
	char *token = NULL;
	struct device_command *new = (struct device_command *)args;
	while (1)
	{
		memset(RxD_Buff, 0, 1024);
		if (read(communication, RxD_Buff, 1024))
		{
			if (strncmp(RxD_Buff, "exit", 4) == 0)
			{
				pthread_cancel(snd);
				break;
			}
			else
			{
				token = strtok(RxD_Buff, " ");
				type = atoi(token);
				switch (type)
				{
				case ADD_DEVICE:
					token = strtok(NULL, " ");
					new->request = ADD_DEVICE;
					new->id_to_handler = atoi(token);
					kill(ppid, SIGUSR2);
					break;
				case REMOVE_DEVICE:
					token = strtok(NULL, " ");
					new->request = REMOVE_DEVICE;
					new->id_to_handler = atoi(token);
					kill(ppid, SIGUSR2);
					break;
				case MODE_AUTO:
					token = strtok(NULL, " ");
					new->request = MODE_AUTO;
					new->id_to_handler = atoi(token);
					kill(ppid, SIGUSR2);
					break;
				case MODE_MANUAL:
					printf("Set Manual\n");
					token = strtok(NULL, " ");
					new->request = MODE_MANUAL;
					new->id_to_handler = atoi(token);

					token = strtok(NULL, " ");
					new->user_set.h_start_0 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.m_start_0 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.h_stop_0 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.m_stop_0 = atoi(token);

					token = strtok(NULL, " ");
					new->user_set.h_start_50 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.m_start_50 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.h_stop_50 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.m_stop_50 = atoi(token);

					token = strtok(NULL, " ");
					new->user_set.h_start_75 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.m_start_75 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.h_stop_75 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.m_stop_75 = atoi(token);

					token = strtok(NULL, " ");
					new->user_set.h_start_100 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.m_start_100 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.h_stop_100 = atoi(token);
					token = strtok(NULL, " ");
					new->user_set.m_stop_100 = atoi(token);

					kill(ppid, SIGUSR2);
					break;
				default:
					break;
				}
			}
		}
		else
		{
			pthread_cancel(snd);
			break;
		}
	}
	close(communication);
	pthread_exit(NULL);
}
static void handler(int num)
{
	socket_flag = 1;
}
int main(int argc, char *argv[])
{
	int shr_fd = shm_open(shared, O_CREAT | O_RDWR, 666);
	if (-1 == shr_fd)
	{
		printf("Create shared memory failure\n");
		return -1;
	}
	ftruncate(shr_fd, sizeof(struct device_command));
	pid = fork();
	if (pid > 0) // parent process
	{
		int tmp;
		int i = 0;
		char tx_buff[PACKET_SIZE];
		unsigned long timeout = SCAN_DURATION;
		struct handling raspi;
		struct LoRa_packet gateway;
		struct timeval tim;
		struct tm *sys_tim;
		struct device_command *share_device;
		share_device = (struct device_command *)mmap(NULL, sizeof(struct device_command), PROT_READ | PROT_WRITE, MAP_SHARED, shr_fd, 0);
		if (share_device == NULL)
		{
			printf("System call mmap failure\n");
			return -1;
		}
		signal(SIGCHLD, sig_chld);
		signal(SIGINT, sig_int);
		signal(SIGUSR1, sig_usr);
		signal(SIGUSR2, handler);
		register_recv_signal_from_driver();
		printf("LoRa GateWay init is successfully!\n");
		gateway.uid		= GATEWAY_ID;
		while (1)
		{
			if (node_count > 0)
			{
				gettimeofday(&tim, NULL);
				sys_tim = localtime(&tim.tv_sec);
				gateway.pkt_type = REQUEST_DATA;
				gateway.destination_id = newLoRa[i].id;
				memset(gateway.data, 0, PACKET_SIZE - BASE_DATA);
				sprintf(gateway.data, "%d %d %d", sys_tim->tm_hour, sys_tim->tm_min, sys_tim->tm_sec);
				gateway.data_length = strlen(gateway.data);
				format_pkt(gateway, tx_buff);
				raspi.id_handling = newLoRa[i].id;
				raspi.status = NOT_DONE;
				lora_transmit(tx_buff);
				printf("\nRequest sent to %d\n", gateway.destination_id);
				i = (i == node_count - 1) ? 0 : (i + 1);
			}
			while (--timeout)
			{
				if(raspi.status == HANDLED)
					break;
				if (rx_sig)
					get_data_from_node(share_device, &raspi);
				if (socket_flag)
				{
					switch (share_device->request)
					{
					case ADD_DEVICE:
						if (add_device(share_device->id_to_handler) < 0)
						{
							memset(share_device->data_to_send, 0, 1024);
							sprintf(share_device->data_to_send, "%s", "Failed");
							share_device->request = ADD_DEVICE;
							share_device->ack = 0;
							kill(pid, SIGUSR2);
							while (!(share_device->ack))
							{
								sleep(0.1);
							}
						}
						else
						{
							memset(share_device->data_to_send, 0, 1024);
							sprintf(share_device->data_to_send, "%d %d", ADD_DEVICE, share_device->id_to_handler);
							share_device->request = ADD_DEVICE;
							share_device->ack = 0;
							kill(pid, SIGUSR2);
							while (!(share_device->ack))
							{
								sleep(0.1);
							}
						}
						break;
					case REMOVE_DEVICE:
						if (remove_device(share_device->id_to_handler) < 0)
						{
							memset(share_device->data_to_send, 0, 1024);
							sprintf(share_device->data_to_send, "%s", "Failed");
							share_device->request = REMOVE_DEVICE;
							share_device->ack = 0;
							kill(pid, SIGUSR2);
							while (!(share_device->ack))
							{
								sleep(0.1);
							}
						}
						else
						{
							memset(share_device->data_to_send, 0, 1024);
							sprintf(share_device->data_to_send, "%d %d", REMOVE_DEVICE, share_device->id_to_handler);
							share_device->request = REMOVE_DEVICE;
							share_device->ack = 0;
							kill(pid, SIGUSR2);
							while (!(share_device->ack))
							{
								sleep(0.1);
							}
						}
						break;
					case GOT_SOCK:
						scan_node_list(share_device);
						sock_status = 1;
						break;
					case CTL_SOCK:
						sock_status = 0;
						break;
					case MODE_MANUAL:
						for (tmp = 0; tmp < node_count; tmp++)
						{
							if (share_device->id_to_handler == newLoRa[tmp].id)
							{
								memcpy(&newLoRa[tmp].working_time, &share_device->user_set, sizeof(struct time_set));
								gateway.pkt_type = MODE_MANUAL;
								gateway.destination_id = newLoRa[tmp].id;
								memset(gateway.data, 0, PACKET_SIZE - BASE_DATA);
								sprintf(gateway.data, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
										newLoRa[tmp].working_time.h_start_0, newLoRa[tmp].working_time.m_start_0, newLoRa[tmp].working_time.h_stop_0, newLoRa[tmp].working_time.m_stop_0,
										newLoRa[tmp].working_time.h_start_50, newLoRa[tmp].working_time.m_start_50, newLoRa[tmp].working_time.h_stop_50, newLoRa[tmp].working_time.m_stop_50,
										newLoRa[tmp].working_time.h_start_75, newLoRa[tmp].working_time.m_start_75, newLoRa[tmp].working_time.h_stop_75, newLoRa[tmp].working_time.m_stop_75,
										newLoRa[tmp].working_time.h_start_100, newLoRa[tmp].working_time.m_start_100, newLoRa[tmp].working_time.h_stop_100, newLoRa[tmp].working_time.m_stop_100);
								gateway.data_length = strlen(gateway.data);
								format_pkt(gateway, tx_buff);
								lora_transmit(tx_buff);
								printf("%s\n", gateway.data);
								break;
							}
						}
						break;
					case MODE_AUTO:
						for (tmp = 0; tmp < node_count; tmp++)
						{
							if (share_device->id_to_handler == newLoRa[tmp].id)
							{
								gettimeofday(&tim, NULL);
								sys_tim = localtime(&tim.tv_sec);
								gateway.pkt_type = MODE_AUTO;
								gateway.destination_id = newLoRa[tmp].id;
								memset(gateway.data, 0, PACKET_SIZE - BASE_DATA);
								sprintf(gateway.data, "%d %d %d", sys_tim->tm_hour, sys_tim->tm_min, sys_tim->tm_sec);
								gateway.data_length = strlen(gateway.data);
								format_pkt(gateway, tx_buff);
								lora_transmit(tx_buff);
								break;
							}
						}
						break;
					default:
						break;
					}
					socket_flag = 0;
				}
				usleep(1000);
			}
			if (raspi.status != HANDLED && sock_status && node_count > 0)
			{
				printf("ID %d Disconnected\n", raspi.id_handling);
				memset(share_device->data_to_send, 0, 1024);
				sprintf(share_device->data_to_send, "%d %d", DISCONNECTED, raspi.id_handling);
				share_device->request = DISCONNECTED;
				share_device->ack = 0;
				kill(pid, SIGUSR2);
				while (!(share_device->ack))
				{
					sleep(0.1);
				}
			}
			else if(raspi.status == HANDLED)
				sleep(1);
			timeout = SCAN_DURATION;
		}
	}
	else if (!pid) // child process
	{
		char client_ip[50];
		ppid = getppid();
		int server_socket, len;
		struct sockaddr_in server_addr, client_addr;
		struct device_command *share_device;
		share_device = (struct device_command *)mmap(NULL, sizeof(struct device_command), PROT_READ | PROT_WRITE, MAP_SHARED, shr_fd, 0);
		if (share_device == NULL)
		{
			printf("System call mmap failure\n");
			return -1;
		}
		signal(SIGUSR2, sig_usr);
		server_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (-1 == server_socket)
		{
			printf("Socket create failed\n");
			return -1;
		}
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(2000);
		server_addr.sin_addr.s_addr = inet_addr("192.168.1.162");
		if (bind(server_socket, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
		{
			printf("Socket binding failure\n");
			return -1;
		}
		listen(server_socket, 1);
		len = sizeof(client_addr);
		while (1)
		{
			communication = accept(server_socket, (struct sockaddr *)&client_addr, (socklen_t *)&len);
			memset(client_ip, 0, 50);
			inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, sizeof(client_ip));
			share_device->request = GOT_SOCK;
			kill(ppid, SIGUSR2);
			printf("Client IP: %s is connected!\n", client_ip);
			pthread_create(&snd, NULL, &send_msg, share_device);
			pthread_create(&recv_t, NULL, &recv_msg, share_device);
			pthread_join(snd, NULL);
			pthread_join(recv_t, NULL);
			share_device->request = CTL_SOCK;
			kill(ppid, SIGUSR2);
			printf("Disconnected from %s!\n", client_ip);
		}
	}
	else
		return -1;
	return 0;
}

void sig_chld(int num)
{
	wait(NULL);
	shm_unlink(shared);
	exit(EXIT_SUCCESS);
}
static int add_device(uint32_t new_id)
{
	if (node_count == 0)
	{
		node_count++;
		newLoRa = (struct LoRa_node *)calloc(node_count, sizeof(struct LoRa_node));
		LoRa_init(&newLoRa[node_count - 1], new_id);
		db_add_node(newLoRa[node_count - 1]);
		return 0;
	}
	else
	{
		for (int i = 0; i < node_count; i++)
		{
			if (newLoRa[i].id == new_id)
			{
				printf("Device existed!\n");
				return -1;
			}
		}
		node_count++;
		newLoRa = (struct LoRa_node *)realloc(newLoRa, node_count * sizeof(struct LoRa_node));
		LoRa_init(&newLoRa[node_count - 1], new_id);
		db_add_node(newLoRa[node_count - 1]);
		return 0;
	}
}
static int remove_device(uint32_t device_id)
{
	uint8_t flag = 0;
	for (int i = 0; i < node_count; i++)
	{
		if (newLoRa[i].id == device_id)
		{
			flag = 1;
			db_remove_node(newLoRa[i]);
			for (int j = i; j < node_count - 1; j++)
			{
				newLoRa[j] = newLoRa[j + 1];
			}
			node_count--;
			newLoRa = (struct LoRa_node *)realloc(newLoRa, node_count * sizeof(uint32_t));
			break;
		}
	}
	if (flag)
		return 0;
	else
		return -1;
}
static void scan_node_list(struct device_command *dev)
{
	for (int i = 0; i < node_count; i++)
	{
		dev->ack = 0;
		memset(dev->data_to_send, 0, 1024);
		sprintf(dev->data_to_send, "%d %d", ADD_DEVICE, newLoRa[i].id);
		printf("Sending %s ...!\n", dev->data_to_send);
		dev->request = ADD_DEVICE;
		kill(pid, SIGUSR2);
		while (!(dev->ack))
		{
			sleep(0.1);
		}
	}
	printf("Done!\n");
}
static int get_data_from_node(struct device_command *dev, struct handling *node)
{
	char buff[PACKET_SIZE];
	block_sigusr1();
	if(node->status == HANDLED)
	{
		rx_sig = 0;
		return -1;
	}
	memset(buff, 0, PACKET_SIZE);
	lora_receive(buff);
	if(buff)
		handler_rx_data(buff, dev, node);
	unblock_sigusr1();
	rx_sig = 0;
	return 0;
}
static int handler_rx_data(uint8_t *buff, struct device_command *dev, struct handling *node)
{
	uint8_t i;
	int ret_val, _light, _ill,_mode;
	float _vol, _curr;
	struct LoRa_packet foo;
	/* Unpack LoRa packet */
	foo.pkt_type = buff[0];
	if(foo.pkt_type != RESPONSE_DATA)
	{
		//LoRa_gotoMode(RXSINGLE);
		return -1;
	}
	foo.uid = (uint32_t)(buff[1] << 24 | buff[2] << 16 | buff[3] << 8 | buff[4]);
	foo.destination_id = (uint32_t)(buff[5] << 24 | buff[6] << 16 | buff[7] << 8 | buff[8]);
	foo.data_length = buff[9];
	memset(foo.data, 0, PACKET_SIZE - BASE_DATA);
	for (i = 0; i < foo.data_length; i++)
	{
		foo.data[i] = buff[BASE_DATA + i];
	}
	printf("\nData from %d: %s\n", foo.uid, foo.data);
	if (foo.pkt_type == RESPONSE_DATA && foo.destination_id == GATEWAY_ID)
	{
		for (i = 0; i < node_count; i++)
		{
			if (foo.uid == newLoRa[i].id)
			{
				ret_val = sscanf(foo.data, "%d %d %f %f %d", &_light, &_ill, &_vol, &_curr, &_mode);
				if(ret_val == 5)
				{
					newLoRa[i].light_sensor_value	= _light;
				       	newLoRa[i].illuminance		= _ill;
					newLoRa[i].voltage		= _vol;
					newLoRa[i].current		= _curr;
					newLoRa[i].current_mode		= _mode;
					db_update_data(newLoRa[i]);
				}
				else
				{
					printf("Data format does not match\n");
					return -1;
				}
				if(sock_status)
				{
					memset(dev->data_to_send, 0, 1024);
					sprintf(dev->data_to_send, "%d %d %s", DATA_AVAILABLE, newLoRa[i].id, foo.data);
					dev->request = DATA_AVAILABLE;
					dev->ack = 0;
					kill(pid, SIGUSR2);
					while (!(dev->ack))
					{
						sleep(0.1);
					}
				}
				if (foo.uid == node->id_handling)
					node->status = HANDLED;
				break;
			}
		}
	}
	return 0;
}
void sig_int(int num)
{
	unregister_recv_signal_from_driver();
	free(newLoRa);
	shm_unlink(shared);
	exit(EXIT_SUCCESS);
}
void sig_usr(int num)
{
	switch (num)
	{
	case SIGUSR1:
		rx_sig = 1;
		break;
	case SIGUSR2:
		ready_flag = 1;
		break;
	default:
		break;
	}
}
static void block_sigusr1(void)
{
	sigemptyset(&newmask);
	sigaddset(&newmask, SIGUSR1);
 	sigprocmask(SIG_BLOCK, &newmask, NULL);
}
static void unblock_sigusr1(void)
{
	sigprocmask(SIG_UNBLOCK, &newmask, NULL);
}
/* RESPONSE PACKET DATA:	light intensity +  illuminance + voltage + current */
