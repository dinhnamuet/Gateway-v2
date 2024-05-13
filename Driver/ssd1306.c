#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/mutex.h>
#include "ssd1306.h"

const int max_X = OLED_WIDTH / FONT_X - 1;
const int max_Y = OLED_HEIGHT / 8 - 1;

struct ssd1306 {
	struct i2c_client *client;
	u8 current_X;
	u8 current_Y;
	u8 *buffer;
	struct work_struct workqueue;
	struct timer_list my_timer;
	struct miscdevice miscdev;
	struct mutex lock;
	struct kern_time time;
};
status_t getLoRa_stt(void *args);
static void ssd1306_write(struct ssd1306 *oled, u8 data, write_mode_t mode);
static void ssd1306_init(struct ssd1306 *oled);
static void ssd1306_clear(struct ssd1306 *oled);
static void ssd1306_goto_xy(struct ssd1306 *oled, u8 x, u8 y);
static void ssd1306_clear_row(struct ssd1306 *oled, u8 row);
static void ssd1306_send_char(struct ssd1306 *oled, u8 data);
static void ssd1306_send_char_inv(struct ssd1306 *oled, u8 data);
static void ssd1306_send_string(struct ssd1306 *oled, u8 *str, color_t color);
static void ssd1306_go_to_next_line(struct ssd1306 *oled);
static int ssd1306_burst_write(struct ssd1306 *oled, const u8 *data, int len,
			       write_mode_t mode);
static void getinfo(struct work_struct *work);
static void tmHandler(struct timer_list *tm);
static int get_network_ip_by_name(struct ssd1306 *oled, const char *name,
				  line_t line);
static void get_time_of_day(struct kern_time *rt);
static void print_time(struct ssd1306 *oled, line_t line);
static int get_cpu_temperature_and_print(struct ssd1306 *oled, line_t line);
static status_t get_lora_status_and_print(struct ssd1306 *oled, line_t line);
static void show_lastest_node_info(struct ssd1306 *oled,
				   struct node_info *node);
static int ssd1306_draw_pixel(struct ssd1306 *oled, u8 x, u8 y,
			      color_t color);
static void ssd1306_draw_bitmap(struct ssd1306 *oled, u8 x, u8 y,
				const u8 *bitmap, u8 width,
				u8 height, color_t color);
static void ssd1306_sync(struct ssd1306 *oled);

static int oled_open(struct inode *inodep, struct file *filep);
static int oled_close(struct inode *inodep, struct file *filep);
static ssize_t oled_write(struct file *filep, const char __user *ubuff,
			  size_t size, loff_t *offset);
static long oled_ioctl(struct file *filep, unsigned int cmd,
		       unsigned long data);

struct file_operations fops = { .owner = THIS_MODULE,
				.open = oled_open,
				.release = oled_close,
				.write = oled_write,
				.unlocked_ioctl = oled_ioctl };

static int oled_probe(struct i2c_client *client)
{
	u8 i = 0;
	struct ssd1306 *oled = NULL;
	oled = devm_kzalloc(&client->dev, sizeof(*oled), GFP_KERNEL);
	if (!oled) {
		pr_err("kzalloc failed\n");
		return -ENOMEM;
	}
	oled->client = client;
	i2c_set_clientdata(client, oled);
	oled->buffer = kzalloc(OLED_WIDTH * OLED_HEIGHT / 8, GFP_KERNEL);
	if (!oled->buffer) {
		pr_err("kzalloc failed\n");
		return -ENOMEM;
	}
	ssd1306_init(oled);
	oled->miscdev.name = "oled";
	oled->miscdev.minor = MISC_DYNAMIC_MINOR;
	oled->miscdev.fops = &fops;
	if (misc_register(&oled->miscdev) < 0) {
		pr_err("Create misc device failed\n");
		goto rm_buff;
	}
	mutex_init(&oled->lock);
	INIT_WORK(&oled->workqueue, getinfo);
	timer_setup(&oled->my_timer, tmHandler, 0);
	for (i = 0; i < 4; i ++)
	{
		ssd1306_draw_bitmap(oled, 0, 0, bootAnimation[i], 128, 64, COLOR_WHITE);
		msleep(1500);
	}
	ssd1306_clear(oled);
	ssd1306_goto_xy(oled, 4, 0);
	ssd1306_send_string(oled, "LoRa Gateway", COLOR_WHITE);
	ssd1306_goto_xy(oled, 0, 2);
	ssd1306_send_string(oled, "Please Wait ...!", COLOR_WHITE);
	oled->my_timer.expires = jiffies + 5 * HZ;
	add_timer(&oled->my_timer);
	pr_info("oled init successfully\n");
	return 0;
rm_buff:
	kfree(oled->buffer);
	return -EFAULT;
}
static void oled_remove(struct i2c_client *client)
{
	struct ssd1306 *oled = i2c_get_clientdata(client);
	if (!oled) {
		pr_err("Cannot get data\n");
	} else {
		cancel_work_sync(&oled->workqueue);
		del_timer(&oled->my_timer);
		ssd1306_clear(oled);
		kfree(oled->buffer);
		misc_deregister(&oled->miscdev);
		mutex_destroy(&oled->lock);
		ssd1306_write(oled, 0xAE, COMMAND); // display off
		pr_info("%s, %d\n", __func__, __LINE__);
	}
}
static void board_shutdown(struct i2c_client *client)
{
	u8 i;
	struct ssd1306 *oled = i2c_get_clientdata(client);
	ssd1306_clear(oled);
	for( i = 0; i < 2; i ++)
	{
		ssd1306_draw_bitmap(oled, 0, 0, shutdownAnimation[i], 128, 64, COLOR_WHITE);
		msleep(1500);
	}
	ssd1306_clear(oled);
	ssd1306_goto_xy(oled, 7, 3);
	msleep(10);
	ssd1306_send_string(oled, "GoodBye!", COLOR_WHITE);
	msleep(2000);
	ssd1306_write(oled, 0xAE, COMMAND);
}
static const struct i2c_device_id oled_device_id[] = { { .name = "nam", 0 },
						       {} };
MODULE_DEVICE_TABLE(i2c, oled_device_id);
static const struct of_device_id oled_of_match_id[] = {
	{
		.compatible = "ssd1306-oled,nam",
	},
	{}
};
MODULE_DEVICE_TABLE(of, oled_of_match_id);

static struct i2c_driver oled_driver = {
	.probe = oled_probe,
	.remove = oled_remove,
	.shutdown = board_shutdown,
	.driver = {
		.name = "oled",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(oled_of_match_id),
	},
	.id_table = oled_device_id,
};

module_i2c_driver(oled_driver);

static void ssd1306_write(struct ssd1306 *oled, u8 data, write_mode_t mode)
{
	/*
	A control byte mainly consists of Co and D/C# bits following by six “0”
	Co | D/C | 000000
	Co bit is equal to 0
	*/
	u8 buff[2];
	if (mode == DATA)
		buff[0] = 0x40; // data
	else
		buff[0] = 0x00; // command
	buff[1] = data;
	mutex_lock(&oled->lock);
	i2c_master_send(oled->client, buff, 2);
	mutex_unlock(&oled->lock);
}
static int ssd1306_burst_write(struct ssd1306 *oled, const u8 *data, int len,
			       write_mode_t mode)
{
	int res;
	u8 *buff;
	buff = kmalloc(len + 1, GFP_KERNEL);
	if (!buff)
		return -1;
	memset(buff, 0, len + 1);
	if (mode == DATA)
		buff[0] = 0x40; // data
	else
		buff[0] = 0x00; // command
	memcpy(&buff[1], data, len);
	mutex_lock(&oled->lock);
	res = i2c_master_send(oled->client, buff, len + 1);
	mutex_unlock(&oled->lock);
	kfree(buff);
	return res;
}
static void ssd1306_init(struct ssd1306 *oled)
{
	msleep(15);
	// set Osc Frequency
	ssd1306_write(oled, 0xD5, COMMAND);
	ssd1306_write(oled, 0x80, COMMAND);
	// set MUX Ratio
	ssd1306_write(oled, 0xA8, COMMAND);
	ssd1306_write(oled, 0x3F, COMMAND);
	// set display offset
	ssd1306_write(oled, 0xD3, COMMAND);
	ssd1306_write(oled, 0x00, COMMAND);
	// set display start line
	ssd1306_write(oled, 0x40, COMMAND);
	// Enable charge pump regulator
	ssd1306_write(oled, 0x8D, COMMAND);
	ssd1306_write(oled, 0x14, COMMAND);
	// Set memory addressing mode
	ssd1306_write(oled, 0x20, COMMAND);
	ssd1306_write(oled, 0x00, COMMAND);
	// Set segment remap with column address 0 mapped to segment 0
	ssd1306_write(oled, 0xA0, COMMAND);
	ssd1306_write(oled, 0xC0, COMMAND);
	// set COM Pin hardware configuration
	ssd1306_write(oled, 0xDA, COMMAND);
	ssd1306_write(oled, 0x12, COMMAND);
	// set contrast control
	ssd1306_write(oled, 0x81, COMMAND);
	ssd1306_write(oled, 0x7F, COMMAND);
	// Set pre-charge period
	ssd1306_write(oled, 0xD9, COMMAND);
	ssd1306_write(oled, 0xF1, COMMAND);
	// Set Vcomh deselect level
	ssd1306_write(oled, 0xDB, COMMAND);
	ssd1306_write(oled, 0x20, COMMAND);
	// disable entire display on
	ssd1306_write(oled, 0xA4, COMMAND);
	// set normal display
	ssd1306_write(oled, 0xA6, COMMAND); //A6 normal a7 inverse
	// set segment re-map
	ssd1306_write(oled, 0xA0, COMMAND);
	// deactive scroll
	ssd1306_write(oled, 0x2E, COMMAND);
	// display on
	ssd1306_write(oled, 0xAF, COMMAND);
	// clear screen
	ssd1306_clear(oled);
}
static void ssd1306_clear(struct ssd1306 *oled)
{
	int i;
	for (i = 0; i < 1024; i++)
		ssd1306_write(oled, 0, DATA);
	ssd1306_goto_xy(oled, 0, 0);
}
static void ssd1306_goto_xy(struct ssd1306 *oled, u8 x, u8 y)
{
	ssd1306_write(oled, 0x21, COMMAND);
	ssd1306_write(oled, x * FONT_X, COMMAND);
	ssd1306_write(oled, OLED_WIDTH - 1, COMMAND);
	ssd1306_write(oled, 0x22, COMMAND);
	ssd1306_write(oled, y, COMMAND);
	ssd1306_write(oled, max_Y, COMMAND);
	oled->current_X = x;
	oled->current_Y = y;
}
static void ssd1306_clear_row(struct ssd1306 *oled, u8 row)
{
	int i;
	if (row <= max_Y) {
		ssd1306_goto_xy(oled, 0, row);
		for (i = 0; i < 20 * FONT_X; i++)
			ssd1306_write(oled, 0, DATA);
	}
}
static void ssd1306_send_char(struct ssd1306 *oled, u8 data)
{
	if (oled->current_X == max_X)
		ssd1306_go_to_next_line(oled);
	ssd1306_burst_write(oled, ssd1306_font[data - 32], FONT_X, DATA);
	oled->current_X++;
}
static void ssd1306_send_char_inv(struct ssd1306 *oled, u8 data)
{
	u8 i;
	u8 buff[FONT_X];
	if (oled->current_X == max_X)
		ssd1306_go_to_next_line(oled);
	for (i = 0; i < FONT_X; i++)
		buff[i] = ~ssd1306_font[data - 32][i];
	ssd1306_burst_write(oled, buff, FONT_X, DATA);
	oled->current_X++;
}
static void ssd1306_send_string(struct ssd1306 *oled, u8 *str, color_t color)
{
	while (*str) {
		if (color == COLOR_WHITE)
			ssd1306_send_char(oled, *str++);
		else
			ssd1306_send_char_inv(oled, *str++);
	}
}
static void ssd1306_go_to_next_line(struct ssd1306 *oled)
{
	oled->current_Y = (oled->current_Y == max_Y) ? 0 :
						       (oled->current_Y + 1);
	ssd1306_goto_xy(oled, 0, oled->current_Y);
}
static void getinfo(struct work_struct *work)
{
	struct ssd1306 *oled = container_of(work, struct ssd1306, workqueue);
	if (oled) {
		get_network_ip_by_name(oled, "wlan0", WIFI);
		print_time(oled, WALL_TIME);
		get_cpu_temperature_and_print(oled, CPU_TEMP);
		get_lora_status_and_print(oled, LORA);
	}
	mod_timer(&oled->my_timer, jiffies + 60 * HZ);
}
static void tmHandler(struct timer_list *tm)
{
	struct ssd1306 *oled = container_of(tm, struct ssd1306, my_timer);
	if (oled)
		schedule_work(&oled->workqueue);
}
static int get_network_ip_by_name(struct ssd1306 *oled, const char *name,
				  line_t line)
{
	char ip_address[20];
	struct net_device *dev = NULL;
	struct in_device *indev = NULL;
	dev = dev_get_by_name(&init_net, name);
	if (!dev) {
		ssd1306_clear_row(oled, line);
		ssd1306_goto_xy(oled, 0, line);
		ssd1306_send_string(oled, "WIFI is disable!", COLOR_WHITE);
		return -1;
	}
	indev = in_dev_get(dev);
	if (indev) {
		memset(ip_address, 0, sizeof(ip_address));
		sprintf(ip_address, "IP: %pI4", &indev->ifa_list->ifa_local);
		ssd1306_clear_row(oled, line);
		ssd1306_goto_xy(oled, 0, line);
		ssd1306_send_string(oled, ip_address, COLOR_WHITE);
		return 0;
	} else
		return -1;
}
static void get_time_of_day(struct kern_time *rt)
{
	time64_t seconds;
	seconds = ktime_to_ns(ktime_get_real()) / NSEC_PER_SEC;
	seconds += 7 * 3600; // GMT +7
	rt->hour = (seconds / 3600) % 24;
	rt->min = (seconds / 60) % 60;
}
static void print_time(struct ssd1306 *oled, line_t line)
{
	u8 buff[20];
	memset(buff, 0, 20);
	get_time_of_day(&oled->time);
	sprintf(buff, "Wall Time: %2d:%2d", oled->time.hour, oled->time.min);
	ssd1306_goto_xy(oled, 0, line);
	ssd1306_send_string(oled, buff, COLOR_WHITE);
}
static int get_cpu_temperature_and_print(struct ssd1306 *oled, line_t line)
{
	int cpu_temperature;
	u8 a[10];
	struct file *file;
	file = filp_open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY, 0);
	if (!file) {
		pr_err("Cannot open sysfs\n");
		return -ENOMEM;
	}
	memset(a, 0, sizeof(a));
	file->f_pos = 0;
	if (kernel_read(file, a, sizeof(a), &file->f_pos) < 0) {
		pr_err("Cannot read sysfs\n");
		filp_close(file, NULL);
		return -1;
	}
	filp_close(file, NULL);
	if (kstrtoint(a, 0, &cpu_temperature) < 0) {
		pr_err("Cannot parsing data to int\n");
		return -EINVAL;
	}
	sprintf(a, "CPU Temp: %d*C", cpu_temperature / 1000);
	ssd1306_clear_row(oled, line);
	ssd1306_goto_xy(oled, 0, line);
	ssd1306_send_string(oled, a, COLOR_WHITE);
	return 0;
}
static status_t get_lora_status_and_print(struct ssd1306 *oled, line_t line)
{
	u8 foo[20];
	status_t lora_status;
	struct file *file;
	file = filp_open("/dev/lora-0", O_RDWR, 0);
	if (!file) {
		pr_err("Cannot open devfs\n");
		return -ENOMEM;
	}
	lora_status = getLoRa_stt(file->private_data);
	filp_close(file, NULL);
	memset(foo, 0, sizeof(foo));
	sprintf(foo, "LoRa is %s!",
		(lora_status == LORA_OK) ? "Okay" : "Disabled");
	ssd1306_clear_row(oled, line);
	ssd1306_goto_xy(oled, 0, line);
	ssd1306_send_string(oled, foo, COLOR_WHITE);
	return lora_status;
}
static void show_lastest_node_info(struct ssd1306 *oled, struct node_info *node)
{
	u8 buff[20];
	memset(buff, 0, sizeof(buff));
	sprintf(buff, "Manager: %d node", node->node_count);
	ssd1306_clear_row(oled, NUMBER_OF_NODE);
	ssd1306_goto_xy(oled, 0, NUMBER_OF_NODE);
	ssd1306_send_string(oled, buff, COLOR_WHITE);

	memset(buff, 0, sizeof(buff));
	sprintf(buff, "Last ID: %d", node->id);
	ssd1306_clear_row(oled, LAST_ID_HANDLED);
	ssd1306_goto_xy(oled, 0, LAST_ID_HANDLED);
	ssd1306_send_string(oled, buff, COLOR_WHITE);

	memset(buff, 0, sizeof(buff));
	sprintf(buff, "Brightness: %d", node->illuminance);
	ssd1306_clear_row(oled, BRIGHTNESS);
	ssd1306_goto_xy(oled, 0, BRIGHTNESS);
	ssd1306_send_string(oled, buff, COLOR_WHITE);

	memset(buff, 0, sizeof(buff));
	sprintf(buff, "Mode: %s", (node->mode == AUTO) ? "Auto" : "Manual");
	ssd1306_clear_row(oled, MODE);
	ssd1306_goto_xy(oled, 0, MODE);
	ssd1306_send_string(oled, buff, COLOR_WHITE);
}
static int oled_open(struct inode *inodep, struct file *filep)
{
	struct ssd1306 *oled =
		container_of(filep->private_data, struct ssd1306, miscdev);
	if (oled)
		filep->private_data = oled;
	return 0;
}
static int oled_close(struct inode *inodep, struct file *filep)
{
	filep->private_data = NULL;
	return 0;
}
static ssize_t oled_write(struct file *filep, const char __user *ubuff,
			  size_t size, loff_t *offset)
{
	struct ssd1306 *oled = filep->private_data;
	memset(oled->buffer, 0, 16);
	if (copy_from_user(oled->buffer, ubuff, size)) {
		pr_err("Write devfs failed\n");
		return -EFAULT;
	}
	ssd1306_send_string(oled, oled->buffer, COLOR_WHITE);
	return size;
}
static long oled_ioctl(struct file *filep, unsigned int cmd, unsigned long data)
{
	struct node_info mynode;
	struct ssd1306 *oled = filep->private_data;
	switch (cmd) {
	case PUT_NODE_INFO:
		if (copy_from_user(&mynode, (struct node_info *)data,
				   sizeof(struct node_info))) {
			pr_err("IOCTL Failed\n");
			return -1;
		}
		show_lastest_node_info(oled, &mynode);
		break;
	default:
		break;
	}
	return 0;
}
static int ssd1306_draw_pixel(struct ssd1306 *oled, u8 x, u8 y,
			      color_t color)
{
	if (x > OLED_WIDTH || y > OLED_HEIGHT)
		return -1;
	if (color == COLOR_WHITE)
		oled->buffer[x + (y / 8) * OLED_WIDTH] |= (1 << (y % 8));
	else
		oled->buffer[x + (y / 8) * OLED_WIDTH] &= ~(1 << (y % 8));
	return 0;
}
static void ssd1306_draw_bitmap(struct ssd1306 *oled, u8 x, u8 y,
				const u8 *bitmap, u8 width,
				u8 height, color_t color)
{
	memset(oled->buffer, 0, (OLED_WIDTH * OLED_HEIGHT / 8));
	int16_t byteWidth = (width + 7) / 8;
	u8 byte = 0;
	for (int16_t j = 0; j < height; j++, y++) {
		for (int16_t i = 0; i < width; i++) {
			if (i & 7)
				byte <<= 1;
			else
				byte = (*(const unsigned char
						  *)(&bitmap[j * byteWidth +
							     i / 8]));
			if (byte & 0x80)
				ssd1306_draw_pixel(oled, x + i, y, color);
		}
	}
	ssd1306_sync(oled);
}
static void ssd1306_sync(struct ssd1306 *oled)
{
	ssd1306_burst_write(oled, oled->buffer, OLED_WIDTH * OLED_HEIGHT / 8,
			    DATA);
	memset(oled->buffer, 0, 1024);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DinhNam <20021163@vnu.edu.vn>");
MODULE_DESCRIPTION("GateWay Screen with oled ssd1306");
MODULE_VERSION("1.0");
