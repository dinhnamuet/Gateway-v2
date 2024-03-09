#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/sched/signal.h>
#include "sx1278.h"

static LIST_HEAD(device_list);

struct LoRa
{
	int irq;
	int current_mode;
	int frequency;
	uint8_t spreadingFactor;
	uint8_t bandWidth;
	uint8_t codingRate;
	uint16_t preamble;
	uint8_t power;
	uint8_t overCurrentProtection;
	uint8_t *receive_buffer;
	uint8_t *transmit_buffer;
	uint8_t *cache_buffer;
	uint8_t rssi_value;
	dev_t dev_num;
	char name[10];

	struct spi_device *spi;
	struct gpio_desc *reset;
	struct gpio_desc *dio0;
	struct class *mclass;
	struct cdev *mcdev;
	struct device *mdevice;
	struct list_head device_entry;
	struct tasklet_struct my_tasklet;
	struct task_struct *task;
};

static void LoRa_Reset(struct LoRa *_LoRa);
static uint8_t LoRa_Read(struct LoRa *_LoRa, uint8_t address);
static void LoRa_Write(struct LoRa *_LoRa, uint8_t address, uint8_t value);
static void LoRa_gotoMode(struct LoRa *_LoRa, int mode);
static void LoRa_setFrequency(struct LoRa *_LoRa, int f);
static void LoRa_setSpreadingFactor(struct LoRa *_LoRa, uint8_t SF);
static void LoRa_setPower(struct LoRa *_LoRa, uint8_t power);
static void LoRa_setOCP(struct LoRa *_LoRa, uint8_t cur);
static void LoRa_setTOMsb_setCRCon(struct LoRa *_LoRa);
static void LoRa_setLowDaraRateOptimization(struct LoRa *_LoRa, uint8_t value);
static void LoRa_setAutoLDO(struct LoRa *_LoRa);
static void LoRa_setSyncWord(struct LoRa *_LoRa, uint8_t syncword);
static void LoRa_setBandWidth(struct LoRa *_LoRa, uint8_t BW);
static void LoRa_setCodingRate(struct LoRa *_LoRa, uint8_t cdRate);
static void LoRa_BurstWrite(struct LoRa *_LoRa, uint8_t address, uint8_t *value);
static uint8_t LoRa_isValid(struct LoRa *_LoRa);
static uint8_t LoRa_transmit(struct LoRa *_LoRa, uint8_t *data, uint8_t length, uint16_t timeout);
static void LoRa_startReceiving(struct LoRa *_LoRa);
static uint8_t LoRa_receive(struct LoRa *_LoRa, uint8_t *data, uint8_t length);
static uint8_t LoRa_getRSSI(struct LoRa *_LoRa);
static uint16_t LoRa_init(struct LoRa *_LoRa);
static void LoRa_free(struct LoRa *_LoRa);

irqreturn_t get_message(int irq, void *dev_id)
{
	uint8_t r = 0;
	struct LoRa *lora = (struct LoRa *)dev_id;
	r = LoRa_receive(lora, lora->cache_buffer, PACKET_SIZE);
	if (r)
		tasklet_schedule(&lora->my_tasklet);
	return IRQ_HANDLED;
}
void tasklet_fn(unsigned long args)
{
	struct LoRa *lora = (struct LoRa *)args;
	struct siginfo info;
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIGUSR1;
	info.si_code = SI_QUEUE;

	if (!strncmp(lora->cache_buffer, lora->transmit_buffer, strlen(lora->cache_buffer)))
		memset(lora->cache_buffer, 0, PACKET_SIZE);
	else
	{
		if (strncmp(lora->cache_buffer, lora->receive_buffer, strlen(lora->cache_buffer)))
		{
			memset(lora->receive_buffer, 0, PACKET_SIZE);
			strncpy(lora->receive_buffer, lora->cache_buffer, strlen(lora->cache_buffer));

			if (lora->task)
			{
				if (send_sig_info(info.si_signo, (struct kernel_siginfo *)&info, lora->task) < 0)
					printk(KERN_ERR "Cannot send signal to process\n");
//				else
//					printk(KERN_INFO "Signal was sent to %d\n", lora->task->pid);
			}
		}
		else
			memset(lora->cache_buffer, 0, PACKET_SIZE);
	}
}
static int sx1278_open(struct inode *inodep, struct file *filep)
{
	int status = -ENXIO;
	struct LoRa *lora = NULL;
	list_for_each_entry(lora, &device_list, device_entry)
	{
		if (lora->dev_num == inodep->i_rdev)
		{
			status = 0;
			break;
		}
	}
	if (status)
	{
		printk(KERN_INFO "Device has not supported \n");
		return status;
	}
	filep->private_data = lora;
	return 0;
}
static int sx1278_close(struct inode *inodep, struct file *filep)
{
	filep->private_data = NULL;
	return 0;
}
static ssize_t sx1278_read(struct file *filep, char __user *user_buff, size_t size, loff_t *offset)
{
	int to_read = 0;
	struct LoRa *lora = filep->private_data;
	to_read = (size > strlen(lora->receive_buffer) - *offset) ? (strlen(lora->receive_buffer) - *offset) : (size);
	if (copy_to_user(user_buff, lora->receive_buffer, strlen(lora->receive_buffer)) != 0)
	{
		return -EFAULT;
	}
	*offset += to_read;
//	printk(KERN_INFO "received: %s\n", &lora->receive_buffer[10]);
	return to_read;
}
static ssize_t sx1278_write(struct file *filep, const char *user_buff, size_t size, loff_t *offset)
{
	struct LoRa *sx1278 = filep->private_data;
	memset(sx1278->transmit_buffer, 0, PACKET_SIZE);
	if (copy_from_user(sx1278->transmit_buffer, user_buff, size) != 0)
		return -EFAULT;
	LoRa_transmit(sx1278, sx1278->transmit_buffer, (uint8_t)strlen(sx1278->transmit_buffer), TX_TIME_OUT);
//	printk(KERN_INFO "transmit: %s\n", sx1278->transmit_buffer);
	return size;
}
static long sx1278_ioctl(struct file *filep, unsigned int cmd, unsigned long data)
{
	int foo = 0;
	struct LoRa *sx1278 = filep->private_data;
	switch (cmd)
	{
	case RG_SIGNAL:
		sx1278->task = get_current();
		printk(KERN_INFO "process (pid %d) registerd to recv signal\n", sx1278->task->pid); 
		break;
	case CTL_SIGNAL:
		printk(KERN_INFO "process (pid %d) stop recv signal\n", sx1278->task->pid); 
		sx1278->task = NULL;
		break;
	case GET_RSSI:
		sx1278->rssi_value = LoRa_getRSSI(sx1278);
		if (copy_to_user((uint8_t *)data, &sx1278->rssi_value, 1) != 0)
		{
			printk(KERN_ERR "Get RSSI failure\n");
			return -1;
		}
		break;
	case GOTO_MODE:
		if (!copy_from_user(&foo ,(int *)data, sizeof(int)))
			LoRa_gotoMode(sx1278, foo);
		break;
	case BAND_WIDTH:
		if (!copy_from_user(&foo ,(uint8_t *)data, 1))
			LoRa_setBandWidth(sx1278, (uint8_t)foo);
		break;
	case SPREADING_FACTOR:
		if (!copy_from_user(&foo ,(uint8_t *)data, 1))
			LoRa_setSpreadingFactor(sx1278, (uint8_t)foo);
		break;
	case CODING_RATE:
		if (!copy_from_user(&foo ,(uint8_t *)data, 1))
			LoRa_setCodingRate(sx1278, (uint8_t)foo);
		break;
	case SYNC_WORD:
		if (!copy_from_user(&foo ,(uint8_t *)data, 1))
			LoRa_setSyncWord(sx1278, (uint8_t)foo);
		break;
	case POWER:
		if (!copy_from_user(&foo ,(uint8_t *)data, 1))
			LoRa_setPower(sx1278, (uint8_t)foo);
		break;
	case FREQUENCY:
		if (!copy_from_user(&foo ,(int *)data, sizeof(int)))
			LoRa_setFrequency(sx1278, foo);
		break;
	default:
		break;
	}
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = sx1278_open,
	.release = sx1278_close,
	.read = sx1278_read,
	.write = sx1278_write,
	.unlocked_ioctl = sx1278_ioctl
};

static int sx1278_probe(struct spi_device *spi)
{
	struct LoRa *sx1278 = NULL;
	struct device *dev = &spi->dev;
	sx1278 = devm_kzalloc(&spi->dev, sizeof(*sx1278), GFP_KERNEL);
	if (!sx1278)
	{
		printk(KERN_ERR "Kzalloc failed\n");
		return -1;
	}
	/* config reset & dio0 */
	sx1278->reset = gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(sx1278->reset, 1);
	sx1278->dio0 = gpiod_get(dev, "dio0", GPIOD_IN);
	sx1278->irq = gpiod_to_irq(sx1278->dio0);
	/* spi configuration */
	sx1278->spi = spi;
	spi_set_drvdata(spi, sx1278);
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	spi->max_speed_hz = 9000000;
	if (spi_setup(spi) < 0)
	{
		printk(KERN_ERR "SPI setup failed\n");
		kfree(sx1278);
		return -1;
	}
	sx1278->frequency = 433;
	sx1278->spreadingFactor = SF_7;
	sx1278->bandWidth = BW_125_KHZ;
	sx1278->codingRate = CR_4_5;
	sx1278->power = POWER_20db;
	sx1278->overCurrentProtection = 100;
	sx1278->preamble = 8;
	memset(sx1278->name, 0, sizeof(sx1278->name));
	sprintf(sx1278->name, "%s-%d", DEV_NAME, sx1278->spi->chip_select);
	if (alloc_chrdev_region(&sx1278->dev_num, 0, 1, sx1278->name) < 0)
	{
		printk(KERN_ERR "Allocate device number failure!\n");
		return -1;
	}
	if ((sx1278->mclass = class_create(THIS_MODULE, DEV_NAME)) == NULL)
	{
		printk(KERN_ERR "Create class device failure\n");
		goto rm_dev_num;
	}
	sx1278->mcdev = cdev_alloc();
	sx1278->mcdev->owner = THIS_MODULE;
	sx1278->mcdev->dev = sx1278->dev_num;
	cdev_init(sx1278->mcdev, &fops);
	if (cdev_add(sx1278->mcdev, sx1278->dev_num, 1) < 0)
	{
		printk(KERN_ERR "Cdev add failure\n");
		goto rm_class;
	}
	if ((sx1278->mdevice = device_create(sx1278->mclass, &spi->dev, sx1278->dev_num, sx1278, sx1278->name)) == NULL)
	{
		printk(KERN_ERR "create device failure\n");
		goto rm_cdev;
	}
	sx1278->receive_buffer = kzalloc(PACKET_SIZE, GFP_KERNEL);
	if (sx1278->receive_buffer == NULL)
	{
		printk(KERN_ERR "Allocate memory failure\n");
		goto rm_device;
	}
	sx1278->transmit_buffer = kzalloc(PACKET_SIZE, GFP_KERNEL);
	if (sx1278->transmit_buffer == NULL)
	{
		printk(KERN_ERR "Allocate failure\n");
		goto rm_buff_rec;
	}
	sx1278->cache_buffer = kzalloc(PACKET_SIZE, GFP_KERNEL);
	if (sx1278->cache_buffer == NULL)
	{
		printk(KERN_ERR "Allocate failure\n");
		goto rm_buff_tr;
	}
	sx1278->my_tasklet.next = NULL;
	sx1278->my_tasklet.state = TASKLET_STATE_SCHED;
	atomic_set(&sx1278->my_tasklet.count, 0);
	sx1278->my_tasklet.func = tasklet_fn;
	sx1278->my_tasklet.data = (unsigned long)sx1278;
	if (LoRa_init(sx1278) != LORA_OK)
	{
		printk(KERN_ERR "LoRa init failure\n");
		goto rm_lora;
	}
	INIT_LIST_HEAD(&sx1278->device_entry);
	list_add(&sx1278->device_entry, &device_list);
	LoRa_gotoMode(sx1278, SLEEP_MODE);
	printk(KERN_EMERG "sx1278: %s has been loadded, Cs: %d, Speed: %d, bits per word: %d!\n", sx1278->name, sx1278->spi->chip_select, sx1278->spi->max_speed_hz, sx1278->spi->bits_per_word);
	return 0;
rm_lora:
	LoRa_free(sx1278);
	kfree(sx1278->cache_buffer);
rm_buff_tr:
	kfree(sx1278->transmit_buffer);
rm_buff_rec:
	kfree(sx1278->receive_buffer);
rm_cdev:
	cdev_del(sx1278->mcdev);
rm_device:
	device_destroy(sx1278->mclass, sx1278->dev_num);
rm_class:
	class_destroy(sx1278->mclass);
rm_dev_num:
	unregister_chrdev_region(sx1278->dev_num, 1);
	return -1;
}

static void sx1278_remove(struct spi_device *spi)
{
	struct LoRa *sx1278 = spi_get_drvdata(spi);
	if (!sx1278)
	{
		printk(KERN_ERR "Couldn't free\n");
	}
	else
	{
		tasklet_kill(&sx1278->my_tasklet);
		kfree(sx1278->receive_buffer);
		kfree(sx1278->transmit_buffer);
		kfree(sx1278->cache_buffer);
		gpiod_set_value(sx1278->reset, 0);
		LoRa_free(sx1278);
		device_destroy(sx1278->mclass, sx1278->dev_num);
		class_destroy(sx1278->mclass);
		cdev_del(sx1278->mcdev);
		unregister_chrdev_region(sx1278->dev_num, 1);
		sx1278->task = NULL;
	}
	printk(KERN_INFO "%s, %d\n", __func__, __LINE__);
}

static struct spi_device_id sx1278_id_table[] = {
	{ "sx1278lora", 0 },
	{	}
};
MODULE_DEVICE_TABLE(spi, sx1278_id_table);

static const struct of_device_id sx1278_of_match_id[] = {
	{ .compatible = "sx1278-lora,nam", },
	{	}
};
MODULE_DEVICE_TABLE(of, sx1278_of_match_id);

static struct spi_driver sx1278_driver = {
	.probe = sx1278_probe,
	.remove = sx1278_remove,
	.driver = {
		.name = "sx1278",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sx1278_of_match_id),
	},
	.id_table = sx1278_id_table,
};
module_spi_driver(sx1278_driver);

static void LoRa_Write(struct LoRa *_LoRa, uint8_t address, uint8_t value)
{
	uint8_t to_send[2];
	to_send[0] = address | (1 << 7);
	to_send[1] = value;
	spi_write(_LoRa->spi, to_send, 2);
}
static uint8_t LoRa_Read(struct LoRa *_LoRa, uint8_t address)
{
	uint8_t readData;
	uint8_t addr;
	addr = address & ~(1 << 7);
	spi_write_then_read(_LoRa->spi, &addr, 1, &readData, 1);
	return readData;
}
static void LoRa_Reset(struct LoRa *_LoRa)
{
	gpiod_set_value(_LoRa->reset, 0);
	msleep(1000);
	gpiod_set_value(_LoRa->reset, 1);
	msleep(100);
}
static void LoRa_gotoMode(struct LoRa *_LoRa, int mode)
{
	uint8_t read, data;
	read = LoRa_Read(_LoRa, RegOpMode);
	switch (mode)
	{
	case SLEEP_MODE:
		data = (read & 0xF8) | SLEEP_MODE;
		_LoRa->current_mode = SLEEP_MODE;
		break;
	case STANDBY_MODE:
		data = (read & 0xF8) | STANDBY_MODE;
		_LoRa->current_mode = STANDBY_MODE;
		break;
	case TRANSMIT_MODE:
		data = (read & 0xF8) | TRANSMIT_MODE;
		_LoRa->current_mode = TRANSMIT_MODE;
		break;
	case RXCONTINUOUS_MODE:
		data = (read & 0xF8) | RXCONTINUOUS_MODE;
		_LoRa->current_mode = RXCONTINUOUS_MODE;
		break;
	case RXSINGLE_MODE:
		data = (read & 0xF8) | RXSINGLE_MODE;
		_LoRa->current_mode = RXSINGLE_MODE;
		break;
	case CAD:
		data = (read & 0xF8) | CAD;
		_LoRa->current_mode = CAD;
		break;
	default:
		break;
	}
	LoRa_Write(_LoRa, RegOpMode, data);
}
static void LoRa_setFrequency(struct LoRa *_LoRa, int f)
{
	uint8_t data;
	uint32_t Fr;
	Fr = ((uint32_t)f * 524288) >> 5;

	data = (uint8_t)(Fr >> 16) & 0xFF;
	LoRa_Write(_LoRa, RegFrMsb, data);
	msleep(5);

	data = (uint8_t)(Fr >> 8) & 0xFF;
	LoRa_Write(_LoRa, RegFrMid, data);
	msleep(5);

	data = (uint8_t)Fr & 0xFF;
	LoRa_Write(_LoRa, RegFrLsb, data);
	msleep(5);
}
static void LoRa_setSpreadingFactor(struct LoRa *_LoRa, uint8_t SF)
{
	uint8_t data;
	uint8_t read;

	if (SF > 12)
		SF = 12;
	if (SF < 7)
		SF = 7;

	read = LoRa_Read(_LoRa, RegModemConfig2);
	msleep(10);

	data = (uint8_t)((SF << 4) | (read & 0x0F));
	LoRa_Write(_LoRa, RegModemConfig2, data);
	msleep(10);
	LoRa_setAutoLDO(_LoRa);
}
static void LoRa_setPower(struct LoRa *_LoRa, uint8_t power)
{
	LoRa_Write(_LoRa, RegPaConfig, power);
	msleep(10);
}
static void LoRa_setOCP(struct LoRa *_LoRa, uint8_t cur)
{
	uint8_t OcpTrim = 0;
	if (cur < 45)
		cur = 45;
	if(cur > 240)
		cur = 240;
	if (cur <= 120)
		OcpTrim = (cur - 45) / 5;
	else if (cur <= 240)
		OcpTrim = (cur + 30) / 10;
	OcpTrim = OcpTrim | (1UL << 5);
	LoRa_Write(_LoRa, RegOcp, OcpTrim);
	msleep(10);
}
static void LoRa_setTOMsb_setCRCon(struct LoRa *_LoRa)
{
	uint8_t read, data;
	read = LoRa_Read(_LoRa, RegModemConfig2);
	data = read | 0x07;
	LoRa_Write(_LoRa, RegModemConfig2, data);
	msleep(10);
}
static void LoRa_setLowDaraRateOptimization(struct LoRa *_LoRa, uint8_t value)
{
	uint8_t read, data;
	read = LoRa_Read(_LoRa, RegModemConfig3);
	if (value)
		data = read | 0x08;
	else
		data = read & 0xF7;
	LoRa_Write(_LoRa, RegModemConfig3, data);
	msleep(10);
}
static void LoRa_setAutoLDO(struct LoRa *_LoRa)
{
	long calculatedBW;
	long BW[] = {78, 104, 156, 208, 313, 417, 625, 1250, 2500, 5000};
	calculatedBW = ((1 << _LoRa->spreadingFactor) * 10) / BW[_LoRa->bandWidth];
	LoRa_setLowDaraRateOptimization(_LoRa, (calculatedBW > 16));
}

static void LoRa_setSyncWord(struct LoRa *_LoRa, uint8_t syncword)
{
	LoRa_Write(_LoRa, RegSyncWord, syncword);
	msleep(10);
}
static void LoRa_setBandWidth(struct LoRa *_LoRa, uint8_t BW)
{
	uint8_t data;
	data = LoRa_Read(_LoRa, RegModemConfig1);
	data &= 0x0F;
	data |= (uint8_t)(BW << 4);
	LoRa_Write(_LoRa, RegModemConfig1, data);
	LoRa_setAutoLDO(_LoRa);
}
static void LoRa_setCodingRate(struct LoRa *_LoRa, uint8_t cdRate)
{
	uint8_t data;
	data = LoRa_Read(_LoRa, RegModemConfig1);
	data &= 0xF1;
	data |= (uint8_t)(cdRate << 1);
	LoRa_Write(_LoRa, RegModemConfig1, data);
}
static void LoRa_BurstWrite(struct LoRa *_LoRa, uint8_t address, uint8_t *value)
{
	uint8_t to_send[PACKET_SIZE];
	memset(to_send, 0, PACKET_SIZE);
	to_send[0] = address | (1 << 7);
	memcpy(&to_send[1], value, strlen(value));
	spi_write(_LoRa->spi, to_send, strlen(to_send));
}
static uint8_t LoRa_isValid(struct LoRa *_LoRa)
{
	(void)_LoRa;
	return 1;
}

static uint8_t LoRa_transmit(struct LoRa *_LoRa, uint8_t *data, uint8_t length, uint16_t timeout)
{
	uint8_t read;
	int mode = _LoRa->current_mode;
	LoRa_gotoMode(_LoRa, STANDBY_MODE);
	msleep(1);
	/* Channel Activity Detection */
	while(1)
	{
		LoRa_gotoMode(_LoRa, CAD);
		while(!(LoRa_Read(_LoRa, RegIrqFlags)>>2 & 0x01))
		{
			msleep(1);
		}
		read = LoRa_Read(_LoRa, RegIrqFlags);
		if(read & 0x01) //CAD detected
		{
			LoRa_Write(_LoRa, RegIrqFlags, 0xFF);
		}
		else
		{
			LoRa_Write(_LoRa, RegIrqFlags, 0xFF);
			break;
		}
		msleep(1);
	}
	/* send data */
	LoRa_gotoMode(_LoRa, STANDBY_MODE);
	read = LoRa_Read(_LoRa, RegFiFoTxBaseAddr);
	LoRa_Write(_LoRa, RegFiFoAddPtr, read);
	LoRa_Write(_LoRa, RegPayloadLength, length);
	LoRa_BurstWrite(_LoRa, RegFifo, data);
	LoRa_gotoMode(_LoRa, TRANSMIT_MODE);
	while (1)
	{
		read = LoRa_Read(_LoRa, RegIrqFlags);
		if (read & (1UL << 3))
		{
			LoRa_Write(_LoRa, RegIrqFlags, 0xFF);
			LoRa_gotoMode(_LoRa, mode);
			return 1;
		}
		else
		{
			if (--timeout == 0)
			{
				LoRa_gotoMode(_LoRa, mode);
				return 0;
			}
		}
		msleep(1);
	}
}
static void LoRa_startReceiving(struct LoRa *_LoRa)
{
	LoRa_gotoMode(_LoRa, RXCONTINUOUS_MODE);
}
static uint8_t LoRa_receive(struct LoRa *_LoRa, uint8_t *data, uint8_t length)
{
	uint8_t i = 0;
	uint8_t read;
	uint8_t data_len;
	uint8_t min = 0;
	LoRa_gotoMode(_LoRa, STANDBY_MODE);
	read = LoRa_Read(_LoRa, RegIrqFlags);
	if (read & (1UL << 6))
	{
		LoRa_Write(_LoRa, RegIrqFlags, 0xFF);
		data_len = LoRa_Read(_LoRa, RegRxNbBytes);
		read = LoRa_Read(_LoRa, RegFiFoRxCurrentAddr);
		LoRa_Write(_LoRa, RegFiFoAddPtr, read);
		min = (length >= data_len) ? data_len : length;
		memset(data, 0, PACKET_SIZE);
		for (i = 0; i < min; i++)
			data[i] = LoRa_Read(_LoRa, RegFifo);
	}
	LoRa_startReceiving(_LoRa);
	return min;
}
static uint8_t LoRa_getRSSI(struct LoRa *_LoRa)
{
	uint8_t read;
	read = LoRa_Read(_LoRa, RegPktRssiValue);
	return read;
}
static uint16_t LoRa_init(struct LoRa *_LoRa)
{
	uint8_t data;
	uint8_t read;
	/* lora pin init */
	// DIO0 rising interrupt:
	if (request_irq(_LoRa->irq, get_message, IRQF_TRIGGER_RISING, _LoRa->name, _LoRa) < 0)
	{
		printk(KERN_ERR "Request irq failure\n");
		return -1;
	}

	LoRa_Reset(_LoRa);

	if (LoRa_isValid(_LoRa))
	{
		// goto sleep mode:
		LoRa_gotoMode(_LoRa, SLEEP_MODE);
		msleep(10);

		// turn on lora mode:
		read = LoRa_Read(_LoRa, RegOpMode);
		msleep(10);
		data = read | 0x80;
		LoRa_Write(_LoRa, RegOpMode, data);
		msleep(100);

		// set frequency:
		LoRa_setFrequency(_LoRa, _LoRa->frequency);

		// set output power gain:
		LoRa_setPower(_LoRa, _LoRa->power);

		// set over current protection:
		LoRa_setOCP(_LoRa, _LoRa->overCurrentProtection);

		// set LNA gain:
		LoRa_Write(_LoRa, RegLna, 0x23);

		// set spreading factor, CRC on, and Timeout Msb:
		LoRa_setTOMsb_setCRCon(_LoRa);
		LoRa_setSpreadingFactor(_LoRa, _LoRa->spreadingFactor);

		// set Timeout Lsb:
		LoRa_Write(_LoRa, RegSymbTimeoutL, 0xFF);

		// set bandwidth, coding rate and expilicit mode:
		// 8 bit RegModemConfig --> | X | X | X | X | X | X | X | X |
		//       bits represent --> |   bandwidth   |     CR    |I/E|
		data = LoRa_Read(_LoRa, RegModemConfig1);
		data &= 0x01;
		data |= (uint8_t)((_LoRa->bandWidth << 4) | (_LoRa->codingRate << 1));
		LoRa_Write(_LoRa, RegModemConfig1, data);
		LoRa_setAutoLDO(_LoRa);

		// set preamble:
		LoRa_Write(_LoRa, RegPreambleMsb, _LoRa->preamble >> 8);
		LoRa_Write(_LoRa, RegPreambleLsb, (uint8_t)_LoRa->preamble);

		// DIO mapping:   --> DIO: RxDone
		read = LoRa_Read(_LoRa, RegDioMapping1);
		data = read | 0x3F;
		LoRa_Write(_LoRa, RegDioMapping1, data);

		// goto standby mode:
		LoRa_gotoMode(_LoRa, STANDBY_MODE);
		_LoRa->current_mode = STANDBY_MODE;
		msleep(10);

		read = LoRa_Read(_LoRa, RegVersion);
		if (read == 0x12)
			return LORA_OK;
		else
			return LORA_NOT_FOUND;
	}
	else
	{
		return LORA_UNAVAILABLE;
	}
}
static void LoRa_free(struct LoRa *_LoRa)
{
	gpiod_put(_LoRa->reset);
	gpiod_put(_LoRa->dio0);
	free_irq(_LoRa->irq, _LoRa);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("20021163@vnu.edu.vn");
MODULE_DESCRIPTION("SPI Driver for SX1278");
MODULE_VERSION("1.1");
