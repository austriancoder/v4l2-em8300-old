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
#include <linux/pci.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include <linux/interrupt.h>


#include "em8300_reg.h"
#include <linux/em8300.h>
#include "em8300_driver.h"
#include "em8300_fifo.h"
#include "em8300_params.h"
#include "em8300_models.h"
#include "em8300_version.h"

int em8300_debug;

#if !defined(CONFIG_I2C_ALGOBIT) && !defined(CONFIG_I2C_ALGOBIT_MODULE)
#error "This needs the I2C Bit Banging Interface in your Kernel"
#endif

MODULE_AUTHOR("Henrik Johansson <henrikjo@post.utfors.se>");
MODULE_DESCRIPTION("EM8300 MPEG-2 decoder");
MODULE_SUPPORTED_DEVICE("em8300");
MODULE_LICENSE("GPL");

static atomic_t em8300_instance = ATOMIC_INIT(0);

static DEFINE_PCI_DEVICE_TABLE(em8300_ids) = {
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
		write_register(INTERRUPT_ACK, 2);

		write_ucregister(Q_IrqStatus, 0x8000);

		if (irqstatus & IRQSTATUS_VIDEO_FIFO)
			em8300_fifo_check(em->mvfifo);

		if (irqstatus & IRQSTATUS_AUDIO_FIFO)
			em8300_alsa_audio_interrupt(em);

		if (irqstatus & IRQSTATUS_VIDEO_VBL) {
			em8300_fifo_check(em->spfifo);
			em8300_video_check_ptsfifo(em);
			em8300_spu_check_ptsfifo(em);

			do_gettimeofday(&tv);
			em->irqtimediff = TIMEDIFF(tv, em->tv);
			em->tv = tv;
			em->irqcount++;
			wake_up(&em->vbi_wait);
		}

		write_ucregister(Q_IrqMask, em->irqmask);
		write_ucregister(Q_IrqStatus, 0x0000);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static void release_em8300(struct em8300_s *em)
{
	v4l2_subdev_call(em->encoder, core, s_power, 0);

#ifdef CONFIG_MTRR
	if (em->mtrr_reg)
		mtrr_del(em->mtrr_reg, em->addr, em->memsize);
#endif

	em8300_i2c_exit(em);

	write_ucregister(Q_IrqMask, 0);
	write_ucregister(Q_IrqStatus, 0);
	write_register(RESET, 0);

	em8300_fifo_free(em->mvfifo);
	em8300_fifo_free(em->spfifo);

	em8300_alsa_disable_card(em);

	/* free it */
	free_irq(em->pci_dev->irq, em);

	/* unmap and free memory */
	iounmap((unsigned *) em->mem);

	video_unregister_device(em->vdev);
	v4l2_device_unregister(&em->v4l2_dev);
	kfree(em);
}

static int init_em8300(struct em8300_s *em)
{
	int identified_model;

	write_register(0x30000, read_register(0x30000));

	write_register(0x1f50, 0x123);

	if (read_register(0x1f50) == 0x123)
		em->chip_revision = 2;
	else
		em->chip_revision = 1;

	em8300_i2c_init(em);

	identified_model = identify_model(em);

	if (em->model == -1) {
		if (identified_model > 0) {
			em->model = identified_model;
			pr_info("em8300-%d: detected card: %s.\n", em->instance,
			       known_models[identified_model].name);
		} else {
			em->model = 0;
			printk(KERN_ERR "em8300-%d: unable to identify model...\n", em->instance);
		}
	}

	if ((em->model != identified_model) && (em->model > 0) && (identified_model > 0))
		printk(KERN_WARNING "em8300-%d: mismatch between detected and requested model.\n", em->instance);

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
	if (dicom_other_pal[em->instance] >= 0)
		em->config.model.dicom_other_pal =
			dicom_other_pal[em->instance];
	if (dicom_fix[em->instance] >= 0)
		em->config.model.dicom_fix =
			dicom_fix[em->instance];
	if (dicom_control[em->instance] >= 0)
		em->config.model.dicom_control =
			dicom_control[em->instance];
	if (bt865_ucode_timeout[em->instance] >= 0)
		em->config.model.bt865_ucode_timeout =
			bt865_ucode_timeout[em->instance];
	if (activate_loopback[em->instance] >= 0)
		em->config.model.activate_loopback =
			activate_loopback[em->instance];

	pr_info("em8300-%d: Chip revision: %d\n", em->instance, em->chip_revision);
	pr_debug("em8300-%d: use_bt865: %d\n", em->instance, em->config.model.use_bt865);

	em8300_i2c_register_encoder(em);

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

	pr_debug("em8300-%d: activate_loopback: %d\n", em->instance, em->config.model.activate_loopback);

	return 0;
}

static int em8300_pci_setup(struct em8300_s *em, struct pci_dev *pdev,
		  const struct pci_device_id *pci_id)
{
	unsigned char pci_latency;

	EM8300_DEBUG_INFO("Enabling pci device\n");

	if (pci_enable_device(pdev)) {
		EM8300_ERR("Can't enable device!\n");
		return -EIO;
	}

	em->addr = pci_resource_start(pdev, 0);
	em->memsize = pci_resource_len(pdev, 0);

	if (!request_mem_region(em->addr, em->memsize, "em8300 decoder")) {
		EM8300_ERR("Cannot request decoder memory region.\n");
		return -EIO;
	}

	pci_read_config_byte(pdev, PCI_LATENCY_TIMER, &pci_latency);

	EM8300_DEBUG_INFO("%d (rev %d) at %02x:%02x.%x, "
		   "irq: %d, latency: %d, memory: 0x%llx\n",
		   pdev->device, pdev->revision, pdev->bus->number,
		   PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn),
		   pdev->irq, pci_latency, (u64)em->addr);

	return 0;
}

static int __devinit em8300_probe(struct pci_dev *pdev,
				  const struct pci_device_id *pci_id)
{
	struct em8300_s *em;
	int retval;

	em = kzalloc(sizeof(struct em8300_s), GFP_KERNEL);
	if (em == NULL)
		return -ENOMEM;

	em->pci_dev = pdev;
	em->instance = v4l2_device_set_name(&em->v4l2_dev, "em8300",
											&em8300_instance);

	retval = v4l2_device_register(&pdev->dev, &em->v4l2_dev);
	if (retval) {
		kfree(em);
		return retval;
	}

	EM8300_INFO("Initializing card %d\n", em->instance);

	/* PCI Device Setup */
	retval = em8300_pci_setup(em, pdev, pci_id);
	if (retval != 0)
		goto mem_free;

	/* setup video_device */
	em8300_register_video(em);

	/* Specify default values if card is not identified */
	memset(&em->config, 0, sizeof(struct em8300_config_s));
	em->config.adv717x_model.pixeldata_adjust_ntsc = 1;
	em->config.adv717x_model.pixeldata_adjust_pal = 1;

	em->model = card_model[atomic_read(&em8300_instance)];

	/* map io memory */
	em->mem = ioremap_nocache(em->addr, em->memsize);
	if (em->mem == NULL) {
		EM8300_ERR("ioremap for memory region failed\n");
		retval = -ENOMEM;
		goto mem_free;
	}

	EM8300_INFO("mapped-memory at 0x%p\n", em->mem);
#ifdef CONFIG_MTRR
	em->mtrr_reg = mtrr_add(em->addr, em->memsize, MTRR_TYPE_UNCACHABLE, 1);
	if (em->mtrr_reg)
		EM8300_INFO("using MTRR\n");
#endif

	init_waitqueue_head(&em->video_ptsfifo_wait);
	init_waitqueue_head(&em->vbi_wait);
	init_waitqueue_head(&em->sp_ptsfifo_wait);

	retval = request_irq(pdev->irq, em8300_irq,
						IRQF_SHARED | IRQF_DISABLED,
						em->v4l2_dev.name, (void *)em);

	if (retval == -EINVAL) {
		printk(KERN_ERR "em8300-%d: Bad irq number or handler\n", em->instance);
		goto irq_error;
	}

	init_em8300(em);

	/* load fw */
	if (!em8300_require_ucode(em)) {
		retval = -ENOTTY;
		goto irq_error;
	}

	return 0;

irq_error:
#ifdef CONFIG_MTRR
	if (em->mtrr_reg)
		mtrr_del(em->mtrr_reg, em->addr, em->memsize);
#endif
	iounmap(em->mem);

mem_free:
	v4l2_device_unregister(&em->v4l2_dev);
	kfree(em);
	return retval;
}

static void __devexit em8300_remove(struct pci_dev *pci_dev)
{
	struct em8300_s *em = pci_get_drvdata(pci_dev);

	if (em)
		release_em8300(em);

	pci_set_drvdata(pci_dev, NULL);
	pci_disable_device(pci_dev);
}

struct pci_driver em8300_driver = {
	.name     = "Sigma Designs EM8300",
	.id_table = em8300_ids,
	.probe    = em8300_probe,
	.remove   = em8300_remove,
};

static int __init module_start(void)
{
	if (pci_register_driver(&em8300_driver)) {
		printk(KERN_ERR "em8300: Error detecting PCI card\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit module_cleanup(void)
{
	pci_unregister_driver(&em8300_driver);
}

module_init(module_start);
module_exit(module_cleanup);
