/*
 * em8300_video.c
 *
 * Copyright (C) 2000-2001 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2001 Rick Haines <rick@kuroyi.net>
 *           (C) 2001 Ralph Zimmermann <rz@ooe.net>
 *           (C) 2001 Eduard Hasenleithner <eduardh@aon.at>
 *           (C) 2001 Daniel Chassot <Daniel.Chassot@vibro-meter.com>
 *           (C) 2002 Daniel Holm <mswitch@users.sourceforge.net>
 *           (C) 2003 Anders Rune Jensen <anders@gnulinux.dk>
 *           (C) 2003-2005 Jon Burgess <jburgess@uklinux.net>
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
#include <linux/delay.h>

#include "em8300_reg.h"
#include <linux/em8300.h>
#include "em8300_driver.h"
#include "em8300_fifo.h"

#include <linux/soundcard.h>

static const struct v4l2_queryctrl em8300_ctls[] = {
	{
		.id            = V4L2_CID_BRIGHTNESS,
		.name          = "Brightness",
		.minimum       = 0x00,
		.maximum       = 0xff,
		.step          = 1,
		.default_value = 0x7f,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	}, {
		.id            = V4L2_CID_CONTRAST,
		.name          = "Contrast",
		.minimum       = 0,
		.maximum       = 0xff,
		.step          = 1,
		.default_value = 0x7f,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	}, {
		.id            = V4L2_CID_HUE,
		.name          = "Hue",
		.minimum       = 0,
		.maximum       = 0xff,
		.step          = 1,
		.default_value = 0x7f,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	}, {
		.id            = V4L2_CID_SATURATION,
		.name          = "Saturation",
		.minimum       = 0,
		.maximum       = 0xff,
		.step          = 1,
		.default_value = 0x7f,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	}
};

/* Must be sorted from low to high control ID! */
const u32 cx88_user_ctrls[] = {
	V4L2_CID_BRIGHTNESS,
	V4L2_CID_CONTRAST,
	V4L2_CID_SATURATION,
	V4L2_CID_HUE,
	0
};

static const u32 * const ctrl_classes[] = {
	cx88_user_ctrls,
	NULL
};

static int video_open(struct file *file)
{
	return 0;
}

static struct v4l2_file_operations em8300_v4l2_fops = {
	.owner      = THIS_MODULE,
	.open		= video_open,
	.ioctl      = video_ioctl2,
};

static int vidioc_querycap (struct file *file, void  *priv,
					struct v4l2_capability *cap)
{
	struct em8300_s *em = video_drvdata(file);

	strcpy(cap->driver, "em8300");
	strlcpy(cap->card, "BOARD", sizeof(cap->card));
	sprintf(cap->bus_info,"PCI:%s",pci_name(em->pci_dev));
	cap->version = 0;
	cap->capabilities =
			V4L2_CAP_VIDEO_OUTPUT |
			V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_queryctrl (struct file *file, void *priv,
				struct v4l2_queryctrl *qctrl)
{
	qctrl->id = v4l2_ctrl_next(ctrl_classes, qctrl->id);
	if (unlikely(qctrl->id == 0))
		return -EINVAL;

	*qctrl = em8300_ctls[qctrl->id];
	return 0;
}

static const struct v4l2_ioctl_ops video_ioctl_ops = {
	.vidioc_querycap 			= vidioc_querycap,
	.vidioc_queryctrl			= vidioc_queryctrl,
};

static const struct video_device em8300_video_template = {
	.fops						= &em8300_v4l2_fops,
	.release					= video_device_release,
	.ioctl_ops					= &video_ioctl_ops,
	.tvnorms					= V4L2_STD_PAL,
	.current_norm				= V4L2_STD_PAL,
};

int em8300_register_video(struct em8300_s *em)
{
	int retval;

	em->vdev = video_device_alloc();
	if (!em->vdev) {
		/* TODO error handling */
		printk(KERN_ERR "em8300-video: kzalloc failed - out of memory!\n");
		return -ENOMEM;
	}

	*em->vdev = em8300_video_template;
	em->vdev->parent = &em->pci_dev->dev;
	strcpy(em->vdev->name, "em8300 video");

	/* register the v4l2 device */
	video_set_drvdata(em->vdev, em);
	retval = video_register_device(em->vdev, VFL_TYPE_GRABBER, -1);
	if (retval != 0) {
		printk(KERN_ERR "unable to register video device (error = %d).\n",
			retval);
		video_device_release(em->vdev);
		return -ENODEV;
	}

	return 0;
}

static int mpegvideo_command(struct em8300_s *em, int cmd)
{
	if (em8300_waitfor(em, ucregister(MV_Command), 0xffff, 0xffff)) {
		return -1;
	}

	write_ucregister(MV_Command, cmd);

	if ((cmd == MVCOMMAND_DISPLAYBUFINFO) || (cmd == 0x10)) {
		return em8300_waitfor_not(em, ucregister(DICOM_Display_Data), 0, 0xffff);
	} else {
		return em8300_waitfor(em, ucregister(MV_Command), 0xffff, 0xffff);
	}
}

int em8300_video_setplaymode(struct em8300_s *em, int mode)
{
	if (mode == EM8300_PLAYMODE_FRAMEBUF) {
		mpegvideo_command(em, MVCOMMAND_DISPLAYBUFINFO);
		em->video_playmode = mode;
		return 0;
	}

	if (em->video_playmode != mode) {
		switch (mode) {
		case EM8300_PLAYMODE_STOPPED:
			em->video_ptsfifo_ptr = 0;
			em->video_offset = 0;
			mpegvideo_command(em, MVCOMMAND_STOP);
			mpegvideo_command(em, MVCOMMAND_DISPLAYBUFINFO);
			em8300_dicom_fill_dispbuffers(em, 0, 0, em->dbuf_info.xsize, em->dbuf_info.ysize, 0x00000000, 0x80808080 );
			break;
		case EM8300_PLAYMODE_PLAY:
			em->video_pts = 0;
			em->video_lastpts = 0;
			if (em->video_playmode == EM8300_PLAYMODE_STOPPED) {
				write_ucregister(MV_FrameEventLo, 0xffff);
				write_ucregister(MV_FrameEventHi, 0x7fff);
			}
			mpegvideo_command(em, MVCOMMAND_START);
			break;
		case EM8300_PLAYMODE_PAUSED:
			mpegvideo_command(em, MVCOMMAND_PAUSE);
			break;
		default:
			return -1;
		}

		em->video_playmode = mode;

		return 0;
	}

	return -1;
}

int em8300_video_sync(struct em8300_s *em)
{
	int rdptr, wrptr, rdptr_last, synctimeout;

	rdptr_last = 0;
	synctimeout = 0;

	do {
		wrptr = read_ucregister(MV_Wrptr_Lo) |
			(read_ucregister(MV_Wrptr_Hi) << 16);
		rdptr = read_ucregister(MV_RdPtr_Lo) |
			(read_ucregister(MV_RdPtr_Hi) << 16);

		if (rdptr == wrptr) {
			break;
		}

		if (rdptr == rdptr_last) {
			pr_debug("em8300-%d: Video sync rdptr is stuck at 0x%08x, wrptr 0x%08x, left %d\n", em->instance, rdptr, wrptr, wrptr - rdptr);
			break;
		}
		rdptr_last = rdptr;

		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ / 2);

		if (signal_pending(current)) {
			set_current_state(TASK_RUNNING);
			printk(KERN_ERR "em8300-%d: Video sync interrupted\n", em->instance);
			return -EINTR;
		}
	} while (++synctimeout < 4);

	if (rdptr != wrptr) {
		pr_debug("em8300-%d: Video sync timeout\n", em->instance);
	}

	set_current_state(TASK_RUNNING);

	return 0;
}

int em8300_video_flush(struct em8300_s *em)
{
	write_ucregister(MV_Wrptr_Lo, 0);
	write_ucregister(MV_Wrptr_Hi, 0);
	write_ucregister(MV_RdPtr_Lo, 0);
	write_ucregister(MV_RdPtr_Hi, 0);
	writel(readl(em->mvfifo->readptr), em->mvfifo->writeptr);

	em->video_ptsvalid = 0;
	em->video_pts = 0;
	em->video_ptsfifo_ptr = 0;
	em->video_offset = 0;

	write_ucregister(SP_Wrptr_Lo, 0);
	write_ucregister(SP_Wrptr_Hi, 0);
	write_ucregister(SP_RdPtr_Lo, 0);
	write_ucregister(SP_RdPtr_Hi, 0);
	writel(readl(em->spfifo->readptr), em->spfifo->writeptr);

	em->sp_ptsfifo_ptr = 0;
	em->sp_ptsvalid = 0;
	em->sp_pts = 0;

	return 0;
}

void set_dicom_kmin(struct em8300_s *em)
{
	int kmin;

	kmin = (em->overlay_70 + 10) * 150645 / em->mystery_divisor;
	if (kmin > 0x900) {
		kmin = 0x900;
	}
	write_ucregister(DICOM_Kmin, kmin);
	pr_debug("em8300-%d: register DICOM_Kmin = 0x%x\n", em->instance, kmin);
}

int em8300_video_setup(struct em8300_s *em)
{
	write_register(0x1f47, 0x0);

	if (em->encoder_type == ENCODER_BT865) {
		write_register(0x1f5e, 0x9efe);
		write_ucregister(DICOM_Control, 0x9efe);
	} else {
		write_register(0x1f5e, 0x9afe);
		write_ucregister(DICOM_Control, 0x9afe);
	}

	write_register(EM8300_I2C_PIN, 0x3c3c);
	write_register(EM8300_I2C_OE, 0x3c00);
	write_register(EM8300_I2C_OE, 0x3c3c);

	write_register(EM8300_I2C_PIN, 0x808);
	write_register(EM8300_I2C_PIN, 0x1010);

	em9010_init(em);

	em9010_write(em, 7, 0x40);
	em9010_write(em, 9, 0x4);

	em9010_read(em, 0);

	udelay(100);

	write_ucregister(DICOM_UpdateFlag, 0x0);

	write_ucregister(DICOM_VisibleLeft, 0x168);
	write_ucregister(DICOM_VisibleTop, 0x2e);
	write_ucregister(DICOM_VisibleRight, 0x36b);
	write_ucregister(DICOM_VisibleBottom, 0x11e);
	write_ucregister(DICOM_FrameLeft, 0x168);
	write_ucregister(DICOM_FrameTop, 0x2e);
	write_ucregister(DICOM_FrameRight, 0x36b);
	write_ucregister(DICOM_FrameBottom, 0x11e);
	em8300_dicom_enable(em);

	em9010_write16(em, 0x8, 0xff);
	em9010_write16(em, 0x10, 0xff);
	em9010_write16(em, 0x20, 0xff);

	em9010_write(em, 0xa, 0x0);

	if (em9010_cabledetect(em)) {
		pr_debug("em8300-%d: overlay loop-back cable detected\n", em->instance);
	}

	pr_debug("em8300-%d: overlay reg 0x80 = %x \n", em->instance, em9010_read16(em, 0x80));

	em9010_write(em, 0xb, 0xc8);

	pr_debug("em8300-%d: register 0x1f4b = %x (0x138)\n", em->instance, read_register(0x1f4b));

	em9010_write16(em, 1, 0x4fe);
	em9010_write(em, 1, 4);
	em9010_write(em, 5, 0);
	em9010_write(em, 6, 0);
	em9010_write(em, 7, 0x40);
	em9010_write(em, 8, 0x80);
	em9010_write(em, 0xc, 0x8c);
	em9010_write(em, 9, 0);

	set_dicom_kmin(em);

	em9010_write(em, 7, 0x80);
	em9010_write(em, 9, 0);

	if (em->config.model.bt865_ucode_timeout) {
		write_register(0x1f47, 0x18);
	}
	if (em->encoder_type == ENCODER_BT865) {
		write_register(0x1f5e, 0x9efe);
		write_ucregister(DICOM_Control, 0x9efe);
	} else {
		if (!em->config.model.bt865_ucode_timeout) {
			write_register(0x1f47, 0x18);
		}
		write_register(0x1f5e, 0x9afe);
		write_ucregister(DICOM_Control, 0x9afe);
	}

	write_ucregister(DICOM_UpdateFlag, 0x1);

	udelay(100);

	write_ucregister(ForcedLeftParity, 0x2);

	write_ucregister(MV_Threshold, 0x90); // was 0x50 for BT865, but this works too

	write_register(EM8300_INTERRUPT_ACK, 0x2);
	write_ucregister(Q_IrqMask, 0x0);
	write_ucregister(Q_IrqStatus, 0x0);
	write_ucregister(Q_IntCnt, 0x64);

	write_register(0x1ffb, em->var_video_value);

	write_ucregister(MA_Threshold, 0x8);

	/* Release reset */
	write_register(0x2000, 0x1);

	if (mpegvideo_command(em, MVCOMMAND_DISPLAYBUFINFO)) {
		printk(KERN_ERR "em8300-%d: mpegvideo_command(0x11) failed\n", em->instance);
		return -ETIME;
	}
	em8300_dicom_get_dbufinfo(em);

	write_ucregister(SP_Status, 0x0);

	if (mpegvideo_command(em, 0x10)) {
		printk(KERN_ERR "em8300-%d: mpegvideo_command(0x10) failed\n", em->instance);
		return -ETIME;
	}

	em8300_video_setspeed(em, 0x900);

	write_ucregister(MV_FrameEventLo, 0xffff);
	write_ucregister(MV_FrameEventHi, 0x7fff);

	em8300_ioctl_setvideomode(em, EM8300_VIDEOMODE_DEFAULT);
	em8300_ioctl_setaspectratio(em, EM8300_ASPECTRATIO_4_3);

	em8300_dicom_setBCS(em, 500, 500, 500);

	if (em8300_dicom_update(em)) {
		printk(KERN_ERR "em8300-%d: DICOM Update failed\n", em->instance);
		return -ETIME;
	}

	em->video_playmode = -1;
	em8300_video_setplaymode(em, EM8300_PLAYMODE_STOPPED);

	return 0;
}

void em8300_video_setspeed(struct em8300_s *em, int speed)
{
	if (read_ucregister(MicroCodeVersion) >= 0x29) {
		write_ucregister(MV_SCRSpeed, speed);
	} else {
		write_ucregister(MV_SCRSpeed, speed >> 8);
	}
}

void em8300_video_check_ptsfifo(struct em8300_s *em)
{
	int ptsfifoptr;

	ptsfifoptr = ucregister(MV_PTSFifo) + 4 * em->video_ptsfifo_ptr;

	if (!(read_register(ptsfifoptr + 3) & 1)) {
		wake_up_interruptible(&em->video_ptsfifo_wait);
	}
}

ssize_t em8300_video_write(struct em8300_s *em, const char *buf, size_t count, loff_t *ppos)
{
	unsigned flags = 0;
	int written;
	long ret;

	if (em->video_ptsvalid) {
		int ptsfifoptr = 0;
		em->video_pts >>= 1;

		flags = 0x40000000;
		ptsfifoptr = ucregister(MV_PTSFifo) + 4 * em->video_ptsfifo_ptr;

		ret = wait_event_interruptible_timeout(em->video_ptsfifo_wait,
						       (read_register(ptsfifoptr + 3) & 1) == 0, HZ);
		if (ret == 0) {
			printk(KERN_ERR "em8300-%d: Video Fifo timeout\n", em->instance);
			return -EINTR;
		} else if (ret < 0)
			return ret;

#ifdef DEBUG_SYNC
		pr_info("em8300-%d: pts: %u\n", em->instance, em->video_pts >> 1);
#endif

		write_register(ptsfifoptr, em->video_offset >> 16);
		write_register(ptsfifoptr + 1, em->video_offset & 0xffff);
		write_register(ptsfifoptr + 2, em->video_pts >> 16);
		write_register(ptsfifoptr + 3, (em->video_pts & 0xffff) | 1);

		em->video_ptsfifo_ptr++;
		em->video_ptsfifo_ptr %= read_ucregister(MV_PTSSize) / 4;

		em->video_ptsvalid = 0;
	}

	if (em->nonblock[2]) {
		written = em8300_fifo_write(em->mvfifo, count, buf, flags);
	} else {
		written = em8300_fifo_writeblocking(em->mvfifo, count, buf, flags);
	}
	if (written > 0) {
		em->video_offset += written;
	}
	return written;
}

int em8300_video_ioctl(struct em8300_s *em, unsigned int cmd, unsigned long arg)
{
	unsigned scr, val;
	switch (_IOC_NR(cmd)) {
	case _IOC_NR(EM8300_IOCTL_VIDEO_SETPTS):
		if (get_user(em->video_pts, (int *) arg)) {
			return -EFAULT;
		}

		if (em->video_pts == 0) {
			pr_debug("em8300-%d: Video SETPTS = 0x%x\n", em->instance, em->video_pts);
		}

		if (em->video_pts != em->video_lastpts) {
			em->video_ptsvalid = 1;
			em->video_lastpts = em->video_pts;
		}
		break;

	case _IOC_NR(EM8300_IOCTL_VIDEO_SETSCR):
		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			if (get_user(val, (unsigned *) arg))
				return -EFAULT;
			val >>= 1;
			scr = read_ucregister(MV_SCRlo) | (read_ucregister(MV_SCRhi) << 16);
			scr -= val;
			if (scr < 0) scr = -scr;
			if (scr > 9000) {
				pr_info("em8300-%d: setting scr: %i\n", em->instance, val);
				write_ucregister(MV_SCRlo, val & 0xffff);
				write_ucregister(MV_SCRhi, (val >> 16) & 0xffff);
			}

		}
		if (_IOC_DIR(cmd) & _IOC_READ) {
			scr = read_ucregister(MV_SCRlo) | (read_ucregister(MV_SCRhi) << 16);
			if (copy_to_user((void *) arg, &scr, sizeof(unsigned)))
				return -EFAULT;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

void em8300_video_open(struct em8300_s *em)
{
	em->irqmask |= IRQSTATUS_VIDEO_FIFO | IRQSTATUS_VIDEO_VBL;
	write_ucregister(Q_IrqMask, em->irqmask);
}

int em8300_video_release(struct em8300_s *em)
{
	em->video_ptsfifo_ptr = 0;
	em->video_offset = 0;
	em->video_ptsvalid = 0;
	em8300_fifo_sync(em->mvfifo);
	em8300_video_sync(em);

	em->irqmask &= ~(IRQSTATUS_VIDEO_FIFO | IRQSTATUS_VIDEO_VBL);
	write_ucregister(Q_IrqMask, em->irqmask);

	return em8300_video_setplaymode(em, EM8300_PLAYMODE_STOPPED);
}
