/*
 * em8300_misc.c
 *
 * Copyright (C) 2000 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2001 Eduard Hasenleithner <eduardh@aon.at>
 *           (C) 2003-2005 Jon Burgess <jburgess@uklinux.net>
 *           (C) 2003-2005 Nicolas Boullis <nboullis@debian.org>
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

#include <linux/pci.h>
#include "em8300_reg.h"
#include <linux/em8300.h>
#include "em8300_driver.h"
#include "em8300_fifo.h"

unsigned default_palette[16] = {
	0xe18080, 0x2b8080, 0x847b9c, 0x51ef5a, 0x7d8080, 0xb48080, 0xa910a5,
	0x6addca, 0xd29210, 0x1c76b8, 0x50505a, 0x30b86d, 0x5d4792,
	0x3dafa5, 0x718947, 0xeb8080
};

int em8300_spu_setpalette(struct em8300_s *em, unsigned *pal)
{
	int i, palette;

	palette = ucregister(SP_Palette);

	for (i = 0; i < 16; i++) {
		write_register(palette + i * 2, pal[i] >> 16);
		write_register(palette + i * 2 + 1, pal[i] & 0xffff);
	}

	return 0;
}

int em8300_spu_button(struct em8300_s *em, em8300_button_t *btn)
{
	write_ucregister(SP_Command, 0x2);

	if (btn == 0) /* btn = 0 means release button */
		return 0;

	write_ucregister(Button_Color, btn->color);
	write_ucregister(Button_Contrast, btn->contrast);
	write_ucregister(Button_Top, btn->top);
	write_ucregister(Button_Bottom, btn->bottom);
	write_ucregister(Button_Left, btn->left);
	write_ucregister(Button_Right, btn->right);

	write_ucregister(DICOM_UpdateFlag, 1);
	write_ucregister(SP_Command, 0x102);

	return 0;
}

void em8300_spu_check_ptsfifo(struct em8300_s *em)
{
	int ptsfifoptr;

		ptsfifoptr = ucregister(SP_PTSFifo) + 2 * em->sp_ptsfifo_ptr;

		if (!(read_register(ptsfifoptr + 1) & 1))
			wake_up_interruptible(&em->sp_ptsfifo_wait);
	}

ssize_t em8300_spu_write(struct em8300_s *em, const char *buf, size_t count, loff_t *ppos)
{
	int flags = 0;
	long ret;

	if (!(em->sp_mode))
		return 0;

//	em->sp_ptsvalid=0;
	if (em->sp_ptsvalid) {
		int ptsfifoptr;

		ptsfifoptr = ucregister(SP_PTSFifo) + 2 * em->sp_ptsfifo_ptr;
		ret = wait_event_interruptible_timeout(em->sp_ptsfifo_wait,
						       (read_register(ptsfifoptr + 1) & 1) == 0, HZ);
		if (ret == 0) {
			printk(KERN_ERR "em8300-%d: SPU Fifo timeout\n", em->instance);
			return -EINTR;
		} else if (ret < 0)
			return ret;

		write_register(ptsfifoptr + 0, em->sp_pts >> 16);
		write_register(ptsfifoptr + 1, (em->sp_pts & 0xffff) | 1);
		em->sp_ptsfifo_ptr++;
		em->sp_ptsfifo_ptr &= read_ucregister(SP_PTSSize) / 2 - 1;

		em->sp_ptsvalid = 0;
	}

	if (em->nonblock[3])
		return em8300_fifo_write(em->spfifo, count, buf, flags);
	else
		return em8300_fifo_writeblocking(em->spfifo, count, buf, flags);
}

int em8300_spu_ioctl(struct em8300_s *em, unsigned int cmd, unsigned long arg)
{
	unsigned clu[16];

	switch (cmd) {
	case EM8300_IOCTL_SPU_SETPTS:
		if (get_user(em->sp_pts, (int *) arg))
			return -EFAULT;

		em->sp_pts >>= 1;
		em->sp_ptsvalid = 1;
		break;
	case EM8300_IOCTL_SPU_SETPALETTE:
		if (copy_from_user(clu, (void *) arg, 16 * 4))
			return -EFAULT;
		em8300_spu_setpalette(em, clu);
		break;
	case EM8300_IOCTL_SPU_BUTTON:
		{
			em8300_button_t btn;
			if (arg == 0) {
				em8300_spu_button(em, 0);
				break;
			}
			if (copy_from_user(&btn, (void *) arg, sizeof(btn)))
				return -EFAULT;
			em8300_spu_button(em, &btn);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int em8300_spu_init(struct em8300_s *em)
{
	return 0;
}

int em8300_spu_open(struct em8300_s *em)
{
	em->sp_ptsfifo_ptr = 0;
	em->sp_ptsvalid = 0;
	em->sp_mode = 1;
	em8300_spu_setpalette(em, default_palette);
	write_ucregister(SP_Command, 0x2);

	return 0;
}

void em8300_spu_release(struct em8300_s *em)
{
	em->sp_pts = 0;
	em->sp_ptsvalid = 0;
	em->sp_count = 0;
	em->sp_ptsfifo_ptr = 0;
	em8300_fifo_sync(em->spfifo);

	return em8300_spu_check_ptsfifo(em);
}
