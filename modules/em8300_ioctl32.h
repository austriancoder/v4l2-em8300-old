/* $Id$
 *
 * em8300_ioctl32.h -- compatibility layer for 32-bit ioctls on 64-bit kernels
 * Copyright (C) 2004 Nicolas Boullis <nboullis@debian.org>
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

#ifndef EM8300_IOCTL32_H
#define EM8300_IOCTL32_H

#ifdef HAVE_COMPAT_IOCTL

extern long em8300_compat_ioctl(struct file* filp, unsigned cmd, unsigned long arg);

#else /* HAVE_COMPAT_IOCTL */

extern void em8300_ioctl32_init(void);
extern void em8300_ioctl32_exit(void);

#endif /* HAVE_COMPAT_IOCTL */

#endif /* EM8300_IOCTL32_H */
