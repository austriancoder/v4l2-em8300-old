/* $Id$
 *
 * em8300_ioctl32.c -- compatibility layer for 32-bit ioctls on 64-bit kernels
 * Copyright (C) 2004 Nicolas Boullis <nboullis@debian.org>
 * Copyright (C) 2006 István Váradi <ivaradi@users.sourceforge.net>
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/fs.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include "em8300_reg.h"
#include <linux/em8300.h>
#include "em8300_ioctl32.h"

typedef struct {
	u32 ucode;
	s32 ucode_size;
} em8300_microcode32_t;

#define EM8300_IOCTL32_INIT       _IOW('C',0,em8300_microcode32_t)

static int get_em8300_microcode32(em8300_microcode_t *kp, em8300_microcode32_t *up)
{
	u32 tmp;
	if (get_user(kp->ucode_size, &up->ucode_size))
		return -EFAULT;
	kp->ucode = kmalloc(kp->ucode_size, GFP_KERNEL);
	if (!kp->ucode)
		return -ENOMEM;
	__get_user(tmp, &up->ucode);
	if (copy_from_user(kp->ucode, (void *) ((unsigned long)tmp), kp->ucode_size)) {
		kfree(kp->ucode);
		return -EFAULT;
	}
	return 0;
}

static int em8300_do_ioctl32_init(unsigned long arg, struct file* filp)
{
	mm_segment_t old_fs = get_fs();
	em8300_microcode_t karg;
	em8300_microcode32_t *up = (em8300_microcode32_t *)arg;
	int err = 0;

	err = get_em8300_microcode32(&karg, up);

	if (!err) {
		set_fs(KERNEL_DS);
		err = filp->f_op->ioctl(filp->f_dentry->d_inode, filp,
					EM8300_IOCTL_INIT, (unsigned long)&karg);
		set_fs(old_fs);

		kfree(karg.ucode);
	}

	return err;
}

#ifdef HAVE_COMPAT_IOCTL

long em8300_compat_ioctl(struct file* filp, unsigned cmd, unsigned long arg)
{
	switch(cmd) {
	case EM8300_IOCTL32_INIT:
		return em8300_do_ioctl32_init(arg, filp);
	default:
		return filp->f_op->ioctl(filp->f_dentry->d_inode, filp, cmd, arg);
	}
}

#else /* HAVE_COMPAT_IOCTL */

int register_ioctl32_conversion(unsigned int cmd, int (*handler)(unsigned int, unsigned int, unsigned long, struct file *));
int unregister_ioctl32_conversion(unsigned int cmd);

static int do_em8300_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg, struct file *filp)
{
	if (cmd==EM8300_IOCTL32_INIT) {
		return em8300_do_ioctl32_init(arg, filp);
	} else {
		return filp->f_op->ioctl(filp->f_dentry->d_inode, filp, cmd, arg);
	}
}

void em8300_ioctl32_init(void) {
	register_ioctl32_conversion(EM8300_IOCTL32_INIT, do_em8300_ioctl);
	register_ioctl32_conversion(EM8300_IOCTL_READREG, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_WRITEREG, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_GETSTATUS, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_SETBCS, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_GETBCS, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_SET_ASPECTRATIO, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_GET_ASPECTRATIO, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_SET_VIDEOMODE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_GET_VIDEOMODE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_SET_PLAYMODE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_GET_PLAYMODE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_SET_AUDIOMODE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_GET_AUDIOMODE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_SET_SPUMODE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_GET_SPUMODE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_OVERLAY_CALIBRATE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_OVERLAY_SETMODE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_OVERLAY_SETWINDOW, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_OVERLAY_SETSCREEN, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_OVERLAY_GET_ATTRIBUTE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_OVERLAY_SET_ATTRIBUTE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_OVERLAY_SIGNALMODE, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_SCR_GET, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_SCR_SET, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_SCR_GETSPEED, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_SCR_SETSPEED, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_FLUSH, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_VBI, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_VIDEO_GETSCR, NULL);
	register_ioctl32_conversion(EM8300_IOCTL_VIDEO_SETSCR, NULL);
}

void em8300_ioctl32_exit(void) {
	unregister_ioctl32_conversion(EM8300_IOCTL32_INIT);
	unregister_ioctl32_conversion(EM8300_IOCTL_READREG);
	unregister_ioctl32_conversion(EM8300_IOCTL_WRITEREG);
	unregister_ioctl32_conversion(EM8300_IOCTL_GETSTATUS);
	unregister_ioctl32_conversion(EM8300_IOCTL_SETBCS);
	unregister_ioctl32_conversion(EM8300_IOCTL_GETBCS);
	unregister_ioctl32_conversion(EM8300_IOCTL_SET_ASPECTRATIO);
	unregister_ioctl32_conversion(EM8300_IOCTL_GET_ASPECTRATIO);
	unregister_ioctl32_conversion(EM8300_IOCTL_SET_VIDEOMODE);
	unregister_ioctl32_conversion(EM8300_IOCTL_GET_VIDEOMODE);
	unregister_ioctl32_conversion(EM8300_IOCTL_SET_PLAYMODE);
	unregister_ioctl32_conversion(EM8300_IOCTL_GET_PLAYMODE);
	unregister_ioctl32_conversion(EM8300_IOCTL_SET_AUDIOMODE);
	unregister_ioctl32_conversion(EM8300_IOCTL_GET_AUDIOMODE);
	unregister_ioctl32_conversion(EM8300_IOCTL_SET_SPUMODE);
	unregister_ioctl32_conversion(EM8300_IOCTL_GET_SPUMODE);
	unregister_ioctl32_conversion(EM8300_IOCTL_OVERLAY_CALIBRATE);
	unregister_ioctl32_conversion(EM8300_IOCTL_OVERLAY_SETMODE);
	unregister_ioctl32_conversion(EM8300_IOCTL_OVERLAY_SETWINDOW);
	unregister_ioctl32_conversion(EM8300_IOCTL_OVERLAY_SETSCREEN);
	unregister_ioctl32_conversion(EM8300_IOCTL_OVERLAY_GET_ATTRIBUTE);
	unregister_ioctl32_conversion(EM8300_IOCTL_OVERLAY_SET_ATTRIBUTE);
	unregister_ioctl32_conversion(EM8300_IOCTL_OVERLAY_SIGNALMODE);
	unregister_ioctl32_conversion(EM8300_IOCTL_SCR_GET);
	unregister_ioctl32_conversion(EM8300_IOCTL_SCR_SET);
	unregister_ioctl32_conversion(EM8300_IOCTL_SCR_GETSPEED);
	unregister_ioctl32_conversion(EM8300_IOCTL_SCR_SETSPEED);
	unregister_ioctl32_conversion(EM8300_IOCTL_FLUSH);
	unregister_ioctl32_conversion(EM8300_IOCTL_VBI);
	unregister_ioctl32_conversion(EM8300_IOCTL_VIDEO_GETSCR);
	unregister_ioctl32_conversion(EM8300_IOCTL_VIDEO_SETSCR);
}

#endif /* HAVE_COMPAT_IOCTL */
