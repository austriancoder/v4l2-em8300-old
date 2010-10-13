/*
	em8300.c - EM8300 MPEG-2 decoder device driver

	Copyright (C) 2000 Henrik Johansson <henrikjo@post.utfors.se>

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
	Foundation Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
#include "em8300_devfs.h"

#ifdef CONFIG_DEVFS_FS

#include <linux/devfs_fs_kernel.h>

#include "em8300_params.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,70)
devfs_handle_t em8300_handle[EM8300_MAX*4];
#endif

extern struct file_operations em8300_fops;

static void em8300_devfs_register_card(struct em8300_s *em)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,70)
	char devname[64];
	sprintf(devname, "%s-%d", EM8300_LOGNAME, em->card_nr);
	em8300_handle[em->card_nr * 4] = devfs_register(NULL, devname, DEVFS_FL_DEFAULT, major,
							em->card_nr * 4, S_IFCHR | S_IRUGO | S_IWUGO, &em8300_fops, NULL);
#else
	devfs_mk_cdev(MKDEV(major, em->card_nr * 4),
		      S_IFCHR | S_IRUGO | S_IWUGO,
		      "%s-%d", EM8300_LOGNAME, em->card_nr);
#endif
}

static void em8300_devfs_enable_card(struct em8300_s *em)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,70)
	char devname[64];
	sprintf(devname, "%s_mv-%d", EM8300_LOGNAME, em->card_nr);
	em8300_handle[(em->card_nr * 4) + 1] = devfs_register(NULL, devname, DEVFS_FL_DEFAULT, major,
							      (em->card_nr * 4) + 1, S_IFCHR | S_IRUGO | S_IWUGO, &em8300_fops, NULL);
	sprintf(devname, "%s_ma-%d", EM8300_LOGNAME, em->card_nr);
	em8300_handle[(em->card_nr * 4) + 2] = devfs_register(NULL, devname, DEVFS_FL_DEFAULT, major,
							      (em->card_nr * 4) + 2, S_IFCHR | S_IRUGO | S_IWUGO, &em8300_fops, NULL);
	sprintf(devname, "%s_sp-%d", EM8300_LOGNAME, em->card_nr);
	em8300_handle[(em->card_nr * 4) + 3] = devfs_register(NULL, devname, DEVFS_FL_DEFAULT, major,
							      (em->card_nr * 4) + 3, S_IFCHR | S_IRUGO | S_IWUGO, &em8300_fops, NULL);
#else
	devfs_mk_cdev(MKDEV(major, (em->card_nr * 4) + 1),
		      S_IFCHR | S_IRUGO | S_IWUGO,
		      "%s_mv-%d", EM8300_LOGNAME, em->card_nr);
	devfs_mk_cdev(MKDEV(major, (em->card_nr * 4) + 2),
		      S_IFCHR | S_IRUGO | S_IWUGO,
		      "%s_ma-%d", EM8300_LOGNAME, em->card_nr);
	devfs_mk_cdev(MKDEV(major, (em->card_nr * 4) + 3),
		      S_IFCHR | S_IRUGO | S_IWUGO,
		      "%s_sp-%d", EM8300_LOGNAME, em->card_nr);
#endif
}

static void em8300_devfs_disable_card(struct em8300_s *em)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,69)
	devfs_unregister(em8300_handle[(em->card_nr * 4) + 1]);
	devfs_unregister(em8300_handle[(em->card_nr * 4) + 2]);
	devfs_unregister(em8300_handle[(em->card_nr * 4) + 3]);
#else
	devfs_remove("%s_mv-%d", EM8300_LOGNAME, em->card_nr);
	devfs_remove("%s_ma-%d", EM8300_LOGNAME, em->card_nr);
	devfs_remove("%s_sp-%d", EM8300_LOGNAME, em->card_nr);
#endif
}

static void em8300_devfs_unregister_card(struct em8300_s *em)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,69)
	devfs_unregister(em8300_handle[em->card_nr * 4]);
#else
	devfs_remove("%s-%d", EM8300_LOGNAME, em->card_nr);
#endif
}

struct em8300_registrar_s em8300_devfs_registrar = {
	.register_driver      = NULL,
	.postregister_driver  = NULL,
	.register_card        = &em8300_devfs_register_card,
	.enable_card          = &em8300_devfs_enable_card,
	.disable_card         = &em8300_devfs_disable_card,
	.unregister_card      = &em8300_devfs_unregister_card,
	.preunregister_driver = NULL,
	.unregister_driver    = NULL,
	.audio_interrupt      = NULL,
	.video_interrupt      = NULL,
	.vbl_interrupt        = NULL,
};

#else /* CONFIG_DEVFS_FS */

struct em8300_registrar_s em8300_devfs_registrar = {
	.register_driver      = NULL,
	.postregister_driver  = NULL,
	.register_card        = NULL,
	.enable_card          = NULL,
	.disable_card         = NULL,
	.unregister_card      = NULL,
	.preunregister_driver = NULL,
	.unregister_driver    = NULL,
	.audio_interrupt      = NULL,
	.video_interrupt      = NULL,
	.vbl_interrupt        = NULL,
};

#endif /* CONFIG_DEVFS_FS */
