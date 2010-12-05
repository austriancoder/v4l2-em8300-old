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

// Register settings come from Rockwell Semiconductor
// Advance Information sheet l865a.pdf

// Bits from the Left for Register A0
// 1. one bit EWSF2 (Enable Wide Screen for Field 2)
//    enable/disable Wide Screen Signaling/Copy Generation Management
//    System encoding for field 2 (16:9)
//    if 0 then Disable WSS/CGMS for Field 2
//    if 1 then Enable WSS/CGMS for Field 2
// 2. one bit EWSF1 (Enable Wide Screen for Field 1)
//    enable/disable Wide Screen Signaling/Copy Generation Management
//    System encoding for field 1 (16:9)
//    if 0 then Disable WSS/CGMS for Field 1
//    if 1 then Enable WSS/CGMS for Field 1
// 3. two reserved bits, zero for normal operation
//    this should have been done by the reset above
// 4. four bits of WSDAT[1:4] (Wide Screen Data)

// Bits from the Left for Register A2
// 1. eight bits of WSDAT[5:12] (Wide Screen Data)

// Bits from the Left for Register A4
// 1. eight bits of WSDAT[13:20] (Wide Screen Data)
//    there is a typo in the document here. it Should Say A4

// Bits from the Left for Register A6
// 1. one bit SRESET (System Reset)
//    if 0 then Do Nothing
//    if 1 then Reset All Registers (including this one) To Zero
// 2. seven reserved bits, zero for normal operation
//    this should be done by the reset anyway

// Bits from the Left for Register A8
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register AA
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register AC
// 1. eight bits TXHS[7:0] (Rising Edge Position of TeleText Request Pin)
//    first eight bits of eleven

// Bits from the Left for Register AE
// 1. eight bits TXHE[7:0] (Falling Edge Position of TeleText Request Pin)
//    first eight bits of eleven

// Bits from the Left for Register B0
// 1. two bits LUMADLY[1:0] (Luminance Delay Mode, B Output)
//    D7   D6             Function
//    ----------------------------
//    0    0              No Delay
//    0    1              1 Pixel Clock Delay
//    1    0              2 Pixel Clock Delay
//    1    1              3 Pixel Clock Delay
//    used to program the Luminance Delay on the CVBS/B Output
// 2. three bits TXHE[10:8] (Last Three Bits Teletext Falling Edge)
//    (Falling Edge Position of TeleText Request Pin)
// 3. three bits TXHS[10:8] (Last Three Bits Teletext Rising Edge)
//    (Rising Edge Position of TeleText Request Pin)
// (It's a programmable pulse generator)

// Bits from the Left for Register B2
// 1. two reserved bits, zero for normal operation
//    this should have been done by the reset above
// 2. one bit TXRM (Teletext Request Mode)
//    if 0 then TTXREQ Pin outputs Request
//    if 1 then TTXREQ Pin outputs TTX Clock
// 3. one bit TXE (Teletext Enable)
//    if 0 then Disable Teletext
//    if 1 then Enable Teletext
// 4. one bit TXEF2[8] (Last Line of Teletext Field 2 (last bit))
// 5. one bit TXBF2[8] (First Line of Teletext Field 2 (last bit))
// 6. one bit TXEF1[8] (Last Line of Teletext Field 1 (last bit))
// 7. one bit TXBF1[8] (First Line of Teletext Field 1 (last bit))

// Bits from the Left for Register B4
// 1. eight bits TXBF1[7:0] (First Line of Teletext Field 1
//    (first eight bits))

// Bits from the Left for Register B6
// 1. eight bits TXEF1[7:0] (Last Line of Teletext Field 1
//    (first eight bits))

// Bits from the Left for Register B8
// 1. eight bits TXBF2[7:0] (First Line of Teletext Field 2
//    (first eight bits))

// Bits from the Left for Register BA
// 1. eight bits TXEF2[7:0] (Last Line of Teletext Field 2
//    (first eight bits))

// Bits from the Left for Register BC
// 1. one bit ECCF2 (Enable Closed Caption Encoding on Field 2)
//    if 0 then Disable Closed Caption Encoding on Field 2
//    if 1 then Enable Closed Caption Encoding on Field 2
// 2. one bit ECCF1 (Enable Closed Caption Encoding on Field 1)
//    if 0 then Disable Closed Caption Encoding on Field 1
//    if 1 then Enable Closed Caption Encoding on Field 1
// 3. one bit ECCGATE (Closed Caption Mode)
//    if 0 then Normal Closed Caption Encoding
//    if 1 then Prevent Encoding of Redundant or Incomplete Data
//         Future Encoding is Disabled Until a Complete Pair of
//         New Data Bytes is Received
// 4. one reserved bit, zero for normal operation
//    this should have been done by the reset above
//    this is the bit that the original code mysteriously sets to one
// 5. one bit DACOFF (Turn Off DAC)
//    used to Limit Curent Consumption to Digital Circuits Only
//    if 0 then Normal Operation
//    if 1 then Disable DAC Output Current and Internal Voltage Reference
//    This Bit is Forced High After Powerup Until Either 8 Fields Have
//    Been Output or Register 0xCE Has Been Written
// 6. one bit YC16 (YC Mode)
//    if 0 then 8 Bit Mode: P[7:0] is Multiplexed YCrCb 8 Bit Video Data
//    if 1 then 16 Bit Mode: P[7:0] is Multiplexed CrCb 8 Bit Video Data
//                       and Y[7:0] is Y 8 Bit Data
// 7. one bit CBSWAP (Chroma Red/Blue Swap)
//    if 0 then Normal Pixel Sequence
//    if 1 then Cr and Cb Pixels are Swapped at the Input to the Pixel Port
// 8. one bit PORCH
//    if 0 then Front and Back Porch Timing Meets ITU-RBT.470-3
//         this must be the standard Porch Timing
//    if 1 then CCIR601 Porch Timing. This allows 720 Pixels Width
//         by Narrowing Front and Back Porch in Favor of Active Video

// Bits from the Left for Register BE
// 1. eight bits CCF2B1[7:0] (First Byte of Closed Captioning
//    Information for Field 2)

// Bits from the Left for Register C0
// 1. eight bits CCF2B2[7:0] (Second Byte of Closed Captioning
//    Information for Field 2)

// Bits from the Left for Register C2
// 1. eight bits CCF1B1[7:0] (First Byte of Closed Captioning
//    Information for Field 1)

// Bits from the Left for Register C4
// 1. eight bits CCF1B2[7:0] (Second Byte of Closed Captioning
//    Information for Field 1)

// Bits from the Left for Register C6
// 1. eight bits HSYNCF[7:0] (First Eight Bits Falling Edge Sync Data)
//    SYNC Pulse Position Relative to Internal Horizontal Pixel Clock
//    for Falling Edge of HSYNC
// Needs ADJHSYNC = 1
// Master Mode Only

// Bits from the Left for Register C8
// 1. eight bits HSYNCR[7:0] (First Eight Bits Rising Edge Sync Data)
//    SYNC Pulse Position Relative to Internal Horizontal Pixel Clock
//    for Rising Edge of HSYNC
// Needs ADJHSYNC = 1
// Master Mode Only

// Bits from the Left for Register CA
// 1. one bit SYNCDLY (Sync Delay Mode)
//    if 0 then Normal SYNC Timing
//    if 1 then Delayed SYNC Timing
//    set this to one to see a pretty green screen :)
// 2. one bit FIELD1 (the so-called Color Flag)
//    if 0 then If FIELD Pin = 1 this Indicates Field 2
//    if 1 then If FIELD Pin = 1 this Indicates Field 1
// 3. one bit SYNCDIS (VBI SYNC Mode)
//    if 0 then Normal HSYNC Operation
//    if 1 then Disable HSYNC Durring VBI (No H Serrations In VBI)
//    Master Mode Only
// 4. one bit ADJHSYNC (HSYNC Pulse Timing Mode)
//    if 0 then Normal 4.7usec HSYNC Pulse
//    if 1 then Use HSYNCR[10:0] and HSYNCF[10:0] to Program HSYNC Pulse
//         Rising and Falling Times
//    See HSYNCR[7:0] and HSYNCF[7:0] Above
//    (It's a programmable pulse generator and this is it's Enable Bit)
// 5. two bits HSYNCF[9:8] (Last Two Bits Falling Edge Sync Data)
// 6. two bits HSYNCR[9:8] (Last Two Bits Rising Edge Sync Data)

// Bits from the Left for Register CC
// 1. one bit SETMODE (Automatic Mode Detection)
//    if 0 then Use Automatic Mode Detection
//    if 1 then Override Automatic Mode Detection
//         Use VIDFORM[3:0], NONINTL and SQUARE Registers Bits to Set Mode
//    Slave Mode Only
// 2. one bit SETUPDIS (Disable 7.5 IRE Setup on Output)
//    if 0 then Setup On: 7.5 IRE Setup Enabled for Active Video Lines
//    if 1 then Setup Off: Disable 7.5 IRE Setup on Output Video
// 3. four bits VIDFORM[3:0]
//    These Bits Control the World Television Standard
//    D5  D4  D3  D2  Format         Market
//    -------------------------------------
//    0   0   0   0   NTSC Normal    USA/Japan
//    0   0   1   0   NTSC-60/HDTV   USA-HDTV (SCRESET Must Be 1)
//    1   1   0   0   PAL-M Normal   Brazil
//    1   1   1   0   PAL-M-60/HDTV  Brazil-HDTV
//    1   0   0   1   PAL-BDGHIN     Western Europe
//    1   1   0   1   PAL-NC         Argentina
// 4. one bit NONINTL (Non Interlace Mode)
//    if 0 then Interlaced Operation
//    if 1 then Non Interlaced Operation (Progressive Scan)
// 5. one bit SQUARE (Square Pixel Operation Mode)
//    if 0 then CCIR601 Operation
//    if 1 then Square Pixel Operation

// Bits from the Left for Register CE
// 1. one bit ESTATUS (I2C Readback Information Mode)
//    if 0 then I2C Readback Information Contains Version Number
//    if 1 then I2C Readback Information Contains Closed Caption Status
//         and Field Number
// 2. one bit RGBO (RGB Output Mode)
//    if 0 then Normal Operation
//    if 1 then Enable RGB Outputs
// 3. one bit DCHROMA (Disable Chroma)
//    if 0 then Normal Operation
//    if 1 then Disable Chroma (B/W Operation)
// 4. one bit ECBAR (Enable Color Bars)
//    if 0 then Normal Operation
//    if 1 then Enable Internally Generated Color Bars on Output
// 5. one bit SCRESET (Sub Carrier Reset Mode)
//    if 0 then Normal Operation
//         (SC Phase is Reset at Beginning of Each Field)
//    if 1 then Disable Sub Carrier Reset Event at the
//         Beginning of Each Field Sequence
// 6. one bit EVBI (Enable HSYNC Durring VBI)
//    if 0 then Video is Blanked Durring Vertical Blanking Interval
//    if 1 then Enable Active Video Durring Vertical Blanking Interval
//         Setup is Added if SETUPDIS = 0
//         Scaling of YCrCb Pixels is based on 100% Blank to White
//         i.e. Normal PAL Input Scaling
// 7. one bit EACTIVE (Enable Active Video)
//    if 0 then Output Black With Burst if ECBAR = 0
//         or Color Bars if ECBAR = 1
//    if 1 then Normal Operation (Output Active Video)
// 8. one bit ECLIP (Enable DAC Clipping)
//    if 0 then Normal Operation
//    if 1 then DAC Values Less Than 31 Are Set To 31 (This Limit
//         Corresponds Roughly to 1/4 of Sync Height)

// Bits from the Left for Register D0
// 1. seven reserved bits, zero for normal operation
//    this should have been done by the reset above
// 2. one bit PALN (Enable PAL-N Mode)
//    if 0 then Normal PAL-BDGHI Operation As Set By VIDFORM[3:0]
//    if 1 then PAL-N Operation As Set By VIDFORM[3:0]

// Bits from the Left for Register D2
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register D4
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register D6
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register D8
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register DA
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register DC
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register DE
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register E0
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register E2
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register E4
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register E6
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register E8
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register EA
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register EC
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register EE
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register F0
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register F2
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register F4
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register F6
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register F8
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register FA
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register FC
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

// Bits from the Left for Register FE
// 1. eight reserved bits, zero for normal operation
//    this should have been done by the reset above

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

