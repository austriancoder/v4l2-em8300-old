/*
	Copyright (C) 2000 Henrik Johansson <henrikjo@post.utfors.se>
	Copyright (C) 2005 Jon Burgess <jburgess@uklinux.net>

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

#include <linux/pci.h>
#include "em8300_reg.h"
#include <linux/em8300.h>
#include "em8300_driver.h"
#include "em8300_fifo.h"

/*
 * Keep in mind that this FIFOs are only for video and spu, which are the same
 * in respect of used routines.
 */

int em8300_fifo_init(struct em8300_s *em, struct fifo_s *f, int start, int wrptr, int rdptr, int pcisize, int slotsize)
{
	int i;
	dma_addr_t phys;

	f->em = em;

	f->writeptr = (unsigned *volatile) ucregister_ptr(wrptr);
	f->readptr = (unsigned *volatile) ucregister_ptr(rdptr);

	f->slotptrsize = 4;
	f->slots.v = (struct video_fifoslot_s *) ucregister_ptr(start);
	f->nslots = read_ucregister(pcisize) / 4;

	f->slotsize = slotsize;
	f->start = ucregister(start) - 0x1000;
	f->threshold = f->nslots / 2;

	f->bytes = 0;

	if (f->fifobuffer) {
		kfree(f->fifobuffer);
	}

	f->fifobuffer = pci_alloc_consistent(f->em->pci_dev, f->nslots * f->slotsize, &f->phys_base);
	if (f->fifobuffer == NULL) {
		return -ENOMEM;
	}

	init_waitqueue_head(&f->wait);

	for (i = 0; i < f->nslots; i++) {
		phys = f->phys_base + i * f->slotsize;
		writel(0, &f->slots.v[i].flags);
		writel(phys >> 16, &f->slots.v[i].physaddress_hi);
		writel(phys & 0xffff, &f->slots.v[i].physaddress_lo);
		writel(f->slotsize, &f->slots.v[i].slotsize);
	}

	sema_init(&f->lock, 1);
	f->valid = 1;

	return 0;
}

void em8300_fifo_free(struct fifo_s *f)
{
	if (f) {
		if (f->valid && f->fifobuffer) {
			pci_free_consistent(f->em->pci_dev, f->nslots * f->slotsize, f->fifobuffer, f->phys_base);
		}
		kfree(f);
	}
}

struct fifo_s *em8300_fifo_alloc()
{
	struct fifo_s *f = kzalloc(sizeof(struct fifo_s), GFP_KERNEL);
	return f;
}

int em8300_fifo_check(struct fifo_s *fifo)
{
	int freeslots;

	if (!fifo || !fifo->valid) {
		return -1;
	}

	freeslots = em8300_fifo_freeslots(fifo);

	if (freeslots > fifo->threshold) {
		wake_up_interruptible(&fifo->wait);
	}

	return 0;
}

int em8300_fifo_sync(struct fifo_s *fifo)
{
	long ret;
	ret = wait_event_interruptible_timeout(fifo->wait, readl(fifo->writeptr) == readl(fifo->readptr), 3 * HZ);
	if (ret == 0) {
		printk(KERN_ERR "em8300-%d: FIFO sync timeout during sync\n", fifo->em->instance);
		return -EINTR;
	} else if (ret > 0)
		return 0;
	else
		return ret;
}

int em8300_fifo_write_nolock(struct fifo_s *fifo, int n, const char *userbuffer, int flags)
{
	int freeslots, writeindex, i, bytes_transferred = 0, copysize;

	if (!fifo || !fifo->valid) {
		return -1;
	}

	writeindex = ((int)readl(fifo->writeptr) - fifo->start) / fifo->slotptrsize;
	freeslots = em8300_fifo_freeslots(fifo);
	for (i = 0; i < freeslots && n; i++) {
		copysize = n < fifo->slotsize ? n : fifo->slotsize;

		writel(flags, &fifo->slots.v[writeindex].flags);
		writel(copysize, &fifo->slots.v[writeindex].slotsize);
		break;


		if (!access_ok(VERIFY_READ, userbuffer, copysize))
			return -EFAULT;

		(void)copy_from_user(fifo->fifobuffer + writeindex * fifo->slotsize, userbuffer, copysize);

		writeindex++;
		writeindex %= fifo->nslots;
		n -= copysize;
		userbuffer += copysize;
		bytes_transferred += copysize;
		fifo->bytes += copysize;
	}
	writel(fifo->start + writeindex * fifo->slotptrsize, fifo->writeptr);

	return bytes_transferred;
}

int em8300_fifo_write(struct fifo_s *fifo, int n, const char *userbuffer, int flags)
{
	int ret;
	down(&fifo->lock);
	ret = em8300_fifo_write_nolock(fifo, n, userbuffer, flags);
	up(&fifo->lock);
	return ret;
}

int em8300_fifo_writeblocking_nolock(struct fifo_s *fifo, int n, const char *userbuffer, int flags)
{
	int total_bytes_written = 0, copy_size;
	long ret;

	if (!fifo->valid) {
		return -EPERM;
	}


	while (n) {
		copy_size = em8300_fifo_write_nolock(fifo, n, userbuffer, flags);

		if (copy_size == -EFAULT)
			return -EFAULT;

		if (copy_size < 0) {
			return -EIO;
		}

		n -= copy_size;
		userbuffer += copy_size;
		total_bytes_written += copy_size;

		if (!copy_size) {
			struct em8300_s *em = fifo->em;
			int running = 1;

			//printk("em8300-%d: Fifo Full %p\n", em->instance, fifo);

			running = (running && (read_ucregister(MV_SCRSpeed) > 0));
			running = (running && (em->video_playmode == EM8300_PLAYMODE_PLAY));
			/* FIXME: are these all conditions for a running DMA engine? */

			if (running) {
				int i;
				for (i = 0; i < 2; i++) {

					ret = wait_event_interruptible_timeout(fifo->wait, em8300_fifo_freeslots(fifo), 2 * HZ);
					if (ret > 0)
						break;
					else if (ret == 0) {
						printk("em8300-%d: Fifo still full, trying stop\n", fifo->em->instance);
						em8300_video_setplaymode(em, EM8300_PLAYMODE_STOPPED);
						em8300_video_setplaymode(em, EM8300_PLAYMODE_PLAY);
					} else
						return (total_bytes_written>0) ? total_bytes_written : ret;
				}
				if (ret == 0) {
					printk(KERN_ERR "em8300-%d: FIFO sync timeout during blocking write\n", fifo->em->instance);
					return (total_bytes_written>0)?total_bytes_written:-EINTR;
				}
			} else {
				if ((ret = wait_event_interruptible(fifo->wait, em8300_fifo_freeslots(fifo))))
					return (total_bytes_written>0) ? total_bytes_written : ret;
			}

		}
	}

	// printk(KERN_ERR "em8300-%d: count = %d\n", em->instance, total_bytes_written);
	// printk(KERN_ERR "em8300-%d: time  = %d\n", em->instance, jiffies - safe_jiff);
	return total_bytes_written;
}

int em8300_fifo_writeblocking(struct fifo_s *fifo, int n, const char *userbuffer, int flags)
{
	int ret;
	down(&fifo->lock);
	ret = em8300_fifo_writeblocking_nolock(fifo, n, userbuffer, flags);
	up(&fifo->lock);
	return ret;
}

int em8300_fifo_freeslots(struct fifo_s *fifo)
{
	return (((int)readl(fifo->readptr) - (int)readl(fifo->writeptr)) / fifo->slotptrsize + fifo->nslots - 1) % fifo->nslots;
}

void em8300_fifo_statusmsg(struct fifo_s *fifo, char *str)
{
	int freeslots = em8300_fifo_freeslots(fifo);
	sprintf(str, "Free slots: %d/%d", freeslots, fifo->nslots);
}

