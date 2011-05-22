/*
 * em8300.h
 *
 * Copyright (C) 2000 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2000 Ze'ev Maor <zeev@users.sourceforge.net>
 *           (C) 2001 Rick Haines <rick@kuroyi.net>
 *           (C) 2001 Edward Salley <drawdeyellas@hotmail.com>
 *           (C) 2001 Jeremy T. Braun <jtbraun@mmit.edu>
 *           (C) 2001 Ralph Zimmermann <rz@ooe.net>
 *           (C) 2001 Daniel Chassot <Daniel.Chassot@vibro-meter.com>
 *           (C) 2002 Michael Hunold <michael@mihu.de>
 *           (C) 2002-2003 David Holm <mswitch@users.sourceforge.net>
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

#ifndef LINUX_EM8300_H
#define LINUX_EM8300_H

typedef struct {
	int reg;
	int val;
	int microcode_register;
} em8300_register_t;

typedef struct {
	int brightness;
	int contrast;
	int saturation;
} em8300_bcs_t;

typedef struct {
	int color;
	int contrast;
	int top;
	int bottom;
	int left;
	int right;
} em8300_button_t;

#define MAX_UCODE_REGISTER 110

#define EM8300_IOCTL_READREG    _IOWR('C',1,em8300_register_t)
#define EM8300_IOCTL_WRITEREG   _IOW('C',2,em8300_register_t)
#define EM8300_IOCTL_SET_ASPECTRATIO _IOW('C',5,int)
#define EM8300_IOCTL_GET_ASPECTRATIO _IOR('C',5,int)
#define EM8300_IOCTL_SET_VIDEOMODE _IOW('C',6,int)
#define EM8300_IOCTL_GET_VIDEOMODE _IOR('C',6,int)
#define EM8300_IOCTL_SET_PLAYMODE _IOW('C',7,int)
#define EM8300_IOCTL_GET_PLAYMODE _IOR('C',7,int)
#define EM8300_IOCTL_SET_SPUMODE _IOW('C',9,int)
#define EM8300_IOCTL_GET_SPUMODE _IOR('C',9,int)
#define EM8300_IOCTL_SCR_GET _IOR('C',16,unsigned)
#define EM8300_IOCTL_SCR_SET _IOW('C',16,unsigned)
#define EM8300_IOCTL_SCR_GETSPEED _IOR('C',17,unsigned)
#define EM8300_IOCTL_SCR_SETSPEED _IOW('C',17,unsigned)
#define EM8300_IOCTL_FLUSH _IOW('C',18,int)
#define EM8300_IOCTL_VBI _IOW('C',19,struct timeval)

#define EM8300_OVERLAY_SIGNAL_ONLY 1
#define EM8300_OVERLAY_SIGNAL_WITH_VGA 2
#define EM8300_OVERLAY_VGA_ONLY 3

#define EM8300_IOCTL_VIDEO_SETPTS _IOW('C',1,int)
#define EM8300_IOCTL_VIDEO_GETSCR _IOR('C',2,unsigned)
#define EM8300_IOCTL_VIDEO_SETSCR _IOW('C',2,unsigned)

#define EM8300_IOCTL_SPU_SETPTS _IOW('C',1,int)
#define EM8300_IOCTL_SPU_SETPALETTE _IOW('C',2,unsigned[16])
#define EM8300_IOCTL_SPU_BUTTON _IOW('C',3,em8300_button_t)

#define EM8300_ASPECTRATIO_4_3 0
#define EM8300_ASPECTRATIO_16_9 1
#define EM8300_ASPECTRATIO_LAST 1

#define EM8300_SPUMODE_OFF 0
#define EM8300_SPUMODE_ON 1

#define EM8300_PLAYMODE_STOPPED         0
#define EM8300_PLAYMODE_PAUSED          1
#define EM8300_PLAYMODE_SLOWFORWARDS    2
#define EM8300_PLAYMODE_SLOWBACKWARDS   3
#define EM8300_PLAYMODE_SINGLESTEP      4
#define EM8300_PLAYMODE_PLAY            5
#define EM8300_PLAYMODE_REVERSEPLAY     6
#define EM8300_PLAYMODE_SCAN            7
#define EM8300_PLAYMODE_FRAMEBUF	8

#define EM8300_SUBDEVICE_VIDEO 1
#define EM8300_SUBDEVICE_SUBPICTURE 3

#ifndef PCI_VENDOR_ID_SIGMADESIGNS
#define PCI_VENDOR_ID_SIGMADESIGNS 0x1105
#define PCI_DEVICE_ID_SIGMADESIGNS_EM8300 0x8300
#endif

#define CLOCKGEN_SAMPFREQ_MASK 0xc0
#define CLOCKGEN_SAMPFREQ_66 0xc0
#define CLOCKGEN_SAMPFREQ_48 0x40
#define CLOCKGEN_SAMPFREQ_44 0x80
#define CLOCKGEN_SAMPFREQ_32 0x00

#define CLOCKGEN_OUTMASK 0x30
#define CLOCKGEN_DIGITALOUT 0x10
#define CLOCKGEN_ANALOGOUT 0x20

#define CLOCKGEN_MODEMASK 0x0f
#define CLOCKGEN_OVERLAYMODE_1 0x07
#define CLOCKGEN_TVMODE_1 0x0b
#define CLOCKGEN_OVERLAYMODE_2 0x04
#define CLOCKGEN_TVMODE_2 0x02

#define MVCOMMAND_STOP 0x0
#define MVCOMMAND_PAUSE 0x1
#define MVCOMMAND_START 0x3
#define MVCOMMAND_PLAYINTRA 0x4
#define MVCOMMAND_SYNC 0x6
#define MVCOMMAND_FLUSHBUF 0x10
#define MVCOMMAND_DISPLAYBUFINFO 0x11

#define MACOMMAND_STOP 0x0
#define MACOMMAND_PAUSE 0x1
#define MACOMMAND_PLAY 0x2

#define IRQSTATUS_VIDEO_VBL 0x10
#define IRQSTATUS_VIDEO_FIFO 0x2
#define IRQSTATUS_AUDIO_FIFO 0x8

#define ENCODER_UNKNOWN 0
#define ENCODER_ADV7175 1 
#define ENCODER_ADV7170 2
#define ENCODER_BT865   3

#endif /* LINUX_EM8300_H */
