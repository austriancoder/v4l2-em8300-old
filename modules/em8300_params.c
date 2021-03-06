/* $Id$
 *
 * em8300_params.c -- parameters for the em8300 driver
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

#include <linux/moduleparam.h>
#include "em8300_params.h"
#include <linux/em8300.h>
#include "em8300_driver.h"

/*
 * Module params by Jonas Birm�� (birme@jpl.nu)
 */
int dicom_other_pal[EM8300_MAX] = { [0 ... EM8300_MAX-1] = -1 };
module_param_array(dicom_other_pal, int, NULL, 0444);
MODULE_PARM_DESC(dicom_other_pal, "If this is set, then some internal register values are swapped for PAL and NTSC. Defaults to 1.");

int dicom_fix[EM8300_MAX] = { [0 ... EM8300_MAX-1] = -1 };
module_param_array(dicom_fix, int, NULL, 0444);
MODULE_PARM_DESC(dicom_fix, "If this is set then some internal register values are changed. Fixes green screen problems for some. Defaults to 1.");

int dicom_control[EM8300_MAX] = { [0 ... EM8300_MAX-1] = -1 };
module_param_array(dicom_control, int, NULL, 0444);
MODULE_PARM_DESC(dicom_control, "If this is set then some internal register values are changed. Fixes green screen problems for some. Defaults to 1.");

int bt865_ucode_timeout[EM8300_MAX] = { [0 ... EM8300_MAX-1] = -1 };
module_param_array(bt865_ucode_timeout, int, NULL, 0444);
MODULE_PARM_DESC(bt865_ucode_timeout, "Set this to 1 if you have a bt865 and get timeouts when uploading the microcode. Defaults to 0.");

int activate_loopback[EM8300_MAX] = { [0 ... EM8300_MAX-1] = -1 };
module_param_array(activate_loopback, int, NULL, 0444);
MODULE_PARM_DESC(activate_loopback, "If you lose video after loading the modules or uploading the microcode set this to 1. Defaults to 0.");

int card_model[EM8300_MAX] = { [0 ... EM8300_MAX-1] = -1 };
module_param_array(card_model, int, NULL, 0444);
MODULE_PARM_DESC(card_model, "Model number for the em8300-based card. -1 (default) means automatic detection; 0 means unknown model with manual setup.");

int stop_video[EM8300_MAX] = { [0 ... EM8300_MAX-1] = 0 };
module_param_array(stop_video, int, NULL, 0444);
MODULE_PARM_DESC(stop_video, "Set this to 1 if you want to stop video output instead of black when there is nothing to display. Defaults to 0.");
