/*
 * encoder_output_mode.h
 *
 * Copyright (C) 2006 Nicolas Boullis <nboullis@debian.org>
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

typedef struct {
	char const * name;
	struct output_conf_s conf;
} mode_info_t;

static const mode_info_t mode_info[];

static int param_set_output_mode_t(const char *val, const struct kernel_param *kp)
{
	if (val) {
		int i;
		for (i=0; i < MODE_MAX; i++)
			if (strcmp(val, mode_info[i].name) == 0) {
				*(output_mode_t *)kp->arg = i;
				return 0;
			}
	}
	printk(KERN_ERR "%s: output_mode parameter expected\n",
	       kp->name);
	return -EINVAL;
}

static int param_get_output_mode_t(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "%s", mode_info[*(output_mode_t *)kp->arg].name);
}

struct kernel_param_ops param_ops_output_mode_t = {
	.set = param_set_output_mode_t,
	.get = param_get_output_mode_t,
};
