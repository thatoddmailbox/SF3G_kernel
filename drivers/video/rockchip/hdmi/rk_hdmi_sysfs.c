/*
 * Copyright (C) 2012-2015 Rockchip Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/display-sys.h>
#include <linux/interrupt.h>
#include "rk_hdmi.h"

static int hdmi_get_enable(struct rk_display_device *device)
{
	struct hdmi *hdmi = device->priv_data;
	int enable;

	mutex_lock(&hdmi->enable_mutex);
	enable = hdmi->enable;
	mutex_unlock(&hdmi->enable_mutex);

	return enable;
}

static int hdmi_set_enable(struct rk_display_device *device, int enable)
{
	struct hdmi *hdmi = device->priv_data;

	mutex_lock(&hdmi->enable_mutex);
	if (hdmi->enable == enable) {
		mutex_unlock(&hdmi->enable_mutex);
		return 0;
	}
	hdmi->enable = enable;

	if (hdmi->suspend) {
		mutex_unlock(&hdmi->enable_mutex);
		return 0;
	}

	if (enable == 0) {
		if (hdmi->irq)
			disable_irq(hdmi->irq);
		mutex_unlock(&hdmi->enable_mutex);
		hdmi->command = HDMI_CONFIG_ENABLE;
		queue_delayed_work(hdmi->workqueue, &hdmi->delay_work, 0);
	} else {
		if (hdmi->irq)
			enable_irq(hdmi->irq);
		queue_delayed_work(hdmi->workqueue, &hdmi->delay_work, 0);
		mutex_unlock(&hdmi->enable_mutex);
	}

	return 0;
}

static int hdmi_get_status(struct rk_display_device *device)
{
	struct hdmi *hdmi = device->priv_data;

	if (hdmi->hotplug == HDMI_HPD_ACTIVED)
		return 1;
	else
		return 0;
}

static int hdmi_get_modelist(struct rk_display_device *device,
			     struct list_head **modelist)
{
	struct hdmi *hdmi = device->priv_data;

	if (!hdmi->hotplug)
		return -1;
	*modelist = &hdmi->edid.modelist;

	return 0;
}

static int hdmi_set_mode(struct rk_display_device *device,
			 struct fb_videomode *mode)
{
	struct hdmi *hdmi = device->priv_data;
	int vic = hdmi_videomode_to_vic(mode);

	hdmi->autoconfig = HDMI_DISABLE;
	if (vic && hdmi->vic != vic) {
		hdmi->vic = vic;
		if (!hdmi->hotplug)
			return 0;
		hdmi->command = HDMI_CONFIG_VIDEO;
		init_completion(&hdmi->complete);
		hdmi->wait = 1;
		queue_delayed_work(hdmi->workqueue, &hdmi->delay_work, 0);
		wait_for_completion_interruptible_timeout(&hdmi->complete,
							  msecs_to_jiffies
							  (10000));
	}
	return 0;
}

static int hdmi_get_mode(struct rk_display_device *device,
			 struct fb_videomode *mode)
{
	struct hdmi *hdmi = device->priv_data;
	struct fb_videomode *vmode;

	if (!hdmi->hotplug)
		return -1;

	vmode = (struct fb_videomode *)hdmi_vic_to_videomode(hdmi->vic);
	if (unlikely(vmode == NULL))
		return -1;
	*mode = *vmode;
	return 0;
}

static int hdmi_set_scale(struct rk_display_device *device, int direction,
			  int value)
{
	struct hdmi *hdmi = device->priv_data;

	if (!hdmi || value < 0 || value > 100)
		return -1;

	if (!hdmi->hotplug)
		return 0;

	if (direction == DISPLAY_SCALE_X)
		hdmi->xscale = value;
	else if (direction == DISPLAY_SCALE_Y)
		hdmi->yscale = value;
	else
		return -1;
	rockchip_fb_disp_scale(hdmi->xscale, hdmi->yscale, hdmi->vop->id);
	return 0;
}

static int hdmi_get_scale(struct rk_display_device *device, int direction)
{
	struct hdmi *hdmi = device->priv_data;

	if (!hdmi)
		return -1;

	if (direction == DISPLAY_SCALE_X)
		return hdmi->xscale;
	else if (direction == DISPLAY_SCALE_Y)
		return hdmi->yscale;
	else
		return -1;
}

static int hdmi_set_debug(struct rk_display_device *device, int cmd)
{
	struct hdmi *hdmi = device->priv_data;

	if (!hdmi)
		return -1;
	if (hdmi->ops && hdmi->ops->hdmi_debug)
		hdmi->ops->hdmi_debug(hdmi, cmd);

	return 0;
}

/*
 * CEA 861-E: Audio Coding Type
 * sync width enum hdmi_audio_type
 */
static const char * const saudioformatstr[] = {
	"",
	"LPCM",
	"AC3",
	"MPEG1",
	"MP3",
	"MPEG2",
	"AAC-LC",
	"DTS",
	"ATARC",
	"DSD",
	"E-AC3",
	"DTS-HD",
	"MLP",
	"DST",
	"WMA-PRO",
};

static int hdmi_get_edidaudioinfo(struct rk_display_device *device,
				  char *audioinfo, int len)
{
	struct hdmi *hdmi = device->priv_data;
	int i = 0;
	int size = 0;
	struct hdmi_audio *audio;

	if (!hdmi)
		return -1;

	memset(audioinfo, 0x00, len);
	mutex_lock(&hdmi->lock);
	for (i = 0; i < hdmi->edid.audio_num; i++) {
		audio = &hdmi->edid.audio[i];
		if (audio->type < 1 || audio->type > HDMI_AUDIO_WMA_PRO) {
			pr_err("audio type: unsupported.");
			continue;
		}
		size = strlen(saudioformatstr[audio->type]);
		memcpy(audioinfo, saudioformatstr[audio->type], size);
		audioinfo[size] = ',';
		audioinfo += (size + 1);
	}
	mutex_unlock(&hdmi->lock);
	return 0;
}

static int hdmi_get_monspecs(struct rk_display_device *device,
			     struct fb_monspecs *monspecs)
{
	struct hdmi *hdmi = device->priv_data;

	if (!hdmi)
		return -1;

	mutex_lock(&hdmi->lock);
	if (hdmi->edid.specs)
		*monspecs = *hdmi->edid.specs;
	mutex_unlock(&hdmi->lock);
	return 0;
}

struct rk_display_ops hdmi_display_ops = {
	.setenable = hdmi_set_enable,
	.getenable = hdmi_get_enable,
	.getstatus = hdmi_get_status,
	.getmodelist = hdmi_get_modelist,
	.setmode = hdmi_set_mode,
	.getmode = hdmi_get_mode,
	.setscale = hdmi_set_scale,
	.getscale = hdmi_get_scale,
	.setdebug = hdmi_set_debug,
	.getedidaudioinfo = hdmi_get_edidaudioinfo,
	.getmonspecs = hdmi_get_monspecs,
};

static int hdmi_display_probe(struct rk_display_device *device, void *devdata)
{
	device->owner = THIS_MODULE;
	strcpy(device->type, "HDMI");
	device->priority = DISPLAY_PRIORITY_HDMI;
/*
	device->name = kmalloc(strlen(name), GFP_KERNEL);
	if(device->name)
		strcpy(device->name, name);
*/
	device->priv_data = devdata;
	device->ops = &hdmi_display_ops;
	return 1;
}

static struct rk_display_driver display_hdmi = {
	.probe = hdmi_display_probe,
};

void hdmi_register_display_sysfs(struct hdmi *hdmi, struct device *parent)
{
	hdmi->ddev =
	    rk_display_device_register(&display_hdmi, parent, hdmi);
}

void hdmi_unregister_display_sysfs(struct hdmi *hdmi)
{
	if (hdmi->ddev)
		rk_display_device_unregister(hdmi->ddev);
}
