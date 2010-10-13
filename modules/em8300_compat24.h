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

/* Interrupt handler backwards compatibility stuff */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) && !defined(IRQ_NONE)
#define IRQ_NONE
#define IRQ_HANDLED
typedef void irqreturn_t;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#ifndef IRQF_DISABLED
#define IRQF_DISABLED SA_INTERRUPT
#endif
#ifndef IRQF_SHARED
#define IRQF_SHARED SA_SHIRQ
#endif
#endif

/* i2c stuff */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,67)
static inline void *i2c_get_clientdata(struct i2c_client *dev)
{
	return dev->data;
}

static inline void i2c_set_clientdata(struct i2c_client *dev, void *data)
{
	dev->data = data;
}

static inline void *i2c_get_adapdata(struct i2c_adapter *dev)
{
	return dev->data;
}

static inline void i2c_set_adapdata(struct i2c_adapter *dev, void *data)
{
	dev->data = data;
}
#endif

/* modules */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,48)
#define EM8300_MOD_INC_USE_COUNT MOD_INC_USE_COUNT
#define EM8300_MOD_DEC_USE_COUNT MOD_DEC_USE_COUNT
#else
#define EM8300_MOD_INC_USE_COUNT do { } while (0)
#define EM8300_MOD_DEC_USE_COUNT do { } while (0)
#endif

#if !defined(MODULE_LICENSE)
#define MODULE_LICENSE(_license)
#endif

#if !defined(MODULE_ALIAS_CHARDEV_MAJOR)
#define MODULE_ALIAS_CHARDEV_MAJOR(major)
#endif

#if !defined(EXPORT_NO_SYMBOLS)
#define EXPORT_NO_SYMBOLS
#endif

/* EM8300_IMINOR */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,2) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define EM8300_IMINOR(inode) (MINOR((inode)->i_rdev))
#else
#define EM8300_IMINOR(inode) (minor((inode)->i_rdev))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
#define kzalloc(size, flags)						\
({									\
	void *__ret = kmalloc(size, flags);				\
	if (__ret)							\
		memset(__ret, 0, size);					\
	__ret;								\
})
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
#define pr_warning(fmt, arg...) \
  printk(KERN_WARNING fmt, ##arg)
#endif

#ifdef _LINUX_WAIT_H

/* Macros backported from linux-2.6/include/linux/wait.h */

#ifndef __wait_event_interruptible_timeout
#define __wait_event_interruptible_timeout(wq, condition, ret)		\
do {									\
	wait_queue_t __wait;						\
	init_waitqueue_entry(&__wait, current);				\
									\
	add_wait_queue(&wq, &__wait);					\
	for (;;) {							\
		set_current_state(TASK_INTERRUPTIBLE);			\
		if (condition)						\
			break;						\
		if (!signal_pending(current)) {				\
			ret = schedule_timeout(ret);			\
			if (!ret)					\
				break;					\
			continue;					\
		}							\
		ret = -ERESTARTSYS;					\
		break;							\
	}								\
	current->state = TASK_RUNNING;					\
	remove_wait_queue(&wq, &__wait);				\
} while (0)
#endif

/**
 * wait_event_interruptible_timeout - sleep until a condition gets true or a timeout elapses
 * @wq: the waitqueue to wait on
 * @condition: a C expression for the event to wait for
 * @timeout: timeout, in jiffies
 *
 * The process is put to sleep (TASK_INTERRUPTIBLE) until the
 * @condition evaluates to true or a signal is received.
 * The @condition is checked each time the waitqueue @wq is woken up.
 *
 * wake_up() has to be called after changing any variable that could
 * change the result of the wait condition.
 *
 * The function returns 0 if the @timeout elapsed, -ERESTARTSYS if it
 * was interrupted by a signal, and the remaining jiffies otherwise
 * if the condition evaluated to true before the timeout elapsed.
 */
#ifndef wait_event_interruptible_timeout
#define wait_event_interruptible_timeout(wq, condition, timeout)	\
({									\
	long __ret = timeout;						\
	if (!(condition))						\
		__wait_event_interruptible_timeout(wq, condition, __ret); \
	__ret;								\
})
#endif

#endif /* _LINUX_WAIT_H */

#endif /* _EM8300_COMPAT24_H_ */
