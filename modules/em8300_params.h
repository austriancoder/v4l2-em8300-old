/* $Id$
 *
 * em8300_params.h -- parameters for the em8300 driver
 * Copyright (C) 2007 Nicolas Boullis <nboullis@debian.org>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _EM8300_PARAMS_H
#define _EM8300_PARAMS_H

#include <linux/version.h>

/* Card-model-dependant parameters */
extern int dicom_other_pal[];
extern int dicom_fix[];
extern int dicom_control[];
extern int bt865_ucode_timeout[];
extern int activate_loopback[];

extern int card_model[];

/* Option to disable the video output when there is nothing to display */
extern int stop_video[];

#endif /* _EM8300_PARAMS_H */
