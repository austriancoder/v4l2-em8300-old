# $Id$
#
# Makefile -- makefile for the em8300 modules
# Copyright (C) 2004 Nicolas Boullis <nboullis@debian.org>
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

ifeq ($(KERNELRELEASE),)

KERNEL_LOCATION := /lib/modules/$(shell uname -r)/build

# If you are using external i2c modules, you should set
# EM8300_I2C_LOCATION to the source directory of the modules.
# Moreover, if the API of the modules is not properly guessed, you can
# force it by setting EM8300_I2C_FORCE_API to NEW. (That is
# especially for using 2.8.x modules with 2.4.x kernels.)
#export EM8300_I2C_LOCATION := /usr/src/modules/i2c
#export EM8300_I2C_FORCE_API := NEW

export EM8300_DIR := $(CURDIR)/..
export INSTALL_MOD_DIR=em8300

build: em8300_version.h
	$(MAKE) -C $(KERNEL_LOCATION) SUBDIRS=$(CURDIR) modules

.PHONY: em8300_version.h
em8300_version.h:
	[ ! -x ./update_em8300_version.sh ] || ./update_em8300_version.sh $@

ifeq ($(strip $(shell printf '\043if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,51)\n1\n\043else\n0\n\043endif\n' | $(CPP) -imacros $(KERNEL_LOCATION)/include/linux/version.h -P - )),1)

clean:
	-rm -f *.o .*.o.flags

KERNVER := $(shell $(CPP) -dM $(KERNEL_LOCATION)/include/linux/version.h | sed -ne 's/^\#define[[:space:]]\+UTS_RELEASE[[:space:]]\+"\(.*\)"$$/\1/p')

MODULES := adv717x.o bt865.o em8300.o

install:
	install -d $(DESTDIR)/lib/modules/$(KERNVER)/$(INSTALL_MOD_DIR)
	install -m 644 $(MODULES) $(DESTDIR)/lib/modules/$(KERNVER)/$(INSTALL_MOD_DIR)

uninstall:
	rm -rf $(DESTDIR)/lib/modules/$(KERNVER)/$(INSTALL_MOD_DIR)

else

clean:
	$(MAKE) -C $(KERNEL_LOCATION) SUBDIRS=$(CURDIR) clean

install:
	$(MAKE) -C $(KERNEL_LOCATION) SUBDIRS=$(CURDIR) INSTALL_MOD_PATH=$(DESTDIR) modules_install

uninstall:
	rm -rf $(DESTDIR)/lib/modules/$(shell cat $(KERNEL_LOCATION)/.kernelrelease 2> /dev/null || cat $(KERNEL_LOCATION)/include/config/kernel.release 2> /dev/null)/$(INSTALL_MOD_DIR)

endif

.PHONY: build clean install uninstall

else

# It should be possible to write it
# $(lastword $(MAKEFILE_LIST))
# but it's not supported by old make.
CURRENT_MAKEFILE := $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))
include $(dir $(CURRENT_MAKEFILE))Kbuild

endif
