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

#define __NO_VERSION__

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
	em8300_overlay_window_t ov_win;
	em8300_overlay_screen_t ov_scr;
	em8300_overlay_calibrate_t ov_cal;
	em8300_attribute_t attr;
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
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

		if (copy_from_user(&reg, (void *) arg, sizeof(em8300_register_t)))
			return -EFAULT;

		if (reg.microcode_register) {
			write_ucregister(reg.reg, reg.val);
		} else {
			write_register(reg.reg, reg.val);
		}
		break;

	case _IOC_NR(EM8300_IOCTL_READREG):
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

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
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

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
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

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
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			get_user(val, (int *) arg);
			return em8300_ioctl_setplaymode(em, val);
		}
		break;

	case _IOC_NR(EM8300_IOCTL_SET_ASPECTRATIO):
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			get_user(val, (int *) arg);
			em8300_ioctl_setaspectratio(em, val);
		}

		if (_IOC_DIR(cmd) & _IOC_READ) {
			if (copy_to_user((void *) arg, &em->aspect_ratio, sizeof(em->aspect_ratio)))
				return -EFAULT;
		}
		break;
	case _IOC_NR(EM8300_IOCTL_GET_AUDIOMODE):
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			get_user(val, (int *) arg);
			em8300_ioctl_setaudiomode(em, val);
		}
		if (_IOC_DIR(cmd) & _IOC_READ) {
			em8300_ioctl_getaudiomode(em, arg);
		}
		break;
	case _IOC_NR(EM8300_IOCTL_SET_SPUMODE):
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			get_user(val, (int *) arg);
			em8300_ioctl_setspumode(em, val);
		}

		if (_IOC_DIR(cmd) & _IOC_READ) {
			if (copy_to_user((void *) arg, &em->sp_mode, sizeof(em->sp_mode)))
				return -EFAULT;
		}
		break;

	case _IOC_NR(EM8300_IOCTL_OVERLAY_SETMODE):
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			get_user(val, (int *) arg);
			if (!em8300_ioctl_overlay_setmode(em, val)) {
				return -EINVAL;
			}
		}
		break;

	case _IOC_NR(EM8300_IOCTL_OVERLAY_SIGNALMODE):
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			get_user(val, (int *) arg);
			if (!em9010_overlay_set_signalmode(em, val)) {
				return -EINVAL;
			}
		}
		break;

	case _IOC_NR(EM8300_IOCTL_OVERLAY_SETWINDOW):
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			if (copy_from_user(&ov_win, (void *) arg, sizeof(em8300_overlay_window_t)))
				return -EFAULT;
			if (!em8300_ioctl_overlay_setwindow(em, &ov_win)) {
				return -EINVAL;
			}
		}
		if (_IOC_DIR(cmd) & _IOC_READ) {
			if (copy_to_user((void *) arg, &ov_win, sizeof(em8300_overlay_window_t)))
				return -EFAULT;
		}
		break;

	case _IOC_NR(EM8300_IOCTL_OVERLAY_SETSCREEN):
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			if (copy_from_user(&ov_scr, (void *) arg, sizeof(em8300_overlay_screen_t)))
				return -EFAULT;
			if (!em8300_ioctl_overlay_setscreen(em, &ov_scr)) {
				return -EINVAL;
			}
		}
		if (_IOC_DIR(cmd) & _IOC_READ) {
			if (copy_to_user((void *) arg, &ov_scr, sizeof(em8300_overlay_screen_t)))
				return -EFAULT;
		}
	break;

	case _IOC_NR(EM8300_IOCTL_OVERLAY_CALIBRATE):
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			if (copy_from_user(&ov_cal, (void *) arg, sizeof(em8300_overlay_calibrate_t)))
				return -EFAULT;
			if(!em8300_ioctl_overlay_calibrate(em, &ov_cal)) {
				return -EIO;
			}
		}

		if (_IOC_DIR(cmd) & _IOC_READ) {
			if (copy_to_user((void *) arg, &ov_cal, sizeof(em8300_overlay_calibrate_t)))
				return -EFAULT;
		}
	break;

	case _IOC_NR(EM8300_IOCTL_OVERLAY_GET_ATTRIBUTE):
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

		if (copy_from_user(&attr, (void *) arg, sizeof(em8300_attribute_t)))
			return -EFAULT;
		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			em9010_set_attribute(em, attr.attribute, attr.value);
		}
		if (_IOC_DIR(cmd) & _IOC_READ) {
			attr.value = em9010_get_attribute(em, attr.attribute);
			if (copy_to_user((void *) arg, &attr, sizeof(em8300_attribute_t)))
				return -EFAULT;
		}
		break;

	case _IOC_NR(EM8300_IOCTL_SCR_GET):
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

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
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

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
		em8300_require_ucode(em);

		if (!em->ucodeloaded) {
			return -ENOTTY;
		}

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
		mpegaudio_command(em, MACOMMAND_PLAY);
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
		mpegaudio_command(em, MACOMMAND_PAUSE);
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

int em8300_ioctl_overlay_setmode(struct em8300_s *em, int val)
{
	switch (val) {
	case EM8300_OVERLAY_MODE_OFF:
		if (em->overlay_enabled) {
			em->clockgen = (em->clockgen & ~CLOCKGEN_MODEMASK) | em->clockgen_tvmode;
			em8300_clockgen_write(em, em->clockgen);
			em->overlay_enabled = 0;
			em->overlay_mode = val;
			em8300_ioctl_setvideomode(em, em->video_mode);
			em9010_overlay_update(em);
			em8300_ioctl_enable_videoout(em, (em->video_playmode == EM8300_PLAYMODE_STOPPED)?0:1);
		}
		break;
	case EM8300_OVERLAY_MODE_RECTANGLE:
	case EM8300_OVERLAY_MODE_OVERLAY:
		if (!em->overlay_enabled) {
			v4l2_subdev_call(em->encoder, core, s_power, 0);

			em->clockgen = (em->clockgen & ~CLOCKGEN_MODEMASK) | em->clockgen_overlaymode;
			em8300_clockgen_write(em, em->clockgen);
			em->overlay_enabled = 1;
			em->overlay_mode = val;
			em8300_dicom_disable(em);
			em8300_dicom_enable(em);
			em8300_dicom_update(em);
			em9010_overlay_update(em);
		} else {
			em->overlay_mode = val;
			em9010_overlay_update(em);
		}
		break;
	default:
		return 0;
	}

	return 1;
}

int em8300_ioctl_overlay_setwindow(struct em8300_s *em, em8300_overlay_window_t *w)
{
	if (w->xpos < -2000 || w->xpos > 2000) {
		return 0;
	}
	if (w->ypos < -2000 || w->ypos > 2000) {
		return 0;
	}
	if (w->width <= 0 || w->width > 2000) {
		return 0;
	}
	if (w->height <= 0 || w->height > 2000) {
		return 0;
	}
	em->overlay_frame_xpos = w->xpos;
	em->overlay_frame_ypos = w->ypos;
	em->overlay_frame_width = w->width;
	em->overlay_frame_height = w->height;

	if (em->overlay_enabled) {
		sub_4288c(em, em->overlay_frame_xpos, em->overlay_frame_ypos, em->overlay_frame_width,
			em->overlay_frame_height, em->overlay_a[EM9010_ATTRIBUTE_XOFFSET],
			em->overlay_a[EM9010_ATTRIBUTE_YOFFSET], em->overlay_a[EM9010_ATTRIBUTE_XCORR], em->overlay_double_y);
	} else {
		em8300_dicom_update(em);
	}

	return 1;
}

int em8300_ioctl_overlay_setscreen(struct em8300_s *em, em8300_overlay_screen_t *s)
{
	if (s->xsize < 0 || s->xsize > 2000) {
		return 0;
	}
	if (s->ysize < 0 || s->ysize > 2000) {
		return 0;
	}

	em9010_overlay_set_res(em, s->xsize, s->ysize);
	return 1;
}
