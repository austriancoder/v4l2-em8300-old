/*
 * em8300_audio.c
 *
 * Copyright (C) 2000 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2000 Ze'ev Maor <zeev@users.sourceforge.net>
 *           (C) 2001 Rick Haines <rick@kuroyi.net>
 *           (C) 2001 Mattias Svensson
 *           (C) 2001-2002 Steven Brookes <stevenjb@mda.co.uk>
 *           (C) 2002 Daniel Holm <mswitch@users.sourceforge.net>
 *           (C) 2002 Michael Roitzsch <mroi@users.sourceforge.net>
 *           (C) 2003-2008 Nicolas Boullis <nboullis@debian.org>
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

#define __NO_VERSION__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/soundcard.h>
#include <linux/types.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include "em8300_reg.h"
#include <linux/em8300.h>
#include "em8300_driver.h"
#include "em8300_fifo.h"

#ifndef AFMT_AC3
#define AFMT_AC3 0x00000400
#endif

#include <asm/byteorder.h>

int em8300_audio_calcbuffered(struct em8300_s *em);
static int set_audiomode(struct em8300_s *em, int mode);

#define SPDIF_PREAMBLE_B 0
#define SPDIF_PREAMBLE_M 2
#define SPDIF_PREAMBLE_W 1

static void sub_prepare_SPDIF(struct em8300_s *em, uint32_t *out,
			      uint16_t *in, unsigned int length)
{
	int i;
	int mute = in == NULL;

	for (i = 0; i < length; i++) {
		unsigned int in_value;
		unsigned int out_value;
		int channel_status_bit = (em->channel_status[em->channel_status_pos/8] & (0x80 >> (em->channel_status_pos%8))) != 0;
		in_value = mute ? 0 : __be16_to_cpu(*(in++));
		out_value = ((em->channel_status_pos == 0) ? SPDIF_PREAMBLE_B : SPDIF_PREAMBLE_M)
			| (in_value << 12)
			| (channel_status_bit ? 0x40000000UL : 0x00000000UL);
		*(out++) = __cpu_to_be32(out_value);
		in_value = mute ? 0:__be16_to_cpu(*(in++));
		out_value = SPDIF_PREAMBLE_W
			| (in_value << 12)
			| (channel_status_bit ? 0x40000000UL : 0x00000000UL);
		*(out++) = __cpu_to_be32(out_value);
		em->channel_status_pos++;
		em->channel_status_pos %= 192;
	}
}

static void preprocess_analog(struct em8300_s *em, unsigned char *outbuf, const unsigned char *inbuf_user, int inlength)
{
	int i;

	if (em->audio.format == AFMT_S16_LE) {
		if (em->audio.channels == 2) {
			for (i = 0; i < inlength; i += 4) {
				get_user(outbuf[i + 3], inbuf_user++);
				get_user(outbuf[i + 2], inbuf_user++);
				get_user(outbuf[i + 1], inbuf_user++);
				get_user(outbuf[i + 0], inbuf_user++);
			}
		} else {
			for (i = 0; i < inlength; i += 2) {
				get_user(outbuf[2 * i + 1], inbuf_user++);
				get_user(outbuf[2 * i + 0], inbuf_user++);
				outbuf[2 * i + 3] = outbuf[2 * i + 1];
				outbuf[2 * i + 2] = outbuf[2 * i + 0];
			}
		}
	} else {
		if (em->audio.channels == 2) {
			for (i = 0; i < inlength; i += 4) {
				get_user(outbuf[i + 2], inbuf_user++);
				get_user(outbuf[i + 3], inbuf_user++);
				get_user(outbuf[i + 0], inbuf_user++);
				get_user(outbuf[i + 1], inbuf_user++);
			}
		} else {
			for (i = 0; i < inlength; i += 2) {
				get_user(outbuf[2 * i + 0], inbuf_user++);
				get_user(outbuf[2 * i + 1], inbuf_user++);
				outbuf[2 * i + 2] = outbuf[2 * i + 0];
				outbuf[2 * i + 3] = outbuf[2 * i + 1];
			}
		}
	}
}

static void preprocess_digital(struct em8300_s *em, unsigned char *outbuf,
			       const unsigned char *inbuf_user, int inlength)
{
	int i;

	if (!em->mafifo->preprocess_buffer)
		return;

        if (em->audio.format == AFMT_S16_LE ||
	    em->audio.format == AFMT_AC3) {
		if (em->audio.channels == 2) {
			for (i = 0; i < inlength; i += 2) {
				get_user(em->mafifo->preprocess_buffer[i + 1], inbuf_user++);
				get_user(em->mafifo->preprocess_buffer[i + 0], inbuf_user++);
			}
		} else {
			for (i = 0; i < inlength; i += 2) {
				get_user(em->mafifo->preprocess_buffer[2 * i + 1], inbuf_user++);
				get_user(em->mafifo->preprocess_buffer[2 * i + 0], inbuf_user++);
				em->mafifo->preprocess_buffer[2 * i + 3] = em->mafifo->preprocess_buffer[2 * i + 1];
				em->mafifo->preprocess_buffer[2 * i + 2] = em->mafifo->preprocess_buffer[2 * i + 0];
			}
			inlength *= 2; /* ensure correct size for sub_prepare_SPDIF */
		}
	} else {
		if (em->audio.channels == 2) {
			(void)copy_from_user(em->mafifo->preprocess_buffer, inbuf_user, inlength);
		} else {
			for (i = 0; i < inlength; i += 2) {
				get_user(em->mafifo->preprocess_buffer[2 * i + 0], inbuf_user++);
				get_user(em->mafifo->preprocess_buffer[2 * i + 1], inbuf_user++);
				em->mafifo->preprocess_buffer[2 * i + 2] = em->mafifo->preprocess_buffer[2 * i + 0];
				em->mafifo->preprocess_buffer[2 * i + 3] = em->mafifo->preprocess_buffer[2 * i + 1];
			}
			inlength *= 2; /* ensure correct size for sub_prepare_SPDIF */
		}
	}

	sub_prepare_SPDIF(em, (uint32_t *)outbuf, (uint16_t *)em->mafifo->preprocess_buffer, inlength/4);
}

static void setup_mafifo(struct em8300_s *em)
{
	if (em->audio_mode == EM8300_AUDIOMODE_ANALOG) {
		em->mafifo->preprocess_ratio = ((em->audio.channels == 2) ? 1 : 2);
		em->mafifo->preprocess_cb = &preprocess_analog;
	} else {
		em->mafifo->preprocess_ratio = ((em->audio.channels == 2) ? 2 : 4);
		em->mafifo->preprocess_cb = &preprocess_digital;
	}
}

int mpegaudio_command(struct em8300_s *em, int cmd)
{
	em8300_waitfor(em, ucregister(MA_Command), 0xffff, 0xffff);

	pr_debug("em8300-%d: MA_Command: %d\n", em->card_nr, cmd);
	write_ucregister(MA_Command, cmd);

	return em8300_waitfor(em, ucregister(MA_Status), cmd, 0xffff);
}

static int audio_start(struct em8300_s *em)
{
	em->irqmask |= IRQSTATUS_AUDIO_FIFO;
	write_ucregister(Q_IrqMask, em->irqmask);
	em->audio.enable_bits = PCM_ENABLE_OUTPUT;
	return mpegaudio_command(em, MACOMMAND_PLAY);
}

static int audio_stop(struct em8300_s *em)
{
	em->irqmask &= ~IRQSTATUS_AUDIO_FIFO;
	write_ucregister(Q_IrqMask, em->irqmask);
	em->audio.enable_bits = 0;
	return mpegaudio_command(em, MACOMMAND_STOP);
}

static int set_speed(struct em8300_s *em, int speed)
{
	em->clockgen &= ~CLOCKGEN_SAMPFREQ_MASK;

	switch (speed) {
	case 48000:
		em->clockgen |= CLOCKGEN_SAMPFREQ_48;
		break;
	case 44100:
		em->clockgen |= CLOCKGEN_SAMPFREQ_44;
		break;
	case 66000:
		em->clockgen |= CLOCKGEN_SAMPFREQ_66;
		break;
	case 32000:
		em->clockgen |= CLOCKGEN_SAMPFREQ_32;
		break;
	default:
		em->clockgen |= CLOCKGEN_SAMPFREQ_48;
		speed = 48000;
	}

	em->audio.speed = speed;

	em8300_clockgen_write(em, em->clockgen);

	return speed;
}

static int set_channels(struct em8300_s *em, int val)
{
	if (val > 2) val = 2;
	em->audio.channels = val;
	setup_mafifo(em);

	return val;
}

static int set_format(struct em8300_s *em, int fmt)
{
	if (fmt != AFMT_QUERY) {
		switch (fmt) {
#ifdef AFMT_AC3
		case AFMT_AC3:
			if (em->audio_mode != EM8300_AUDIOMODE_DIGITALAC3) {
				set_speed(em, 48000);
				set_audiomode(em, EM8300_AUDIOMODE_DIGITALAC3);
				setup_mafifo(em);
			}
			em->audio.format = fmt;
			break;
#endif
		case AFMT_S16_BE:
		case AFMT_S16_LE:
			if (em->audio_mode == EM8300_AUDIOMODE_DIGITALAC3) {
				set_audiomode(em, em->pcm_mode);
				setup_mafifo(em);
			}
			em->audio.format = fmt;
			break;
		default:
			if (em->audio_mode == EM8300_AUDIOMODE_DIGITALAC3) {
				set_audiomode(em, em->pcm_mode);
				setup_mafifo(em);
			}
			fmt = AFMT_S16_BE;
			break;
		}
	}
	return em->audio.format;
}

int em8300_audio_ioctl(struct em8300_s *em, unsigned int cmd, unsigned long arg)
{
	int len = 0;
	int val = 0;

	if (_SIOC_DIR(cmd) != _SIOC_NONE && _SIOC_DIR(cmd) != 0) {
		/*
		 * Have to validate the address given by the process.
		 */
		len = _SIOC_SIZE(cmd);
		if (len < 1 || len > 65536 || arg == 0) {
			return -EFAULT;
		}
		if (_SIOC_DIR(cmd) & _SIOC_WRITE) {
			if (!access_ok(VERIFY_READ, (void *) arg, len)) {
				return -EFAULT;
			}
		}
		if (_SIOC_DIR(cmd) & _SIOC_READ) {
			if (!access_ok(VERIFY_WRITE, (void *) arg, len)) {
				return -EFAULT;
			}
		}
	}

	switch (cmd) {
	case SNDCTL_DSP_RESET: /* reset device */
		pr_debug("em8300-%d: SNDCTL_DSP_RESET\n", em->card_nr);
		em8300_audio_flush(em);
		return 0;

	case SNDCTL_DSP_SYNC:  /* wait until last byte is played and reset device */
		pr_debug("em8300-%d: SNDCTL_DSP_SYNC\n", em->card_nr);
		em8300_fifo_sync(em->mafifo);
		return 0;

	case SNDCTL_DSP_SPEED: /* set sample rate */
		if (get_user(val, (int *) arg)) {
			return -EFAULT;
		}
		pr_debug("em8300-%d: SNDCTL_DSP_SPEED %i ", em->card_nr, val);
		val = set_speed(em, val);
		pr_debug("%i\n", val);
		break;

	case SOUND_PCM_READ_RATE: /* read sample rate */
		pr_debug("em8300-%d: SNDCTL_DSP_RATE %i ", em->card_nr, val);
		val = em->audio.speed;
		pr_debug("%i\n", val);
		break;

	case SNDCTL_DSP_STEREO: /* set stereo or mono mode */
		if (get_user(val, (int *) arg)) {
			return -EFAULT;
		}
		if (val > 1 || val < 0) {
			return -EINVAL;
		}
		pr_debug("em8300-%d: SNDCTL_DSP_STEREO %i\n", em->card_nr, val);
		set_channels(em, val + 1);
		break;

	case SNDCTL_DSP_GETBLKSIZE: /* get fragment size */
		val = em->audio.slotsize;
		pr_debug("em8300-%d: SNDCTL_DSP_GETBLKSIZE %i\n", em->card_nr, val);
		break;

	case SNDCTL_DSP_CHANNELS: /* set number of channels */
		if (get_user(val, (int *) arg)) {
			return -EFAULT;
		}
		if (val > 2 || val < 1) {
			return -EINVAL;
		}
		pr_debug("em8300-%d: SNDCTL_DSP_CHANNELS %i\n", em->card_nr, val);
		set_channels(em, val);
		break;

	case SOUND_PCM_READ_CHANNELS: /* read number of channels */
		val = em->audio.channels;
		pr_debug("em8300-%d: SOUND_PCM_READ_CHANNELS %i\n", em->card_nr, val);
		break;

	case SNDCTL_DSP_POST: /* "there is likely to be a pause in the output" */
		pr_debug("em8300-%d: SNDCTL_DSP_POST\n", em->card_nr);
		pr_debug("em8300-%d: SNDCTL_DSP_GETPOST not implemented yet\n", em->card_nr);
		return -ENOSYS;
		break;

	case SNDCTL_DSP_SETFRAGMENT: /* set fragment size */
		pr_debug("em8300-%d: SNDCTL_DSP_SETFRAGMENT %i\n", em->card_nr, val);
		pr_debug("em8300-%d: SNDCTL_DSP_SETFRAGMENT not supported by hardware!\n", em->card_nr);
		break;

	case SNDCTL_DSP_GETFMTS: /* get possible formats */
#ifdef AFMT_AC3
		val = AFMT_AC3 | AFMT_S16_BE | AFMT_S16_LE;
#else
		val = AFMT_S16_BE | AFMT_S16_LE;
#endif
		pr_debug("em8300-%d: SNDCTL_DSP_GETFMTS\n", em->card_nr);
		break;

	case SNDCTL_DSP_SETFMT: /* set sample format */
		if (get_user(val, (int *) arg)) {
			return -EFAULT;
		}
		pr_debug("em8300-%d: SNDCTL_DSP_SETFMT %i ", em->card_nr, val);
		val = set_format(em, val);
		pr_debug("%i\n", val);
		break;

	case SOUND_PCM_READ_BITS: /* read sample format */
		val = em->audio.format;
		pr_debug("em8300-%d: SOUND_PCM_READ_BITS\n", em->card_nr);
		break;

	case SNDCTL_DSP_GETOSPACE:
	{
		audio_buf_info buf_info;
		switch (em->audio_mode) {
			case EM8300_AUDIOMODE_ANALOG:
				buf_info.fragments =
					em8300_fifo_freeslots(em->mafifo) -
					em->mafifo->nslots / 2;
				break;
			default:
				buf_info.fragments =
					em8300_fifo_freeslots(em->mafifo) / 2;
				break;
		}
		buf_info.fragments = (buf_info.fragments > 0) ? buf_info.fragments : 0;
		buf_info.fragstotal = em->mafifo->nslots / 2;
		buf_info.fragsize = em->audio.slotsize;
		buf_info.bytes = em->mafifo->nslots * em->audio.slotsize / 2;
		pr_debug("em8300-%d: SNDCTL_DSP_GETOSPACE\n", em->card_nr);
		if (copy_to_user((void *) arg, &buf_info, sizeof(audio_buf_info)))
			return -EFAULT;
		return 0;
	}

	case SNDCTL_DSP_GETISPACE:
		pr_debug("em8300-%d: SNDCTL_DSP_GETISPACE\n", em->card_nr);
		return -ENOSYS;
		break;

	case SNDCTL_DSP_GETCAPS:
		val = DSP_CAP_REALTIME | DSP_CAP_BATCH | DSP_CAP_TRIGGER;
		pr_debug("em8300-%d: SNDCTL_DSP_GETCAPS\n", em->card_nr);
		break;

	case SNDCTL_DSP_GETTRIGGER:
		val = em->audio.enable_bits;
		pr_debug("em8300-%d: SNDCTL_DSP_GETTRIGGER\n", em->card_nr);
		break;

	case SNDCTL_DSP_SETTRIGGER:
		if (val & PCM_ENABLE_OUTPUT) {
			if (em->audio.enable_bits & PCM_ENABLE_OUTPUT) {
				em->audio.enable_bits |= PCM_ENABLE_OUTPUT;
				mpegaudio_command(em, MACOMMAND_PLAY);
			}
		}
		pr_debug("em8300-%d: SNDCTL_DSP_SETTRIGGER\n", em->card_nr);
		pr_info("em8300-%d: SNDCTL_DSP_SETTRIGGER not implemented properly yet\n", em->card_nr);
		break;

	case SNDCTL_DSP_GETIPTR:
		pr_debug("em8300-%d: SNDCTL_DSP_GETIPTR\n", em->card_nr);
		return -ENOSYS;
		break;

	case SNDCTL_DSP_GETOPTR:
	{
		count_info ci;
		ci.bytes = em->mafifo->bytes - em8300_audio_calcbuffered(em);
		if (ci.bytes < 0) ci.bytes = 0;
		ci.blocks = 0;
		ci.ptr = 0;
		pr_debug("em8300-%d: SNDCTL_DSP_GETOPTR %i\n", em->card_nr, ci.bytes);
		if (copy_to_user((void *) arg, &ci, sizeof(count_info)))
			return -EFAULT;
		return 0;
	}
	case SNDCTL_DSP_GETODELAY:
		val = em8300_audio_calcbuffered(em);
		pr_debug("em8300-%d: SNDCTL_DSP_GETODELAY %i\n", em->card_nr, val);
		break;

	default:
		pr_info("em8300-%d: unknown audio ioctl called\n", em->card_nr);
		return -EINVAL;
	}

	return put_user(val, (int *) arg);
}

int em8300_audio_flush(struct em8300_s *em)
{
	int pcirdptr = read_ucregister(MA_PCIRdPtr);
	write_ucregister(MA_PCIWrPtr, pcirdptr);
	writel(readl(em->mafifo->readptr), em->mafifo->writeptr);
	em8300_fifo_sync(em->mafifo);
	return 0;
}

int em8300_audio_open(struct em8300_s *em)
{
	em8300_require_ucode(em);

	if (!em->ucodeloaded) {
		return -ENODEV;
	}

	set_speed(em, em->audio.speed);
	set_audiomode(em, em->audio_mode);

	em->mafifo->bytes = 0;

	return audio_start(em);
}

int em8300_audio_release(struct em8300_s *em)
{
	em8300_fifo_sync(em->mafifo);
	em8300_audio_flush(em);
	return audio_stop(em);
}

static int set_audiomode(struct em8300_s *em, int mode)
{
	em->audio_mode = mode;

	if (em->audio_driver_style != OSS)
		return 0;

	em->clockgen &= ~CLOCKGEN_OUTMASK;

	if (em->audio_mode == EM8300_AUDIOMODE_ANALOG) {
		em->clockgen |= CLOCKGEN_ANALOGOUT;
	} else {
		em->clockgen |= CLOCKGEN_DIGITALOUT;
	}

	em8300_clockgen_write(em, em->clockgen);

	em->channel_status_pos = 0;
	memset(em->channel_status, 0, sizeof(em->channel_status));

	em->channel_status[1] = 0x98;

	switch (em->audio.speed) {
	case 32000:
		em->channel_status[3] = 0xc0;
		break;
	case 44100:
		em->channel_status[3] = 0;
		break;
	case 48000:
		em->channel_status[3] = 0x40;
		break;
	}

	switch (em->audio_mode) {
	case EM8300_AUDIOMODE_ANALOG:
		em->pcm_mode = EM8300_AUDIOMODE_ANALOG;

		write_register(EM8300_AUDIO_RATE, 0x62);
		em8300_setregblock(em, 2 * ucregister(Mute_Pattern), 0, 0x600);
		printk(KERN_NOTICE "em8300-%d: Analog audio enabled\n", em->card_nr);
		break;
	case EM8300_AUDIOMODE_DIGITALPCM:
		em->pcm_mode = EM8300_AUDIOMODE_DIGITALPCM;

		write_register(EM8300_AUDIO_RATE, 0x3a0);

		em->channel_status[0] = 0x0;
		sub_prepare_SPDIF(em, (uint32_t *)em->mafifo->preprocess_buffer, NULL, 192);

		em8300_writeregblock(em, 2*ucregister(Mute_Pattern), (unsigned *)em->mafifo->preprocess_buffer, em->mafifo->slotsize);

		printk(KERN_NOTICE "em8300-%d: Digital PCM audio enabled\n", em->card_nr);
		break;
	case EM8300_AUDIOMODE_DIGITALAC3:
		write_register(EM8300_AUDIO_RATE, 0x3a0);

		em->channel_status[0] = 0x40;
		sub_prepare_SPDIF(em, (uint32_t *)em->mafifo->preprocess_buffer, NULL, 192);

		em8300_writeregblock(em, 2*ucregister(Mute_Pattern), (unsigned *)em->mafifo->preprocess_buffer, em->mafifo->slotsize);
		printk(KERN_NOTICE "em8300-%d: Digital AC3 audio enabled\n", em->card_nr);
		break;
	}
	return 0;
}

int em8300_audio_setup(struct em8300_s *em)
{
	int ret;

	em->audio.channels = 2;
	em->audio.format = AFMT_S16_NE;
	em->audio.slotsize = em->mafifo->slotsize;

	em->audio.speed = 48000;

	em->audio_mode = EM8300_AUDIOMODE_DEFAULT;

	ret = em8300_audio_flush(em);

	setup_mafifo(em);

	if (ret) {
		printk(KERN_ERR "em8300-%d: Couldn't zero audio buffer\n", em->card_nr);
		return ret;
	}

	write_ucregister(MA_Threshold, 6);

	mpegaudio_command(em, MACOMMAND_PLAY);
	mpegaudio_command(em, MACOMMAND_PAUSE);

	em->audio.enable_bits = 0;

	return 0;
}

int em8300_audio_calcbuffered(struct em8300_s *em)
{
	int readptr, writeptr, bufsize, n;

	readptr = read_ucregister(MA_Rdptr) | (read_ucregister(MA_Rdptr_Hi) << 16);
	writeptr = read_ucregister(MA_Wrptr) | (read_ucregister(MA_Wrptr_Hi) << 16);
	bufsize = read_ucregister(MA_BuffSize) | (read_ucregister(MA_BuffSize_Hi) << 16);

	n = ((bufsize+writeptr-readptr) % bufsize);

	return (em8300_fifo_calcbuffered(em->mafifo) + n) /
		em->mafifo->preprocess_ratio;
}

ssize_t em8300_audio_write(struct em8300_s *em, const char *buf, size_t count, loff_t *ppos)
{
	if (em->nonblock[1]) {
		return em8300_fifo_write(em->mafifo, count, buf, 0);
	} else {
		return em8300_fifo_writeblocking(em->mafifo, count, buf, 0);
	}
}

/* 18-09-2000 - Ze'ev Maor - added these two ioctls to set and get audio mode. */

int em8300_ioctl_setaudiomode(struct em8300_s *em, int mode)
{
	if (em->audio_driver_style == OSS)
		em8300_audio_flush(em);
	set_audiomode(em, mode);
	setup_mafifo(em);
	if (em->audio_driver_style == OSS)
		mpegaudio_command(em, MACOMMAND_PLAY);
	em->audio.enable_bits = PCM_ENABLE_OUTPUT;
	return 0;
}

int em8300_ioctl_getaudiomode(struct em8300_s *em, long int mode)
{
	int a = em->audio_mode;
	if (copy_to_user((void *) mode, &a, sizeof(int)))
		return -EFAULT;
	return 0;
}
