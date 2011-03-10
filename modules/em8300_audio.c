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

#include <linux/string.h>
#include <linux/pci.h>
#include <linux/soundcard.h>
#include <linux/types.h>

#include "em8300_reg.h"
#include <linux/em8300.h>
#include "em8300_driver.h"
#include "em8300_fifo.h"

#ifndef AFMT_AC3
#define AFMT_AC3 0x00000400
#endif

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

	pr_debug("em8300-%d: MA_Command: %d\n", em->instance, cmd);
	write_ucregister(MA_Command, cmd);

	return em8300_waitfor(em, ucregister(MA_Status), cmd, 0xffff);
}

int em8300_audio_flush(struct em8300_s *em)
{
	int pcirdptr = read_ucregister(MA_PCIRdPtr);
	write_ucregister(MA_PCIWrPtr, pcirdptr);
	writel(readl(em->mafifo->readptr), em->mafifo->writeptr);
	em8300_fifo_sync(em->mafifo);
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
		printk(KERN_ERR "em8300-%d: Couldn't zero audio buffer\n", em->instance);
		return ret;
	}

	write_ucregister(MA_Threshold, 6);

	mpegaudio_command(em, MACOMMAND_PLAY);
	mpegaudio_command(em, MACOMMAND_PAUSE);

	em->audio.enable_bits = 0;

	return 0;
}
