/*
 * em9010.c -- EM9010 overlay unit support
 *
 * Copyright (C) 2001 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2001 Rick Haines <rick@kuroyi.net>
 *           (C) 2001 Daniel Chassot <Daniel.Chassot@vibro-meter.com>
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

/* sub_2ac2d */
static int testcable(struct em8300_s *em)
{
	int attempts;

	attempts = 0;
	while (em9010_read(em, 0x0) & 0x20) {
		if(attempts++ > 100) {
			return 0;
		}
		mdelay(1);
	}

	attempts = 0;
	while (!(em9010_read(em, 0x0) & 0x20)) {
		if(attempts++ > 100) {
			return 0;
		}
		mdelay(1);
	}

	return 1;
}

/* loc_2abac */
int em9010_cabledetect(struct em8300_s *em)
{
	em9010_write(em, 0xb, 0xc8);
	em9010_write(em, 0x9, 0x4);
	em9010_write(em, 0xd, 0x44);

	if (testcable(em)) {
		return 1;
	}

	em9010_write(em, 0xd, 0x4c);

	if (testcable(em)) {
		return 1;
	}

	em9010_write(em, 0x9, 0x0);

	return 0;
}

/*
  sub_2AC2D

  locals:
  a = [ebp-8]
*/

/* computes `a - b'  and write the result in `result', assumes `a >= b' */
static inline void my_timeval_less(struct timeval a, struct timeval b, struct timeval * result)
{
		if (a.tv_usec < b.tv_usec) {
			a.tv_sec--;
			a.tv_usec += 1000000;
		}

		result->tv_sec = a.tv_sec - b.tv_sec;
		result->tv_usec = a.tv_usec - b.tv_usec;
}

static
int sub_2AC2D(struct em8300_s *em)
{
	int a;
	struct timeval t, t2, tr;

	do_gettimeofday(&t);
	a = 1000;
	while (em9010_read(em, 0x0) & 0x20) {
		if (!a--) {
			do_gettimeofday(&t2);
			my_timeval_less(t2, t, &tr);
			if (tr.tv_usec >= 50 * 1000)
			return(0);
			a = 1000;
		}
	}

	do_gettimeofday(&t);
	a = 1000;
	while (!(em9010_read(em, 0x0) & 0x20)) {
		if (!a--) {
			do_gettimeofday(&t2);
			my_timeval_less(t2, t, &tr);
			if(tr.tv_usec >= 50*1000)
			return(0);
			a = 1000;
		}
	}

	return(1);
}


/* sub_4288C

   Arguments
   pa = [ebp+0x8]
   pb = [ebp+0xc]
   pc = [ebp+0x10]
   pd = [ebp+0x14]
   pe = [ebp+0x18]
   pf = [ebp+0x1c]
   pg = [ebp+0x20]
   ph = [ebp+0x24]
 */
void sub_4288c(struct em8300_s *em, int pa, int pb, int pc, int pd, int pe, int pf, int pg, int ph)
{
  int pav, pbv, pcv, pdv, i;
	/*	pr_debug("sub_4288c:  xpos=%d, ypos=%d, xwin=%d, ywin=%d, xoff=%d, yoff=%d, xcorr=%d, xd=%d\n",
	  pa,pb,pc,pd,pe,pf,pg,ph); */
	if (pg >= 800) {
		if (ph) {
			pb >>= 1;
			pd >>= 1;
			pf >>= 1;
		}
		if (pa < 0) {
			pav = 0;
			pcv = pc + pa;
		}
		else {
			pav = pa;
			pcv = pc;
		}
		if (pb < 0) {
			pbv = 0;
			pdv = pd + pb;
		}
		else {
			pbv = pb;
			pdv = pd;
		}
		if (pav + pcv>em->overlay_xres) {
			pcv=em->overlay_xres - pav;
		}
		if (pb+pd > em->overlay_yres) {
		  pdv=em->overlay_yres - pb;
		}

		pa = (pa * 1000) / pg;
		pc = (pc * 1000) / pg;
		pav = (pav * 1000) / pg;
		pcv = (pcv * 1000) / pg;

		if (read_ucregister(DICOM_UpdateFlag) == 1) {
			i=0;
			while ((read_ucregister(DICOM_UpdateFlag) == 1) & (i < 20)) {
				udelay(50);
				i++;
			}
			if (read_ucregister(DICOM_UpdateFlag) == 1) {
				write_ucregister(DICOM_UpdateFlag, 0);
				udelay(50);
			}
		}

		write_ucregister(DICOM_VisibleLeft, pe + pav);
		write_ucregister(DICOM_VisibleRight, pe + pav + pcv - 1);
		write_ucregister(DICOM_VisibleTop, pf + pbv);
		write_ucregister(DICOM_VisibleBottom, pf + pbv + pdv - 1);

		write_ucregister(DICOM_FrameLeft, pe + pa);
		write_ucregister(DICOM_FrameRight, pe + pa + pc - 1);
		write_ucregister(DICOM_FrameTop, pf + pb);
		write_ucregister(DICOM_FrameBottom, pf + pb + pd - 1);

		write_ucregister(DICOM_UpdateFlag, 1);
	}
}

static int loc_2BE50(struct em8300_s *em)
{
	em9010_write(em, 0xb, 0xc8);
	sub_2AC2D(em);
	sub_2AC2D(em);
	if (read_register(0x1f4b) < (2 * em->overlay_yres / 3)) {
		return 1;
	} else {
		return 0;
	}
}

int em9010_calibrate_yoffset(struct em8300_s *em)
{
	int i;

	pr_debug("em9010: Starting yoffset calibration\n");

	//clear the stability value
	em9010_write(em, 0x6, 0x0);

	em->overlay_a[EM9010_ATTRIBUTE_XCORR] = em->overlay_xcorr_default;
	em->overlay_a[EM9010_ATTRIBUTE_XOFFSET] = 100000 / em->overlay_a[EM9010_ATTRIBUTE_XCORR];
	em->overlay_a[EM9010_ATTRIBUTE_YOFFSET] = 4;

	em9010_write(em, 0xb, 0x14);

	em9010_write16(em, 0x8, 0x2000);
	em9010_write16(em, 0x10, 0x2000);
	em9010_write16(em, 0x20, 0xff20);

	em9010_write(em, 0xa, 0x6);

	em8300_video_setplaymode(em, EM8300_PLAYMODE_FRAMEBUF );

	if (em->overlay_double_y) {
		em8300_dicom_fill_dispbuffers(em, 0, 0, em->dbuf_info.xsize, 4, 0xffffffff, 0x80808080 );
	} else {
		em8300_dicom_fill_dispbuffers(em, 0, 0, em->dbuf_info.xsize, 2, 0xffffffff, 0x80808080 );
	}

	sub_4288c(em, 0, 0, em->dbuf_info.xsize, em->dbuf_info.ysize, em->overlay_a[EM9010_ATTRIBUTE_XOFFSET],
			em->overlay_a[EM9010_ATTRIBUTE_YOFFSET], em->overlay_a[EM9010_ATTRIBUTE_XCORR], em->overlay_double_y);

	pr_debug("em9010: Done drawing y testpattern\n");

	mdelay(20);

	if (!sub_2AC2D(em)) {
		return 0;
	}

	em9010_write(em, 0, 0x14);
	em9010_write(em, 0, 0x10);

	for (i = 0; i < 60; i++) {
		if (!sub_2AC2D(em)) {
			pr_debug("em9010: sub_2AC2D failed\n");
			return 0;
		}
		if (!(em9010_read(em,0) & 4)) {
			sub_4288c(em, 0, i, em->dbuf_info.xsize, em->dbuf_info.ysize, em->overlay_a[EM9010_ATTRIBUTE_XOFFSET],
					em->overlay_a[EM9010_ATTRIBUTE_YOFFSET], em->overlay_a[EM9010_ATTRIBUTE_XCORR], em->overlay_double_y);
			if (!sub_2AC2D(em)) {
			    return 0;
			}
		} else {
			break;
		}
	}

	if (i == 60) {
		return 0;
	}

	em->overlay_a[EM9010_ATTRIBUTE_YOFFSET] = em->overlay_a[EM9010_ATTRIBUTE_YOFFSET] + i - 2;

	if (em->overlay_double_y) {
		em->overlay_a[EM9010_ATTRIBUTE_YOFFSET] >>= 1;
	}

	pr_debug("em9010: Sucessfully calibrated yoffset (%d)\n", em->overlay_a[EM9010_ATTRIBUTE_YOFFSET]);

	return 1;
}

int em9010_calibrate_xoffset(struct em8300_s *em)
{
	int i;

	em9010_write(em, 0xb, 0x14);

	em9010_write16(em, 0x8, 0x2000);
	em9010_write16(em, 0x10, 0x2000);
	em9010_write16(em, 0x20, 0xff20);

	em9010_write(em, 0xa, 0x6);

	em8300_dicom_fill_dispbuffers(em, 0, 0, em->dbuf_info.xsize, 4, 0, 0x80808080 );
	em8300_dicom_fill_dispbuffers(em, 2, 0, 2, em->dbuf_info.ysize, 0xffffffff, 0x80808080 );

	em->overlay_a[EM9010_ATTRIBUTE_XCORR] = 1000;
	em->overlay_a[EM9010_ATTRIBUTE_XOFFSET] = 100000 / em->overlay_a[EM9010_ATTRIBUTE_XCORR];

	sub_4288c(em, 0, 0, em->dbuf_info.xsize, em->dbuf_info.ysize, em->overlay_a[EM9010_ATTRIBUTE_XOFFSET],
			em->overlay_a[EM9010_ATTRIBUTE_YOFFSET], em->overlay_a[EM9010_ATTRIBUTE_XCORR], em->overlay_double_y);

	pr_debug("em9010: Done drawing x testpattern\n");

	mdelay(20);

	if (!sub_2AC2D(em)) {
		return 0;
	}

	em9010_write(em, 0, 0x14);
	em9010_write(em, 0, 0x10);

	for (i = 0; i <  220; i++) {
		if (!sub_2AC2D(em)) {
			pr_debug("em9010: sub_2AC2D failed\n");
			return 0;
		}
		if (!(em9010_read(em,0) & 4)) {
			sub_4288c(em, i, 0, em->dbuf_info.xsize, em->dbuf_info.ysize, em->overlay_a[EM9010_ATTRIBUTE_XOFFSET],
					em->overlay_a[EM9010_ATTRIBUTE_YOFFSET], em->overlay_a[EM9010_ATTRIBUTE_XCORR], em->overlay_double_y);
			if (!sub_2AC2D(em)) {
			    return 0;
			}
		} else {
			break;
		}
	}

	if (i == 220) {
		return 0;
	}

	em->overlay_a[EM9010_ATTRIBUTE_XOFFSET] = em->overlay_a[EM9010_ATTRIBUTE_XOFFSET] + i;

	pr_debug("em9010: Sucessfully calibrated xoffset (%d)\n", em->overlay_a[EM9010_ATTRIBUTE_XOFFSET]);

	return 1;
}

static int color_cal(struct em8300_s *em,int ul, int a, int b, int c,int d, int *res)
{
	int i;

	if (ul == 1) {
		/* Measure upper threshold level */

		em9010_write16(em, a, d << 8);
		em9010_write(em, 0xa, b);

		if (!sub_2AC2D(em)) {
			return 0;
		}

		em9010_write(em, 0, 0x17);
		em9010_write(em, 0, 0x10);

		for (i = d; i > 0; i--) {
			em9010_write16(em, a, i << 8);
			if (em9010_read(em, 0) & (a >> 3)) {
				break;
			} else {
				continue;
			}
		}
	} else if (ul == 2) {
		/* Measure lower threshold level */
		em9010_write16(em, a, 0x0);
		em9010_write(em, 0xa, c);
		if (!sub_2AC2D(em)) {
			return 0;
		}
		em9010_write(em, 0, 0x17);
		em9010_write(em, 0, 0x10);

		for (i = 0; i < d; i++) {
			em9010_write16(em, a, i);
			if (em9010_read(em, 0) & (a >> 3)) {
				break;
			} else {
				continue;
			}
		}
	} else {
		return -1;
	}

	*res = i;

	return 1;
}

int em9010_calibrate_xcorrection(struct em8300_s *em)
{
	int i, j;

	em9010_write(em, 0xb, 0x14);

	em9010_write16(em, 0x8, 0x2000);
	em9010_write16(em, 0x10, 0x2000);
	em9010_write16(em, 0x20, 0xff20);

	em9010_write(em, 0xa, 0x6);

	em8300_dicom_fill_dispbuffers(em, 2, 0, 2, em->dbuf_info.ysize, 0, 0x80808080 );
	em8300_dicom_fill_dispbuffers(em, 356, 0, 2, em->dbuf_info.ysize, 0xffffffff, 0x80808080 );

	em->overlay_a[EM9010_ATTRIBUTE_XCORR] = em->overlay_xcorr_default;

	sub_4288c(em, 0, 0, em->dbuf_info.xsize, em->dbuf_info.ysize, em->overlay_a[EM9010_ATTRIBUTE_XOFFSET],
			em->overlay_a[EM9010_ATTRIBUTE_YOFFSET], em->overlay_a[EM9010_ATTRIBUTE_XCORR], em->overlay_double_y);

	pr_debug("em9010: Done drawing xcorr testpattern\n");

	mdelay(20);

	if (!sub_2AC2D(em)) {
		return 0;
	}

	em9010_write(em, 0, 0x14);
	em9010_write(em, 0, 0x10);

	j = 1;
	if (em->overlay_xcorr_default > 1200) {
		j = 2;
	}

	for (i = -100; i < 150; i++) {
		if (!sub_2AC2D(em)) {
			return 0;
		}
		if (!(em9010_read(em, 0) & 4)) {
			em->overlay_a[EM9010_ATTRIBUTE_XCORR] = i * j + em->overlay_xcorr_default;
			sub_4288c(em, 0, 0, em->dbuf_info.xsize, em->dbuf_info.ysize, em->overlay_a[EM9010_ATTRIBUTE_XOFFSET],
					em->overlay_a[EM9010_ATTRIBUTE_YOFFSET], em->overlay_a[EM9010_ATTRIBUTE_XCORR], em->overlay_double_y);
			if (!sub_2AC2D(em)) {
			    return 0;
			}
		} else {
			break;
		}

	}

	if (em->overlay_xcorr_default > 1500) {
		em->overlay_a[EM9010_ATTRIBUTE_XCORR] += 2;
	} else {
		em->overlay_a[EM9010_ATTRIBUTE_XCORR] -= 2;
	}
	sub_4288c(em, 0, 0, em->dbuf_info.xsize, em->dbuf_info.ysize, em->overlay_a[EM9010_ATTRIBUTE_XOFFSET],
			em->overlay_a[EM9010_ATTRIBUTE_YOFFSET], em->overlay_a[EM9010_ATTRIBUTE_XCORR], em->overlay_double_y);

	pr_debug("em9010: Sucessfully calibrated x correction (%d)\n", em->overlay_a[EM9010_ATTRIBUTE_XCORR]);

	return 1;
}

/*
  l7 = [ebp-0x18]
  l6 = [ebp-0xc]
  l5 = [ebp-0x1d]
  l4 = [ebp-0x14]
  l2 = [ebp-8]
  l3 = [ebp-4]
  l1 = [ebp-0x10]
 */
int loc_2bcfe(struct em8300_s *em)
{
	int l1 = 10,l2,l3,l4,l5,l6,l7;

	em9010_write(em, 4, 0);
	em9010_write(em, 3, 0x80);
	l2 = em9010_read(em, 2);
	l3 = em9010_read(em, 1);

	while (--l1 != 0) {
		l4 = em9010_read(em, 1);
		if ((l4 & 0xff) == (l3 & 0xff)) {
			continue;
		}
		if ((l3 & 0xff) > (l4 & 0xff)) {
			l5 = l3;
		} else {
			l5 = l4;
		}
		l3 = l5;
		l2 = 0;
		break;
	}

	l6 = (l3 << 8) + l2;
	l7 = 165000 / (l6 + 2);

	if ((l7 > em->overlay_dword_24bb8 + 1) || (l7 < em->overlay_dword_24bb8 - 1)) {
		em->overlay_dword_24bb8 = l7;
	} else {
		l7 = em->overlay_dword_24bb8;
	}

	if ((l7 < 100) || (l7 > 1000)) {
		if (em->overlay_yres) {
			l7 = em->overlay_yres * 62 / 100;
		}
	}

	pr_debug("em9010: loc_2bcfe -> %d\n", l7);
	return l7;
}
/*
  loc_2a66e

  l13 [ebp-38]
  l12 [ebp-8]
  l11 [ebp-20]
  l10 [ebp-34]
  l9 [ebp-28]
  l8 [ebp-4]
  l7 [ebp-18]
  l6 [ebp-0c]
  l5 [ebp-30]
  l4 [ebp-10]
  l1 [ebp-14]
  l2 [ebp-1c]
  l3�[ebp-24]
 */
int loc_2A66E(struct em8300_s *em)
{
	int l1, l2, l3, l4, l5, l6 = 0, l7, l8, l9, l10, l11, l12, l13;

	l1 = 70000;
	l2 = 0;
	l3 = 0;

	l7 = em->overlay_70 / 10;
	l4 = 1000;

	l5 = em->overlay_xres;
	if (l5 <= 720) {
		if (l5 == 720) {
			l6 = 0x398;
		} else if (l5 == 640) {
			l6 = 0x370;
		}
	} else if (l5 <= 1024) {
		if (l5 == 1024) {
			l6 = 0x500;
		} else if (l5 == 800) {
			l6 = 0x400;
		}
	} else if (l5 <= 1280) {
		if (l5 == 1280) {
			l6 = 0x60e;
		} else if (l5 == 1152) {
			l6 = 0x5a0;
		}
	} else if (l5 == 1600) {
		l6 = 0x7d0;
	} else if (l5 == 2048) {
		l6 = 0xa00;
	} else {
		l6 = 0x500;
	}

	pr_debug("l6 * l7 * (1 << l2)=%d, l1=%d\n", l6 * l7 * (1 << l2), l1);
	if (l6 * l7 * (1 << l2) >= l1) {
		l4 = l6 * l7 * (1 << l2) * 1000 / l1;
		l8 = l1 / (l7*(1 << l2)) ;
		l9 = l1 / (1 << l2);
	} else {
		l4 = 1000;
		l8 = l6;
		l9 = l8 * l7;
	}
	em->overlay_xcorr_default = l4;
	if ((em->overlay_xcorr_default < 0x352) || (em->overlay_xcorr_default > 0xdac)) {
		em->overlay_xcorr_default = 0x5dc;
	}

	if (l9 * (1 << l2) > 0x7530) {
		l3 = l8 * (1 << l2) - 2;
		if (l3 > 0xfff) {
			l10 = 0xfff;
		} else {
			l10 = l3;
		}
		l3 = l10;
		l11 = ((l3 >> 8) & 0xf) | (l2 << 4);
		l12 = l3;
	} else {
		l2++;
		l3 = l8 * (1 << l2) - 2;
		if (l3 > 0xfff) {
			l13 = 0xfff;
		} else {
			l13 = l3;
		}
		l3 = l13;
		l11 = ((l3 >> 8) & 0xf) | (l2 << 4);
		l12 = l3;
	}
	pr_debug("em9010: Writing %x to 16-bit register 1.\n", l12 | (l11 << 8));
	em9010_write16(em, 1, l12 | (l11 << 8));
	return 1;
}

int em9010_overlay_set_res(struct em8300_s *em, int xres, int yres)
{
	pr_debug("em9010: Setting resolution %d x %d\n", xres, yres);
	em->overlay_xres = xres;
	em->overlay_yres = yres;
	em->overlay_70 = loc_2bcfe(em);
	loc_2A66E(em);
	em->overlay_a[EM9010_ATTRIBUTE_XCORR] = em->overlay_xcorr_default;
	pr_debug("em9010: Xcorrector: %d\n", em->overlay_a[EM9010_ATTRIBUTE_XCORR]);
	em9010_overlay_update(em);
	return 1;
}

static int set_keycolor(struct em8300_s *em, unsigned upper, unsigned lower)
{
	int ru = (upper >> 16) & 0xff;
	int gu = (upper >> 8) & 0xff;
	int bu = (upper) & 0xff;
	int rl = (lower >> 16) & 0xff;
	int gl = (lower >> 8) & 0xff;
	int bl = (lower) & 0xff;

	em9010_write16(em, 0x8, (ru << 8) | (rl & 0xff));
	em9010_write16(em, 0x10, (gu << 8) | (gl & 0xff));
	em9010_write16(em, 0x20, (bu << 8) | (bl & 0xff));

	return 1;
}

int em9010_overlay_set_signalmode(struct em8300_s *em, int val)
{
	switch (val) {
	case EM8300_OVERLAY_SIGNAL_ONLY:
		em9010_write(em, 7, em->overlay_a[EM9010_ATTRIBUTE_JITTER]);
		break;
	case EM8300_OVERLAY_SIGNAL_WITH_VGA:
		em9010_write(em, 7, em->overlay_a[EM9010_ATTRIBUTE_JITTER] | 0x40);
		break;
	case EM8300_OVERLAY_VGA_ONLY:
		em9010_write(em, 7, em->overlay_a[EM9010_ATTRIBUTE_JITTER] | 0x80);
		break;
	default:
		return 0;
	}
	pr_debug("em9010: overlay reg 7 = %x \n", em9010_read(em, 7));

	return 1;
}

int em9010_overlay_update(struct em8300_s *em)
{
	pr_debug("em9010: Update overlay: enabled=%d, gamma_enabled=%d\n", em->overlay_enabled, em->overlay_gamma_enable);

	em9010_write(em, 5, 0);
	em9010_write(em, 4, 0);
	em9010_write(em, 6, em->overlay_a[EM9010_ATTRIBUTE_STABILITY]);

	if (em->overlay_enabled) {
		em->overlay_gamma_enable=4;
		em9010_write(em, 9, em->overlay_gamma_enable);
		em9010_overlay_set_signalmode(em, EM8300_OVERLAY_SIGNAL_WITH_VGA);
	} else {
		em->overlay_gamma_enable=0;
		em9010_write(em, 9, em->overlay_gamma_enable);
		em9010_overlay_set_signalmode(em, EM8300_OVERLAY_VGA_ONLY);
	}

	em9010_write(em, 8, 0x80);

	if (em->overlay_gamma_enable) {
		em9010_write(em, 0xc, 0x8e);
	} else {
		em9010_write(em, 0xc, 0xe);
	}

	//wait on the display of one frame at least
	mdelay(20);

	//the setting of the overlay mode shall be done before calling loc_2BE50!
	em->overlay_double_y = loc_2BE50(em);
	pr_debug("em9010: ydouble: %d\n", em->overlay_double_y);

	switch(em->overlay_mode) {
	case EM8300_OVERLAY_MODE_RECTANGLE:
		em9010_write(em, 0xa, 0x77);
		break;
	case EM8300_OVERLAY_MODE_OVERLAY:
		em9010_write(em, 0xb, 0xc8);
		em9010_write(em, 0xa, 0x0);
		set_keycolor(em, em->overlay_a[EM9010_ATTRIBUTE_KEYCOLOR_UPPER], em->overlay_a[EM9010_ATTRIBUTE_KEYCOLOR_LOWER]);
		break;
	}

	return 0;
}


int em9010_init(struct em8300_s *em)
{
	em->overlay_dword_24bb8 = 2000;
	em9010_overlay_set_res(em, 1280, 1024);
	em->overlay_frame_xpos = 0;
	em->overlay_frame_ypos = 0;
	em->overlay_frame_width = 720;
	em->overlay_frame_height = 480;
	em->overlay_a[EM9010_ATTRIBUTE_YOFFSET] = 43;
	em->overlay_a[EM9010_ATTRIBUTE_XOFFSET] = 225;
	em->overlay_gamma_enable = 4;

	return 1;
}

int em9010_set_attribute(struct em8300_s *em, int attribute, int value)
{
	if (attribute <= EM9010_ATTRIBUTE_MAX) {
		em->overlay_a[attribute] = value;
		switch(attribute) {
		case EM9010_ATTRIBUTE_JITTER:
			em9010_write(em, 7, (em9010_read(em, 7) & 0xf0) | value);
			break;
		case EM9010_ATTRIBUTE_STABILITY:
			em9010_write(em, 6, value);
			break;
		}
		return 0;
	} else {
		return -1;
	}
}

int em9010_get_attribute(struct em8300_s *em, int attribute)
{
	if (attribute <= EM9010_ATTRIBUTE_MAX) {
		return em->overlay_a[attribute];
	} else {
		return -1;
	}
}

int em8300_ioctl_overlay_calibrate(struct em8300_s *em, em8300_overlay_calibrate_t *c)
{
	int r1;
	em9010_write(em, 0xc, 0xe);

	switch (c->cal_mode) {
	case EM8300_OVERLAY_CALMODE_XOFFSET:
		if (em9010_calibrate_xoffset(em)) {
			c->result = em->overlay_a[EM9010_ATTRIBUTE_XOFFSET];
		} else {
			return 0;
		}
		break;
	case EM8300_OVERLAY_CALMODE_YOFFSET:
		if (em9010_calibrate_yoffset(em)) {
			c->result = em->overlay_a[EM9010_ATTRIBUTE_YOFFSET];
		} else {
			return 0;
		}
		break;
	case EM8300_OVERLAY_CALMODE_XCORRECTION:
		if (em9010_calibrate_xcorrection(em)) {
			c->result = em->overlay_a[EM9010_ATTRIBUTE_XCORR];
			em8300_dicom_fill_dispbuffers(em, 0x164, 0, 2, em->dbuf_info.ysize, 0x0, 0x80808080 );
		} else {
			return 0;
		}
		break;
	case EM8300_OVERLAY_CALMODE_COLOR:
		r1=1;

		em9010_write(em, 0xb, 0xc8);

		mdelay(1);

		if (color_cal(em, c->arg2, 8, 0x37, 0x73, c->arg, &r1)) {
			c->result = r1 << 16;
		} else {
			return 0;
		}

		if (color_cal(em, c->arg2, 0x10, 0x57, 0x75, c->arg, &r1)) {
			c->result |= r1 << 8;
		} else {
			return 0;
		}

		if (color_cal(em, c->arg2, 0x20, 0x67, 0x76, c->arg, &r1)) {
			c->result |= r1;
		} else {
			return 0;
		}
		break;
	}

	if (1) {
		em9010_write(em, 0xc, 0xe);
	}
	return 1;
}

