/* $Id$
 *
 * em8300_compat24.h -- compatibility layer for 2.4 and some 2.5 kernels
 * Copyright (C) 2004 Andreas Schultz <aschultz@warp10.net>
 * Copyright (C) 2004 Nicolas Boullis <nboullis@debian.org>
 * Copyright (C) 2005 Jon Burgess <jburgess@uklinux.net>
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

#ifndef _EM8300_COMPAT24_H_
#define _EM8300_COMPAT24_H_

/* EM8300_IMINOR */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,2) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define EM8300_IMINOR(inode) (MINOR((inode)->i_rdev))
#else
#define EM8300_IMINOR(inode) (minor((inode)->i_rdev))
#endif

#endif /* _EM8300_COMPAT24_H_ */
