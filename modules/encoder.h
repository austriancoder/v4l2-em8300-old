/*
 * encoder.h
 *
 * Copyright (C) 2000 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2001 Chris C. Hoover <cchoover@charter.net>
 *           (C) 2001-2002 Rick Haines <rick@kuroyi.net>
 *           (C) 2002 Daniel Holm <mswitch@users.sourceforge.net>
 *           (C) 2008 Nicolas Boullis <nboullis@debian.org>
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

#ifndef _ENCODER_H_
#define _ENCODER_H_

#include <linux/em8300.h>

#define ENCODER_PARAM_COLORBARS  1
#define ENCODER_PARAM_OUTPUTMODE 2
#define ENCODER_PARAM_PPORT      3 /* ADV717x specific */
#define ENCODER_PARAM_PDADJ      4 /* ADV717x specific */

#define ENCODER_CMD_SETMODE      1
#define ENCODER_CMD_ENABLEOUTPUT 2

struct setparam_s {
	int param;
	uint32_t modes;
	int val;
};
#define ENCODER_CMD_SETPARAM     3

struct getconfig_s {
	unsigned int card_nr;
	int config[6];
};
#define ENCODER_CMD_GETCONFIG    0xDEADBEEF

#endif

