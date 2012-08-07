/*
    ioctl control functions
    Copyright (C) 2012  Christian Gmeiner <christian.gmeiner@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "em8300_driver.h"
#include "em8300_controls.h"

static int em8300_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct em8300_s *em = container_of(ctrl->handler, struct em8300_s, ctrl_handler);
	int val = ctrl->val;

	switch (ctrl->id) {
	case V4L2_CID_CONTRAST:
		em8300_dicom_setBCS(em, em->bcs.brightness, val, em->bcs.saturation);
		break;

	case V4L2_CID_BRIGHTNESS:
		em8300_dicom_setBCS(em, val, em->bcs.contrast, em->bcs.saturation);
		break;

	case V4L2_CID_SATURATION:
		em8300_dicom_setBCS(em, em->bcs.brightness, em->bcs.contrast, val);
		break;

	default:
		return -EINVAL;
		break;
	}

	return 0;
}

static int em8300_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct em8300_s *em = container_of(ctrl->handler, struct em8300_s, ctrl_handler);
	int  val;

	switch (ctrl->id) {
	case V4L2_CID_CONTRAST:
		val = em->bcs.contrast;
		break;
	case V4L2_CID_BRIGHTNESS:
		val = em->bcs.brightness;
		break;
	case V4L2_CID_SATURATION:
		val = em->bcs.saturation;
		break;
	default:
		return -EINVAL;
	}

	if (val < 0)
		return val;

	ctrl->val = val;

	return 0;
}

const struct v4l2_ctrl_ops em8300_hdl_ops = {
	.s_ctrl = em8300_s_ctrl,
	.g_volatile_ctrl = em8300_g_volatile_ctrl,
};
