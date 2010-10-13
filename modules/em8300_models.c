/* em8300_models.c -- identify and configure known models of em8300-based cards
 * Copyright (C) 2007,2008 Nicolas Boullis <nboullis@debian.org>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "em8300_models.h"
#include "em8300_eeprom.h"
#include "em8300_reg.h"
#include <linux/em8300.h>
#include <linux/pci.h>

const struct em8300_model_s known_models[] = {
	{
		.name = "unknown",
		.module = { .name=NULL, .addr=0 }
	},
	{
		.name = "DXR3 with BT865",
		.module = { .name="bt865", .addr=0x45 },
		.em8300_config = {
			.use_bt865 = 1,
			.dicom_other_pal = 1,
			.dicom_fix = 1,
			.dicom_control = 1,
			.bt865_ucode_timeout = 1,
			.activate_loopback = 0,
		},
		.bt865_config = {}
	},
	{
		.name = "DXR3 with ADV7175A",
		.module = { .name="adv717x", .addr=0x6a },
		.em8300_config = {
			.use_bt865 = 0,
			.dicom_other_pal = 0,
			.dicom_fix = 1,
			.dicom_control = 0,
			.bt865_ucode_timeout = 0,
			.activate_loopback = 0,
		},
		.adv717x_config = {
			.pixelport_16bit = 1,
			.pixelport_other_pal = 0,
			.pixeldata_adjust_ntsc = 1,
			.pixeldata_adjust_pal = 1,
		}
	},
	{
		.name = "Hollywood+ type 1",
		.module = { .name="adv717x", .addr=0x6a },
		.em8300_config = {
			.use_bt865 = 0,
			.dicom_other_pal = 0,
			.dicom_fix = 1,
			.dicom_control = 0,
			.bt865_ucode_timeout = 0,
			.activate_loopback = 0,
		},
		.adv717x_config = {
			.pixelport_16bit = 1,
			.pixelport_other_pal = 0,
			.pixeldata_adjust_ntsc = 1,
			.pixeldata_adjust_pal = 1,
		}
	},
	{
		.name = "Hollywood+ type 2",
		.module = { .name="adv717x", .addr=0x6a },
		.em8300_config = {
			.use_bt865 = 0,
			.dicom_other_pal = 0,
			.dicom_fix = 0,
			.dicom_control = 1,
			.bt865_ucode_timeout = 0,
			.activate_loopback = 0,
		},
		.adv717x_config = {
			.pixelport_16bit = 0,
			.pixelport_other_pal = 0,
			.pixeldata_adjust_ntsc = 1,
			.pixeldata_adjust_pal = 1,
		}
	},
};

const unsigned known_models_number = sizeof(known_models) / sizeof(known_models[0]);

int identify_model(struct em8300_s *em)
{
	u8 *buf;
	int ret;
	int i;

	buf = kmalloc(256, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	ret = em8300_eeprom_read(em, buf);
	if (ret < 0)
		goto cleanup;

	for (i = 0x40; i < 0x50; i++)
		if (buf[i] != 0xff)
			break;

	if (i < 0x50) {
		/* The board is a Hollywood+ one */
		switch (read_register(0x1c08)) {
		case 0x01:
			ret = 3; /* type 1 */
			break;
		case 0x09:
			ret = 4; /* type 2 */
			break;
		default:
			ret = 0; /* unknown */
		}
	} else {
		/* The board is a DXR3 one */
		switch (read_register(0x1c08)) {
		case 0x01:
			ret = 2; /* with ADV7175A */
			break;
		case 0x41:
			ret = 1; /* with DXR3 */
			break;
		default:
			ret = 0; /* unknown */
		}
	}

 cleanup:
	kfree(buf);

	return ret;
}
