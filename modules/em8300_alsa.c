/* $Id$
 *
 * em8300_alsa.c -- alsa interface to the audio part of the em8300 chip
 * Copyright (C) 2004-2006 Nicolas Boullis <nboullis@debian.org>
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

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <linux/em8300.h>
#include <linux/pci.h>

#include "em8300_reg.h"
#include "em8300_driver.h"

typedef struct snd_em8300_pcm_indirect {
	unsigned int hw_buffer_size;    /* Byte size of hardware buffer */
	unsigned int hw_queue_size;     /* Max queue size of hw buffer (0 = buffer size) */
	unsigned int hw_data;   /* Offset to next dst (or src) in hw ring buffer */
	unsigned int hw_io;     /* Ring buffer hw pointer */
	int hw_ready;           /* Bytes ready for play (or captured) in hw ring buffer */
	unsigned int sw_buffer_size;    /* Byte size of software buffer */
	unsigned int sw_data;   /* Offset to next dst (or src) in sw ring buffer */
	unsigned int sw_io;     /* Current software pointer in bytes */
	int sw_ready;           /* Bytes ready to be transferred to/from hw */
	snd_pcm_uframes_t appl_ptr;     /* Last seen appl_ptr */
} snd_em8300_pcm_indirect_t;

typedef void (*snd_em8300_pcm_indirect_copy_t)(struct snd_pcm_substream *substream,
					       snd_em8300_pcm_indirect_t *rec, size_t bytes);

typedef struct {
	struct em8300_s *em;
	struct snd_card *card;
	struct snd_pcm *pcm_analog;
	struct snd_pcm *pcm_digital;
	struct snd_pcm_substream *substream;
	struct semaphore lock;
	snd_em8300_pcm_indirect_t indirect;
} em8300_alsa_t;

#define chip_t em8300_alsa_t

#define EM8300_ALSA_ANALOG_DEVICENUM 0
#define EM8300_ALSA_DIGITAL_DEVICENUM 1

#define EM8300_BLOCK_SIZE 4096
#define EM8300_MID_BUFFER_SIZE (1024*1024)

static struct snd_pcm_hardware snd_em8300_playback_hw = {
	.info = (
		 SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED |
//		 SNDRV_PCM_INFO_BLOCK_TRANSFER |
		 SNDRV_PCM_INFO_MMAP_VALID |
		 SNDRV_PCM_INFO_PAUSE |
		 0),
	.rates = SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
	.rate_min = 32000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = EM8300_MID_BUFFER_SIZE,
	.period_bytes_min = EM8300_BLOCK_SIZE,
	.period_bytes_max = EM8300_BLOCK_SIZE,
	.periods_min = 2,
	.periods_max = EM8300_MID_BUFFER_SIZE / EM8300_BLOCK_SIZE,
};

static int snd_em8300_playback_open(struct snd_pcm_substream *substream)
{
	em8300_alsa_t *em8300_alsa = snd_pcm_substream_chip(substream);
	struct em8300_s *em = em8300_alsa->em;
	struct snd_pcm_runtime *runtime = substream->runtime;

	down(&em8300_alsa->lock);
	if (em8300_alsa->substream) {
		up(&em8300_alsa->lock);
		printk("em8300-%d: snd_em8300_playback_open: em->audio_driver_style == NONE but em8300_alsa->substream is not NULL !?\n", em->instance);
		return -EBUSY;
	}
	em8300_alsa->substream = substream;
	up(&em8300_alsa->lock);

	if (substream->pcm->device == EM8300_ALSA_ANALOG_DEVICENUM)
		snd_em8300_playback_hw.formats = SNDRV_PCM_FMTBIT_S16_BE;
	else
		snd_em8300_playback_hw.formats = SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_BE;
	runtime->hw = snd_em8300_playback_hw;

//	printk("em8300-%d: snd_em8300_playback_open called.\n", em->instance);

	em->clockgen &= ~CLOCKGEN_OUTMASK;
	if (substream->pcm->device == EM8300_ALSA_ANALOG_DEVICENUM)
		em->clockgen |= CLOCKGEN_ANALOGOUT;
	else
		em->clockgen |= CLOCKGEN_DIGITALOUT;
	em8300_clockgen_write(em, em->clockgen);

	if (substream->pcm->device == EM8300_ALSA_ANALOG_DEVICENUM) {
		write_register(AUDIO_RATE, 0x62);
		em8300_setregblock(em, 2 * ucregister(Mute_Pattern), 0, 0x600);
	} else {
		write_register(AUDIO_RATE, 0x3a0);
	}
	write_ucregister(MA_Threshold, 6);

	return 0;
}

static int snd_em8300_playback_close(struct snd_pcm_substream *substream)
{
	em8300_alsa_t *em8300_alsa = snd_pcm_substream_chip(substream);

	em8300_alsa->substream = NULL;
//	printk("em8300-%d: snd_em8300_playback_close called.\n", em->instance);
	return 0;
}

static int snd_em8300_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
//	printk("em8300-%d: snd_em8300_pcm_hw_params called.\n", em->instance);
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
}

static int snd_em8300_pcm_hw_free(struct snd_pcm_substream *substream)
{
//	printk("em8300-%d: snd_em8300_pcm_hw_free called.\n", em->instance);
	return snd_pcm_lib_free_pages(substream);
}

static int snd_em8300_pcm_prepare(struct snd_pcm_substream *substream)
{
	em8300_alsa_t *em8300_alsa = snd_pcm_substream_chip(substream);
	struct em8300_s *em = em8300_alsa->em;
	struct snd_pcm_runtime *runtime = substream->runtime;
//	printk("em8300-%d: snd_em8300_pcm_prepare called.\n", em->instance);

	em->clockgen &= ~CLOCKGEN_SAMPFREQ_MASK;
	switch (runtime->rate) {
	case 48000:
//		printk("em8300-%d: runtime->rate set to 48000\n", em->instance);
		em->clockgen |= CLOCKGEN_SAMPFREQ_48;
		break;
	case 44100:
//		printk("em8300-%d: runtime->rate set to 44100\n", em->instance);
		em->clockgen |= CLOCKGEN_SAMPFREQ_44;
		break;
	case 32000:
//		printk("em8300-%d: runtime->rate set to 32000\n", em->instance);
		em->clockgen |= CLOCKGEN_SAMPFREQ_32;
		break;
	default:
//		printk("em8300-%d: bad runtime->rate\n", em->instance);
		em->clockgen |= CLOCKGEN_SAMPFREQ_48;
	}
	em8300_clockgen_write(em, em->clockgen);

	memset(&em8300_alsa->indirect, 0, sizeof(em8300_alsa->indirect));
	em8300_alsa->indirect.hw_buffer_size =
		(read_ucregister(MA_BuffSize_Hi) << 16)
		| read_ucregister(MA_BuffSize);
	em8300_alsa->indirect.sw_buffer_size =
		snd_pcm_lib_buffer_bytes(substream);

	write_ucregister(MA_PCIRdPtr, ucregister(MA_PCIStart) - 0x1000);
	write_ucregister(MA_PCIWrPtr, ucregister(MA_PCIStart) - 0x1000);

	return 0;
}

static int snd_em8300_pcm_ack(struct snd_pcm_substream *substream);

static int snd_em8300_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	em8300_alsa_t *em8300_alsa = snd_pcm_substream_chip(substream);
	struct em8300_s *em = em8300_alsa->em;
//	snd_pcm_runtime_t *runtime = substream->runtime;
//	printk("em8300-%d: snd_em8300_pcm_trigger(%d) called.\n", em->instance, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		em8300_alsa->indirect.hw_io =
		em8300_alsa->indirect.hw_data =
			((read_ucregister(MA_Rdptr_Hi) << 16)
			| read_ucregister(MA_Rdptr)) & ~3;
		snd_em8300_pcm_ack(substream);
		em->irqmask |= IRQSTATUS_AUDIO_FIFO;
		write_ucregister(Q_IrqMask, em->irqmask);
		mpegaudio_command(em, MACOMMAND_PLAY);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		em->irqmask &= ~IRQSTATUS_AUDIO_FIFO;
		write_ucregister(Q_IrqMask, em->irqmask);
		mpegaudio_command(em, MACOMMAND_STOP);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		mpegaudio_command(em, MACOMMAND_PAUSE);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		mpegaudio_command(em, MACOMMAND_PLAY);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}


static inline snd_pcm_uframes_t
snd_em8300_pcm_indirect_playback_pointer(struct snd_pcm_substream *substream,
					 snd_em8300_pcm_indirect_t *rec, unsigned int ptr)
{
	int bytes = ptr - rec->hw_io;
	if (bytes < 0)
		bytes += rec->hw_buffer_size;
	rec->hw_io = ptr;
	rec->hw_ready -= bytes;
	rec->sw_io += bytes;
	if (rec->sw_io >= rec->sw_buffer_size)
		rec->sw_io -= rec->sw_buffer_size;
	if (substream->ops->ack)
		substream->ops->ack(substream);
	return bytes_to_frames(substream->runtime, rec->sw_io);
}

static snd_pcm_uframes_t snd_em8300_pcm_pointer(struct snd_pcm_substream *substream)
{
//	snd_pcm_runtime_t *runtime = substream->runtime;
	em8300_alsa_t *em8300_alsa = snd_pcm_substream_chip(substream);
	struct em8300_s *em = em8300_alsa->em;
	unsigned int hw_ptr =
		((read_ucregister(MA_Rdptr_Hi) << 16)
		 | read_ucregister(MA_Rdptr)) & ~3;
//	snd_pcm_uframes_t ret = snd_pcm_indirect_playback_pointer(substream,
//								  &em8300_alsa->indirect,
//								  hw_ptr);
//	printk("em8300-%d: snd_em8300_pcm_pointer called: %d\n", em->instance, ret);
//	return ret;
	return snd_em8300_pcm_indirect_playback_pointer(substream,
							&em8300_alsa->indirect,
							hw_ptr);
}

static void snd_em8300_pcm_trans_dma(struct snd_pcm_substream *substream,
				     snd_em8300_pcm_indirect_t *rec,
				     size_t bytes)
{
//	snd_pcm_runtime_t *runtime = substream->runtime;
	em8300_alsa_t *em8300_alsa = snd_pcm_substream_chip(substream);
	struct em8300_s *em = em8300_alsa->em;
	int writeindex = ((int)read_ucregister(MA_PCIWrPtr) - (ucregister(MA_PCIStart) - 0x1000)) / 3;
	int readindex = ((int)read_ucregister(MA_PCIRdPtr) - (ucregister(MA_PCIStart) - 0x1000)) / 3;
	writel((unsigned long int)(substream->runtime->dma_addr + rec->sw_data) >> 16,
	       ((uint32_t *)ucregister_ptr(MA_PCIStart))+3*writeindex);
	writel((unsigned long int)(substream->runtime->dma_addr + rec->sw_data) & 0xffff,
	       ((uint32_t *)ucregister_ptr(MA_PCIStart))+3*writeindex+1);
	writel(bytes,
	       ((uint32_t *)ucregister_ptr(MA_PCIStart))+3*writeindex+2);
	writeindex += 1;
	writeindex %= read_ucregister(MA_PCISize) / 3;
//	printk("em8300-%d: snd_em8300_pcm_trans_dma(%d) called.\n", em->instance, bytes);
	if (readindex != writeindex)
		write_ucregister(MA_PCIWrPtr, ucregister(MA_PCIStart) - 0x1000 + writeindex * 3);
	else
		printk("em8300-%d: snd_em8300_pcm_trans_dma failed.\n", em->instance);
}

static inline void
snd_em8300_pcm_indirect_playback_transfer(struct snd_pcm_substream *substream,
					  snd_em8300_pcm_indirect_t *rec,
					  snd_em8300_pcm_indirect_copy_t copy)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_pcm_uframes_t appl_ptr = runtime->control->appl_ptr;
	snd_pcm_sframes_t diff = appl_ptr - rec->appl_ptr;
	int qsize;

	if (diff) {
		if (diff < -(snd_pcm_sframes_t) (runtime->boundary / 2))
			diff += runtime->boundary;
		rec->sw_ready += (int)frames_to_bytes(runtime, diff);
		rec->appl_ptr = appl_ptr;
	}
	qsize = rec->hw_queue_size ? rec->hw_queue_size : rec->hw_buffer_size;
	while (rec->hw_ready < qsize - 4096 && rec->sw_ready > 0) {
		unsigned int sw_to_end = rec->sw_buffer_size - rec->sw_data;
		unsigned int bytes = qsize - rec->hw_ready;
		if (rec->sw_ready < (int)bytes)
			bytes = rec->sw_ready;
		if (sw_to_end < bytes)
			bytes = sw_to_end;
		if (8192 < bytes)
			bytes = 8192;
		if (!bytes)
			break;
		copy(substream, rec, bytes);
		rec->hw_data += bytes;
		if (rec->hw_data == rec->hw_buffer_size)
			rec->hw_data = 0;
		rec->sw_data += bytes;
		if (rec->sw_data == rec->sw_buffer_size)
			rec->sw_data = 0;
		rec->hw_ready += bytes;
		rec->sw_ready -= bytes;
	}
}

static int snd_em8300_pcm_ack(struct snd_pcm_substream *substream)
{
	em8300_alsa_t *em8300_alsa = snd_pcm_substream_chip(substream);
//	printk("em8300-%d: snd_em8300_pcm_ack called.\n", em->instance);
	snd_em8300_pcm_indirect_playback_transfer(substream, &em8300_alsa->indirect,
						  snd_em8300_pcm_trans_dma);
	return 0;
}

static struct snd_pcm_ops snd_em8300_playback_ops = {
	.open =		snd_em8300_playback_open,
	.close =	snd_em8300_playback_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	snd_em8300_pcm_hw_params,
	.hw_free =	snd_em8300_pcm_hw_free,
	.prepare =	snd_em8300_pcm_prepare,
	.trigger =	snd_em8300_pcm_trigger,
	.pointer =	snd_em8300_pcm_pointer,
	.ack =		snd_em8300_pcm_ack,
};

static void snd_em8300_pcm_analog_free(struct snd_pcm *pcm)
{
	em8300_alsa_t *em8300_alsa = (em8300_alsa_t *)(pcm->private_data);
	em8300_alsa->pcm_analog = NULL;
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int snd_em8300_pcm_analog(em8300_alsa_t *em8300_alsa)
{
	struct em8300_s *em = em8300_alsa->em;
	struct snd_pcm *pcm;
	int ret;

	ret = snd_pcm_new(em8300_alsa->card, "EM8300 PCM Analog",
			EM8300_ALSA_ANALOG_DEVICENUM,
			1, /* 1 playback substream */
			0, /* 0 capture substream */
			&pcm);

	if (ret) {
		printk(KERN_ERR "em8300-alsa: snd_em8300_pcm_analog failed with err %d\n", ret);
		return ret;
	}

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_em8300_playback_ops);

	pcm->private_data = em8300_alsa;
	pcm->private_free = snd_em8300_pcm_analog_free;
	//	pcm->info_flags = 0;

	strcpy(pcm->name, "EM8300 DAC");

	em8300_alsa->pcm_analog = pcm;

	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
					      snd_dma_pci_data(em->pci_dev),
					      0,
					      EM8300_MID_BUFFER_SIZE);

	return 0;
}

static void snd_em8300_pcm_digital_free(struct snd_pcm *pcm)
{
	em8300_alsa_t *em8300_alsa = (em8300_alsa_t *)(pcm->private_data);
	em8300_alsa->pcm_digital = NULL;
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int snd_em8300_pcm_digital(em8300_alsa_t *em8300_alsa)
{
	struct em8300_s *em = em8300_alsa->em;
	struct snd_pcm *pcm;
	int ret;

	ret = snd_pcm_new(em8300_alsa->card, "EM8300 PCM Digital",
			EM8300_ALSA_DIGITAL_DEVICENUM,
			1, /* 1 playback substream */
			0, /* 0 capture substream */
			&pcm);

	if (ret) {
		printk(KERN_ERR "em8300-alsa: snd_em8300_pcm_digital failed with err %d\n", ret);
		return ret;
	}

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_em8300_playback_ops);

	pcm->private_data = em8300_alsa;
	pcm->private_free = snd_em8300_pcm_digital_free;
	//	pcm->info_flags = 0;

	strcpy(pcm->name, "EM8300 IEC958");

	em8300_alsa->pcm_digital = pcm;

	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
					      snd_dma_pci_data(em->pci_dev),
					      0,
					      EM8300_MID_BUFFER_SIZE);

	return 0;
}

static int snd_em8300_free(em8300_alsa_t *em8300_alsa)
{
	kfree(em8300_alsa);
	return 0;
}

static int snd_em8300_dev_free(struct snd_device *device)
{
	em8300_alsa_t *em8300_alsa = (em8300_alsa_t *)(device->device_data);
	return snd_em8300_free(em8300_alsa);
}

static int snd_em8300_create(struct snd_card *card, struct em8300_s *em, em8300_alsa_t **rem8300_alsa)
{
	em8300_alsa_t *em8300_alsa;
	int err;
	static struct snd_device_ops ops = {
		.dev_free = snd_em8300_dev_free,
	};

	if (rem8300_alsa)
		*rem8300_alsa = NULL;

	em8300_alsa = (em8300_alsa_t *)kmalloc(sizeof(em8300_alsa_t), GFP_KERNEL);
	if (em8300_alsa == NULL)
		return -ENOMEM;

	memset(em8300_alsa, 0, sizeof(em8300_alsa_t));

	sema_init(&em8300_alsa->lock, 1);

	em8300_alsa->em = em;
	em8300_alsa->card = card;

	if ((err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, em8300_alsa, &ops)) < 0) {
		snd_em8300_free(em8300_alsa);
		return err;
	}

	snd_card_set_dev(card, &em->pci_dev->dev);

	*rem8300_alsa = em8300_alsa;
	return 0;
}

void em8300_alsa_enable_card(struct em8300_s *em)
{
	struct snd_card *card;
	em8300_alsa_t *em8300_alsa;
	int err;

	em->alsa_card = NULL;

	if ((err = snd_card_create(SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1, THIS_MODULE, 0, &card)) < 0)
		return;

	if ((err = snd_em8300_create(card, em, &em8300_alsa)) < 0) {
		snd_card_free(card);
		return;
	}

	card->private_data = em8300_alsa;

	if ((err = snd_em8300_pcm_analog(em8300_alsa)) < 0) {
		snd_card_free(card);
		return;
	}

	if ((err = snd_em8300_pcm_digital(em8300_alsa)) < 0) {
		snd_card_free(card);
		return;
	}

	strcpy(card->driver, "EM8300");
	strcpy(card->shortname, "Sigma Designs' EM8300");
	sprintf(card->longname, "%s at %#lx irq %d",
		card->shortname, (unsigned long int)em->mem, em->pci_dev->irq);

	if ((err = snd_card_register(card)) < 0) {
		snd_card_free(card);
		return;
	}

	em->alsa_card = card;
}

void em8300_alsa_disable_card(struct em8300_s *em)
{
	if (em->alsa_card)
		snd_card_free(em->alsa_card);
}

void em8300_alsa_audio_interrupt(struct em8300_s *em)
{
	if (em->alsa_card) {
		em8300_alsa_t *em8300_alsa = (em8300_alsa_t *)(em->alsa_card->private_data);
		if (em8300_alsa->substream) {
//			printk("em8300-%d: calling snd_pcm_period_elapsed\n", em->instance);
			snd_pcm_period_elapsed(em8300_alsa->substream);
		}
	}
}
