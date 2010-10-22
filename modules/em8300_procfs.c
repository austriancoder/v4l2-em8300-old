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
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <asm/io.h>
#include "em8300_procfs.h"
#include "em8300_reg.h"
#include "em8300_eeprom.h"

#include "em8300_version.h"

#ifdef CONFIG_PROC_FS

#ifndef EM8300_PROCFS_DIR
#define EM8300_PROCFS_DIR "em8300"
#endif

struct proc_dir_entry *em8300_proc;


static int em8300_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	struct em8300_s *em = (struct em8300_s *) data;
	char *encoder_name;

	*start = 0;
	*eof = 1;

	len += sprintf(page + len,
		       "----- Driver Info -----\n");
	len += sprintf(page + len,
		       "em8300 module version %s\n",
		       EM8300_VERSION);
	/* Device information */
	len += sprintf(page + len,
		       "Card revision %d\n",
		       em->pci_revision);
	len += sprintf(page + len,
		       "Chip revision %d\n",
		       em->chip_revision);
	switch (em->encoder_type) {
	case ENCODER_BT865:
		encoder_name = "BT865";
		break;
	case ENCODER_ADV7170:
		encoder_name = "ADV7170";
		break;
	case ENCODER_ADV7175:
		encoder_name = "ADV7175";
		break;
	default:
		len += sprintf(page + len, "No known video encoder found.\n");
		goto encoder_done;
	}
	len += sprintf(page + len,
		       "Video encoder: %s at address 0x%02x on %s\n",
		       encoder_name, em->encoder->addr,
		       em->encoder->adapter->name);
 encoder_done:
	{
		u8 *buf;
		int i;
		if ((buf = kmalloc(256, GFP_KERNEL)) != NULL) {
			if (!em8300_eeprom_read(em, buf)) {
				len += sprintf(page + len, "EEPROM data:");
				for (i=0; i<256; i++) {
					if (i%32 == 0)
						len += sprintf(page + len, "\n\t");
					len += sprintf(page + len, "%02x", buf[i]);
				}
				len += sprintf(page + len, "\n");
			}
			kfree(buf);
		}
		if (em->eeprom_checksum) {
			len += sprintf(page + len, "EEPROM checksum: ");
			for (i=0; i<16; i++) {
				len += sprintf(page + len, "%02x", em->eeprom_checksum[i]);
			}
			len += sprintf(page + len, "\n");
		}
	}

	len += sprintf(page + len,
		       "Memory mapped at address range 0x%0lx->0x%0lx%s\n",
		       (unsigned long int) em->mem,
		       (unsigned long int) em->mem
		       + (unsigned long int) em->memsize,
		       em->mtrr_reg ? " (FIFOs using MTRR)" : "");
	if (em->ucodeloaded) {
		len += sprintf(page + len,
			       "Microcode version 0x%02x loaded\n",
			       read_ucregister(MicroCodeVersion));
		em8300_dicom_get_dbufinfo(em);
		len += sprintf(page + len,
			       "Display buffer resolution: %dx%d\n",
			       em->dbuf_info.xsize, em->dbuf_info.ysize);
		len += sprintf(page + len,
			       "Dicom set to %s\n",
			       em->dicom_tvout ? "TV-out" : "overlay");
		if (em->dicom_tvout) {
			len += sprintf(page + len,
				       "Using %s\n",
				       (em->video_mode == EM8300_VIDEOMODE_PAL) ? "PAL" : "NTSC");
			len += sprintf(page + len,
				       "Aspect is %s\n",
				       (em->aspect_ratio == EM8300_ASPECTRATIO_4_3) ? "4:3" : "16:9");
		} else {
			len += sprintf(page + len,
				       "em9010 %s\n",
				       em->overlay_enabled ? "online" : "offline");
			len += sprintf(page + len,
				       "Video mapped to screen coordinates %dx%d (%dx%d)\n",
				       em->overlay_frame_xpos, em->overlay_frame_ypos,
				       em->overlay_xres, em->overlay_yres);
		}
	} else {
		len += sprintf(page + len, "Microcode not loaded\n");
	}
	len += sprintf(page + len,
		       "%s audio output\n",
		       (em->audio_mode == EM8300_AUDIOMODE_ANALOG) ? "Analog" : "Digital");
	return len;
}


static void em8300_procfs_register_card(struct em8300_s *em)
{
	char devname[64];
	if (em8300_proc) {
		struct proc_dir_entry *proc;
		sprintf(devname, "%d", em->card_nr);
		proc = create_proc_entry(devname,
					 S_IFREG | S_IRUGO,
					 em8300_proc);
		if (proc) {
			proc->data = (void *) em;
			proc->read_proc = em8300_proc_read;
		}
	}
}

static void em8300_procfs_unregister_card(struct em8300_s *em)
{
	char devname[64];
	if (em8300_proc) {
		sprintf(devname, "%d", em->card_nr);
		remove_proc_entry(devname, em8300_proc);
	}
}

static void em8300_procfs_unregister_driver(void)
{
	if (em8300_proc) {
		remove_proc_entry(EM8300_PROCFS_DIR, NULL);
	}
}

static void em8300_procfs_register_driver(void)
{
	em8300_proc = create_proc_entry(EM8300_PROCFS_DIR,
					S_IFDIR | S_IRUGO | S_IXUGO,
					NULL);
	if (!em8300_proc)
		printk(KERN_ERR "em8300: unable to register proc entry!\n");
}

struct em8300_registrar_s em8300_procfs_registrar =
{
	.register_driver      = &em8300_procfs_register_driver,
	.postregister_driver  = NULL,
	.register_card        = &em8300_procfs_register_card,
	.enable_card          = NULL,
	.disable_card         = NULL,
	.unregister_card      = &em8300_procfs_unregister_card,
	.preunregister_driver = NULL,
	.unregister_driver    = &em8300_procfs_unregister_driver,
	.audio_interrupt      = NULL,
	.video_interrupt      = NULL,
	.vbl_interrupt        = NULL,
};

#else /* CONFIG_PROC_FS */

struct em8300_registrar_s em8300_procfs_registrar =
{
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

#endif /* CONFIG_PROC_FS */
