#! /bin/sh
# This is a simple script to create the devices for em8300 driver.
# It should abort if you have the devfs filesystem mounted, em8300
# supports devfs so there should be no need to create the nodes manually
# on such systems.  Systems using sysfs and udev shouldn't need this either.

DEVFS=`cat /proc/mounts | grep " devfs "`

if [ -z "$DEVFS" ] ; then
	echo "devfs not mounted, creating device nodes"
	mknod /dev/em8300-0    c 121 0
	mknod /dev/em8300_mv-0 c 121 1
	mknod /dev/em8300_ma-0 c 121 2
	mknod /dev/em8300_sp-0 c 121 3
	chmod 666 /dev/em8300*
else
	echo "Looks like you have devfs mounted, I'm going to leave things alone!"
fi

