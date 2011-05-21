/*
 * em8300_misc.c
 *
 * Copyright (C) 2000-2001 Henrik Johansson <lhj@users.sourceforge.net>
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
#include <linux/delay.h>

#include "em8300_reg.h"
#include <linux/em8300.h>
#include "em8300_driver.h"

#include <linux/soundcard.h>

int em8300_waitfor(struct em8300_s *em, int reg, int val, int mask)
{
	int tries;

	for (tries = 0; tries < 100; tries++) {
		if ((readl(&em->mem[reg]) & mask) == val) {
			return 0;
		}
//		current->state = TASK_INTERRUPTIBLE;
//		schedule_timeout(HZ/100);
		mdelay(10);
	}

	return -ETIME;
}

int em8300_waitfor_not(struct em8300_s *em, int reg, int val, int mask)
{
	int tries;

	for (tries = 0; tries < 100; tries++) {
		if ((readl(&em->mem[reg]) & mask) != val) {
			return 0;
		}
//		current->state = TASK_INTERRUPTIBLE;
//		schedule_timeout(HZ/100);
		mdelay(10);
	}

	return -ETIME;
}

int em8300_setregblock(struct em8300_s *em, int offset, int val, int len)
{
	int i;

	for (i = 1000; i; i--) {
		if (!read_register(0x1c1a)) {
			break;
		}
		if (!i) {
			return -ETIME;
		}
	}
#if 0 /* FIXME: was in the zeev01 branch, verify if it is necessary */
	val = val | (val << 8) | (val << 16) | (val << 24);
#endif

	writel(offset & 0xffff, &em->mem[0x1c11]);
	writel((offset >> 16) & 0xffff, &em->mem[0x1c12]);
	writel(len, &em->mem[0x1c13]);
	writel(len, &em->mem[0x1c14]);
	writel(0, &em->mem[0x1c15]);
	writel(1, &em->mem[0x1c16]);
	writel(1, &em->mem[0x1c17]);
	writel(offset & 0xffff, &em->mem[0x1c18]);
	writel(0, &em->mem[0x1c19]);

	writel(1, &em->mem[0x1c1a]);

	for (i = 0; i < len / 4; i++) {
		writel(val, &em->mem[0x11800]);
	}

	switch (len % 4) {
	case 1:
		writel(val, &em->mem[0x10000]);
		break;
	case 2:
		writel(val, &em->mem[0x10800]);
		break;
	case 3:
		writel(val, &em->mem[0x11000]);
		break;
	}

	for (i = 1000; i; i--) {
		if (!read_register(0x1c1a)) {
			break;
		}
		if (!i) {
			return -ETIME;
		}
	}

#if 0 /* FIXME: was in zeev01 branch, verify if it is necessary */
	if (em8300_waitfor(em, 0x1c1a, 0, 1))
		return -ETIME;
#endif

	return 0;
}

int em8300_writeregblock(struct em8300_s *em, int offset, unsigned *buf, int len)
{
	int i;

	writel(offset & 0xffff, &em->mem[0x1c11]);
	writel((offset >> 16) & 0xffff, &em->mem[0x1c12]);
	writel(len, &em->mem[0x1c13]);
	writel(len, &em->mem[0x1c14]);
	writel(0, &em->mem[0x1c15]);
	writel(1, &em->mem[0x1c16]);
	writel(1, &em->mem[0x1c17]);
	writel(offset & 0xffff, &em->mem[0x1c18]);
	writel((offset >> 16) & 0xffff, &em->mem[0x1c19]);

	writel(1, &em->mem[0x1c1a]);

	for (i = 0; i < len / 4; i++) {
		writel(*buf++, &em->mem[0x11800]);
	}

	if (em8300_waitfor(em, 0x1c1a, 0, 1)) {
		return -ETIME;
	}
	return 0;
}
