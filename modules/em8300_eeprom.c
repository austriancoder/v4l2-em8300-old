/* $Id$
 *
 * em8300_eeprom.c -- read the eeprom on em8300-based boards
 * Copyright (C) 2007 Nicolas Boullis <nboullis@debian.org>
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

#include "em8300_driver.h"

int em8300_eeprom_read(struct em8300_s *em, u8 *data)
{
	struct i2c_msg message[] = {
		{
			.addr = 0x50,
			.flags = 0,
			.len = 1,
			.buf = "",
		},
		{
			.addr = 0x50,
			.flags = I2C_M_RD,
			.len = 256,
			.buf = data,
		}
	};

	if (i2c_transfer(&em->i2c_adap[1], message, 2) != 2)
		return -1;

	{
		int i;

		printk(KERN_INFO "full 256-byte eeprom dump:\n");
		for (i = 0; i < 256; i++) {
			if (0 == (i % 16))
				printk(KERN_INFO "%02x:", i);
			printk(KERN_CONT " %02x", data[i]);
			if (15 == (i % 16))
				printk(KERN_CONT "\n");
			}
	 }

	return -1;
}
