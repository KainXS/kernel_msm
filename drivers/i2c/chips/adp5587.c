/* drivers/i2c/chips/adp5587.c - adp5587 keyboard driver
 *
 * Copyright (C) 2007-2009 Huawei.
 * Author: Liang YUAN <yuanliang@huawei.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <hsad/config_interface.h>
#include <linux/earlysuspend.h>
#include <mach/gpio.h>
#include <linux/hrtimer.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include "huawei_qwerty.h"
#include <asm/gpio.h>
#include <linux/module.h>

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif



/* creates /sys/module/adp5587/parameters/adp5587_debug_mask file */
static int adp5587_debug_mask = 0;
module_param(adp5587_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define KEYPAD_DEBUG(x...)		\
	do {						\
		if(adp5587_debug_mask)	\
			printk(KERN_ERR x);	\
	} while (0)

//Registers
#define DEV_ID  		0x00 //Device ID
#define INT_MODE_CFG  	0x01 //Configuration Register 1
#define INT_STAT  		0x02 //Interrupt status register
#define KEY_LCK_EC_STAT 0x03 //Keylock and event counter register
#define KEY_EVENTA  	0x04 //Key Event Register A
#define KEY_EVENTB  	0x05 //Key Event Register B
#define KEY_EVENTC  	0x06 //Key Event Register C
#define KEY_EVENTD  	0x07 //Key Event Register D
#define KEY_EVENTE  	0x08 //Key Event Register E
#define KEY_EVENTF  	0x09 //Key Event Register F
#define KEY_EVENTG  	0x0A //Key Event Register G
#define KEY_EVENTH  	0x0B //Key Event Register H
#define KEY_EVENTI  	0x0C //Key Event Register I
#define KEY_EVENTJ  	0x0D //Key Event Register J
#define KP_LCK_TMR  	0x0E //Keypad Unlock 1 to Keypad Unlock 2 timer
#define UNLOCK1  		0x0F //Unlock Key 1
#define UNLOCK2  		0x10 //Unlock Key 2
#define GPIO_INT_STAT1  0x11 //GPIO interrupt status
#define GPIO_INT_STAT2  0x12 //GPIO interrupt status
#define GPIO_INT_STAT3  0x13 //GPIO interrupt status
#define GPIO_DAT_STAT1  0x14 //GPIO data status, read twice to clear
#define GPIO_DAT_STAT2  0x15 //GPIO data status, read twice to clear
#define GPIO_DAT_STAT3  0x16 //GPIO data status, read twice to clear
#define GPIO_DAT_OUT1  	0x17 //GPIO data out
#define GPIO_DAT_OUT2  	0x18 //GPIO data out
#define GPIO_DAT_OUT3  	0x19 //GPIO data out
#define GPIO_INT_EN1  	0x1A //GPIO interrupt enable
#define GPIO_INT_EN2  	0x1B //GPIO interrupt enable
#define GPIO_INT_EN3  	0x1C //GPIO interrupt enable
#define KP_GPIO1  		0x1D //Keypad or GPIO selection
#define KP_GPIO2  		0x1E //Keypad or GPIO selection
#define KP_GPIO3  		0x1F //Keypad or GPIO selection
#define GPI_EM_REG1  	0x20 //GPI Event Mode 1
#define GPI_EM_REG2  	0x21 //GPI Event Mode 2
#define GPI_EM_REG3  	0x22 //GPI Event Mode 3
#define GPIO_DIR1  		0x23 //GPIO data direction
#define GPIO_DIR2  		0x24 //GPIO data direction
#define GPIO_DIR3  		0x25 //GPIO data direction
#define GPIO_INT_LVL1  	0x26 //GPIO edge/level detect
#define GPIO_INT_LVL2  	0x27 //GPIO edge/level detect
#define GPIO_INT_LVL3  	0x28 //GPIO edge/level detect
#define DEBOUNCE_DIS1  	0x29 //Debounce disable
#define DEBOUNCE_DIS2  	0x2A //Debounce disable
#define DEBOUNCE_DIS3  	0x2B //Debounce disable
#define GPIO_PULL1  	0x2C //GPIO pull disable
#define GPIO_PULL2  	0x2D //GPIO pull disable
#define GPIO_PULL3  	0x2E //GPIO pull disable

//INT_STAT
#define KE_INT			1	//INT_STAT[0]
#define CLEAR_INTERRUPT	1

//CFG
#define	INT_CFG			1<<4 //Interrupt configuration
#define KE_IEN			1	 //Key events interrupt enable
#define KE_IDISABLE		0	 //Key events interrupt disable


//KEY_LCK_EC_STAT
#define KEY_INDEX		0x7F //Key Event Register A status KE[6:0] = Key number
#define KEY_RELEASED	1<<7 //Key Event Register A status KP[7] = 0: released, 1: pressed (cleared on read)

//KP_GPIO1
#define KEYPAD_ROW_R4R3R2R1R0		0x1F
#define KEYPAD_COL_C7C6C5C4C3C2C1C0 0xFF
#define KEYPAD_COL_C9C8				0x03

#define ADP5587_RESET_GPIO			18
#define ADP5587_INT_GPIO			19

#define ADP5587_I2C_NAME			"adp5587"

struct adp5587_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct work_struct work;
	unsigned short *keymap;
	struct early_suspend early_suspend;
};

static int adp5587_chip_reset(void);
static int adp5587_chip_init(struct adp5587_data *client_data);
static void adp5587_report_key(struct adp5587_data *client_data, unsigned char key_event);
static irqreturn_t adp5587_irq_handler(int irq, void *dev_id);

static int adp5587_input_device_init(struct i2c_client *client);
static int adp5587_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int adp5587_remove(struct i2c_client *client);
static int adp5587_resume(struct i2c_client *client);
static int adp5587_suspend(struct i2c_client *client,pm_message_t mesg);

static struct workqueue_struct *adp5587_wq;

static const struct i2c_device_id adp5587_id[] = {
	{ ADP5587_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver adp5587_driver = {
	.probe = adp5587_probe,
	.remove = adp5587_remove,
	.resume = adp5587_resume,
	.suspend = adp5587_suspend,  
	.id_table = adp5587_id,
	.driver = {
		.name = ADP5587_I2C_NAME,
	},
};

static int adp5587_chip_reset(void)
{
	int rc = 0;
	
	rc = gpio_request(ADP5587_RESET_GPIO, "adp5587_reset");
	if(rc)
	{
		printk(KERN_ERR "%s: gpio %d request failed code = %d\n",
			__FUNCTION__,ADP5587_RESET_GPIO,rc);
		return rc;
	}

	rc = gpio_tlmm_config(GPIO_CFG(ADP5587_RESET_GPIO,
		0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP,
		GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if(rc)
	{
		printk(KERN_ERR "%s: gpio %d config failed code = %d\n",
			__FUNCTION__,ADP5587_RESET_GPIO,rc);
		gpio_free(ADP5587_RESET_GPIO);
		return rc;
	}
	
	gpio_direction_output(ADP5587_RESET_GPIO,1);
	mdelay(10);
	gpio_direction_output(ADP5587_RESET_GPIO,0);
	mdelay(10);
	gpio_direction_output(ADP5587_RESET_GPIO,1);
	mdelay(10);

	return rc;
}

static int adp5587_chip_init(struct adp5587_data *client_data)
{
	int rc = 0;
	
	rc = i2c_smbus_write_byte_data(client_data->client,KP_GPIO1,KEYPAD_ROW_R4R3R2R1R0);
	if(rc)
	{
		printk(KERN_ERR "%s: KP_GPIO1 failed\n",__FUNCTION__);
		rc = -EIO;
	}
	
	rc = i2c_smbus_write_byte_data(client_data->client,KP_GPIO2,KEYPAD_COL_C7C6C5C4C3C2C1C0);
	if(rc)
	{
		printk(KERN_ERR "%s: KP_GPIO2 failed\n",__FUNCTION__);
		rc = -EIO;
	}

	rc = i2c_smbus_write_byte_data(client_data->client,KP_GPIO3,KEYPAD_COL_C9C8);
	if(rc)
	{
		printk(KERN_ERR "%s: KP_GPIO3 failed\n",__FUNCTION__);
		rc = -EIO;
	}

	rc = i2c_smbus_write_byte_data(client_data->client,INT_MODE_CFG,0x11);
	if(rc)
	{
		printk(KERN_ERR "%s: INT_MODE_CFG failed\n",__FUNCTION__);
		rc = -EIO;
	}

	return rc;
}

static void adp5587_report_key(struct adp5587_data *client_data, unsigned char key_event)
{
	unsigned int keycode = 0;
	int pressed = 0;
	unsigned char key_event_number = 0;

	//Get key event number
	key_event_number = key_event & KEY_INDEX;
	//Get keycode
	keycode = client_data->keymap[key_event_number - 1];
	
	KEYPAD_DEBUG("%s:keymap_index %d keycode =%d  \n",
		__FUNCTION__,(key_event_number - 1),keycode);

	pressed = (int)(key_event & KEY_RELEASED);
	input_report_key(client_data->input_dev, keycode, pressed);
	input_sync(client_data->input_dev);

	return;
}

static void adp5587_work_fuc(struct work_struct *work)
{
	unsigned char status = 0;
	unsigned char key_event_counter = 0;
	unsigned char key_event = 0;

	struct adp5587_data *adp5587_data = container_of(work, struct adp5587_data, work);
	unsigned int i = 0;
	
	KEYPAD_DEBUG("%s:+++ \n",__FUNCTION__);
	
	//Read interrupt status
	status = i2c_smbus_read_byte_data(adp5587_data->client, INT_STAT);
	KEYPAD_DEBUG(" INT_STAT=%d \n",status);
	
	//Check if key events interrupt is detected.
	if(!status & KE_INT)
	{
		goto exit_clear_interrupt_and_enable_irq;
	}
	
	status = i2c_smbus_read_byte_data(adp5587_data->client, KEY_LCK_EC_STAT);
	key_event_counter = status & 0x0F;//KEC[3:0] key event counter of key event register
	KEYPAD_DEBUG("KEY_LCK_EC_STAT =%d \n",key_event_counter);

	while (key_event_counter == 0)
	{
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(20));
		
		status = i2c_smbus_read_byte_data(adp5587_data->client, KEY_LCK_EC_STAT);
		key_event_counter = status & 0x0F;
		if(key_event_counter != 0)
		{
			KEYPAD_DEBUG("key_event_counter =%d \n",key_event_counter);
			break;
		}
		if(i++>20)
		{
			printk(KERN_ERR "adp5587_work_fuc: loop times too big!!\n");
			break;
		}
	}
	
	while(key_event_counter--)
	{
		key_event = i2c_smbus_read_byte_data(adp5587_data->client, KEY_EVENTA);
		KEYPAD_DEBUG("KEY_EVENTA =0x%x \n",key_event);
		adp5587_report_key(adp5587_data, key_event);
	}

exit_clear_interrupt_and_enable_irq:
	//Clear interrupt
	i2c_smbus_write_byte_data(adp5587_data->client,INT_STAT,KE_IEN);
	KEYPAD_DEBUG("%s:--- \n",__FUNCTION__);

	//Enable irq
	enable_irq(adp5587_data->client->irq);
}

static irqreturn_t adp5587_irq_handler(int irq, void *dev_id)
{
	struct adp5587_data *adp5587_data = dev_id;
	disable_irq_nosync(adp5587_data->client->irq);
	queue_work(adp5587_wq, &adp5587_data->work);
	KEYPAD_DEBUG("%s:--- \n",__FUNCTION__);
	return IRQ_HANDLED;
}

static int adp5587_setup_irq(struct i2c_client *client)
{
	int rc = 0;
	struct adp5587_data *client_data = NULL;
	
	client_data = i2c_get_clientdata(client);

	rc = gpio_request(ADP5587_INT_GPIO, "adp5587_int");
	if(rc)
	{
		printk(KERN_ERR "%s: gpio %d request failed code = %d\n",
			__FUNCTION__,ADP5587_INT_GPIO,rc);
		return rc;
	}
	rc = gpio_tlmm_config(GPIO_CFG(ADP5587_INT_GPIO,
		0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
		GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if(rc)
	{
		printk(KERN_ERR "%s: gpio %d config failed code = %d\n",
			__FUNCTION__,ADP5587_INT_GPIO,rc);
		gpio_free(ADP5587_INT_GPIO);
		return rc;
	}
	rc = gpio_direction_input(ADP5587_INT_GPIO);
	if(rc)
	{
		printk(KERN_ERR "%s: gpio %d set dirtction failed code = %d\n",
			__FUNCTION__,ADP5587_INT_GPIO,rc);
		gpio_free(ADP5587_INT_GPIO);
		return rc;
	}

	client->irq = gpio_to_irq(ADP5587_INT_GPIO);
	irq_set_irq_type(client->irq, IRQF_TRIGGER_FALLING);  
	rc = request_irq(client->irq, adp5587_irq_handler,
		IRQF_TRIGGER_FALLING,ADP5587_I2C_NAME,client_data);
	if(rc)
	{
		printk(KERN_ERR "%s: request irq failed\n",__FUNCTION__);
		rc = -EIO;
	}

	return rc;
}

static int adp5587_input_device_init(struct i2c_client *client)
{
	struct adp5587_data *client_data = NULL;
	int i = 0;
	int rc = 0;
	int keynum = 0;
	
	client_data = i2c_get_clientdata(client);
	//Allocate input device
	client_data->input_dev = input_allocate_device();
	
	if(!client_data->input_dev)
	{
		rc = -ENOMEM;
		printk(KERN_ERR "%s: Failed to allocate input device\n",__FUNCTION__);
		goto exit_input_dev_alloc_failed;
	}

	client_data->keymap = keypad_keymap_8930;
	keynum = sizeof(keypad_keymap_8930)/sizeof(keypad_keymap_8930[0]);
	KEYPAD_DEBUG("%s: keynum %d  \n", __FUNCTION__,keynum);

	//Configure the accepted event type
	set_bit(EV_KEY, client_data->input_dev->evbit);

	//Configure the accepted keycodes
	for (i = 0; i < keynum; i++)
	{
		if(client_data->keymap[i])
			set_bit(client_data->keymap[i] & KEY_MAX,client_data->input_dev->keybit);
	}
	
	client_data->input_dev->name = ADP5587_I2C_NAME;
	//Register input device
	rc = input_register_device(client_data->input_dev);
	if(rc)
	{
		printk(KERN_ERR"%s: Unable to register input device: %s\n",
				__FUNCTION__,client_data->input_dev->name);
		goto exit_input_register_device_failed;
	}

exit_input_register_device_failed:
	input_free_device(client_data->input_dev);
exit_input_dev_alloc_failed:
	return rc;
}

static int adp5587_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct adp5587_data *client_data = NULL;
	int rc = 0;
	int i = 0;
	
	if(!qwerty_is_supported())
	{
		printk(KERN_ERR "project doesn't support qwerty \n");		
		rc = -ENODEV;
		goto exit_probe_failed;
	}

	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk(KERN_ERR "adp5587 need I2C_FUNC_I2C\n");
		rc = -ENODEV;
		goto exit_probe_failed;
	}

	client_data = kzalloc(sizeof(struct adp5587_data), GFP_KERNEL);
	if(!client_data)
	{
		printk(KERN_ERR "adp5587 keypad allocate memory failed\n");
		rc = -ENOMEM;
		goto exit_probe_failed;
	}

	adp5587_wq = create_singlethread_workqueue("adp5587_wq");
	if(!adp5587_wq)
	{
		printk(KERN_ERR "create adp5587_wq error\n");
		rc = -ENOMEM;
		goto exit_create_workqueue_failed;
	}

	client_data->client = client;
	i2c_set_clientdata(client, client_data);
	INIT_WORK(&client_data->work, adp5587_work_fuc);
	
	KEYPAD_DEBUG("%s:client->addr=0x%x \n",__FUNCTION__,client->addr);
	rc = adp5587_chip_reset();
	if(rc)
	{
		printk(KERN_ERR "adp5587 reset failed!\n");
		goto exit_chip_reset_failed;
	}
	else
	{
		KEYPAD_DEBUG("%s:set reset ok \n",__FUNCTION__);
	}

	for(i = 0; i < 3; i++)
	{
		rc = i2c_smbus_read_byte_data(client, DEV_ID);
		if(rc < 0)
			continue;
		else
			break;
	}
	if(3 == i)
	{
		printk(KERN_ERR "adp5587 i2c read failed code = %d \n",rc);
		goto exit_chip_reset_failed;
	}
	
	rc = i2c_smbus_read_byte_data(client, INT_STAT);
	printk("adp5587 interrupt status reg = 0x%x\n",rc);

	rc = adp5587_chip_init(client_data);
	if(rc < 0)
	{
		printk(KERN_ERR "%s:adp5587 init failed code = %d \n",__FUNCTION__,rc);
		goto exit_chip_init_failed;
	}

	rc = adp5587_setup_irq(client);
	if(rc < 0)
	{
		printk(KERN_ERR "adp5587_chip_irq rc=%d \n",rc);
		goto exit_reqeust_irq_failed;
	}
	
	rc = irq_set_irq_wake(client->irq, 1);
	if(rc < 0)
	{
		printk(KERN_ERR "adp5587_chip_irq rc=%d \n",rc);
		goto exit_reqeust_irq_failed;
	}
	
	rc = adp5587_input_device_init(client);
	if(rc < 0)
	{
		printk(KERN_ERR "adp5587_init rc=%d \n",rc);
		goto exit_input_device_init_failed;
	}

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
	/* detect current device successful, set the flag as present */
	set_hw_dev_flag(DEV_I2C_KEYPAD);
#endif

	return 0;

exit_input_device_init_failed:
	free_irq(client->irq, client_data);
exit_reqeust_irq_failed:
exit_chip_init_failed:
exit_chip_reset_failed:
	if(adp5587_wq)
	{
		destroy_workqueue(adp5587_wq);
	}
exit_create_workqueue_failed:
	kfree(client_data);
exit_probe_failed:
	return rc;
}

static int adp5587_remove(struct i2c_client *client)
{
	struct adp5587_data *client_data = i2c_get_clientdata(client);
	
	irq_set_irq_wake(client->irq, 0);
	free_irq(client->irq, client_data);
	input_unregister_device(client_data->input_dev);
	kfree(client_data);
	return 0;
}

static int adp5587_resume(struct i2c_client *client)
{
	unsigned char status = 0;
	unsigned char key_event_counter = 0;
	unsigned char key_event = 0;

	struct adp5587_data *adp5587_data = i2c_get_clientdata(client);
	
	//Read interrupt status
	status = i2c_smbus_read_byte_data(client, INT_STAT);
	KEYPAD_DEBUG(" INT_STAT=%d \n",status);
	
	//Check if key events interrupt is detected.
	if(!status & KE_INT)
	{
		goto exit_clear_interrupt_and_enable_irq;
	}
	
	status = i2c_smbus_read_byte_data(client, KEY_LCK_EC_STAT);
	key_event_counter = status & 0x0F;//KEC[3:0] key event counter of key event register
	
	//Keep reading from the keyboard until its queue is empty
	while(key_event_counter--)
	{
		key_event = i2c_smbus_read_byte_data(client, KEY_EVENTA);
		adp5587_report_key(adp5587_data, key_event);
	}

exit_clear_interrupt_and_enable_irq:
	//clear and then enable interrupt
	i2c_smbus_write_byte_data(client,INT_STAT,KE_IEN);
	return 0;
}
static int adp5587_suspend(struct i2c_client *client, pm_message_t mesg)
{
	KEYPAD_DEBUG(" adp5587_suspend do nothing.\n");
	return 0;
}


static int __init adp5587_init(void)
{
	printk(KERN_INFO "ADP5587 keyboard driver: init\n");
	return i2c_add_driver(&adp5587_driver);
}

static void __exit adp5587_exit(void)
{
	i2c_del_driver(&adp5587_driver);
	if(adp5587_wq)
		destroy_workqueue(adp5587_wq);
}

module_init(adp5587_init);
module_exit(adp5587_exit);

MODULE_AUTHOR("Liang YUAN <yuanliang@huawei.com>");
MODULE_DESCRIPTION("ADP5587 keyboard driver");
MODULE_LICENSE("GPL");
