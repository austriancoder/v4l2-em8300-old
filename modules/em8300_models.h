/* $Id$
 *
 * em8300_models.h -- identify and configure known models of em8300-based cards
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

#ifndef _EM8300_MODELS_H
#define _EM8300_MODELS_H

#include <linux/types.h>
#include <linux/em8300.h>
#include "em8300_driver.h"

struct em8300_model_s {
	char const *name;
	struct { char const *name; unsigned short addr; } module;
	struct em8300_model_config_s em8300_config;
	struct adv717x_model_config_s adv717x_config;
};

extern const struct em8300_model_s known_models[];

extern const unsigned known_models_number;

extern int identify_model(struct em8300_s *em);

#endif /* _EM8300_MODELS_H */
