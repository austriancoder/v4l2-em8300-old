/*
	em8300.c - EM8300 MPEG-2 decoder device driver

	Copyright (C) 2000 Henrik Johansson <henrikjo@post.utfors.se>
	Copyright (C) 2008 Nicolas Boullis <nboullis@debian.org>

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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/sound.h>
#include <linux/signal.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/poll.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <linux/sched.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/smp_lock.h>

#include "em8300_compat24.h"
#include "encoder.h"

#include "em8300_reg.h"
#include <linux/em8300.h>
#include "em8300_driver.h"
#include "em8300_fifo.h"
#include "em8300_registration.h"
#include "em8300_params.h"
#include "em8300_eeprom.h"
#include "em8300_models.h"

#ifdef CONFIG_EM8300_IOCTL32
#include "em8300_ioctl32.h"
#endif

#include "em8300_version.h"

#if !defined(CONFIG_I2C_ALGOBIT) && !defined(CONFIG_I2C_ALGOBIT_MODULE)
#error "This needs the I2C Bit Banging Interface in your Kernel"
#endif

MODULE_AUTHOR("Henrik Johansson <henrikjo@post.utfors.se>");
MODULE_DESCRIPTION("EM8300 MPEG-2 decoder");
MODULE_SUPPORTED_DEVICE("em8300");
MODULE_LICENSE("GPL");
#if EM8300_MAJOR != 0
MODULE_ALIAS_CHARDEV_MAJOR(EM8300_MAJOR);
#endif
#ifdef MODULE_VERSION
MODULE_VERSION(EM8300_VERSION);
#endif

EXPORT_NO_SYMBOLS;

static int em8300_cards, clients;

static struct em8300_s *em8300[EM8300_MAX];

#if defined(CONFIG_SOUND) || defined(CONFIG_SOUND_MODULE)
static int dsp_num_table[16];
#endif

/* structure to keep track of the memory that has been allocated by
   the user via mmap() */
struct memory_info {
	struct list_head item;
	long length;
	char *ptr;
};

static struct pci_device_id em8300_ids[] = {
	{ PCI_VENDOR_ID_SIGMADESIGNS, PCI_DEVICE_ID_SIGMADESIGNS_EM8300,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, em8300_ids);

static irqreturn_t em8300_irq(int irq, void *dev_id)
{
	struct em8300_s *em = (struct em8300_s *) dev_id;
	int irqstatus;
	struct timeval tv;

	irqstatus = read_ucregister(Q_IrqStatus);

	if (irqstatus & 0x8000) {
		write_ucregister(Q_IrqMask, 0x0);
		write_register(EM8300_INTERRUPT_ACK, 2);

		write_ucregister(Q_IrqStatus, 0x8000);

		if (irqstatus & IRQSTATUS_VIDEO_FIFO) {
			em8300_fifo_check(em->mvfifo);
			em8300_video_interrupt(em);
		}

		if (irqstatus & IRQSTATUS_AUDIO_FIFO) {
			em8300_audio_interrupt(em);
			if (em->audio_driver_style == OSS)
				em8300_fifo_check(em->mafifo);
		}

		if (irqstatus & IRQSTATUS_VIDEO_VBL) {
			em8300_fifo_check(em->spfifo);
			em8300_video_check_ptsfifo(em);
			em8300_spu_check_ptsfifo(em);

			do_gettimeofday(&tv);
			em->irqtimediff = TIMEDIFF(tv, em->tv);
			em->tv = tv;
			em->irqcount++;
			wake_up(&em->vbi_wait);
			em8300_vbl_interrupt(em);
		}

		write_ucregister(Q_IrqMask, em->irqmask);
		write_ucregister(Q_IrqStatus, 0x0000);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static void release_em8300(struct em8300_s *em)
{
	if ((em->encoder) && (em->encoder->driver)
	    && (em->encoder->driver->command))
		em->encoder->driver->command(em->encoder,
					     ENCODER_CMD_ENABLEOUTPUT,
					     (void *) 0);

#ifdef CONFIG_MTRR
	if (em->mtrr_reg)
		mtrr_del(em->mtrr_reg, em->adr, em->memsize);
#endif

	em8300_eeprom_checksum_deinit(em);
	em8300_i2c_exit(em);

	write_ucregister(Q_IrqMask, 0);
	write_ucregister(Q_IrqStatus, 0);
	write_register(0x2000, 0);

	em8300_fifo_free(em->mvfifo);
	em8300_fifo_free(em->mafifo);
	em8300_fifo_free(em->spfifo);

	/* free it */
	free_irq(em->dev->irq, em);

	/* unmap and free memory */
	if (em->mem)
		iounmap((unsigned *) em->mem);

	kfree(em);
}

static long em8300_io_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct em8300_s *em = filp->private_data;
	struct inode *inode = filp->f_path.dentry->d_inode;

	int subdevice = EM8300_IMINOR(inode) % 4;
	long ret;

	lock_kernel();
	switch (subdevice) {
	case EM8300_SUBDEVICE_AUDIO:
		ret = em8300_audio_ioctl(em, cmd, arg);
	case EM8300_SUBDEVICE_VIDEO:
		ret = em8300_video_ioctl(em, cmd, arg);
	case EM8300_SUBDEVICE_SUBPICTURE:
		ret =  em8300_spu_ioctl(em, cmd, arg);
	case EM8300_SUBDEVICE_CONTROL:
		ret = em8300_control_ioctl(em, cmd, arg);
	}
	unlock_kernel();

	return ret;
}

static int em8300_io_open(struct inode *inode, struct file *filp)
{
	int card = EM8300_IMINOR(inode) / 4;
	int subdevice = EM8300_IMINOR(inode) % 4;
	struct em8300_s *em = em8300[card];
	int err = 0;

	if (card >= em8300_cards)
		return -ENODEV;

	if (subdevice != EM8300_SUBDEVICE_CONTROL) {
		if (em->inuse[subdevice])
			return -EBUSY;
	}

	filp->private_data = em;

	/* initalize the memory list */
	INIT_LIST_HEAD(&em->memory);

	switch (subdevice) {
	case EM8300_SUBDEVICE_CONTROL:
		em->nonblock[0] = ((filp->f_flags&O_NONBLOCK) == O_NONBLOCK);
		break;
	case EM8300_SUBDEVICE_AUDIO:
		down(&em->audio_driver_style_lock);
		if (em->audio_driver_style != NONE) {
			up(&em->audio_driver_style_lock);
			return -EBUSY;
		}
		em->audio_driver_style = OSS;
		up(&em->audio_driver_style_lock);

		em->nonblock[1] = ((filp->f_flags&O_NONBLOCK) == O_NONBLOCK);
		err = em8300_audio_open(em);
		if (err)
			em->audio_driver_style = NONE;
		break;
	case EM8300_SUBDEVICE_VIDEO:
		em8300_require_ucode(em);
		if (!em->ucodeloaded)
			return -ENODEV;

		em->nonblock[2] = ((filp->f_flags&O_NONBLOCK) == O_NONBLOCK);
		em8300_video_open(em);

		if (!em->overlay_enabled)
			em8300_ioctl_enable_videoout(em, 1);

		em8300_video_setplaymode(em, EM8300_PLAYMODE_PLAY);
		break;
	case EM8300_SUBDEVICE_SUBPICTURE:
		em8300_require_ucode(em);
		if (!em->ucodeloaded)
			return -ENODEV;

		em->nonblock[3] = ((filp->f_flags&O_NONBLOCK) == O_NONBLOCK);
		err = em8300_spu_open(em);
		break;
	default:
		return -ENODEV;
		break;
	}

	if (err)
		return err;

	em->inuse[subdevice]++;

	clients++;
	pr_debug("em8300-%d: Opening device %d, Clients:%d\n", em->card_nr, subdevice, clients);

	return 0;
}

static ssize_t em8300_io_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct em8300_s *em = file->private_data;
	int subdevice = EM8300_IMINOR(file->f_dentry->d_inode) % 4;

	switch (subdevice) {
	case EM8300_SUBDEVICE_VIDEO:
		em->nonblock[2] = ((file->f_flags&O_NONBLOCK) == O_NONBLOCK);
		return em8300_video_write(em, buf, count, ppos);
		break;
	case EM8300_SUBDEVICE_AUDIO:
		em->nonblock[1] = ((file->f_flags&O_NONBLOCK) == O_NONBLOCK);
		return em8300_audio_write(em, buf, count, ppos);
		break;
	case EM8300_SUBDEVICE_SUBPICTURE:
		em->nonblock[3] = ((file->f_flags&O_NONBLOCK) == O_NONBLOCK);
		return em8300_spu_write(em, buf, count, ppos);
		break;
	default:
		return -EPERM;
	}
}

int em8300_io_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct em8300_s *em = file->private_data;
	unsigned long size = vma->vm_end - vma->vm_start;
	int subdevice = EM8300_IMINOR(file->f_dentry->d_inode) % 4;

	if (subdevice != EM8300_SUBDEVICE_CONTROL)
		return -EPERM;

	switch (vma->vm_pgoff) {
	case 1: {
		/* fixme: we should count the total size of allocated memories
		   so we don't risk a out-of-memory or denial-of-service attack... */

		char *mem = 0;
		struct memory_info *info = NULL;
		unsigned long adr = 0;
		unsigned long size = vma->vm_end - vma->vm_start;
		unsigned long pages = (size+(PAGE_SIZE-1))/PAGE_SIZE;
		/* round up the memory */
		size = pages * PAGE_SIZE;

		/* allocate the physical contiguous memory */
		mem = kmalloc(pages*PAGE_SIZE, GFP_KERNEL);
		if (mem == NULL)
			return -ENOMEM;

		/* clear out the memory for sure */
		memset(mem, 0x00, pages*PAGE_SIZE);

		/* reserve all pages */
		for (adr = (long)mem; adr < (long)mem + size; adr += PAGE_SIZE)
			SetPageReserved(virt_to_page(adr));

		/* lock the area*/
		vma->vm_flags |= VM_LOCKED;

		/* remap the memory to user space */
		if (remap_pfn_range(vma, vma->vm_start, virt_to_phys((void *)mem) >> PAGE_SHIFT, size, vma->vm_page_prot)) {
			kfree(mem);
			return -EAGAIN;
		}

		/* put the physical address into the first dword of the memory */
		*((long *)mem) = virt_to_phys((void *)mem);

		/* keep track of the memory we have allocated */
		info = (struct memory_info *)vmalloc(sizeof(struct memory_info));
		if (NULL == info) {
			kfree(mem);
			return -ENOMEM;
		}

		info->ptr = mem;
		info->length = size;
		list_add_tail(&info->item, &em->memory);

		break;
	}
	case 0:
		if (size > em->memsize)
			return -EINVAL;

		remap_pfn_range(vma, vma->vm_start, em->adr >> PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot);
		vma->vm_file = file;
		atomic_inc(&file->f_dentry->d_inode->i_count);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned int em8300_poll(struct file *file, struct poll_table_struct *wait)
{
	struct em8300_s *em = file->private_data;
	int subdevice = EM8300_IMINOR(file->f_dentry->d_inode) % 4;
	unsigned int mask = 0;

	switch (subdevice) {
	case EM8300_SUBDEVICE_AUDIO:
		poll_wait(file, &em->mafifo->wait, wait);
		if (file->f_mode & FMODE_WRITE) {
			if (em8300_fifo_freeslots(em->mafifo)) {
				pr_debug("em8300-%d: Poll audio - Free slots: %d\n", em->card_nr, em8300_fifo_freeslots(em->mafifo));
				mask |= POLLOUT | POLLWRNORM;
			}
		}
		break;
	case EM8300_SUBDEVICE_VIDEO:
		poll_wait(file, &em->mvfifo->wait, wait);
		if (file->f_mode & FMODE_WRITE) {
			if (em8300_fifo_freeslots(em->mvfifo)) {
				pr_debug("em8300-%d: Poll video - Free slots: %d\n", em->card_nr, em8300_fifo_freeslots(em->mvfifo));
				mask |= POLLOUT | POLLWRNORM;
			}
		}
		break;
	case EM8300_SUBDEVICE_SUBPICTURE:
		poll_wait(file, &em->spfifo->wait, wait);
		if (file->f_mode & FMODE_WRITE) {
			if (em8300_fifo_freeslots(em->spfifo)) {
				pr_debug("em8300-%d: Poll subpic - Free slots: %d\n", em->card_nr, em8300_fifo_freeslots(em->spfifo));
				mask |= POLLOUT | POLLWRNORM;
			}
		}
	}

	return mask;
}

int em8300_io_release(struct inode *inode, struct file *filp)
{
	struct em8300_s *em = filp->private_data;
	int subdevice = EM8300_IMINOR(inode) % 4;

	switch (subdevice) {
	case EM8300_SUBDEVICE_AUDIO:
		em8300_audio_release(em);
		em->audio_driver_style = NONE;
		break;
	case EM8300_SUBDEVICE_VIDEO:
		em8300_video_release(em);
		if (!em->overlay_enabled)
			em8300_ioctl_enable_videoout(em, 0);
		break;
	case EM8300_SUBDEVICE_SUBPICTURE:
		em8300_spu_release(em);    /* Do we need this one ? */
		break;
	}

	while (0 == list_empty(&em->memory)) {
		unsigned long adr = 0;

		struct memory_info *info = list_entry(em->memory.next, struct memory_info, item);
		list_del(&info->item);

		for (adr = (long)info->ptr; adr < (long)info->ptr + info->length; adr += PAGE_SIZE)
			ClearPageReserved(virt_to_page(adr));

		kfree(info->ptr);
		vfree(info);
	}

	em->inuse[subdevice]--;

	clients--;
	pr_debug("em8300-%d: Releasing device %d, clients:%d\n", em->card_nr, subdevice, clients);

	return 0;
}

struct file_operations em8300_fops = {
	.owner = THIS_MODULE,
	.write = em8300_io_write,
	.unlocked_ioctl = em8300_io_ioctl,
	.mmap = em8300_io_mmap,
	.poll = em8300_poll,
	.open = em8300_io_open,
	.release = em8300_io_release,
#if defined(CONFIG_EM8300_IOCTL32) && defined(HAVE_COMPAT_IOCTL)
	.compat_ioctl = em8300_compat_ioctl,
#endif
};

#if defined(CONFIG_SOUND) || defined(CONFIG_SOUND_MODULE)
static long em8300_dsp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct em8300_s *em = filp->private_data;
	long ret;

	lock_kernel();
	ret = em8300_audio_ioctl(em, cmd, arg);
	unlock_kernel();
	
	return ret;
}

static int em8300_dsp_open(struct inode *inode, struct file *filp)
{
	int dsp_number = ((EM8300_IMINOR(inode) >> 4) & 0x0f);
	int card = dsp_num_table[dsp_number] - 1;
	int err = 0;
	struct em8300_s *em = em8300[card];

	pr_debug("em8300-%d: opening dsp %i for card %i\n", em->card_nr, dsp_number, card);

	if (card < 0 || card >= em8300_cards)
		return -ENODEV;

	down(&em->audio_driver_style_lock);
	if (em->audio_driver_style != NONE) {
		up(&em->audio_driver_style_lock);
		return -EBUSY;
	}
	em->audio_driver_style = OSS;
	up(&em->audio_driver_style_lock);

	if (em->inuse[EM8300_SUBDEVICE_AUDIO]) {
		printk("em8300-%d: em8300_dsp_open: em->audio_driver_style == NONE but em->inuse[EM8300_SUBDEVICE_AUDIO] !?\n", em->card_nr);
		em->audio_driver_style = NONE;
		return -EBUSY;
	}

	filp->private_data = em;

	err = em8300_audio_open(em);

	if (err) {
		em->audio_driver_style = NONE;
		return err;
	}

	em->inuse[EM8300_SUBDEVICE_AUDIO]++;

	clients++;
	pr_debug("em8300-%d: Opening device %d, Clients:%d\n", em->card_nr, EM8300_SUBDEVICE_AUDIO, clients);

	return 0;
}

static ssize_t em8300_dsp_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct em8300_s *em = file->private_data;
	return em8300_audio_write(em, buf, count, ppos);
}

static unsigned int em8300_dsp_poll(struct file *file, struct poll_table_struct *wait)
{
	struct em8300_s *em = file->private_data;
	unsigned int mask = 0;
	poll_wait(file, &em->mafifo->wait, wait);
	if (file->f_mode & FMODE_WRITE) {
		if (em8300_fifo_freeslots(em->mafifo)) {
			pr_debug("em8300-%d: Poll dsp - Free slots: %d\n", em->card_nr, em8300_fifo_freeslots(em->mafifo));
			mask |= POLLOUT | POLLWRNORM;
		}
	}
	return mask;
}

int em8300_dsp_release(struct inode *inode, struct file *filp)
{
	struct em8300_s *em = filp->private_data;

	em8300_audio_release(em);

	em->audio_driver_style = NONE;

	em->inuse[EM8300_SUBDEVICE_AUDIO]--;

	clients--;
	pr_debug("em8300-%d: Releasing device %d, clients:%d\n", em->card_nr, EM8300_SUBDEVICE_AUDIO, clients);

	return 0;
}

static struct file_operations em8300_dsp_audio_fops = {
	.owner = THIS_MODULE,
	.write = em8300_dsp_write,
	.unlocked_ioctl = em8300_dsp_ioctl,
	.poll = em8300_dsp_poll,
	.open = em8300_dsp_open,
	.release = em8300_dsp_release,
};
#endif

static int init_em8300(struct em8300_s *em)
{
	int identified_model;

	write_register(0x30000, read_register(0x30000));

	write_register(0x1f50, 0x123);

	if (read_register(0x1f50) == 0x123)
		em->chip_revision = 2;
	else
		em->chip_revision = 1;

	em8300_i2c_init1(em);
	em8300_eeprom_checksum_init(em);

	identified_model = identify_model(em);

	if (em->model == -1) {
		if (identified_model > 0) {
			em->model = identified_model;
			pr_info("em8300-%d: detected card: %s.\n", em->card_nr,
			       known_models[identified_model].name);
		} else {
			em->model = 0;
			printk(KERN_ERR "em8300-%d: unable to identify model...\n", em->card_nr);
		}
	}

	if ((em->model != identified_model) && (em->model > 0) && (identified_model > 0))
		printk(KERN_WARNING "em8300-%d: mismatch between detected and requested model.\n", em->card_nr);

	if (em->model > 0) {
		em->config.model = known_models[em->model].em8300_config;
		em->config.adv717x_model = known_models[em->model].adv717x_config;
	}

	if (em->chip_revision == 2) {
		if (0x40 & read_register(0x1c08)) {
			em->var_video_value = 3375; /* was 0xd34 = 3380 */
			em->mystery_divisor = 0x107ac;
			em->var_ucode_reg2 = 0x272;
			em->var_ucode_reg3 = 0x8272;
			if (0x20 & read_register(0x1c08)) {
				if (em->config.model.use_bt865)
					em->var_ucode_reg1 = 0x800;
				else
					em->var_ucode_reg1 = 0x818;
			}
		} else {
			em->var_video_value = 0xce4;
			em->mystery_divisor = 0x101d0;
			em->var_ucode_reg2 = 0x25a;
			em->var_ucode_reg3 = 0x825a;
		}
	} else {
		em->var_ucode_reg1 = 0x80;
		em->var_video_value = 0xce4;
		em->mystery_divisor = 0x101d0;
		em->var_ucode_reg2 = 0xC7;
		em->var_ucode_reg3 = 0x8c7;
	}

	/*
	 * Override default (or detected) values with module parameters.
	 */
	if (use_bt865[em->card_nr] >= 0)
		em->config.model.use_bt865 =
			use_bt865[em->card_nr];
	if (dicom_other_pal[em->card_nr] >= 0)
		em->config.model.dicom_other_pal =
			dicom_other_pal[em->card_nr];
	if (dicom_fix[em->card_nr] >= 0)
		em->config.model.dicom_fix =
			dicom_fix[em->card_nr];
	if (dicom_control[em->card_nr] >= 0)
		em->config.model.dicom_control =
			dicom_control[em->card_nr];
	if (bt865_ucode_timeout[em->card_nr] >= 0)
		em->config.model.bt865_ucode_timeout =
			bt865_ucode_timeout[em->card_nr];
	if (activate_loopback[em->card_nr] >= 0)
		em->config.model.activate_loopback =
			activate_loopback[em->card_nr];

	pr_info("em8300-%d: Chip revision: %d\n", em->card_nr, em->chip_revision);
	pr_debug("em8300-%d: use_bt865: %d\n", em->card_nr, em->config.model.use_bt865);

	em8300_i2c_init2(em);

	if (em->config.model.activate_loopback == 0) {
		em->clockgen_tvmode = CLOCKGEN_TVMODE_1;
		em->clockgen_overlaymode = CLOCKGEN_OVERLAYMODE_1;
	} else {
		em->clockgen_tvmode = CLOCKGEN_TVMODE_2;
		em->clockgen_overlaymode = CLOCKGEN_OVERLAYMODE_2;
	}

	em->clockgen = em->clockgen_tvmode;
	em8300_clockgen_write(em, em->clockgen);

	em->zoom = 100;

	pr_debug("em8300-%d: activate_loopback: %d\n", em->card_nr, em->config.model.activate_loopback);

	return 0;
}

static int em8300_pci_setup(struct pci_dev *dev)
{
	struct em8300_s *em = pci_get_drvdata(dev);
	unsigned char revision;
	int rc = 0;

	rc = pci_enable_device(dev);
	if (rc < 0) {
		printk(KERN_ERR "em8300-%d: Unable to enable PCI device\n", em->card_nr);
		return rc;
	}

	pci_set_master(dev);

	em->adr = pci_resource_start(dev, 0);
	em->memsize = pci_resource_len(dev, 0);

	pci_read_config_byte(dev, PCI_CLASS_REVISION, &revision);
	em->pci_revision = revision;

	return 0;
}

static int __devinit em8300_probe(struct pci_dev *dev,
				  const struct pci_device_id *pci_id)
{
	struct em8300_s *em;
	int result;

	em = kzalloc(sizeof(struct em8300_s), GFP_KERNEL);
	if (!em) {
		printk(KERN_ERR "em8300: kzalloc failed - out of memory!\n");
		return -ENOMEM;
	}

	em->dev = dev;
	em->card_nr = em8300_cards;

	pci_set_drvdata(dev, em);
	result = em8300_pci_setup(dev);
	if (result != 0) {
		printk(KERN_ERR "em8300-%d: pci setup failed\n", em->card_nr);
		goto mem_free;
	}

	/*
	 * Specify default values if card is not identified.
	 */
	em->config.model.use_bt865 =
		0;
#ifdef CONFIG_EM8300_DICOMPAL
	em->config.model.dicom_other_pal =
		1;
#else
	em->config.model.dicom_other_pal =
		0;
#endif
#ifdef CONFIG_EM8300_DICOMFIX
	em->config.model.dicom_fix =
		1;
#else
	em->config.model.dicom_fix =
		0;
#endif
#ifdef CONFIG_EM8300_DICOMCTRL
	em->config.model.dicom_control =
		1;
#else
	em->config.model.dicom_control =
		0;
#endif
#ifdef CONFIG_EM8300_UCODETIMEOUT
	em->config.model.bt865_ucode_timeout =
		1;
#else
	em->config.model.bt865_ucode_timeout =
		0;
#endif
#ifdef CONFIG_EM8300_LOOPBACK
	em->config.model.activate_loopback =
		1;
#else
	em->config.model.activate_loopback =
		0;
#endif

#ifdef CONFIG_ADV717X_PIXELPORT16BIT
	em->config.adv717x_model.pixelport_16bit = 1;
#else
	em->config.adv717x_model.pixelport_16bit = 0;
#endif
#ifdef CONFIG_ADV717X_PIXELPORTPAL
	em->config.adv717x_model.pixelport_other_pal = 1;
#else
	em->config.adv717x_model.pixelport_other_pal = 0;
#endif
	em->config.adv717x_model.pixeldata_adjust_ntsc = 1;
	em->config.adv717x_model.pixeldata_adjust_pal = 1;

	em->model = card_model[em8300_cards];

	pr_info("em8300-%d: EM8300 %x (rev %d) ", em->card_nr, dev->device, em->pci_revision);
	pr_info("bus: %d, devfn: %d, irq: %d, ", dev->bus->number, dev->devfn, dev->irq);
	pr_info("memory: 0x%08lx.\n", em->adr);

	em->mem = ioremap(em->adr, em->memsize);
	if (!em->mem) {
		printk(KERN_ERR "em8300-%d: ioremap for memory region failed\n", em->card_nr);
		result = -ENOMEM;
		goto mem_free;
	}

	pr_info("em8300-%d: mapped-memory at 0x%p\n", em->card_nr, em->mem);
#ifdef CONFIG_MTRR
	em->mtrr_reg = mtrr_add(em->adr, em->memsize, MTRR_TYPE_UNCACHABLE, 1);
	if (em->mtrr_reg)
		pr_info("em8300-%d: using MTRR\n", em->card_nr);
#endif

	init_waitqueue_head(&em->video_ptsfifo_wait);
	init_waitqueue_head(&em->vbi_wait);
	init_waitqueue_head(&em->sp_ptsfifo_wait);

	em->audio_driver_style = NONE;
	sema_init(&em->audio_driver_style_lock, 1);

	result = request_irq(dev->irq, em8300_irq, IRQF_SHARED | IRQF_DISABLED, "em8300", (void *) em);

	if (result == -EINVAL) {
		printk(KERN_ERR "em8300-%d: Bad irq number or handler\n", em->card_nr);
		goto irq_error;
	}

	init_em8300(em);

	em8300_register_card(em);

#if defined(CONFIG_SOUND) || defined(CONFIG_SOUND_MODULE)
	em->dsp_num = register_sound_dsp(&em8300_dsp_audio_fops, dsp_num[em->card_nr]);
	if (em->dsp_num < 0) {
		printk(KERN_ERR "em8300-%d: cannot register oss audio device!\n", em->card_nr);
	} else {
		dsp_num_table[em->dsp_num >> 4 & 0x0f] = em8300_cards + 1;
		pr_debug("em8300-%d: registered dsp %i for device %i\n", em->card_nr, em->dsp_num >> 4 & 0x0f, em8300_cards);
	}
#endif

#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
	em8300_enable_card(em);
#endif

	em8300[em8300_cards++] = em;
	return 0;

irq_error:
#ifdef CONFIG_MTRR
	if (em->mtrr_reg)
		mtrr_del(em->mtrr_reg, em->adr, em->memsize);
#endif
	iounmap(em->mem);

mem_free:
	kfree(em);
	return result;
}

static void __devexit em8300_remove(struct pci_dev *pci)
{
	struct em8300_s *em = pci_get_drvdata(pci);

	if (em) {
#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
		em8300_disable_card(em);
#else
		if (em->ucodeloaded == 1)
			em8300_disable_card(em);
#endif

#if defined(CONFIG_SOUND) || defined(CONFIG_SOUND_MODULE)
		unregister_sound_dsp(em->dsp_num);
#endif

		em8300_unregister_card(em);

		release_em8300(em);
	}

	pci_set_drvdata(pci, NULL);
	pci_disable_device(pci);
}

struct pci_driver em8300_driver = {
	.name     = "Sigma Designs EM8300",
	.id_table = em8300_ids,
	.probe    = em8300_probe,
	.remove   = __devexit_p(em8300_remove),
};

static void __exit em8300_exit(void)
{
#if defined(CONFIG_EM8300_IOCTL32) && !defined(HAVE_COMPAT_IOCTL)
	em8300_ioctl32_exit();
#endif

	em8300_preunregister_driver();

	pci_unregister_driver(&em8300_driver);

	unregister_chrdev(major, EM8300_LOGNAME);

	em8300_unregister_driver();
}

static int __init em8300_init(void)
{
	int err;

	/*memset(&em8300, 0, sizeof(em8300) * EM8300_MAX);*/
#if defined(CONFIG_SOUND) || defined(CONFIG_SOUND_MODULE)
	memset(&dsp_num_table, 0, sizeof(dsp_num_table));
#endif

	em8300_register_driver();

	if (major) {
		if (register_chrdev(major, EM8300_LOGNAME, &em8300_fops)) {
			printk(KERN_ERR "em8300: unable to get major %d\n", major);
			err = -ENODEV;
			goto err_chrdev;
		}
	} else {
		int m = register_chrdev(major, EM8300_LOGNAME, &em8300_fops);
		if (m > 0) {
			major = m;
		} else {
			printk(KERN_ERR "em8300: unable to get any major\n");
			err = -ENODEV;
			goto err_chrdev;
		}
	}

	err = pci_register_driver(&em8300_driver);
	if (err < 0) {
		printk(KERN_ERR "em8300: unable to register PCI driver\n");
		goto err_init;
	}

	em8300_postregister_driver();

#if defined(CONFIG_EM8300_IOCTL32) && !defined(HAVE_COMPAT_IOCTL)
	em8300_ioctl32_init();
#endif

	return 0;

 err_init:
	unregister_chrdev(major, EM8300_LOGNAME);

 err_chrdev:
	em8300_unregister_driver();
	return err;
}

module_init(em8300_init);
module_exit(em8300_exit);
