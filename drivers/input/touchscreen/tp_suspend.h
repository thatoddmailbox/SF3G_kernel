/*
 * TP  suspend Control Abstraction
 *
 * Copyright (C) 2014 Yangjie <yangjie@rock-chips.com>
 * Copyright (C) 2014 ROCKCHIP, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _RK_TP_SUSPEND_H
#define _RK_TP_SUSPEND_H
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/notifier.h>

struct  tp_device{
	struct notifier_block fb_notif;
	int(*tp_suspend)(struct  tp_device*);
	int(*tp_resume)(struct  tp_device*);
	struct mutex ops_lock;
};

static inline int fb_notifier_callback(struct notifier_block *self,
				unsigned long action, void *data)
{
	struct tp_device *tp;
	struct fb_event *event = data;
	int blank_mode = *((int *)event->data);
	int ret = 0;

	tp = container_of(self, struct tp_device, fb_notif);

	//printk("%s.....lin=%d tp->status=%x,blank_mode=%x\n",__func__,__LINE__,tp->status,blank_mode);

	mutex_lock(&tp->ops_lock);

	if (action == FB_EARLY_EVENT_BLANK) {
		switch (blank_mode) {
		case FB_BLANK_UNBLANK:
			break;
		default:
			ret = tp->tp_suspend(tp);
			break;
		}
	}
	else if (action == FB_EVENT_BLANK) {
		switch (blank_mode) {
		case FB_BLANK_UNBLANK:
			tp->tp_resume(tp);
			break;
		default:
			break;
		}
	}
	mutex_unlock(&tp->ops_lock);

	if (ret < 0)
	{
		printk("TP_notifier_callback error action=%x,blank_mode=%x\n",(int)action,blank_mode);
		return ret;
	}

	return NOTIFY_OK;
}

static inline int tp_register_fb(struct tp_device *tp)
{
	memset(&tp->fb_notif, 0, sizeof(tp->fb_notif));
	tp->fb_notif.notifier_call = fb_notifier_callback;
	mutex_init(&tp->ops_lock);

	return fb_register_client(&tp->fb_notif);
}

static inline void tp_unregister_fb(struct tp_device *tp)
{
	fb_unregister_client(&tp->fb_notif);
}
#endif
