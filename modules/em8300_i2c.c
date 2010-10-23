/*
 * em8300_i2c.c
 *
 * Copyright (C) 2000-2001 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2001 Rick Haines <rick@kuroyi.net>
 *           (C) 2004 Hakon Skjelten <skjelten@pvv.org>
 *           (C) 2003-2009 Nicolas Boullis <nboullis@debian.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define __NO_VERSION__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include "em8300_reg.h"
#include "em8300_models.h"
#include <linux/em8300.h>
#include "em8300_driver.h"

#include "adv717x.h"
#include "bt865.h"
#include "encoder.h"
#include <linux/sysfs.h>

#define I2C_HW_B_EM8300 0xa

struct i2c_bus_s {
	int clock_pio;
	int data_pio;
	struct em8300_s *em;
};

/* ----------------------------------------------------------------------- */
/* I2C bitbanger functions						   */
/* ----------------------------------------------------------------------- */

/* software I2C functions */

static void em8300_setscl(void *data, int state)
{
	struct i2c_bus_s *bus = (struct i2c_bus_s *) data;
	struct em8300_s *em = bus->em;
	int sel = bus->clock_pio << 8;

	write_register(em->i2c_oe_reg, (sel | bus->clock_pio));
	write_register(em->i2c_pin_reg, sel | (state ? bus->clock_pio : 0));
}

static void em8300_setsda(void *data, int state)
{
	struct i2c_bus_s *bus = (struct i2c_bus_s *) data;
	struct em8300_s *em = bus->em;
	int sel = bus->data_pio << 8;

	write_register(em->i2c_oe_reg, (sel | bus->data_pio));
	write_register(em->i2c_pin_reg, sel | (state ? bus->data_pio : 0));
}

static int em8300_getscl(void *data)
{
	struct i2c_bus_s *bus = (struct i2c_bus_s *)data;
	struct em8300_s *em = bus->em;

	return read_register(em->i2c_pin_reg) & (bus->clock_pio << 8);
}

static int em8300_getsda(void *data)
{
	struct i2c_bus_s *bus = (struct i2c_bus_s *)data;
	struct em8300_s *em = bus->em;

	return read_register(em->i2c_pin_reg) & (bus->data_pio << 8);
}

/* template for i2c_algo_bit */
static const struct i2c_algo_bit_data em8300_i2c_algo_template = {
	.setscl = em8300_setscl,
	.setsda = em8300_setsda,
	.getscl = em8300_getscl,
	.getsda = em8300_getsda,
	.udelay = 10,
	.timeout = 100,
};

static int em8300_i2c_lock_client(struct i2c_client *client)
{
	struct em8300_s *em = i2c_get_adapdata(client->adapter);

	if (!try_module_get(client->driver->driver.owner))
	{
		printk(KERN_ERR "em8300-%d: i2c: Unable to lock client module\n", em->card_nr);
		return -ENODEV;
	}
	return 0;
}

static void em8300_i2c_unlock_client(struct i2c_client *client)
{
	module_put(client->driver->driver.owner);
}

static void em8300_adv717x_setup(struct em8300_s *em,
				 struct i2c_client *client)
{
	struct getconfig_s data;
	struct setparam_s param;

	if (!((client->driver) && (client->driver->command))) {
		printk("em8300-%d: cannot configure adv717x encoder: "
		       "no client->driver->command\n", em->card_nr);
		return;
	}

	client->driver->command(client, ENCODER_CMD_ENABLEOUTPUT, (void *)0);

	data.card_nr = em->card_nr;
	if (client->driver->command(client, ENCODER_CMD_GETCONFIG,
				    (void *) &data) != 0) {
		printk("em8300-%d: ENCODER_CMD_GETCONFIG failed\n",
		       em->card_nr);
		return;
	}

	if (data.config[0] >= 0)
		em->config.adv717x_model.pixelport_16bit =
			data.config[0];
	if (data.config[1] >= 0)
		em->config.adv717x_model.pixelport_other_pal =
			data.config[1];
	if (data.config[2] >= 0)
		em->config.adv717x_model.pixeldata_adjust_ntsc =
			data.config[2];
	if (data.config[3] >= 0)
		em->config.adv717x_model.pixeldata_adjust_pal =
			data.config[3];

	param.param = ENCODER_PARAM_COLORBARS;
	param.modes = (uint32_t)-1;
	param.val = data.config[4] ? 1 : 0;
	client->driver->command(client, ENCODER_CMD_SETPARAM,
				&param);
	param.param = ENCODER_PARAM_OUTPUTMODE;
	param.val = data.config[5];
	client->driver->command(client, ENCODER_CMD_SETPARAM,
				&param);
	param.param = ENCODER_PARAM_PPORT;
	param.modes = NTSC_MODES_MASK;
	param.val = em->config.adv717x_model.pixelport_16bit ? 1 : 0;
	client->driver->command(client, ENCODER_CMD_SETPARAM,
				&param);
	param.modes = PAL_MODES_MASK;
	param.val = em->config.adv717x_model.pixelport_other_pal
		? (em->config.adv717x_model.pixelport_16bit ? 0 : 1)
		: (em->config.adv717x_model.pixelport_16bit ? 1 : 0);
	client->driver->command(client, ENCODER_CMD_SETPARAM,
				&param);
	param.param = ENCODER_PARAM_PDADJ;
	param.modes = NTSC_MODES_MASK;
	param.val = em->config.adv717x_model.pixeldata_adjust_ntsc;
	client->driver->command(client, ENCODER_CMD_SETPARAM,
				&param);
	param.modes = PAL_MODES_MASK;
	param.val = em->config.adv717x_model.pixeldata_adjust_pal;
	client->driver->command(client, ENCODER_CMD_SETPARAM,
				&param);
}

/* ----------------------------------------------------------------------- */
/* I2C functions							   */
/* ----------------------------------------------------------------------- */

/* template for i2c-bit-algo */
static const struct i2c_adapter em8300_i2c_adap_template = {
	.name = "em8300 i2c driver",
	.algo = NULL,                   /* set by i2c-algo-bit */
	.algo_data = NULL,              /* filled from template */
	.owner = THIS_MODULE,
};

int em8300_i2c_init1(struct em8300_s *em)
{
	int ret, i;
	struct i2c_bus_s *pdata;

	//request_module("i2c-algo-bit");

	switch (em->chip_revision) {
	case 2:
		em->i2c_oe_reg = EM8300_I2C_OE;
		em->i2c_pin_reg = EM8300_I2C_PIN;
		break;
	case 1:
		em->i2c_oe_reg = 0x1f4f;
		em->i2c_pin_reg = EM8300_I2C_OE;
		break;
	}

	/*
	  Reset devices on I2C bus
	*/
	write_register(em->i2c_pin_reg, 0x3f3f);
	write_register(em->i2c_oe_reg, 0x3b3b);
	write_register(em->i2c_pin_reg, 0x0100);
	write_register(em->i2c_pin_reg, 0x0101);
	write_register(em->i2c_pin_reg, 0x0808);


	/*
	  Setup algo data structs
	*/
	em->i2c_algo[0] = em8300_i2c_algo_template;
	em->i2c_algo[1] = em8300_i2c_algo_template;

	pdata = kmalloc(sizeof(struct i2c_bus_s), GFP_KERNEL);
	pdata->clock_pio = 0x10;
	pdata->data_pio = 0x8;
	pdata->em = em;

	em->i2c_algo[0].data = pdata;

	pdata = kmalloc(sizeof(struct i2c_bus_s), GFP_KERNEL);
	pdata->clock_pio = 0x4;
	pdata->data_pio = 0x8;
	pdata->em = em;

	em->i2c_algo[1].data = pdata;

	/*
	  Setup i2c adapters
	*/
	for (i = 0; i < 2; i++) {
		/* Setup adapter */
		em->i2c_adap[i] = em8300_i2c_adap_template;
		sprintf(em->i2c_adap[i].name + strlen(em->i2c_adap[i].name),
			" #%d-%d", em->card_nr, i);
		em->i2c_adap[i].algo_data = &em->i2c_algo[i];
		em->i2c_adap[i].dev.parent = &em->dev->dev;

		i2c_set_adapdata(&em->i2c_adap[i], (void *)em);
	}

	/* add only bus 2 */
	ret = i2c_bit_add_bus(&em->i2c_adap[1]);
	if (ret)
		return ret;

	{
		struct i2c_board_info i2c_info;
		const unsigned short eeprom_addr[] = { 0x50, I2C_CLIENT_END };
		i2c_info = (struct i2c_board_info){ I2C_BOARD_INFO("eeprom", 0) };
		em->eeprom = i2c_new_probed_device(&em->i2c_adap[1], &i2c_info, eeprom_addr, NULL);
		if (em->eeprom) {
			if (sysfs_create_link(&em->dev->dev.kobj, &em->eeprom->dev.kobj, "eeprom"))
				printk(KERN_WARNING "em8300-%d: i2c: unable to create the eeprom link\n", em->card_nr);
		}
	}
	return 0;
}

int em8300_i2c_init2(struct em8300_s *em)
{
	int i, ret;

	/* add only bus 1 */
	ret = i2c_bit_add_bus(&em->i2c_adap[0]);
	if (ret)
		return ret;;

	if (known_models[em->model].module.name != NULL)
		request_module(known_models[em->model].module.name);

	if (known_models[em->model].module.name != NULL) {
		struct i2c_board_info i2c_info;
		memset(&i2c_info, 0, sizeof(i2c_info));

		strncpy((char *)&i2c_info.type,
			known_models[em->model].module.name,
			sizeof(i2c_info.type));

		i2c_info.addr = known_models[em->model].module.addr;
		em->encoder = i2c_new_device(&em->i2c_adap[0], &i2c_info);
		if (em->encoder)
			goto found;
	} else {
		struct i2c_board_info i2c_info;
		const unsigned short adv717x_addr[] = { 0x6a, I2C_CLIENT_END };
		const unsigned short bt865_addr[] = { 0x45, I2C_CLIENT_END };
		i2c_info = (struct i2c_board_info){ I2C_BOARD_INFO("adv717x", 0) };
		em->encoder = i2c_new_probed_device(&em->i2c_adap[0], &i2c_info, adv717x_addr, NULL);
		if (em->encoder)
			goto found;
		i2c_info = (struct i2c_board_info){ I2C_BOARD_INFO("bt865", 0) };
		em->encoder = i2c_new_probed_device(&em->i2c_adap[0], &i2c_info, bt865_addr, NULL);
		if (em->encoder)
			goto found;
	}
	printk(KERN_WARNING "em8300-%d: video encoder chip not found\n", em->card_nr);
	return 0;

 found:
	for (i = 0; (i < 50) && !em->encoder->driver; i++) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ/10);
	}
	if (!em->encoder->driver) {	
		printk(KERN_ERR "em8300-%d: encoder chip found but no driver found within 5 seconds\n", em->card_nr);
		i2c_unregister_device(em->encoder);
		em->encoder = NULL;
		return 0;
	}

	em8300_i2c_lock_client(em->encoder);
	if (!strncmp(em->encoder->name, "ADV7175", 7)) {
		em->encoder_type = ENCODER_ADV7175;
		em8300_adv717x_setup(em, em->encoder);
	}
	if (!strncmp(em->encoder->name, "ADV7170", 7)) {
		em->encoder_type = ENCODER_ADV7170;
		em8300_adv717x_setup(em, em->encoder);
	}
	if (!strncmp(em->encoder->name, "BT865", 5)) {
		em->encoder_type = ENCODER_BT865;
	}
	if (sysfs_create_link(&em->dev->dev.kobj, &em->encoder->dev.kobj, "encoder"))
		printk(KERN_WARNING "em8300-%d: i2c: unable to create the encoder link\n", em->card_nr);
	return 0;
}

void em8300_i2c_exit(struct em8300_s *em)
{
	int i;

	if (em->eeprom) {
		sysfs_remove_link(&em->dev->dev.kobj, "eeprom");
		i2c_unregister_device(em->eeprom);
		em->eeprom = NULL;
	}
	if (em->encoder) {
		em8300_i2c_unlock_client(em->encoder);
		sysfs_remove_link(&em->dev->dev.kobj, "encoder");
		i2c_unregister_device(em->encoder);
		em->encoder = NULL;
	}

	/* unregister i2c_bus */
	for (i = 0; i < 2; i++) {
		kfree(em->i2c_algo[i].data);
		i2c_del_adapter(&em->i2c_adap[i]);
	}
}

void em8300_clockgen_write(struct em8300_s *em, int abyte)
{
	int i;

	write_register(em->i2c_pin_reg, 0x808);
	for (i = 0; i < 8; i++) {
		write_register(em->i2c_pin_reg, 0x2000);
		write_register(em->i2c_pin_reg, 0x800 | ((abyte & 1) ? 8 : 0));
		write_register(em->i2c_pin_reg, 0x2020);
		abyte >>= 1;
	}

	write_register(em->i2c_pin_reg, 0x200);
	udelay(10);
	write_register(em->i2c_pin_reg, 0x202);
}

static void I2C_clk(struct em8300_s *em, int level)
{
	write_register(em->i2c_pin_reg, 0x1000 | (level ? 0x10 : 0));
	udelay(10);
}

static void I2C_data(struct em8300_s *em, int level)
{
	write_register(em->i2c_pin_reg, 0x800 | (level ? 0x8 : 0));
	udelay(10);
}

static void I2C_drivedata(struct em8300_s *em, int level)
{
	write_register(em->i2c_oe_reg, 0x800 | (level ? 0x8 : 0));
	udelay(10);
}

#define I2C_read_data ((readl(&em->mem[em->i2c_pin_reg]) & 0x800) ? 1 : 0)

static void I2C_out(struct em8300_s *em, int data, int bits)
{
	int i;
	for (i = bits - 1; i >= 0; i--) {
		I2C_data(em, data & (1 << i));
		I2C_clk(em, 1);
		I2C_clk(em, 0);
	}
}

static int I2C_in(struct em8300_s *em, int bits)
{
	int i, data = 0;

	for (i = bits - 1; i >= 0; i--) {
		data |= I2C_read_data << i;
		I2C_clk(em, 0);
		I2C_clk(em, 1);
	}
	return data;
}

static void sub_23660(struct em8300_s *em, int arg1, int arg2)
{
	I2C_clk(em, 0);
	I2C_out(em, arg1, 8);
	I2C_data(em, arg2);
	I2C_clk(em, 1);
}


static void sub_236f0(struct em8300_s *em, int arg1, int arg2, int arg3)
{
	I2C_clk(em, 1);
	I2C_data(em, 1);
	I2C_clk(em, 0);
	I2C_data(em, 1);
	I2C_clk(em, 1);
	I2C_clk(em, 0);

	sub_23660(em, 1, arg2);

	sub_23660(em, arg1, arg3);
}

void em9010_write(struct em8300_s *em, int reg, int data)
{
	sub_236f0(em, reg, 1, 0);
	sub_23660(em, data, 1);
}

int em9010_read(struct em8300_s *em, int reg)
{
	int val;

	sub_236f0(em, reg, 0, 0);
	I2C_drivedata(em, 0);
	val = I2C_in(em, 8);
	I2C_drivedata(em, 1);
	I2C_clk(em, 0);
	I2C_data(em, 1);
	I2C_clk(em, 1);

	return val;
}

/* loc_2A5d8 in cl.asm
   call dword ptr [exx+0x14]
*/
int em9010_read16(struct em8300_s *em, int reg)
{
	if (reg > 128) {
		em9010_write(em, 3, 0);
		em9010_write(em, 4, reg);
	} else {
		em9010_write(em, 4, 0);
		em9010_write(em, 3, reg);
	}

	return em9010_read(em, 2) | (em9010_read(em, 1) << 8);
}

/* loc_2A558 in cl.asm
   call dword ptr [exx+0x10]
*/
void em9010_write16(struct em8300_s *em, int reg, int value)
{
	if (reg > 128) {
		em9010_write(em, 3, 0);
		em9010_write(em, 4, reg);
	} else {
		em9010_write(em, 4, 0);
		em9010_write(em, 3, reg);
	}
	em9010_write(em, 2, value & 0xff);
	em9010_write(em, 1, value >> 8);
}

