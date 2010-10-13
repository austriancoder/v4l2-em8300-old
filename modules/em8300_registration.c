/* $Id$
 *
 * em8300_registration.c -- common interface for everything that needs
 *                          to be registered
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

#include "em8300_registration.h"

#include "em8300_procfs.h"
#include "em8300_devfs.h"
#include "em8300_udev.h"
#include "em8300_sysfs.h"
#include "em8300_alsa.h"

static struct em8300_registrar_s *registrars[] =
{
	&em8300_procfs_registrar,
	&em8300_devfs_registrar,
	&em8300_udev_registrar,
	&em8300_sysfs_registrar,
	&em8300_alsa_registrar,
	NULL
};


void em8300_register_driver(void)
{
	int i;
	for (i = 0; registrars[i]; i++) {
		if (registrars[i]->register_driver)
			registrars[i]->register_driver();
	}
}

void em8300_postregister_driver(void)
{
	int i;
	for (i = 0; registrars[i]; i++) {
		if (registrars[i]->postregister_driver)
			registrars[i]->postregister_driver();
	}
}

void em8300_register_card(struct em8300_s *em)
{
	int i;
	for (i = 0; registrars[i]; i++) {
		if (registrars[i]->register_card)
			registrars[i]->register_card(em);
	}
}

void em8300_enable_card(struct em8300_s *em)
{
	int i;
	for (i = 0; registrars[i]; i++) {
		if (registrars[i]->enable_card)
			registrars[i]->enable_card(em);
	}
}

void em8300_disable_card(struct em8300_s *em)
{
	int i;
	for (i = 0; registrars[i]; i++) {
		if (registrars[i]->disable_card)
			registrars[i]->disable_card(em);
	}
}

void em8300_unregister_card(struct em8300_s *em)
{
	int i;
	for (i = 0; registrars[i]; i++) {
		if (registrars[i]->unregister_card)
			registrars[i]->unregister_card(em);
	}
}

void em8300_preunregister_driver(void)
{
	int i;
	for (i = 0; registrars[i]; i++) {
		if (registrars[i]->preunregister_driver)
			registrars[i]->preunregister_driver();
	}
}

void em8300_unregister_driver(void)
{
	int i;
	for (i = 0; registrars[i]; i++) {
		if (registrars[i]->unregister_driver)
			registrars[i]->unregister_driver();
	}
}

void em8300_audio_interrupt(struct em8300_s *em)
{
	int i;
	for (i = 0; registrars[i]; i++) {
		if (registrars[i]->audio_interrupt)
			registrars[i]->audio_interrupt(em);
	}
}

void em8300_video_interrupt(struct em8300_s *em)
{
	int i;
	for (i = 0; registrars[i]; i++) {
		if (registrars[i]->video_interrupt)
			registrars[i]->video_interrupt(em);
	}
}

void em8300_vbl_interrupt(struct em8300_s *em)
{
	int i;
	for (i = 0; registrars[i]; i++) {
		if (registrars[i]->vbl_interrupt)
			registrars[i]->vbl_interrupt(em);
	}
}
