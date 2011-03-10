/*
 * em8300_ucode.c
 *
 * Copyright (C) 2000-2001 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2000 Jeremy T. Braun <jtbraun@mit.edu>
 *           (C) 2003-2007 Nicolas Boullis <nboullis@debian.org>
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

#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/em8300.h>
#include <linux/soundcard.h>

#include "em8300_reg.h"
#include "em8300_driver.h"
#include "em8300_reg.c"
#include "em8300_fifo.h"
#include "em8300_params.h"

static int upload_block(struct em8300_s *em, int blocktype, int offset, int len, unsigned char *buf)
{
	int i, val;

	switch (blocktype) {
	case 4:
		offset *= 2;
		write_register(0x1c11, offset & 0xffff);
		write_register(0x1c12, (offset >> 16) & 0xffff);
		write_register(0x1c13, len);
		write_register(0x1c14, len);
		write_register(0x1c15, 0);
		write_register(0x1c16, 1);
		write_register(0x1c17, 1);
		write_register(0x1c18, offset & 0xffff);
		write_register(0x1c19, (offset >> 16) & 0xffff);

		write_register(0x1c1a, 1);

		for (i = 0; i < len; i += 4) {
			val = (buf[i + 2] << 24) | (buf[i + 3] << 16) | (buf[i] << 8) | buf[i + 1];
			write_register(0x11800, val);
		}

		if (em8300_waitfor(em, 0x1c1a, 0, 1))
			return -ETIME;

		break;
	case 1:
		for (i = 0; i < len; i += 4) {
			val = (buf[i + 1] << 24) | (buf[i] << 16) | (buf[i + 3] << 8) | buf[i + 2];
			write_register(offset / 2 + i / 4, val);
		}
		break;
	case 2:
		for (i = 0; i < len; i += 2) {
			val = (buf[i + 1] << 8) | buf[i];
			write_register(0x1000 + offset + i / 2, val);
		}
		break;
	}

	return 0;
}

static
int upload_prepare(struct em8300_s *em)
{
	write_register(0x30000, 0x1ff00);
	write_register(0x1f50, 0x123);

	write_register(0x20001, 0x0);
	write_register(0x2000, 0x2);
	write_register(0x2000, 0x0);
	write_register(0x1ff8, 0xffff);
	write_register(0x1ff9, 0xffff);
	write_register(0x1ff8, 0xff00);
	write_register(0x1ff9, 0xff00);

	if (em->chip_revision == 1) {
		write_register(0x1c04, 0x8c7);
		write_register(0x1c00, 0x80);
		write_register(0x1c04, 0xc7);
	}
	write_register(0x1c04, em->var_ucode_reg3);
	write_register(0x1c00, em->var_ucode_reg1);
	write_register(0x1c04, em->var_ucode_reg2);

	/* em->mem[0x1c08]; */
	write_register(0x1c10, 0x8);
	write_register(0x1c20, 0x8);
	write_register(0x1c30, 0x8);
	write_register(0x1c40, 0x8);
	write_register(0x1c50, 0x8);
	write_register(0x1c60, 0x8);
	write_register(0x1c70, 0x8);
	write_register(0x1c80, 0x8);
	write_register(0x1c90, 0x10);
	write_register(0x1ca0, 0x10);
	write_register(0x1cb0, 0x8);
	write_register(0x1cc0, 0x8);
	write_register(0x1cd0, 0x8);
	write_register(0x1ce0, 0x8);
	write_register(0x1c01, 0x5555);
	write_register(0x1c02, 0x55a);
	write_register(0x1c03, 0x0);

	return 0;
}

void em8300_ucode_upload(struct em8300_s *em, void *ucode, int ucode_size)
{
	int flags, offset, len;
	unsigned char *p;
	int memcount, i;
	char regname[128];

	upload_prepare(em);

	memcount = 0;

	p = ucode;
	while (memcount < ucode_size) {
		flags =  p[0] | (p[1] << 8); p += 2;
		offset = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); p += 4;
		len = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); p += 4;
		memcount += 10;
		len *= 2;

		if (!flags)
			break;

		switch (flags & 0xf00) {
		case 0:
			upload_block(em, flags, offset, len, p);
			break;
		case 0x200:
			for (i = 0; i < len; i++) {
				if (p[i])
					regname[i] = p[i] ^ 0xff;
				else
					break;
			}
			regname[i] = 0;

			for (i = 0; i < MAX_UCODE_REGISTER; i++) {
				if (!strcmp(ucodereg_names[i], regname)) {
					em->ucode_regs[i] = 0x1000 + offset;
					break;
				}
			}
			break;
		}
		memcount += len;
		p += len;
	}
}

int em8300_require_ucode(struct em8300_s *em)
{
	const struct firmware *fw_entry = NULL;

	if (request_firmware(&fw_entry, "em8300.bin", &em->pci_dev->dev) != 0) {
		dev_err(&em->pci_dev->dev,
			"firmware %s is missing, cannot start.\n",
			"em8300.bin");
		return 0;
	}
	em8300_ucode_upload(em, (void *)fw_entry->data, fw_entry->size);

	em8300_dicom_init(em);

	if (em8300_video_setup(em))
		return 0;

	if (em->mvfifo)
		em8300_fifo_free(em->mvfifo);

	if (em->mafifo)
		em8300_fifo_free(em->mafifo);

	if (em->spfifo)
		em8300_fifo_free(em->spfifo);

	em->mvfifo = em8300_fifo_alloc();
	if (!em->mvfifo)
		return 0;

	em->mafifo = em8300_fifo_alloc();
	if (!em->mafifo)
		return 0;

	em->spfifo = em8300_fifo_alloc();
	if (!em->spfifo)
		return 0;

	em8300_fifo_init(em, em->mvfifo, MV_PCIStart, MV_PCIWrPtr, MV_PCIRdPtr, MV_PCISize, 0x900, FIFOTYPE_VIDEO);
	em8300_fifo_init(em, em->mafifo, MA_PCIStart, MA_PCIWrPtr, MA_PCIRdPtr, MA_PCISize, 0x1000, FIFOTYPE_AUDIO);
	/*	em8300_fifo_init(em,em->spfifo, SP_PCIStart, SP_PCIWrPtr, SP_PCIRdPtr, SP_PCISize, 0x1000, FIFOTYPE_VIDEO); */
	em8300_fifo_init(em,em->spfifo, SP_PCIStart, SP_PCIWrPtr, SP_PCIRdPtr, SP_PCISize, 0x800, FIFOTYPE_VIDEO);
	em8300_spu_init(em);

	if (em8300_audio_setup(em))
		return 0;

	em8300_ioctl_enable_videoout(em, 0);

	printk(KERN_NOTICE "em8300-%d: Microcode version 0x%02x loaded\n", em->instance, read_ucregister(MicroCodeVersion));

	return 1;
}
