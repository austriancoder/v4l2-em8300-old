/*
 * em8300_ioctl.c
 *
 * Copyright (C) 2000-2001 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2000 Ze'ev Maor <zeev@users.sourceforge.net>
 *           (C) 2001 Rick Haines <rick@kuroyi.net>
 *           (C) 2001 Edward Salley <drawdeyellas@hotmail.com>
 *           (C) 2001 Daniel Chassot <Daniel.Chassot@vibro-meter.com>
 *           (C) 2002 Daniel Holm <mswitch@users.sourceforge.net>
 *           (C) 2003-2005 Jon Burgess <jburgess@uklinux.net>
 *           (C) 2005-2007 Nicolas Boullis <nboullis@debian.org>
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

#include <linux/string.h>
#include <linux/pci.h>

#include "em8300_reg.h"
#include <linux/em8300.h>
#include "em8300_driver.h"
#include "em8300_fifo.h"

#include "em8300_params.h"

int em8300_control_ioctl(struct em8300_s *em, int cmd, unsigned long arg)
{
	em8300_register_t reg;
	int val, len;
	int old_count;
	long ret;

	if (_IOC_DIR(cmd) != 0) {
		len = _IOC_SIZE(cmd);

		if (len < 1 || len > 65536 || arg == 0) {
			return -EFAULT;
		}
		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			if (!access_ok(VERIFY_READ, (void *) arg, len)) {
				return -EFAULT;
			}
		}
		if (_IOC_DIR(cmd) & _IOC_READ) {
			if (!access_ok(VERIFY_WRITE, (void *) arg, len)) {
				return -EFAULT;
			}
		}
	}

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(EM8300_IOCTL_WRITEREG):

		if (copy_from_user(&reg, (void *) arg, sizeof(em8300_register_t)))
			return -EFAULT;

		if (reg.microcode_register) {
			write_ucregister(reg.reg, reg.val);
		} else {
			write_register(reg.reg, reg.val);
		}
		break;

	case _IOC_NR(EM8300_IOCTL_READREG):

		if (copy_from_user(&reg, (void *) arg, sizeof(em8300_register_t)))
			return -EFAULT;

		if (reg.microcode_register) {
			reg.val = read_ucregister(reg.reg);
			reg.reg = ucregister(reg.reg);
		} else {
			reg.val = read_register(reg.reg);
		}
		if (copy_to_user((void *) arg, &reg, sizeof(em8300_register_t)))
			return -EFAULT;
		break;

	case _IOC_NR(EM8300_IOCTL_VBI):

		old_count = em->irqcount;
		em->irqmask |= IRQSTATUS_VIDEO_VBL;
		write_ucregister(Q_IrqMask, em->irqmask);

		ret = wait_event_interruptible_timeout(em->vbi_wait, em->irqcount != old_count, HZ);
		if (ret == 0)
			return -EINTR;
		else if (ret < 0)
			return ret;

		/* copy timestamp and return */
		if (copy_to_user((void *) arg, &em->tv, sizeof(struct timeval)))
			return -EFAULT;
		return 0;

	case _IOC_NR(EM8300_IOCTL_SET_VIDEOMODE):

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			get_user(val, (int *) arg);
			return em8300_ioctl_setvideomode(em, val);
		}

		if (_IOC_DIR(cmd) & _IOC_READ) {
			if (copy_to_user((void *) arg, &em->video_mode, sizeof(em->video_mode)))
				return -EFAULT;
		}
		break;

	case _IOC_NR(EM8300_IOCTL_SET_PLAYMODE):

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			get_user(val, (int *) arg);
			return em8300_ioctl_setplaymode(em, val);
		}
		break;

	case _IOC_NR(EM8300_IOCTL_SET_ASPECTRATIO):

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			get_user(val, (int *) arg);
			em8300_ioctl_setaspectratio(em, val);
		}

		if (_IOC_DIR(cmd) & _IOC_READ) {
			if (copy_to_user((void *) arg, &em->aspect_ratio, sizeof(em->aspect_ratio)))
				return -EFAULT;
		}
		break;
	case _IOC_NR(EM8300_IOCTL_SET_SPUMODE):

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			get_user(val, (int *) arg);
			em8300_ioctl_setspumode(em, val);
		}

		if (_IOC_DIR(cmd) & _IOC_READ) {
			if (copy_to_user((void *) arg, &em->sp_mode, sizeof(em->sp_mode)))
				return -EFAULT;
		}
		break;

	case _IOC_NR(EM8300_IOCTL_SCR_GET):

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			unsigned scr;
			if (get_user(val, (unsigned *) arg))
				return -EFAULT;
			scr = read_ucregister(MV_SCRlo) | (read_ucregister(MV_SCRhi) << 16);

			if (scr > val)
				scr = scr - val;
			else
				scr = val - scr;

			if (scr > 2 * 1800) { /* Tolerance: 2 frames */
				pr_info("em8300-%d: adjusting scr: %i\n", em->instance, val);
				write_ucregister(MV_SCRlo, val & 0xffff);
				write_ucregister(MV_SCRhi, (val >> 16) & 0xffff);
			}
		}
		if (_IOC_DIR(cmd) & _IOC_READ) {
			val = read_ucregister(MV_SCRlo) | (read_ucregister(MV_SCRhi) << 16);
			if (copy_to_user((void *) arg, &val, sizeof(unsigned)))
				return -EFAULT;
		}
	break;

	case _IOC_NR(EM8300_IOCTL_SCR_GETSPEED):

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			get_user(val, (int *) arg);
			val &= 0xFFFF;

			write_ucregister(MV_SCRSpeed,
			 read_ucregister(MicroCodeVersion) >= 0x29 ? val : val >> 8);
		}
		if (_IOC_DIR(cmd) & _IOC_READ) {
			val = read_ucregister(MV_SCRSpeed);
			if (! read_ucregister(MicroCodeVersion) >= 0x29)
				val <<= 8;

			if (copy_to_user((void *) arg, &val, sizeof(unsigned)))
				return -EFAULT;
		}
	break;

	case _IOC_NR(EM8300_IOCTL_FLUSH):

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			if (get_user(val, (unsigned *) arg))
				return -EFAULT;

			switch (val) {
			case EM8300_SUBDEVICE_VIDEO:
				return em8300_video_flush(em);
			case EM8300_SUBDEVICE_SUBPICTURE:
				return -ENOSYS;
			default:
				return -EINVAL;
			}
		}
	break;

	default:
		return -ETIME;
	}

	return 0;
}

int em8300_ioctl_setvideomode(struct em8300_s *em, v4l2_std_id std)
{
	em->video_mode = std;

	em8300_dicom_disable(em);

	v4l2_subdev_call(em->encoder, video, s_std_output, std);

	em8300_dicom_enable(em);
	em8300_dicom_update(em);

	return 0;
}

void em8300_ioctl_enable_videoout(struct em8300_s *em, int mode)
{
	em8300_dicom_disable(em);

	v4l2_subdev_call(em->encoder, core, s_power, stop_video[em->instance]?mode:1);

	em8300_dicom_enable(em);
}


int em8300_ioctl_setaspectratio(struct em8300_s *em, int ratio)
{
	em->aspect_ratio = ratio;
	em8300_dicom_update_aspect_ratio(em);

	return 0;
}

int em8300_ioctl_setplaymode(struct em8300_s *em, int mode)
{
	switch (mode) {
	case EM8300_PLAYMODE_PLAY:
		//mpegaudio_command(em, MACOMMAND_PLAY);
		if (em->playmode == EM8300_PLAYMODE_STOPPED) {
			em8300_ioctl_enable_videoout(em, 1);
		}
		em8300_video_setplaymode(em, mode);
		break;
	case EM8300_PLAYMODE_STOPPED:
		em8300_ioctl_enable_videoout(em, 0);
		em8300_video_setplaymode(em, mode);
		break;
	case EM8300_PLAYMODE_PAUSED:
		//mpegaudio_command(em, MACOMMAND_PAUSE);
		em8300_video_setplaymode(em, mode);
		break;
	default:
		return -1;
	}
	em->playmode = mode;

	return 0;
}

int em8300_ioctl_setspumode(struct em8300_s *em, int mode)
{
	em->sp_mode = mode;
	return 0;
}
