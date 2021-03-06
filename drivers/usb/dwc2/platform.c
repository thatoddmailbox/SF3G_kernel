/*
 * platform.c - DesignWare HS OTG Controller platform driver
 *
 * Copyright (C) Matthijs Kooijman <matthijs@stdin.nl>
 * Copyright (C) 2014-2015 Intel Mobile Communications GmbH
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include <linux/usb/of.h>

#include "core.h"
#include "hcd.h"
#include "debug.h"

static const char dwc2_driver_name[] = "dwc2";

static const struct dwc2_core_params params_bcm2835 = {
	.otg_cap			= 0,	/* HNP/SRP capable */
	.otg_ver			= 0,	/* 1.3 */
	.dma_enable			= 1,
	.dma_desc_enable		= 0,
	.speed				= 0,	/* High Speed */
	.enable_dynamic_fifo		= 1,
	.en_multiple_tx_fifo		= 1,
	.host_rx_fifo_size		= 774,	/* 774 DWORDs */
	.host_nperio_tx_fifo_size	= 256,	/* 256 DWORDs */
	.host_perio_tx_fifo_size	= 512,	/* 512 DWORDs */
	.max_transfer_size		= 65535,
	.max_packet_count		= 511,
	.host_channels			= 8,
	.phy_type			= 1,	/* UTMI */
	.phy_utmi_width			= 8,	/* 8 bits */
	.phy_ulpi_ddr			= 0,	/* Single */
	.phy_ulpi_ext_vbus		= 0,
	.i2c_enable			= 0,
	.ulpi_fs_ls			= 0,
	.host_support_fs_ls_low_power	= 0,
	.host_ls_low_power_phy_clk	= 0,	/* 48 MHz */
	.ts_dline			= 0,
	.reload_ctl			= 0,
	.ahbcfg				= 0x10,
	.uframe_sched			= 0,
	.external_id_pin_ctl		= -1,
};

static const struct dwc2_core_params params_rk3066 = {
	.otg_cap			= 2,	/* non-HNP/non-SRP */
	.otg_ver			= -1,
	.dma_enable			= -1,
	.dma_desc_enable		= 0,
	.speed				= -1,
	.enable_dynamic_fifo		= 1,
	.en_multiple_tx_fifo		= -1,
	.host_rx_fifo_size		= 520,	/* 520 DWORDs */
	.host_nperio_tx_fifo_size	= 128,	/* 128 DWORDs */
	.host_perio_tx_fifo_size	= 256,	/* 256 DWORDs */
	.max_transfer_size		= 65535,
	.max_packet_count		= -1,
	.host_channels			= -1,
	.phy_type			= -1,
	.phy_utmi_width			= -1,
	.phy_ulpi_ddr			= -1,
	.phy_ulpi_ext_vbus		= -1,
	.i2c_enable			= -1,
	.ulpi_fs_ls			= -1,
	.host_support_fs_ls_low_power	= -1,
	.host_ls_low_power_phy_clk	= -1,
	.ts_dline			= -1,
	.reload_ctl			= -1,
	.ahbcfg				= 0x7, /* INCR16 */
	.uframe_sched			= -1,
	.external_id_pin_ctl		= -1,
};

/**
 * dwc2_driver_remove() - Called when the DWC_otg core is unregistered with the
 * DWC_otg driver
 *
 * @dev: Platform device
 *
 * This routine is called, for example, when the rmmod command is executed. The
 * device may or may not be electrically present. If it is present, the driver
 * stops device processing. Any resources used on behalf of this device are
 * freed.
 */
static int dwc2_driver_remove(struct platform_device *dev)
{
	struct dwc2_hsotg *hsotg = platform_get_drvdata(dev);

	dwc2_hcd_remove(hsotg);
	s3c_hsotg_remove(hsotg);

	return 0;
}

static const struct of_device_id dwc2_of_match_table[] = {
	{ .compatible = "brcm,bcm2835-usb", .data = &params_bcm2835 },
	{ .compatible = "rockchip,rk3066-usb", .data = &params_rk3066 },
	{ .compatible = "snps,dwc2", .data = NULL },
	{ .compatible = "samsung,s3c6400-hsotg", .data = NULL},
	{},
};
MODULE_DEVICE_TABLE(of, dwc2_of_match_table);

/**
 * dwc2_driver_probe() - Called when the DWC_otg core is bound to the DWC_otg
 * driver
 *
 * @dev: Platform device
 *
 * This routine creates the driver components required to control the device
 * (core, HCD, and PCD) and initializes the device. The driver components are
 * stored in a dwc2_hsotg structure. A reference to the dwc2_hsotg is saved
 * in the device private data. This allows the driver to access the dwc2_hsotg
 * structure on subsequent calls to driver methods for this device.
 */
static int dwc2_driver_probe(struct platform_device *dev)
{
	struct device_node *np = dev->dev.of_node;
	const struct dwc2_core_params *params;
	struct dwc2_core_params defparams;
	const struct of_device_id *match;
	struct dwc2_hsotg *hsotg;
	struct resource *res;
	struct phy *phy;
	struct usb_phy *uphy;
	int retval;
	int irq;
	int len;

	match = of_match_device(dwc2_of_match_table, &dev->dev);
	if (match && match->data) {
		params = match->data;
	} else if (of_find_property(np, "dwc2_params", &len)) {
		/* Try to get params from device tree */
		int i = 0;
		params = devm_kzalloc(&dev->dev,
				sizeof(struct dwc2_core_params), GFP_KERNEL);
		len /= sizeof(u32);
		if (of_property_read_u32_array(np, "dwc2_params",
					(u32 *) params, len)) {
			dev_err(&dev->dev,
				"can't get dwc2 params from device tree\n");
			return -EINVAL;
		}
		/* Dump all paramters */
		while (i < len) {
			dev_dbg(&dev->dev, "dwc_params[%d] = %d\n",
					i, ((int *) params)[i]);
			i++;
		}
	} else {
		/* Default all params to autodetect */
		dwc2_set_all_params(&defparams, -1);
		params = &defparams;

		/*
		 * Disable descriptor dma mode by default as the HW can support
		 * it, but does not support it for SPLIT transactions.
		 */
		defparams.dma_desc_enable = 0;
	}

	hsotg = devm_kzalloc(&dev->dev, sizeof(*hsotg), GFP_KERNEL);
	if (!hsotg)
		return -ENOMEM;

	hsotg->dev = &dev->dev;

	/*
	 * Use reasonable defaults so platforms don't have to provide these.
	 */
	if (!dev->dev.dma_mask)
		dev->dev.dma_mask = &dev->dev.coherent_dma_mask;
	retval = dma_set_coherent_mask(&dev->dev, DMA_BIT_MASK(32));
	if (retval)
		return retval;

	irq = platform_get_irq(dev, 0);
	if (irq < 0) {
		dev_err(&dev->dev, "missing IRQ resource\n");
		return irq;
	}

	dev_dbg(hsotg->dev, "registering common handler for irq%d\n",
		irq);
	retval = devm_request_irq(hsotg->dev, irq,
				  dwc2_handle_common_intr, IRQF_SHARED,
				  dev_name(hsotg->dev), hsotg);
	if (retval)
		return retval;

	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	hsotg->regs = devm_ioremap_resource(&dev->dev, res);
	if (IS_ERR(hsotg->regs))
		return PTR_ERR(hsotg->regs);

	dev_dbg(&dev->dev, "mapped PA %08lx to VA %p\n",
		(unsigned long)res->start, hsotg->regs);

	hsotg->dr_mode = of_usb_get_dr_mode(dev->dev.of_node);

	/*
	 * Attempt to find a generic PHY, then look for an old style
	 * USB PHY
	 */
	phy = devm_phy_get(&dev->dev, "usb2-phy");
	if (IS_ERR(phy)) {
		hsotg->phy = NULL;
		uphy = devm_usb_get_phy(&dev->dev, USB_PHY_TYPE_USB2);
		if (IS_ERR(uphy))
			hsotg->uphy = NULL;
		else
			hsotg->uphy = uphy;
	} else {
		hsotg->phy = phy;
		phy_power_on(hsotg->phy);
		phy_init(hsotg->phy);
	}

	spin_lock_init(&hsotg->lock);
	mutex_init(&hsotg->init_mutex);
	retval = dwc2_gadget_init(hsotg, irq);
	if (retval)
		return retval;
	retval = dwc2_hcd_init(hsotg, irq, params);
	if (retval)
		return retval;

	platform_set_drvdata(dev, hsotg);

	dwc2_debugfs_init(hsotg);

	return retval;
}

static int __maybe_unused dwc2_suspend(struct device *dev)
{
	struct dwc2_hsotg *dwc2 = dev_get_drvdata(dev);
	int ret = 0;

	if (dwc2_is_device_mode(dwc2)) {
		ret = s3c_hsotg_suspend(dwc2);
	} else {
		if (dwc2->lx_state == DWC2_L0)
			return 0;
		phy_exit(dwc2->phy);
		phy_power_off(dwc2->phy);

	}
	return ret;
}

static int __maybe_unused dwc2_resume(struct device *dev)
{
	struct dwc2_hsotg *dwc2 = dev_get_drvdata(dev);
	int ret = 0;

	if (dwc2_is_device_mode(dwc2)) {
		ret = s3c_hsotg_resume(dwc2);
	} else {
		phy_power_on(dwc2->phy);
		phy_init(dwc2->phy);

	}
	return ret;
}

static const struct dev_pm_ops dwc2_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc2_suspend, dwc2_resume)
};

static struct platform_driver dwc2_platform_driver = {
	.driver = {
		.name = dwc2_driver_name,
		.of_match_table = dwc2_of_match_table,
		.pm = &dwc2_dev_pm_ops,
	},
	.probe = dwc2_driver_probe,
	.remove = dwc2_driver_remove,
};

module_platform_driver(dwc2_platform_driver);

MODULE_DESCRIPTION("DESIGNWARE HS OTG Platform Glue");
MODULE_AUTHOR("Matthijs Kooijman <matthijs@stdin.nl>");
MODULE_LICENSE("Dual BSD/GPL");
