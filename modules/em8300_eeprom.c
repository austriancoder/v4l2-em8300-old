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

#include "em8300_eeprom.h"
#include <linux/em8300.h>
#include "em8300_driver.h"
#include <linux/i2c.h>
#include <linux/crypto.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/err.h>

#if !defined(CONFIG_CRYPTO_MD5) && !defined(CONFIG_CRYPTO_MD5_MODULE)
#warning CONFIG_CRYPTO_MD5 is missing.
#warning Full hardware detection (and autoconfiguration) will be impossible.
#endif

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

	if (i2c_transfer(&em->i2c_adap[1], message, 2) == 2)
		return 0;

	return -1;
}

int em8300_eeprom_checksum_init(struct em8300_s *em)
{
#if defined(CONFIG_CRYPTO) || defined(CONFIG_CRYPTO_MODULE)
	u8 *buf;
	int err;

	buf = kmalloc(256, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	err = em8300_eeprom_read(em, buf);
	if (err != 0)
		goto cleanup1;

	em->eeprom_checksum = kmalloc(16, GFP_KERNEL);
	if (em->eeprom_checksum == NULL) {
		err = -ENOMEM;
		goto cleanup1;
	}

	{
		struct crypto_hash *tfm;
		struct hash_desc desc;
		struct scatterlist tmp;

		tfm = crypto_alloc_hash("md5", 0, CRYPTO_ALG_ASYNC);
		if (IS_ERR(tfm)) {
			err = PTR_ERR(tfm);
			goto cleanup2;
		}

		desc.tfm = tfm;
		desc.flags = 0;
		sg_init_one(&tmp, buf, 256);

		err = crypto_hash_digest(&desc, &tmp, 128, em->eeprom_checksum);

		crypto_free_hash(tfm);

		if (err != 0)
			goto cleanup2;
	}

	kfree(buf);

	return 0;

 cleanup2:
	kfree(em->eeprom_checksum);
	em->eeprom_checksum = NULL;

 cleanup1:
	kfree(buf);

	return err;
#else
	return -5;
#endif
}

void em8300_eeprom_checksum_deinit(struct em8300_s *em)
{
	if (em->eeprom_checksum) {
		kfree(em->eeprom_checksum);
		em->eeprom_checksum = NULL;
	}
}
