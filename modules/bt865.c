/*
   BT865A - Brook Tree BT865A video encoder driver version 0.0.4

   Henrik Johannson <henrikjo@post.utfors.se>
   As modified by Chris C. Hoover <cchoover@home.com>

   Modified by Luis Correia <lfcorreia@users.sourceforge.net>
   added rgb_mode (requires hacking DXR3 hardware)

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

#include "em8300_compat24.h"
#include "bt865.h"
#include "encoder.h"

#include "em8300_driver.h"
#include "em8300_version.h"

MODULE_SUPPORTED_DEVICE("bt865");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(EM8300_VERSION);
#endif

EXPORT_NO_SYMBOLS;

static int color_bars[EM8300_MAX] = { [ 0 ... EM8300_MAX-1 ] = 0 };
module_param_array(color_bars, bool, NULL, 0444);
MODULE_PARM_DESC(color_bars, "If you set this to 1 a set of color bars will be displayed on your screen (used for testing if the chip is working). Defaults to 0.");

typedef enum {
	MODE_COMPOSITE_SVIDEO,
	MODE_RGB,
	MODE_MAX
} output_mode_t;

struct output_conf_s {
};

#include "encoder_output_mode.h"

static output_mode_t output_mode_nr[EM8300_MAX] = { [ 0 ... EM8300_MAX-1 ] = MODE_COMPOSITE_SVIDEO };

module_param_array_named(output_mode, output_mode_nr, output_mode_t, NULL, 0444);
MODULE_PARM_DESC(output_mode, "Specifies the output mode to use for the BT865 video encoder. See the README-modoptions file for the list of mode names to use. Default is SVideo + composite (\"comp+svideo\").");

static int bt865_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int bt865_remove(struct i2c_client *client);
static int bt865_command(struct i2c_client *client, unsigned int cmd, void *arg);
static int bt865_setup(struct i2c_client *client);

static const mode_info_t mode_info[] = {
	[ MODE_COMPOSITE_SVIDEO ] =		{ "comp+svideo" , { } },
	[ MODE_RGB ] =				{ "rgb"         , { } },
};

struct bt865_data_s {
	int chiptype;
	int mode;
	int bars;
	int rgbmode;
	int enableoutput;

	unsigned char config[48];
	int configlen;
};

static struct i2c_device_id bt865_idtable[] = {
	{ "bt865", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, bt865_idtable);

/* This is the driver that will be inserted */
static struct i2c_driver bt865_driver = {
	.driver = {
		.name =		"bt865",
	},
	.id_table =		bt865_idtable,
	.probe =		&bt865_probe,
	.remove =		&bt865_remove,
	.command =		&bt865_command
};

int bt865_id = 0;

// starts at register A0 by twos
static unsigned char NTSC_CONFIG_BT865[ 48 ] = {
/*  0 A0 */	0x00,	// EWSF2 EWSF1 RSRVD[1:0] WSDAT[4:1]
/*  1 A2 */	0x00,	// WSDAT[12:5]
/*  2 A4 */	0x00,	// WSDAT[20:13]
/*  3 A6 */	0x00,	// SRESET RSRVD[6:0]
/*  4 A8 */	0x00,	// RSRVD[7:0]
/*  5 AA */	0x00,	// RSRVD[7:0]
/*  6 AC */	0x00,	// TXHS[7:0]
/*  7 AE */	0x00,	// TXHE[7:0]
/*  8 B0 */	0x00,	// LUMADLY[1:0] TXHE[10:8] TXHS[10:8]
/*  9 B2 */	0x00,	// RSRVD[1:0] TXRM TXE TXEF2[8] TXBF2[8] TXEF1[8] TXBF1[8]
/* 10 B4 */	0x00,	// TXBF1[7:0]
/* 11 B6 */	0x00,	// TXEF1[7:0]
/* 12 B8 */	0x00,	// TXBF2[7:0]
/* 13 BA */	0x00,	// TXEF2[7:0]
/* 14 BC */	0xc1,	// ECCF2 ECCF1 ECCGATE RSRVD DACOFF YC16 CBSWAP PORCH
/* 15 BE */	0x00,	// CCF2B1[7:0]
/* 16 C0 */	0x00,	// CCF2B2[7:0]
/* 17 C2 */	0x00,	// CCF1B1[7:0]
/* 18 C4 */	0x00,	// CCF1B2[7:0]
/* 19 C6 */	0x00,	// HSYNCF[7:0]
/* 20 C8 */	0x00,	// HSYNCR[7:0]
/* 21 CA */	0x00,	// SYNCDLY FIELD1 SYNCDIS ADJHSYNC HSYNCF[9:8] HSYNCR[9:8]
/* 22 CC */	0x00,	// SETMODE SETUPDIS VIDFORM[3:0] NONINTL SQUARE
/* 23 CE */	0x04,	// ESTATUS RGBO DCHROMA ECBAR SCRESET EVBI EACTIVE ECLIP
/* 24 D0 */	0x00,	// RSRVD[6:0] PALN
/* 25 D2 */	0x00,	// RSRVD[7:0]
/* 26 D4 */	0x00,	// RSRVD[7:0]
/* 27 D6 */	0x00,	// RSRVD[7:0]
/* 28 D8 */	0x00,	// RSRVD[7:0]
/* 29 DA */	0x00,	// RSRVD[7:0]
/* 30 DC */	0x00,	// RSRVD[7:0]
/* 31 DE */	0x00,	// RSRVD[7:0]
/* 32 E0 */	0x00,	// RSRVD[7:0]
/* 33 E2 */	0x00,	// RSRVD[7:0]
/* 34 E4 */	0x00,	// RSRVD[7:0]
/* 35 E6 */	0x00,	// RSRVD[7:0]
/* 36 E8 */	0x00,	// RSRVD[7:0]
/* 37 EA */	0x00,	// RSRVD[7:0]
/* 38 EC */	0x00,	// RSRVD[7:0]
/* 39 EE */	0x00,	// RSRVD[7:0]
/* 40 F0 */	0x00,	// RSRVD[7:0]
/* 41 F2 */	0x00,	// RSRVD[7:0]
/* 42 F4 */	0x00,	// RSRVD[7:0]
/* 43 F6 */	0x00,	// RSRVD[7:0]
/* 44 F8 */	0x00,	// RSRVD[7:0]
/* 45 FA */	0x00,	// RSRVD[7:0]
/* 46 FC */	0x00,	// RSRVD[7:0]
/* 47 FE */	0x00,	// RSRVD[7:0]
};

// starts at register A0 by twos
static unsigned char NTSC60_CONFIG_BT865[ 48 ] = {
	0x00,	// EWSF2 EWSF1 RSRVD[1:0] WSDAT[4:1]
	0x00,	// WSDAT[12:5]
	0x00,	// WSDAT[20:13]
	0x00,	// SRESET RSRVD[6:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// TXHS[7:0]
	0x00,	// TXHE[7:0]
	0x00,	// LUMADLY[1:0] TXHE[10:8] TXHS[10:8]
	0x00,	// RSRVD[1:0] TXRM TXE TXEF2[8] TXBF2[8] TXEF1[8] TXBF1[8]
	0x00,	// TXBF1[7:0]
	0x00,	// TXEF1[7:0]
	0x00,	// TXBF2[7:0]
	0x00,	// TXEF2[7:0]
	0x00,	// ECCF2 ECCF1 ECCGATE RSRVD DACOFF YC16 CBSWAP PORCH
	0x00,	// CCF2B1[7:0]
	0x00,	// CCF2B2[7:0]
	0x00,	// CCF1B1[7:0]
	0x00,	// CCF1B2[7:0]
	0x00,	// HSYNCF[7:0]
	0x00,	// HSYNCR[7:0]
	0x00,	// SYNCDLY FIELD1 SYNCDIS ADJHSYNC HSYNCF[9:8] HSYNCR[9:8]
	0x88,	// SETMODE SETUPDIS VIDFORM[3:0] NONINTL SQUARE
	0x0a,	// ESTATUS RGBO DCHROMA ECBAR SCRESET EVBI EACTIVE ECLIP
	0x00,	// RSRVD[6:0] PALN
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
};

// starts at register A0 by twos
static unsigned char PALM_CONFIG_BT865[ 48 ] = {
	0x00,	// EWSF2 EWSF1 RSRVD[1:0] WSDAT[4:1]
	0x00,	// WSDAT[12:5]
	0x00,	// WSDAT[20:13]
	0x00,	// SRESET RSRVD[6:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// TXHS[7:0]
	0x00,	// TXHE[7:0]
	0x00,	// LUMADLY[1:0] TXHE[10:8] TXHS[10:8]
	0x00,	// RSRVD[1:0] TXRM TXE TXEF2[8] TXBF2[8] TXEF1[8] TXBF1[8]
	0x00,	// TXBF1[7:0]
	0x00,	// TXEF1[7:0]
	0x00,	// TXBF2[7:0]
	0x00,	// TXEF2[7:0]
	0x00,	// ECCF2 ECCF1 ECCGATE RSRVD DACOFF YC16 CBSWAP PORCH
	0x00,	// CCF2B1[7:0]
	0x00,	// CCF2B2[7:0]
	0x00,	// CCF1B1[7:0]
	0x00,	// CCF1B2[7:0]
	0x00,	// HSYNCF[7:0]
	0x00,	// HSYNCR[7:0]
	0x00,	// SYNCDLY FIELD1 SYNCDIS ADJHSYNC HSYNCF[9:8] HSYNCR[9:8]
	0xf0,	// SETMODE SETUPDIS VIDFORM[3:0] NONINTL SQUARE
	0x02,	// ESTATUS RGBO DCHROMA ECBAR SCRESET EVBI EACTIVE ECLIP
	0x00,	// RSRVD[6:0] PALN
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
};

// starts at register A0 by twos
static unsigned char PALM60_CONFIG_BT865[ 48 ] = {
	0x00,	// EWSF2 EWSF1 RSRVD[1:0] WSDAT[4:1]
	0x00,	// WSDAT[12:5]
	0x00,	// WSDAT[20:13]
	0x00,	// SRESET RSRVD[6:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// TXHS[7:0]
	0x00,	// TXHE[7:0]
	0x00,	// LUMADLY[1:0] TXHE[10:8] TXHS[10:8]
	0x00,	// RSRVD[1:0] TXRM TXE TXEF2[8] TXBF2[8] TXEF1[8] TXBF1[8]
	0x00,	// TXBF1[7:0]
	0x00,	// TXEF1[7:0]
	0x00,	// TXBF2[7:0]
	0x00,	// TXEF2[7:0]
	0x00,	// ECCF2 ECCF1 ECCGATE RSRVD DACOFF YC16 CBSWAP PORCH
	0x00,	// CCF2B1[7:0]
	0x00,	// CCF2B2[7:0]
	0x00,	// CCF1B1[7:0]
	0x00,	// CCF1B2[7:0]
	0x00,	// HSYNCF[7:0]
	0x00,	// HSYNCR[7:0]
	0x00,	// SYNCDLY FIELD1 SYNCDIS ADJHSYNC HSYNCF[9:8] HSYNCR[9:8]
	0xf8,	// SETMODE SETUPDIS VIDFORM[3:0] NONINTL SQUARE
	0x02,	// ESTATUS RGBO DCHROMA ECBAR SCRESET EVBI EACTIVE ECLIP
	0x00,	// RSRVD[6:0] PALN
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
};

// starts at register A0 by twos
static unsigned char PAL_CONFIG_BT865[ 48 ] = {
	0x00,	// EWSF2 EWSF1 RSRVD[1:0] WSDAT[4:1]
	0x00,	// WSDAT[12:5]
	0x00,	// WSDAT[20:13]
	0x00,	// SRESET RSRVD[6:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// TXHS[7:0]
	0x00,	// TXHE[7:0]
	0x00,	// LUMADLY[1:0] TXHE[10:8] TXHS[10:8]
	0x00,	// RSRVD[1:0] TXRM TXE TXEF2[8] TXBF2[8] TXEF1[8] TXBF1[8]
	0x00,	// TXBF1[7:0]
	0x00,	// TXEF1[7:0]
	0x00,	// TXBF2[7:0]
	0x00,	// TXEF2[7:0]
	0x00,	// ECCF2 ECCF1 ECCGATE RSRVD DACOFF YC16 CBSWAP PORCH
	0x00,	// CCF2B1[7:0]
	0x00,	// CCF2B2[7:0]
	0x00,	// CCF1B1[7:0]
	0x00,	// CCF1B2[7:0]
	0x00,	// HSYNCF[7:0]
	0x00,	// HSYNCR[7:0]
	0x00,	// SYNCDLY FIELD1 SYNCDIS ADJHSYNC HSYNCF[9:8] HSYNCR[9:8]
	0xe4,	// SETMODE SETUPDIS VIDFORM[3:0] NONINTL SQUARE // or 0x24
	0x02,	// ESTATUS RGBO DCHROMA ECBAR SCRESET EVBI EACTIVE ECLIP
	0x00,	// RSRVD[6:0] PALN
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
};

// starts at register A0 by twos
static unsigned char PALNC_CONFIG_BT865[ 48 ] = {
	0x00,	// EWSF2 EWSF1 RSRVD[1:0] WSDAT[4:1]
	0x00,	// WSDAT[12:5]
	0x00,	// WSDAT[20:13]
	0x00,	// SRESET RSRVD[6:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// TXHS[7:0]
	0x00,	// TXHE[7:0]
	0x00,	// LUMADLY[1:0] TXHE[10:8] TXHS[10:8]
	0x00,	// RSRVD[1:0] TXRM TXE TXEF2[8] TXBF2[8] TXEF1[8] TXBF1[8]
	0x00,	// TXBF1[7:0]
	0x00,	// TXEF1[7:0]
	0x00,	// TXBF2[7:0]
	0x00,	// TXEF2[7:0]
	0x00,	// ECCF2 ECCF1 ECCGATE RSRVD DACOFF YC16 CBSWAP PORCH
	0x00,	// CCF2B1[7:0]
	0x00,	// CCF2B2[7:0]
	0x00,	// CCF1B1[7:0]
	0x00,	// CCF1B2[7:0]
	0x00,	// HSYNCF[7:0]
	0x00,	// HSYNCR[7:0]
	0x00,	// SYNCDLY FIELD1 SYNCDIS ADJHSYNC HSYNCF[9:8] HSYNCR[9:8]
	0xf4,	// SETMODE SETUPDIS VIDFORM[3:0] NONINTL SQUARE
	0x02,	// ESTATUS RGBO DCHROMA ECBAR SCRESET EVBI EACTIVE ECLIP
	0x01,	// RSRVD[6:0] PALN
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
	0x00,	// RSRVD[7:0]
};

static int bt865_update( struct i2c_client *client )
{
	struct bt865_data_s *data = i2c_get_clientdata(client);
	char tmpconfig[48];
	int i;

	if (memcpy(tmpconfig, data->config, data->configlen) != tmpconfig) {
		printk(KERN_NOTICE "bt865_update: memcpy error\n");
		return -1;
	}

	if (data->bars) {
		tmpconfig[23] |= 0x10;
	}

	if (data->enableoutput) {
		tmpconfig[23] |= 0x02;
	}

	if (data->rgbmode) {
		tmpconfig[23] |= 0x40;
	}

	for (i = 0; i < data->configlen; i++) {
		i2c_smbus_write_byte_data(client, 2 * i + 0xA0, tmpconfig[i]);
	}

	return 0;
}

static int bt865_setmode(int mode, struct i2c_client *client)
{
	struct bt865_data_s *data = i2c_get_clientdata(client);
	unsigned char *config = NULL;

	pr_debug("bt865_setmode( %d, %p )\n", mode, client);

	switch (mode) {
	case ENCODER_MODE_NTSC:
		printk(KERN_NOTICE "bt865.o: Configuring for NTSC\n");
		config = NTSC_CONFIG_BT865;
		data->configlen = sizeof(NTSC_CONFIG_BT865);
		break;
	case ENCODER_MODE_NTSC60:
		printk(KERN_NOTICE "bt865.o: Configuring for NTSC\n");
		config = NTSC60_CONFIG_BT865;
		data->configlen = sizeof(NTSC60_CONFIG_BT865);
		break;
	case ENCODER_MODE_PAL_M:
		printk(KERN_NOTICE "bt865.o: Configuring for PAL_M\n");
		config = PALM_CONFIG_BT865;
		data->configlen = sizeof(PALM_CONFIG_BT865);
		break;
	case ENCODER_MODE_PALM60:
		printk(KERN_NOTICE "bt865.o: Configuring for PAL_M60\n");
		config = PALM60_CONFIG_BT865;
		data->configlen = sizeof(PALM60_CONFIG_BT865);
		break;
	case ENCODER_MODE_PAL:
		printk(KERN_NOTICE "bt865.o: Configuring for PAL\n");
		config = PAL_CONFIG_BT865;
		data->configlen = sizeof(PAL_CONFIG_BT865);
		break;
	case ENCODER_MODE_PALNC:
		printk(KERN_NOTICE "bt865.o: Configuring for PAL\n");
		config = PALNC_CONFIG_BT865;
		data->configlen = sizeof(PALNC_CONFIG_BT865);
		break;
	default:
		return -1;
	}

	data->mode = mode;

	if (config) {
		if (memcpy(data->config, config, data->configlen) != data->config) {
			printk(KERN_NOTICE "bt865_setmode: memcpy error\n");
			return -1;
		}
	}

	return 0;
}

static int bt865_setup(struct i2c_client *client)
{
	struct bt865_data_s *data = i2c_get_clientdata(client);
	struct em8300_s *em = i2c_get_adapdata(client->adapter);

	if (memset(data->config, 0, sizeof(data->config)) != data->config) {
		printk(KERN_NOTICE "bt865_setup: memset error\n");
		return -1;
	}

	data->bars = color_bars[em->instance];
	data->rgbmode = output_mode_nr[em->instance] == MODE_RGB;
	data->enableoutput = 0;

	if (EM8300_VIDEOMODE_DEFAULT == EM8300_VIDEOMODE_PAL) {
		printk(KERN_NOTICE "bt865.o: Defaulting to PAL\n");
		bt865_setmode(ENCODER_MODE_PAL, client);
	} else if (EM8300_VIDEOMODE_DEFAULT == EM8300_VIDEOMODE_NTSC) {
		printk(KERN_NOTICE "bt865.o: Defaulting to NTSC\n");
		bt865_setmode(ENCODER_MODE_NTSC, client);
	}

	if (bt865_update(client)) {
		printk(KERN_NOTICE "bt865_setup: bt865_update error\n");
		return -1;
	}

	return 0;
}

static int bt865_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct bt865_data_s *data;
	int err = 0;

/*
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
		return 0;
	}
*/

	if (!(data = kmalloc(sizeof(struct bt865_data_s), GFP_KERNEL)))
		return -ENOMEM;
	memset(data, 0, sizeof(struct bt865_data_s));

	i2c_set_clientdata(client, data);

	strcpy(client->name, "BT865 chip");
	printk(KERN_NOTICE "bt865.o: BT865 chip detected\n");

	if ((err = bt865_setup(client)))
		goto cleanup;

	return 0;

 cleanup:
	kfree(data);
	return err;
}

static int bt865_remove(struct i2c_client *client)
{
	struct bt865_data_s *data = i2c_get_clientdata(client);

	kfree(data);

	return 0;
}

static int bt865_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	struct bt865_data_s *data = i2c_get_clientdata(client);

	switch(cmd) {
	case ENCODER_CMD_SETMODE:
		bt865_setmode((long int) arg, client);
		bt865_update(client);
		break;
	case ENCODER_CMD_ENABLEOUTPUT:
		data->enableoutput = (long int) arg;
		bt865_update(client);
		break;
	default:
		return -EINVAL;
		break;
	}
	return 0;
}

/* ----------------------------------------------------------------------- */


int __init bt865_init(void)
{
	//request_module("i2c-algo-bit");
	return i2c_add_driver(&bt865_driver);
}

void __exit bt865_cleanup(void)
{
	i2c_del_driver(&bt865_driver);
}

module_init(bt865_init);
module_exit(bt865_cleanup);

