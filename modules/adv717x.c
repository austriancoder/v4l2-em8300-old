/*
   ADV7175A - Analog Devices ADV7175A video encoder driver version 0.0.3

   Copyright (C) 2000 Henrik Johannson <henrikjo@post.utfors.se>
   Copyright (C) 2007, 2009 Nicolas Boullis <nboullis@debian.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/signal.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <linux/sched.h>
#include <linux/types.h>

#include <linux/videodev.h>
#include <asm/uaccess.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include <linux/em8300.h>

#include "em8300_compat24.h"
#include "em8300_reg.h"
#include "em8300_driver.h"

#include "adv717x.h"
#include "encoder.h"

#include "em8300_version.h"

MODULE_SUPPORTED_DEVICE("adv717x");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(EM8300_VERSION);
#endif

EXPORT_NO_SYMBOLS;

int pixelport_16bit[EM8300_MAX] = { [ 0 ... EM8300_MAX-1 ] = -1 };
module_param_array(pixelport_16bit, bool, NULL, 0444);
MODULE_PARM_DESC(pixelport_16bit, "Changes how the ADV717x expects its input data to be formatted. If the colours on the TV appear green, try changing this. Defaults to 1.");

int pixelport_other_pal[EM8300_MAX] = { [ 0 ... EM8300_MAX-1 ] = -1 };
module_param_array(pixelport_other_pal, bool, NULL, 0444);
MODULE_PARM_DESC(pixelport_other_pal, "If this is set to 1, then the pixelport setting is swapped for PAL from the setting given with pixelport_16bit. Defaults to 1.");

int pixeldata_adjust_ntsc[EM8300_MAX] = { [ 0 ... EM8300_MAX-1 ] = -1 };
module_param_array(pixeldata_adjust_ntsc, int, NULL, 0444);
MODULE_PARM_DESC(pixeldata_adjust_ntsc, "If your red and blue colours are swapped in NTSC, try setting this to 0,1,2 or 3. Defaults to 1.");

int pixeldata_adjust_pal[EM8300_MAX] = { [ 0 ... EM8300_MAX-1 ] = -1 };
module_param_array(pixeldata_adjust_pal, int, NULL, 0444);
MODULE_PARM_DESC(pixeldata_adjust_pal, "If your red and blue colours are swapped in PAL, try setting this to 0,1,2 or 3. Defaults to 1.");

static int color_bars[EM8300_MAX] = { [ 0 ... EM8300_MAX-1 ] = 0 };
module_param_array(color_bars, bool, NULL, 0444);
MODULE_PARM_DESC(color_bars, "If you set this to 1 a set of color bars will be displayed on your screen (used for testing if the chip is working). Defaults to 0.");

typedef enum {
	MODE_COMPOSITE_SVIDEO,
	MODE_SVIDEO,
	MODE_COMPOSITE,
	MODE_COMPOSITE_PSEUDO_SVIDEO,
	MODE_PSEUDO_SVIDEO,
	MODE_COMPOSITE_OVER_SVIDEO,
	MODE_YUV,
	MODE_RGB,
	MODE_RGB_NOSYNC,
	MODE_MAX
} output_mode_t;

struct output_conf_s {
	int component;
	int yuv;
	int euroscart;
	int progressive;
	int sync_all;
	int dacA;
	int dacB;
	int dacC;
	int dacD;
};

#include "encoder_output_mode.h"

static output_mode_t output_mode_nr[EM8300_MAX] = { [ 0 ... EM8300_MAX-1 ] = MODE_COMPOSITE_SVIDEO };
module_param_array_named(output_mode, output_mode_nr, output_mode_t, NULL, 0444);
MODULE_PARM_DESC(output_mode, "Specifies the output mode to use for the ADV717x video encoder. See the README-modoptions file for the list of mode names to use. Default is SVideo + composite (\"comp+svideo\").");


/* Common register offset definitions */
#define ADV717X_REG_MR0		0x00
#define ADV717X_REG_MR1		0x01
#define ADV717X_REG_TR0		0x07

/* Register offsets specific to the ADV717[56]A chips */
#define ADV7175_REG_SCFREQ0	0x02
#define ADV7175_REG_SCFREQ1	0x03
#define ADV7175_REG_SCFREQ2	0x04
#define ADV7175_REG_SCFREQ3	0x05
#define ADV7175_REG_SCPHASE	0x06
#define ADV7175_REG_CCED0	0x08
#define ADV7175_REG_CCED1	0x09
#define ADV7175_REG_CCD0	0x0a
#define ADV7175_REG_CCD1	0x0b
#define ADV7175_REG_TR1		0x0c
#define ADV7175_REG_MR2		0x0d
#define ADV7175_REG_PCR0	0x0e
#define ADV7175_REG_PCR1	0x0f
#define ADV7175_REG_PCR2	0x10
#define ADV7175_REG_PCR3	0x11
#define ADV7175_REG_MR3		0x12
#define ADV7175_REG_TTXRQ_CTRL	0x24

/* Register offsets specific to the ADV717[01] chips */
#define ADV7170_REG_MR2		0x02
#define ADV7170_REG_MR3		0x03
#define ADV7170_REG_MR4		0x04
#define ADV7170_REG_TR1		0x08
#define ADV7170_REG_SCFREQ0	0x09
#define ADV7170_REG_SCFREQ1	0x0a
#define ADV7170_REG_SCFREQ2	0x0b
#define ADV7170_REG_SCFREQ3	0x0c
#define ADV7170_REG_SCPHASE	0x0d
#define ADV7170_REG_CCED0	0x0e
#define ADV7170_REG_CCED1	0x0f
#define ADV7170_REG_CCD0	0x10
#define ADV7170_REG_CCD1	0x11
#define ADV7170_REG_PCR0	0x12
#define ADV7170_REG_PCR1	0x13
#define ADV7170_REG_PCR2	0x14
#define ADV7170_REG_PCR3	0x15
#define ADV7170_REG_TTXRQ_CTRL	0x19

static int adv717x_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int adv717x_remove(struct i2c_client *client);
static int adv717x_command(struct i2c_client *client, unsigned int cmd, void *arg);

static const mode_info_t mode_info[] = {
	[ MODE_COMPOSITE_SVIDEO ] =		{ "comp+svideo" , { 0, 0, 0, 0, 0, 1, 0, 0, 0 } },
	[ MODE_SVIDEO ] =			{ "svideo"      , { 0, 0, 0, 0, 0, 1, 1, 0, 0 } },
	[ MODE_COMPOSITE ] =			{ "comp"        , { 0, 0, 0, 0, 0, 1, 0, 1, 1 } },
	[ MODE_COMPOSITE_PSEUDO_SVIDEO ] =	{ "comp+psvideo", { 0, 0, 1, 0, 0, 1, 0, 0, 0 } },
	[ MODE_PSEUDO_SVIDEO ] =		{ "psvideo"     , { 0, 0, 1, 0, 0, 1, 1, 0, 0 } },
	[ MODE_COMPOSITE_OVER_SVIDEO ] =	{ "composvideo" , { 0, 0, 1, 0, 0, 1, 1, 1, 0 } },
	[ MODE_YUV ] =				{ "yuv"         , { 1, 1, 0, 0, 0, 1, 0, 0, 0 } },
	[ MODE_RGB ] =				{ "rgbs"        , { 1, 0, 0, 0, 1, 0, 0, 0, 0 } },
	[ MODE_RGB_NOSYNC ] =			{ "rgb"         , { 1, 0, 0, 0, 0, 0, 0, 0, 0 } },
};

#define CHIP_ADV7175A 1
#define CHIP_ADV7170  2

struct mode_config_s {
	char const *name;
	unsigned char * val;
};

struct adv717x_data_s {
	int chiptype;
	int chiprev;
	int mode;
	int enableoutput;

	int configlen;

	unsigned int modes;
	struct mode_config_s *conf;
};

static struct i2c_device_id adv717x_idtable[] = {
	{ "adv717x", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, adv717x_idtable);

/* This is the driver that will be inserted */
static struct i2c_driver adv717x_driver = {
	.driver = {
		.name =		"adv717x",
	},
	.id_table =		adv717x_idtable,
	.probe =		&adv717x_probe,
	.remove =		&adv717x_remove,
	.command =		&adv717x_command
};

int adv717x_id = 0;

static unsigned char PAL_config_7170[27] = {
	0x05,   // Mode Register 0
	0x00,   // Mode Register 1 (was: 0x07)
	0x02,   // Mode Register 2 (was: 0x48)
	0x00,   // Mode Register 3
	0x00,   // Mode Register 4
	0x00,   // Reserved
	0x00,   // Reserved
	0x0d,   // Timing Register 0
	0x77,   // Timing Register 1
	0xcb,   // Subcarrier Frequency Register 0
	0x8a,   // Subcarrier Frequency Register 1
	0x09,   // Subcarrier Frequency Register 2
	0x2a,   // Subcarrier Frequency Register 3
	0x00,   // Subcarrier Phase Register
	0x00,   // Closed Captioning Ext Register 0
	0x00,   // Closed Captioning Ext Register 1
	0x00,   // Closed Captioning Register 0
	0x00,   // Closed Captioning Register 1
	0x00,   // Pedestal Control Register 0
	0x00,   // Pedestal Control Register 1
	0x00,   // Pedestal Control Register 2
	0x00,   // Pedestal Control Register 3
	0x00,	// CGMS_WSS Reg 0
	0x00,	// CGMS_WSS Reg 1
	0x00,	// CGMS_WSS Reg 2
	0x00	// TeleText Control Register
};

static unsigned char NTSC_config_7170[27] = {
	0x10,   // Mode Register 0
	0x06,   // Mode Register 1
	0x08,   // Mode Register 2
	0x00,   // Mode Register 3
	0x00,   // Mode Register 4
	0x00,   // Reserved
	0x00,   // Reserved
	0x0d,   // Timing Register 0
	0x77,   // Timing Register 1
	0x16,   // Subcarrier Frequency Register 0
	0x7c,   // Subcarrier Frequency Register 1
	0xf0,   // Subcarrier Frequency Register 2
	0x21,   // Subcarrier Frequency Register 3
	0x00,   // Subcarrier Phase Register
	0x00,   // Closed Captioning Ext Register 0
	0x00,   // Closed Captioning Ext Register 1
	0x00,   // Closed Captioning Register 0
	0x00,   // Closed Captioning Register 1
	0x00,   // Pedestal Control Register 0
	0x00,   // Pedestal Control Register 1
	0x00,   // Pedestal Control Register 2
	0x00,   // Pedestal Control Register 3
	0x00,	// CGMS_WSS Reg 0
	0x00,	// CGMS_WSS Reg 1
	0x00,	// CGMS_WSS Reg 2
	0x00	// TeleText Control Register
};

static unsigned char PAL_M_config_7175[19] = {   //These need to be tested
	0x06,   // Mode Register 0
	0x00,   // Mode Register 1
	0xa3,   // Subcarrier Frequency Register 0
	0xef,	// Subcarrier Frequency Register 1
	0xe6,	// Subcarrier Frequency Register 2
	0x21,	// Subcarrier Frequency Register 3
	0x00,   // Subcarrier Phase Register
	0x0d,   // Timing Register 0
	0x00,   // Closed Captioning Ext Register 0
	0x00,   // Closed Captioning Ext Register 1
	0x00,   // Closed Captioning Register 0
	0x00,   // Closed Captioning Register 1
	0x70,	// Timing Register 1
	0x73,	// Mode Register 2
	0x00,   // Pedestal Control Register 0
	0x00,   // Pedestal Control Register 1
	0x00,   // Pedestal Control Register 2
	0x00,   // Pedestal Control Register 3
	0x42,   // Mode Register 3
};

static unsigned char PAL_config_7175[19] = {
	0x01,   // Mode Register 0
	0x06,   // Mode Register 1
	0xcb,   // Subcarrier Frequency Register 0
	0x8a,   // Subcarrier Frequency Register 1
	0x09,   // Subcarrier Frequency Register 2
	0x2a,   // Subcarrier Frequency Register 3
	0x00,   // Subcarrier Phase Register
	0x0d,   // Timing Register 0
	0x00,   // Closed Captioning Ext Register 0
	0x00,   // Closed Captioning Ext Register 1
	0x00,   // Closed Captioning Register 0
	0x00,   // Closed Captioning Register 1
	0x73,   // Timing Register 1
	0x08,   // Mode Register 2
	0x00,   // Pedestal Control Register 0
	0x00,   // Pedestal Control Register 1
	0x00,   // Pedestal Control Register 2
	0x00,   // Pedestal Control Register 3
	0x42,   // Mode Register 3
};

static unsigned char PAL60_config_7175[19] = {
	0x12,   // Mode Register 0
	0x0,	// Mode Register 1
	0xcb,   // Subcarrier Frequency Register 0
	0x8a,   // Subcarrier Frequency Register 1
	0x09,   // Subcarrier Frequency Register 2
	0x2a,   // Subcarrier Frequency Register 3
	0x00,   // Subcarrier Phase Register
	0x0d,   // Timing Register 0
	0x00,   // Closed Captioning Ext Register 0
	0x00,   // Closed Captioning Ext Register 1
	0x00,   // Closed Captioning Register 0
	0x00,   // Closed Captioning Register 1
	0x73,   // Timing Register 1
	0x08,   // Mode Register 2
	0x00,   // Pedestal Control Register 0
	0x00,   // Pedestal Control Register 1
	0x00,   // Pedestal Control Register 2
	0x00,   // Pedestal Control Register 3
	0x42,   // Mode Register 3
};

static unsigned char NTSC_config_7175[19] = {
	0x04,   // Mode Register 0
	0x00,   // Mode Register 1
	0x16,   // Subcarrier Frequency Register 0
	0x7c,	// Subcarrier Frequency Register 1
	0xf0,	// Subcarrier Frequency Register 2
	0x21,	// Subcarrier Frequency Register 3
	0x00,   // Subcarrier Phase Register
	0x0d,   // Timing Register 0
	0x00,   // Closed Captioning Ext Register 0
	0x00,   // Closed Captioning Ext Register 1
	0x00,   // Closed Captioning Register 0
	0x00,   // Closed Captioning Register 1
	0x77,	// Timing Register 1
	0x00,	// Mode Register 2
	0x00,   // Pedestal Control Register 0
	0x00,   // Pedestal Control Register 1
	0x00,   // Pedestal Control Register 2
	0x00,   // Pedestal Control Register 3
	0x42,   // Mode Register 3
};

#define SET_REG(f,o,v) (f) = ((f) & ~(1<<(o))) | (((v) & 1) << (o))

static int adv717x_reset(struct i2c_client *client)
{
	struct adv717x_data_s *data = i2c_get_clientdata(client);
	i2c_smbus_write_byte_data(client, ADV717X_REG_TR0,
				  data->conf[data->mode].val[ADV717X_REG_TR0] & 0x7f);
	i2c_smbus_write_byte_data(client, ADV717X_REG_TR0,
				  data->conf[data->mode].val[ADV717X_REG_TR0] | 0x80);
	i2c_smbus_write_byte_data(client, ADV717X_REG_TR0,
				  data->conf[data->mode].val[ADV717X_REG_TR0] & 0x7f);
	return 0;
}

static int adv717x_update(struct i2c_client *client)
{
	struct adv717x_data_s *data = i2c_get_clientdata(client);
	int i;

	for(i=0; i < data->configlen; i++) {
		i2c_smbus_write_byte_data(client, i,
					  data->conf[data->mode].val[i]
					  | (((i == ADV717X_REG_MR1)&&(!data->enableoutput))?0x78:0x00));
	}

	return adv717x_reset(client);
}

static int adv717x_setmode(int mode, struct i2c_client *client) {
	struct adv717x_data_s *data = i2c_get_clientdata(client);

	pr_debug("adv717x_setmode(%d,%p)\n", mode, client);

	if ((mode < 0)
	    || (mode >= data->modes)
	    || ! data->conf[mode].val) {
		return -EINVAL;
	}

	data->mode = mode;

	return 0;
}

static int adv717x_set_pixelport(struct i2c_client *client,
				 uint32_t modes, int val)
{
	struct adv717x_data_s *data = i2c_get_clientdata(client);
	int i;

	if ((val != ADV717X_PIXELPORT_8BIT)
	    &&(val != ADV717X_PIXELPORT_16BIT))
		return -EINVAL;

	for (i = 0; i < data->modes; i++) {
		if ((((modes >> i) & 1) == 0) || (data->conf[i].val == NULL))
			continue;
		data->conf[i].val[ADV717X_REG_TR0] =
			(data->conf[i].val[ADV717X_REG_TR0] & ~0x40)
			| ((val == ADV717X_PIXELPORT_16BIT)?0x40:0x00);
	}
	return 0;
}

static int adv717x_set_pixeldataadj(struct i2c_client *client,
				    uint32_t modes, int val)
{
	struct adv717x_data_s *data = i2c_get_clientdata(client);
	int i;

	if ((val < 0) || (val > 3))
		return -EINVAL;

	for (i = 0; i < data->modes; i++) {
		if ((((modes >> i) & 1) == 0) || (data->conf[i].val == NULL))
			continue;
		switch(data->chiptype) {
		case CHIP_ADV7170:
			data->conf[i].val[ADV7170_REG_TR1] =
				(data->conf[i].val[ADV7170_REG_TR1] & ~0xC0)
				| ((val & 3) << 6);
			break;
		case CHIP_ADV7175A:
			data->conf[i].val[ADV7175_REG_TR1] =
				(data->conf[i].val[ADV7175_REG_TR1] & ~0xC0)
				| ((val & 3) << 6);
			break;
		}
	}
	return 0;
}

static int adv717x_set_outputmode(struct i2c_client *client,
				  uint32_t modes, output_mode_t out_mode)
{
	struct adv717x_data_s *data = i2c_get_clientdata(client);
	int i;

	if ((out_mode < 0) || (out_mode >= MODE_MAX))
		return -EINVAL;

	for (i = 0; i < data->modes; i++) {
		if ((((modes >> i) & 1) == 0) || (data->conf[i].val == NULL))
			continue;
		switch(data->chiptype) {
		case CHIP_ADV7175A:
			/* ADV7175/6A component out: MR06 (register 0, bit 6) */
			SET_REG(data->conf[i].val[ADV717X_REG_MR0], 6,
				mode_info[out_mode].conf.component);
			/* ADV7175/6A YUV out: MR26 (register 13, bit 6) */
			SET_REG(data->conf[i].val[ADV7175_REG_MR2], 6,
				mode_info[out_mode].conf.yuv);
			/* ADV7175/6A EuroSCART: MR37 (register 18, bit 7) */
			SET_REG(data->conf[i].val[ADV7175_REG_MR3], 7,
				mode_info[out_mode].conf.euroscart);
			/* ADV7175/6A RGB sync: MR05 (register 0, bit 5) */
			SET_REG(data->conf[i].val[ADV717X_REG_MR0], 5,
				mode_info[out_mode].conf.sync_all);
			break;
		case CHIP_ADV7170:
			/* ADV7170/1 component out: MR40 (register 4, bit 0) */
			SET_REG(data->conf[i].val[ADV7170_REG_MR4], 0,
				mode_info[out_mode].conf.component);
			/* ADV7170/1 YUV out: MR41 (register 4, bit 1) */
			SET_REG(data->conf[i].val[ADV7170_REG_MR4], 1,
				mode_info[out_mode].conf.yuv);
			/* ADV7170/1 EuroSCART: MR33 (register 3, bit 3) */
			SET_REG(data->conf[i].val[ADV7170_REG_MR3], 3,
				mode_info[out_mode].conf.euroscart);
			/* ADV7170/1 RGB sync: MR42 (register 4, bit 2) */
			SET_REG(data->conf[i].val[ADV7170_REG_MR4], 2,
				mode_info[out_mode].conf.sync_all);
			break;
		}
		/* ADV7170/1/5A/6A non-interlace: MR10 (register 1, bit 0) */
		SET_REG(data->conf[i].val[ADV717X_REG_MR1], 0,
			mode_info[out_mode].conf.progressive);
		/* ADV7170/1/5A/6A DAC A control: MR16 (register 1, bit 6) */
		SET_REG(data->conf[i].val[ADV717X_REG_MR1], 6,
			mode_info[out_mode].conf.dacA);
		/* ADV7170/1/5A/6A DAC B control: MR15 (register 1, bit 5) */
		SET_REG(data->conf[i].val[ADV717X_REG_MR1], 5,
			mode_info[out_mode].conf.dacB);
		/* ADV7170/1/5A/6A DAC C control: MR13 (register 1, bit 3) */
		SET_REG(data->conf[i].val[ADV717X_REG_MR1], 3,
			mode_info[out_mode].conf.dacC);
		/* ADV7170/1/5A/6A DAC D control: MR14 (register 1, bit 4) */
		SET_REG(data->conf[i].val[ADV717X_REG_MR1], 4,
			mode_info[out_mode].conf.dacD);
	}
	return 0;
}

static int adv717x_set_colorbars(struct i2c_client *client,
				 uint32_t modes, int val)
{
	struct adv717x_data_s *data = i2c_get_clientdata(client);
	int i;

	if ((val < 0) || (val > 1))
		return -EINVAL;

	for (i = 0; i < data->modes; i++) {
		if ((((modes >> i) & 1) == 0) || (data->conf[i].val == NULL))
			continue;
		SET_REG(data->conf[i].val[ADV717X_REG_MR1], 7, val);
	}
	return 0;
}

static int adv7170_setup(struct i2c_client *client)
{
	struct adv717x_data_s *data = i2c_get_clientdata(client);
	unsigned int i, count;

	data->configlen = 27;
	data->modes = 8;
	data->conf = kzalloc(data->modes*sizeof(struct mode_config_s)
			     + 4*data->configlen*sizeof(unsigned char),
			     GFP_KERNEL);
	if (!data->conf)
		return -ENOMEM;

	count = 0;
	for (i = 0; i < data->modes; i++) {
		switch (i) {
		case ENCODER_MODE_PAL:
		case ENCODER_MODE_PAL_M:
		case ENCODER_MODE_PAL60:
			data->conf[i].name = "PAL";
			data->conf[i].val = ((unsigned char *)data->conf)
				+ data->modes*sizeof(struct mode_config_s)
				+ (count++)*data->configlen*sizeof(unsigned char);
			memcpy(data->conf[i].val, PAL_config_7170, data->configlen);
			goto common;
		case ENCODER_MODE_NTSC:
			data->conf[i].name = "NTSC";
			data->conf[i].val = ((unsigned char *)data->conf)
				+ data->modes*sizeof(struct mode_config_s)
				+ (count++)*data->configlen*sizeof(unsigned char);
			memcpy(data->conf[i].val, NTSC_config_7170, data->configlen);
			goto common;
		common:
			if (strncmp(client->adapter->name, "EM8300", 6) == 0) {
				struct em8300_s *em = i2c_get_adapdata(client->adapter);
				SET_REG(data->conf[i].val[ADV717X_REG_MR1], 7, color_bars[em->instance]);
			}
			break;
		default:
			data->conf[i].name = NULL;
			data->conf[i].val = NULL;
		}
	}

	data->mode = ENCODER_MODE_PAL60;
	data->enableoutput = 1;

	return 0;
}

static int adv7175a_setup(struct i2c_client *client)
{
	struct adv717x_data_s *data = i2c_get_clientdata(client);
	unsigned int i, count;

	data->configlen = 19;
	data->modes = 8;
	data->conf = kzalloc(data->modes*sizeof(struct mode_config_s)
			     + 4*data->configlen*sizeof(unsigned char),
			     GFP_KERNEL);
	if (!data->conf)
		return -ENOMEM;

	count = 0;
	for (i = 0; i < data->modes; i++) {
		switch (i) {
		case ENCODER_MODE_PAL:
			data->conf[i].name = "PAL";
			data->conf[i].val = ((unsigned char *)data->conf)
				+ data->modes*sizeof(struct mode_config_s)
				+ (count++)*data->configlen*sizeof(unsigned char);
			memcpy(data->conf[i].val, PAL_config_7175, data->configlen);
			goto common;
		case ENCODER_MODE_PAL_M:
			data->conf[i].name = "PAL M";
			data->conf[i].val = ((unsigned char *)data->conf)
				+ data->modes*sizeof(struct mode_config_s)
				+ (count++)*data->configlen*sizeof(unsigned char);
			memcpy(data->conf[i].val, PAL_M_config_7175, data->configlen);
			goto common;
		case ENCODER_MODE_PAL60:
			data->conf[i].name = "PAL 60";
			data->conf[i].val = ((unsigned char *)data->conf)
				+ data->modes*sizeof(struct mode_config_s)
				+ (count++)*data->configlen*sizeof(unsigned char);
			memcpy(data->conf[i].val, PAL60_config_7175, data->configlen);
			goto common;
		case ENCODER_MODE_NTSC:
			data->conf[i].name = "NTSC";
			data->conf[i].val = ((unsigned char *)data->conf)
				+ data->modes*sizeof(struct mode_config_s)
				+ (count++)*data->configlen*sizeof(unsigned char);
			memcpy(data->conf[i].val, NTSC_config_7175, data->configlen);
			goto common;
		common:
			if (strncmp(client->adapter->name, "EM8300", 6) == 0) {
				struct em8300_s *em = i2c_get_adapdata(client->adapter);
				SET_REG(data->conf[i].val[ADV717X_REG_MR1], 7, color_bars[em->instance]);
			}
			break;
		default:
			data->conf[i].name = NULL;
			data->conf[i].val = NULL;
		}
	}

	data->mode = ENCODER_MODE_PAL60;
	data->enableoutput = 1;

	return 0;
}

static int adv717x_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct adv717x_data_s *data;
	int result;
	int err = 0;

/*
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
		return 0;
	}
*/

	if (!(data = kzalloc(sizeof(struct adv717x_data_s), GFP_KERNEL))) {
		return -ENOMEM;
	}

	i2c_set_clientdata(client, data);

	/* Registers 0x25 to 0x2f are implemented on ADV7170 but not on ADV7175A chips */
	result = i2c_smbus_read_byte_data(client, 0x25);
	data->chiptype = (result < 0)?CHIP_ADV7175A:CHIP_ADV7170;

	switch (data->chiptype) {
	case CHIP_ADV7175A:
		result = i2c_smbus_read_byte_data(client, ADV7175_REG_MR3);
		if (result < 0) {
			err = -ENODEV;
			goto cleanup;
		}
		data->chiprev = result & 0x1;
		strcpy(client->name, "ADV7175A chip");
		printk(KERN_NOTICE "adv717x.o: ADV7175A rev. %d chip probed\n", data->chiprev);
		if ((err = adv7175a_setup(client)))
			goto cleanup;
		break;
	case CHIP_ADV7170:
		result = i2c_smbus_read_byte_data(client, ADV7170_REG_MR3);
		if (result < 0) {
			err = -ENODEV;
			goto cleanup;
		}
		data->chiprev = result & 0x3;
		strcpy(client->name, "ADV7170 chip");
		printk(KERN_NOTICE "adv717x.o: ADV7170 rev. %d chip probed\n", data->chiprev);
		if ((err = adv7170_setup(client)))
			goto cleanup;
		break;
	default:
		printk(KERN_ERR "adv717x.o: WTF!?\n");
		goto cleanup;
	}

	adv717x_update(client);

	return 0;

 cleanup:
	kfree(data);

	return err;
}

static int adv717x_remove(struct i2c_client *client)
{
	struct adv717x_data_s *data = i2c_get_clientdata(client);

	kfree(data->conf);
	kfree(data);

	return 0;
}

static int adv717x_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct adv717x_data_s *data = i2c_get_clientdata(client);

	switch (cmd) {
	case ENCODER_CMD_SETMODE:
		adv717x_setmode((long int) arg, client);
		adv717x_update(client);
		break;
	case ENCODER_CMD_ENABLEOUTPUT:
		data->enableoutput = (long int) arg;
		adv717x_update(client);
		break;
	case ENCODER_CMD_SETPARAM:
	{
		struct setparam_s *data = arg;
		switch (data->param) {
		case ENCODER_PARAM_COLORBARS:
			ret = adv717x_set_colorbars(client, data->modes, data->val);
			break;
		case ENCODER_PARAM_OUTPUTMODE:
			ret = adv717x_set_outputmode(client, data->modes, data->val);
			break;
		case ENCODER_PARAM_PPORT:
			ret = adv717x_set_pixelport(client, data->modes, data->val);
			break;
		case ENCODER_PARAM_PDADJ:
			ret = adv717x_set_pixeldataadj(client, data->modes, data->val);
			break;
		default:
			ret = -EINVAL;
		}
		break;
	}
	case ENCODER_CMD_GETCONFIG:
	{
		struct getconfig_s *data = arg;
		data->config[0] = pixelport_16bit[data->card_nr];
		data->config[1] = pixelport_other_pal[data->card_nr];
		data->config[2] = pixeldata_adjust_ntsc[data->card_nr];
		data->config[3] = pixeldata_adjust_pal[data->card_nr];
		data->config[4] = color_bars[data->card_nr];
		data->config[5] = output_mode_nr[data->card_nr];
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* ----------------------------------------------------------------------- */


int __init adv717x_init(void)
{
	//request_module("i2c-algo-bit");
	return i2c_add_driver(&adv717x_driver);
}

void __exit adv717x_cleanup(void)
{
	i2c_del_driver(&adv717x_driver);
}

module_init(adv717x_init);
module_exit(adv717x_cleanup);
