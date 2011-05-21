/*
 * em8300_fifo.h
 *
 * Copyright (C) 2000 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2001-2002 Rick Haines <rick@kuroyi.net>
 *           (C) 2003-2004 Nicolas Boullis <nboullis@debian.org>
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

#ifndef EM8300_FIFO_H
#define EM8300_FIFO_H

#include <linux/semaphore.h>

struct video_fifoslot_s {
	uint32_t flags;
	uint32_t physaddress_hi;
	uint32_t physaddress_lo;
	uint32_t slotsize;
};

struct pts_fifoslot_s {
	uint32_t streamoffset_hi;
	uint32_t streamoffset_lo;
	uint32_t pts_hi;
	uint32_t pts_lo;
};

struct em8300_s;

struct fifo_s {
	struct em8300_s *em;

	int valid;

	int nslots;
	union {
		struct video_fifoslot_s *v;
		struct pts_fifoslot_s *pts;
	} slots;
	int slotptrsize;
	int slotsize;

	int start;
	int * volatile writeptr;
	int * volatile readptr;
	int localreadptr;
	int threshold;

	int bytes;

	char *fifobuffer;

	int preprocess_ratio;
	char *preprocess_buffer;

	wait_queue_head_t wait;

	struct semaphore lock;

	dma_addr_t phys_base;
};

struct em8300_s;

/*
  Prototypes
*/
int em8300_fifo_init(struct em8300_s *em, struct fifo_s *f,
		     int start, int wrptr, int rdptr,
		     int pcisize, int slotsize);

struct fifo_s * em8300_fifo_alloc(void);
void em8300_fifo_free(struct fifo_s *f);

int em8300_fifo_write(struct fifo_s *fifo, int n, const char *userbuffer,
		      int flags);
int em8300_fifo_writeblocking(struct fifo_s *fifo, int n,
			      const char *userbuffer, int flags);
int em8300_fifo_check(struct fifo_s *fifo);
int em8300_fifo_sync(struct fifo_s *fifo);
int em8300_fifo_freeslots(struct fifo_s *fifo);
void em8300_fifo_statusmsg(struct fifo_s *fifo, char *str);

#endif /* EM8300_FIFO_H */
