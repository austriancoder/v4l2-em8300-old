/* $Id$
 *
 * em8300_sysfs.c -- interface to the sysfs filesystem
 * Copyright (C) 2004 Eric Donohue <epd3j@hotmail.com>
 * Copyright (C) 2004,2006,2008 Nicolas Boullis <nboullis@debian.org>
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

#include "em8300_sysfs.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,46)

#include <linux/device.h>
#include <linux/pci.h>

#include "em8300_params.h"
#include "em8300_eeprom.h"
#include "em8300_reg.h"
#include "em8300_models.h"
#include "encoder.h"

#include "em8300_version.h"

extern struct pci_driver em8300_driver;

static ssize_t show_version(struct device_driver *dd, char *buf)
{
	return sprintf(buf, "%s\n", EM8300_VERSION);
}

static DRIVER_ATTR(version, S_IRUGO, show_version, NULL);

static ssize_t show_model(struct device *dev,
			  struct device_attribute *attr,
			  char  *buf)
{
	struct em8300_s *em = dev_get_drvdata(dev);
	ssize_t len = 0;
	char *encoder_name = NULL;
	u8 *tmp;
	int i;
	int model;

	len += sprintf(buf + len,
		       "\n**** Card model ****\n\n");

/* Identified model */
	model = identify_model(em);
	if (model < 0)
		model = 0;
	len += sprintf(buf + len,
		       "Identified model: %s\n",
		       known_models[model].name);

	len += sprintf(buf + len,
		       "\n**** Detected data ****\n\n");

/* General information */
	len += sprintf(buf + len,
		       "PCI revision: %d\n",
		       em->pci_revision);
	len += sprintf(buf + len,
		       "Chip revision: %d\n",
		       em->chip_revision);

/* Video encoder */
	switch (em->encoder_type) {
	case ENCODER_BT865:
		encoder_name = "BT865";
		break;
	case ENCODER_ADV7170:
		encoder_name = "ADV7170";
		break;
	case ENCODER_ADV7175:
		encoder_name = "ADV7175A";
		break;
	}
	if (encoder_name) {
		len += sprintf(buf + len,
			       "Video encoder: %s at address 0x%02x on %s\n",
			       encoder_name, em->encoder->addr,
			       em->encoder->adapter->name);
	} else {
		len += sprintf(buf + len,
			       "No known video encoder found.\n");
	}

/* EEPROM data */
	if ((tmp = kmalloc(256, GFP_KERNEL)) != NULL) {
		if (!em8300_eeprom_read(em, tmp)) {
			len += sprintf(buf + len, "EEPROM data:");
			for (i = 0; i < 256; i++) {
				if (i%32 == 0)
					len += sprintf(buf + len, "\n\t");
				len += sprintf(buf + len, "%02x", tmp[i]);
			}
			len += sprintf(buf + len, "\n");
		}
		kfree(tmp);
	}
	if (em->eeprom_checksum) {
		len += sprintf(buf + len, "EEPROM checksum: ");
		for (i = 0; i < 16; i++) {
			len += sprintf(buf + len, "%02x",
				       em->eeprom_checksum[i]);
		}
		len += sprintf(buf + len, "\n");
	}

	if (em->chip_revision == 2)
		len += sprintf(buf + len,
			       "read_register(0x1c08) = 0x%02x\n",
			       read_register(0x1c08));

	len += sprintf(buf + len,
		       "\n**** Current configuration ****\n\n");

/* Configuration */
	len += sprintf(buf + len,
		       "em8300.ko options:\n");
	len += sprintf(buf + len,
		       ((em->chip_revision == 2)
			&& ((0x60 & read_register(0x1c08)) == 0x60)) ?
		       "  use_bt865=%d\n" :
		       "  [use_bt865=%d]\n",
		       em->config.model.use_bt865);
	len += sprintf(buf + len,
		       "  dicom_other_pal=%d\n",
		       em->config.model.dicom_other_pal);
	len += sprintf(buf + len,
		       (em->encoder_type != ENCODER_BT865) ?
		       "  dicom_fix=%d\n" :
		       "  [dicom_fix=%d]\n",
		       em->config.model.dicom_fix);
	len += sprintf(buf + len,
		       (em->encoder_type != ENCODER_BT865) ?
		       "  dicom_control=%d\n" :
		       "  [dicom_control=%d]\n",
		       em->config.model.dicom_control);
	len += sprintf(buf + len,
		       ((em->encoder_type != ENCODER_ADV7170)
			&& (em->encoder_type != ENCODER_ADV7175)) ?
		       "  bt865_ucode_timeout=%d\n" :
		       "  [bt865_ucode_timeout=%d]\n",
		       em->config.model.bt865_ucode_timeout);
	len += sprintf(buf + len,
		       "  activate_loopback=%d\n",
		       em->config.model.activate_loopback);

	switch (em->encoder_type) {
	case ENCODER_ADV7170:
	case ENCODER_ADV7175:
	{
		len += sprintf(buf + len,
			       "adv717x.ko options:\n");
		len += sprintf(buf + len,
			       "  pixelport_16bit=%d\n",
			       em->config.adv717x_model.pixelport_16bit);
		len += sprintf(buf + len,
			       "  pixelport_other_pal=%d\n",
			       em->config.adv717x_model.pixelport_other_pal);
		len += sprintf(buf + len,
			       "  pixeldata_adjust_ntsc=%d\n",
			       em->config.adv717x_model.pixeldata_adjust_ntsc);
		len += sprintf(buf + len,
			       "  pixeldata_adjust_pal=%d\n",
			       em->config.adv717x_model.pixeldata_adjust_pal);
		break;
	}
	}

	len += sprintf(buf + len,
		       "\n**** Form ****\n\n");

	len += sprintf(buf + len,
		       "PAL video output\n"
		       " [ ] works fine\n"
		       " [ ] does not work (please describe problem)\n"
		       " [ ] was not tried\n"
		       "\n");
	len += sprintf(buf + len,
		       "NTSC video output\n"
		       " [ ] works fine\n"
		       " [ ] does not work (please describe problem)\n"
		       " [ ] was not tried\n"
		       "\n");
	len += sprintf(buf + len,
		       "video passthrough and overlay\n"
		       " [ ] work fine\n"
		       " [ ] do not work (please describe problem)\n"
		       " [ ] were not tried\n"
		       "\n");
	len += sprintf(buf + len,
		       "%schanging the use_bt865 option (use_bt865=%s)\n"
		       " [ ] makes no difference\n"
		       " [ ] breaks something (please describe problem)\n"
		       " [ ] was not tried\n"
		       "\n",
		       ((em->chip_revision == 2) && ((0x60 & read_register(0x1c08)) == 0x60)) ? "[important] " : "",
		       use_bt865[em->card_nr] ? "off" : "on");
	if ((em->encoder_type != ENCODER_ADV7170) && (em->encoder_type != ENCODER_ADV7175))
		len += sprintf(buf + len,
			       "changing the bt865_ucode_timeout option (bt865_ucode_timeout=%s)\n"
			       " [ ] makes no difference\n"
			       " [ ] breaks something (please describe problem)\n"
			       " [ ] was not tried\n"
			       "\n", bt865_ucode_timeout[em->card_nr] ? "off" : "on");
	len += sprintf(buf + len,
		       "changing the activate_loopback option (activate_loopback=%s)\n"
		       "(relevant even if you only use video out)\n"
		       " [ ] makes no difference\n"
		       " [ ] breaks something (please describe problem)\n"
		       " [ ] was not tried\n"
		       "\n", activate_loopback[em->card_nr] ? "off" : "on");
	len += sprintf(buf + len,
		       "changing the dicom_other_pal option (dicom_other_pal=%s)\n"
		       "(only relevant for PAL mode)\n"
		       " [ ] makes no difference\n"
		       " [ ] breaks something (please describe problem)\n"
		       " [ ] was not tried\n"
		       "\n", dicom_other_pal[em->card_nr] ? "off" : "on");
	if (em->encoder_type != ENCODER_BT865)
		len += sprintf(buf + len,
			       "[important] changing the dicom_fix option (dicom_fix=%s)\n"
			       " [ ] makes no difference\n"
			       " [ ] breaks something (please describe problem)\n"
			       " [ ] was not tried\n"
			       "\n", dicom_fix[em->card_nr] ? "off" : "on");
	len += sprintf(buf + len,
		       "[optional] card model:\n"
		       "(something like \"CT7260\" for DXR3 boards or "
		       "\"ASSY: 53-000569-02\" for H+ board;\n"
		       " both written on the PCB; not always available.)\n");

	len += sprintf(buf + len,
		       "\n**** The END ****\n\n");

	len += sprintf(buf + len,
		       "Please fill in the form above and send everything to\n"
		       "  dxr3\055poll\100lists\056sourceforge\056net\n"
		       "with \"model\" as the subject.\n\n");

	return len;
}

static DEVICE_ATTR(model, S_IRUGO, show_model, NULL);

static ssize_t show_zoom(struct device *dev,
			 struct device_attribute *attr,
			 char  *buf)
{
	struct em8300_s *em = dev_get_drvdata(dev);
	return sprintf(buf, "%d%%\n", em->zoom);
}

static ssize_t store_zoom(struct device *dev,
			  struct device_attribute *attr,
			  const char  *buf,
			  size_t count)
{
	struct em8300_s *em = dev_get_drvdata(dev);
	int z;
	if (sscanf(buf, "%d", &z)) {
		if ((z > 0) && (z <= 100)) {
			em->zoom = z;
			em8300_dicom_update(em);
		}
	}
	return count;
}

static DEVICE_ATTR(zoom, S_IRUGO|S_IWUSR, show_zoom, store_zoom);

static void em8300_sysfs_postregister_driver(void)
{
	int result = driver_create_file(&em8300_driver.driver, &driver_attr_version);
	if (result != 0) {
		printk(KERN_ERR "em8300: driver_create_file failed with error %d\n", result);
	}
}

static void em8300_sysfs_register_card(struct em8300_s *em)
{
	device_create_file(&em->pci_dev->dev, &dev_attr_model);
	device_create_file(&em->pci_dev->dev, &dev_attr_zoom);
}

static void em8300_sysfs_unregister_card(struct em8300_s *em)
{
	device_remove_file(&em->pci_dev->dev, &dev_attr_model);
	device_remove_file(&em->pci_dev->dev, &dev_attr_zoom);
}

static void em8300_sysfs_preunregister_driver(void)
{
	driver_remove_file(&em8300_driver.driver, &driver_attr_version);
}

struct em8300_registrar_s em8300_sysfs_registrar = {
	.register_driver      = NULL,
	.postregister_driver  = &em8300_sysfs_postregister_driver,
	.register_card        = &em8300_sysfs_register_card,
	.enable_card          = NULL,
	.disable_card         = NULL,
	.unregister_card      = &em8300_sysfs_unregister_card,
	.preunregister_driver = &em8300_sysfs_preunregister_driver,
	.unregister_driver    = NULL,
	.audio_interrupt      = NULL,
	.video_interrupt      = NULL,
	.vbl_interrupt        = NULL,
};

#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,46) */

struct em8300_registrar_s em8300_sysfs_registrar = {
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

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,46) */
