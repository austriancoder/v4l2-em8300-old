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

#include <linux/string.h>
#include <linux/pci.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/delay.h>

#include "em8300_reg.h"
#include "em8300_models.h"

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

#if 0
static void em8300_adv717x_setup(struct em8300_s *em,
				 struct i2c_client *client)
{
	struct getconfig_s data;
	struct setparam_s param;

	if (!((client->driver) && (client->driver->command))) {
		printk("em8300-%d: cannot configure adv717x encoder: "
		       "no client->driver->command\n", em->instance);
		return;
	}

	client->driver->command(client, ENCODER_CMD_ENABLEOUTPUT, (void *)0);

	data.card_nr = em->instance;
	if (client->driver->command(client, ENCODER_CMD_GETCONFIG,
				    (void *) &data) != 0) {
		printk("em8300-%d: ENCODER_CMD_GETCONFIG failed\n",
		       em->instance);
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
#endif

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

static void do_i2c_scan(char *name, struct i2c_client *c)
{
	unsigned char buf;
	int i, rc;

	for (i = 0; i < 128; i++) {
		c->addr = i;
		rc = i2c_master_recv(c, &buf, 0);
		if (rc < 0)
			continue;
		printk("%s: i2c scan: found device @ 0x%x  [%s]\n",
		       name, i << 1, "???");
	}
}

int em8300_i2c_init1(struct em8300_s *em)
{
	int ret, i;
	struct i2c_bus_s *pdata;

	//request_module("i2c-algo-bit");

	switch (em->chip_revision) {
	case 2:
		em->i2c_oe_reg = I2C_OE;
		em->i2c_pin_reg = I2C_PIN;
		break;
	case 1:
		em->i2c_oe_reg = 0x1f4f;
		em->i2c_pin_reg = I2C_OE;
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
			" #%d-%d", em->instance, i);
		em->i2c_adap[i].algo_data = &em->i2c_algo[i];
		em->i2c_adap[i].dev.parent = &em->pci_dev->dev;

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
	}


	em->i2c_client.adapter = &em->i2c_adap[1];
	strlcpy(em->i2c_client.name, "em8300 internal", I2C_NAME_SIZE);
	do_i2c_scan("i2c bus 1", &em->i2c_client);

	return 0;
}

int em8300_i2c_init2(struct em8300_s *em)
{
	int ret;

	/* add only bus 1 */
	ret = i2c_bit_add_bus(&em->i2c_adap[0]);
	if (ret)
		return ret;

	em->i2c_client.adapter = &em->i2c_adap[0];
	strlcpy(em->i2c_client.name, "em8300 internal", I2C_NAME_SIZE);
	do_i2c_scan("i2c bus 0", &em->i2c_client);

	if (known_models[em->model].module.name != NULL)
		request_module(known_models[em->model].module.name);

	if (known_models[em->model].module.name != NULL) {
		em->encoder = v4l2_i2c_new_subdev(&em->v4l2_dev, &em->i2c_adap[0],
				known_models[em->model].module.name, 0,
				&known_models[em->model].module.addr);
	} else {

		/* simply try to find devices */
		em->encoder = v4l2_i2c_new_subdev(&em->v4l2_dev, &em->i2c_adap[0],
						"adv717x", 0, I2C_ADDRS(0x6a));

		if (!em->encoder)
			em->encoder = v4l2_i2c_new_subdev(&em->v4l2_dev, &em->i2c_adap[0],
									"bt865", 0, I2C_ADDRS(0x45));
	}

	if (em->encoder == NULL) {
		printk(KERN_WARNING "em8300-%d: video encoder chip not found\n", em->instance);
		return 0;
	}

	if (!strncmp(em->encoder->name, "ADV7175", 7)) {
		em->encoder_type = ENCODER_ADV7175;
		/*em8300_adv717x_setup(em, em->encoder);*/
	}
	if (!strncmp(em->encoder->name, "ADV7170", 7)) {
		em->encoder_type = ENCODER_ADV7170;
		/*em8300_adv717x_setup(em, em->encoder);*/
	}
	if (!strncmp(em->encoder->name, "bt865", 5)) {
		em->encoder_type = ENCODER_BT865;
	}

	return 0;
}

void em8300_i2c_exit(struct em8300_s *em)
{
	int i;

	if (em->eeprom) {
		i2c_unregister_device(em->eeprom);
		em->eeprom = NULL;
	}
	if (em->encoder) {
		//i2c_unregister_device(em->encoder);
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

/* ----------------------------------------------------------------------- */
/* em9300 access  functions						   */
/* ----------------------------------------------------------------------- */
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
