/*
 * em8300_dicom.c
 *
 * Copyright (C) 2000-2001 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2000 Ze'ev Maor <zeev@users.sourceforge.net>
 *           (C) 2001 Rick Haines <rick@kuroyi.net>
 *           (C) 2001 Chris C. Hoover <cchoover@charter.net>
 *           (C) 2001 Jonas Birme <birme@users.sourceforge.net>
 *           (C) 2003-2008 Nicolas Boullis <nboullis@debian.org>
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

#include "em8300_params.h"

struct dicom_tvmode {
	int vertsize;
	int horizsize;
	int vertoffset;
	int horizoffset;
};

struct dicom_tvmode tvmodematrix[3] = {
	{576, 720, 46, 130},     // PAL 4:3
	{480, 720, 46, 138},     // PAL60 4:3
	{480, 720, 31, 138},     // NTSC 4:3
};

void em8300_dicom_setBCS(struct em8300_s *em, int brightness, int contrast, int saturation)
{
	int luma_factor, luma_offset, chroma_factor;
	em->dicom_brightness = brightness;
	em->dicom_contrast = contrast;
	em->dicom_saturation = saturation;

	if (read_ucregister(DICOM_UpdateFlag) == 1) {
		write_ucregister(DICOM_UpdateFlag, 0);
		udelay(1);
	}

	luma_factor = (contrast * 127 + 500) / 1000;
	luma_offset = 128 - 2 * luma_factor + ((brightness - 500) * 255 + 500) / 1000;
	if (luma_offset < -128)
		luma_offset = -128;
	if (luma_offset > 127)
		luma_offset = 127;
	chroma_factor = (luma_factor * saturation + 250) / 500;
	if (chroma_factor > 127)
		chroma_factor = 127;

	write_ucregister(DICOM_BCSLuma,
			 ((luma_factor & 255) << 8) | (luma_offset & 255));
	write_ucregister(DICOM_BCSChroma,
			 ((chroma_factor & 255) << 8) | (chroma_factor & 255));

	write_ucregister(DICOM_UpdateFlag, 1);
}

int em8300_dicom_update(struct em8300_s *em)
{
	int ret;
	int vmode_ntsc = 1;
	int f_vs, f_hs, f_vo, f_ho;
	int v_vs, v_hs, v_vo, v_ho;

	if (em->config.model.dicom_other_pal) {
		vmode_ntsc = (em->video_mode == V4L2_STD_NTSC);
	}

	if ((ret = em8300_waitfor(em, ucregister(DICOM_UpdateFlag), 0, 1))) {
		return ret;
	}

	v_vs = f_vs = tvmodematrix[em->video_mode].vertsize;
	v_hs = f_hs = tvmodematrix[em->video_mode].horizsize;
	v_vo = f_vo = tvmodematrix[em->video_mode].vertoffset;
	v_ho = f_ho = tvmodematrix[em->video_mode].horizoffset;

	f_vo += ((100 - em->zoom) * f_vs + 100) / 200;
	f_ho += 2 * (((100 - em->zoom) * f_hs + 200) / 400);
	v_vo += ((100 - em->zoom) * v_vs + 100) / 200;
	v_ho += 2 * (((100 - em->zoom) * v_hs + 200) / 400);
	f_vs = (em->zoom * f_vs + 50) / 100;
	f_hs = (em->zoom * f_hs + 50) / 100;
	v_vs = (em->zoom * v_vs + 50) / 100;
	v_hs = (em->zoom * v_hs + 50) / 100;

	write_ucregister(DICOM_FrameTop, f_vo);
	write_ucregister(DICOM_FrameBottom, f_vo + f_vs - 1);
	write_ucregister(DICOM_FrameLeft, f_ho);
	write_ucregister(DICOM_FrameRight, f_ho + f_hs - 1);
	write_ucregister(DICOM_VisibleTop, v_vo);
	write_ucregister(DICOM_VisibleBottom, v_vo + v_vs - 1);
	write_ucregister(DICOM_VisibleLeft, v_ho);
	write_ucregister(DICOM_VisibleRight,v_ho + v_hs - 1);

	if (em->aspect_ratio == EM8300_ASPECTRATIO_16_9) {
		em->dicom_tvout |= 0x10;
	} else {
		em->dicom_tvout &= ~0x10;
	}

	write_ucregister(DICOM_TvOut, em->dicom_tvout);

	if (em->encoder_type == ENCODER_BT865) {
		write_register(0x1f47, 0x0);
		if (em->video_mode == V4L2_STD_NTSC) {
			write_register(VIDEO_HSYNC_LO, 134);
			write_register(VIDEO_HSYNC_HI, 720);
		} else {
			write_register(VIDEO_HSYNC_LO, 140);
			write_register(VIDEO_HSYNC_HI, 720);
		}
		if (vmode_ntsc) {
			write_register(VIDEO_VSYNC_HI, 260);
			write_register(0x1f5e, 0xfefe);
		} else {
			write_register(VIDEO_VSYNC_HI, 310);
			write_register(0x1f5e, 0x9cfe);
		}

		write_ucregister(DICOM_VSyncLo1, 0x1);
		write_ucregister(DICOM_VSyncLo2, 0x0);
		write_ucregister(DICOM_VSyncDelay1, 0xd2);
		write_ucregister(DICOM_VSyncDelay2, 0x00);

		write_register(0x1f46, 0x00);
		write_register(0x1f47, 0x1f);

		write_ucregister(DICOM_Control, 0x9efe);
	} else { /* ADV7170 or ADV7175A */
		write_register(0x1f47, 0x18);

		if (vmode_ntsc) {
			if (em->config.model.dicom_fix) {
				write_register(0x1f5e, 0x1efe);
			} else {
				write_register(0x1f5e, 0x1afe);
			}

			if (em->config.model.dicom_control) {
				write_ucregister(DICOM_Control, 0x9efe);
			} else {
				write_ucregister(DICOM_Control, 0x9afe);
			}
		} else {
			if (em->config.model.dicom_fix) {
				write_register(0x1f5e, 0x1afe);
			} else {
				write_register(0x1f5e, 0x1efe);
			}

			if (em->config.model.dicom_control) {
				write_ucregister(DICOM_Control, 0x9afe);
			} else {
				write_ucregister(DICOM_Control, 0x9efe);
			}
		}
	}

	pr_debug("em8300-%d: vmode_ntsc: %d\n", em->instance, vmode_ntsc);
	pr_debug("em8300-%d: dicom_other_pal: %d\n", em->instance, em->config.model.dicom_other_pal);
	pr_debug("em8300-%d: dicom_control: %d\n", em->instance, em->config.model.dicom_control);
	pr_debug("em8300-%d: dicom_fix: %d\n", em->instance, em->config.model.dicom_fix);

	write_ucregister(DICOM_UpdateFlag, 1);

	return em8300_waitfor(em, ucregister(DICOM_UpdateFlag), 0, 1);
}

void em8300_dicom_update_aspect_ratio(struct em8300_s *em)
{
	if (em->aspect_ratio == EM8300_ASPECTRATIO_16_9) {
		em->dicom_tvout |= 0x10;
	} else {
		em->dicom_tvout &= ~0x10;
	}
	write_ucregister(DICOM_TvOut, em->dicom_tvout);
}

void em8300_dicom_disable(struct em8300_s *em)
{
	em->dicom_tvout = 0x8000;
	write_ucregister(DICOM_TvOut, em->dicom_tvout);
}

void em8300_dicom_enable(struct em8300_s *em)
{
	em->dicom_tvout = 0x4001;

	if (em->aspect_ratio == EM8300_ASPECTRATIO_16_9) {
		em->dicom_tvout |= 0x10;
	} else {
		em->dicom_tvout &= ~0x10;
	}

	write_ucregister(DICOM_TvOut, em->dicom_tvout);
}

int em8300_dicom_get_dbufinfo(struct em8300_s *em)
{
	int displaybuffer;
	struct displaybuffer_info_s *di = &em->dbuf_info;

	displaybuffer = read_ucregister(DICOM_DisplayBuffer) + 0x1000;

	di->xsize = read_register(displaybuffer);
	di->ysize = read_register(displaybuffer+1);
	di->xsize2 = read_register(displaybuffer+2) & 0xfff;
	di->flag1 = read_register(displaybuffer+2) & 0x8000;
	di->flag2 = read_ucregister(Vsync_DBuf) & 0x4000;

	if (read_ucregister(MicroCodeVersion) <= 0xf) {
		di->buffer1 = (read_register(displaybuffer + 3) | (read_register(displaybuffer + 4) << 16)) << 4;
		di->buffer2 = (read_register(displaybuffer + 5) | (read_register(displaybuffer + 6) << 16)) << 4;
	} else {
		di->buffer1 = read_register(displaybuffer + 3) << 6;
		di->buffer2 = read_register(displaybuffer + 4) << 6;
	}

	if (displaybuffer == ucregister(Width_Buf3)) {
		di->unk_present = 1;
		if(read_ucregister(MicroCodeVersion) <= 0xf) {
			di->unknown1 = read_register(displaybuffer + 7);
			di->unknown2 = (read_register(displaybuffer + 8) | (read_register(displaybuffer + 9) << 16)) << 4;
			di->unknown3 = (read_register(displaybuffer + 0xa) | (read_register(displaybuffer + 0xb) << 16)) << 4;
		} else {
			di->unknown2 = read_register(displaybuffer + 6);
			di->unknown3 = read_register(displaybuffer + 7);
		}
	} else {
		di->unk_present = 0;
	}

	pr_debug("em8300-%d: DICOM buffer: xsize=0x%x(%d)\n", em->instance, di->xsize, di->xsize);
	pr_debug("em8300-%d:               ysize=0x%x(%d)\n", em->instance, di->ysize, di->ysize);
	pr_debug("em8300-%d:               xsize2=0x%x(%d)\n", em->instance, di->xsize2, di->xsize2);
	pr_debug("em8300-%d:               flag1=%d, flag2=%d\n", em->instance, di->flag1, di->flag2);
	pr_debug("em8300-%d:               buffer1=0x%x(%d)\n", em->instance, di->buffer1, di->buffer1);
	pr_debug("em8300-%d:               buffer2=0x%x(%d)\n", em->instance, di->buffer2, di->buffer2);

	if (di->unk_present) {
		pr_debug("em8300-%d:               unknown1=0x%x(%d)\n", em->instance, di->unknown1, di->unknown1);
		pr_debug("em8300-%d:               unknown2=0x%x(%d)\n", em->instance, di->unknown2, di->unknown2);
		pr_debug("em8300-%d:               unknown3=0x%x(%d)\n", em->instance, di->unknown3, di->unknown3);
	}
	return 0;
}

/* sub_42A32
   Arguments
   xoffset = ebp+0x8
   yoffset = ebp+0xc
   c = ebp+0x10
   lines = ebp+0x14
   pat1 = ebp+0x18
   pat2 = ebp+0x1c
 */
void em8300_dicom_fill_dispbuffers(struct em8300_s *em, int xpos, int ypos, int xsize, int ysize, unsigned int pat1, unsigned int pat2)
{
	int i;

	pr_debug("em8300-%d: ysize: %d, xsize: %d\n", em->instance, ysize, xsize);
	pr_debug("em8300-%d: buffer1: %d, buffer2: %d\n", em->instance, em->dbuf_info.buffer1, em->dbuf_info.buffer2);

	for (i = 0; i < ysize; i++) {
		em8300_setregblock(em, em->dbuf_info.buffer1 + xpos + (ypos + i) * em->dbuf_info.xsize, pat1, xsize);
		em8300_setregblock(em, em->dbuf_info.buffer2 + xpos + (ypos + i) / 2 * em->dbuf_info.xsize, pat2, xsize);
	}
}

void em8300_dicom_init(struct em8300_s *em)
{
	em8300_dicom_disable(em);
}
