/* $Id$
 *
 * em8300_eeprom.h -- read the eeprom on em8300-based boards
 * Copyright (C) 2007 Nicolas Boullis <nboullis@debian.org>
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

#ifndef _EM8300_EEPROM_H
#define _EM8300_EEPROM_H

#include <linux/types.h>

struct em8300_s;

extern int em8300_eeprom_read(struct em8300_s *em, u8 *data);

#endif /* _EM8300_EEPROM_H */
