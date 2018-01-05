/******************** (C) COPYRIGHT 2012 STMicroelectronics ********************
 *
 * File Name          : lis2dh_acc.c
 * Authors            : MSH - Motion Mems BU - Application Team
 *		      : Matteo Dameno (matteo.dameno@st.com)
 *		      : Denis Ciocca (carmine.iascone@st.com)
 *		      : Both authors are willing to be considered the contact
 *		      : and update points for the driver.
 * Version            : V.1.0.12
 * Date               : 2012/Feb/29
 * Description        : LIS2DH accelerometer sensor API
 *
 *******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 ******************************************************************************
 Revision 1.0.6 15/11/2010
  first revision
  supports sysfs;
  no more support for ioctl;
 Revision 1.0.7 26/11/2010
  checks for availability of interrupts pins
  correction on FUZZ and FLAT values;
 Revision 1.0.8 2010/Apr/01
  corrects a bug in interrupt pin management in 1.0.7
 Revision 1.0.9: 2011/May/23
  update_odr func correction;
 Revision 1.0.10: 2011/Aug/16
  introduces default_platform_data, i2c_read and i2c_write function rewritten,
  manages smbus beside i2c
 Revision 1.0.11: 2012/Jan/09
  moved under input/misc
 Revision 1.0.12: 2012/Feb/29
  moved use_smbus inside status struct; modified:-update_fs_range;-set_range
  input format; allows gpio_intX to be passed as parameter at insmod time;
 ******************************************************************************/

#include	<linux/err.h>
#include	<linux/errno.h>
#include	<linux/delay.h>
#include	<linux/fs.h>
#include	<linux/i2c.h>
#include	<linux/input.h>
#include	<linux/uaccess.h>
#include	<linux/workqueue.h>
#include	<linux/irq.h>
#include	<linux/gpio.h>
#include	<linux/interrupt.h>
#include	<linux/slab.h>
#include	<linux/kernel.h>
#include	<linux/device.h>
#include	<linux/module.h>
#include	<linux/moduleparam.h>

#include <linux/miscdevice.h>
#include <linux/sensor-dev.h>

#ifndef CONFIG_PINCTRL
#include <mach/gpio.h>
#else
#include <linux/pinctrl/consumer.h>
#endif


#include	<linux/input/lis2dh.h>

/* #define	DEBUG		1 */

#define	G_MAX		16000


#define SENSITIVITY_2G		1	/**	mg/LSB	*/
#define SENSITIVITY_4G		2	/**	mg/LSB	*/
#define SENSITIVITY_8G		4	/**	mg/LSB	*/
#define SENSITIVITY_16G		12	/**	mg/LSB	*/




/* Accelerometer Sensor Operating Mode */
#define LIS2DH_ACC_ENABLE	(0x01)
#define LIS2DH_ACC_DISABLE	(0x00)

#define	HIGH_RESOLUTION		(0x08)

#define	AXISDATA_REG		(0x28)
#define WHOAMI_LIS2DH_ACC	(0x33)	/*	Expctd content for WAI	*/

/*	CONTROL REGISTERS	*/
#define WHO_AM_I		(0x0F)	/*	WhoAmI register		*/
#define	TEMP_CFG_REG		(0x1F)	/*	temper sens control reg	*/
/* ctrl 1: ODR3 ODR2 ODR ODR0 LPen Zenable Yenable Zenable */
#define	CTRL_REG1		(0x20)	/*	control reg 1		*/
#define	CTRL_REG2		(0x21)	/*	control reg 2		*/
#define	CTRL_REG3		(0x22)	/*	control reg 3		*/
#define	CTRL_REG4		(0x23)	/*	control reg 4		*/
#define	CTRL_REG5		(0x24)	/*	control reg 5		*/
#define	CTRL_REG6		(0x25)	/*	control reg 6		*/

#define	FIFO_CTRL_REG		(0x2E)	/*	FiFo control reg	*/

#define	INT_CFG1		(0x30)	/*	interrupt 1 config	*/
#define	INT_SRC1		(0x31)	/*	interrupt 1 source	*/
#define	INT_THS1		(0x32)	/*	interrupt 1 threshold	*/
#define	INT_DUR1		(0x33)	/*	interrupt 1 duration	*/


#define	TT_CFG			(0x38)	/*	tap config		*/
#define	TT_SRC			(0x39)	/*	tap source		*/
#define	TT_THS			(0x3A)	/*	tap threshold		*/
#define	TT_LIM			(0x3B)	/*	tap time limit		*/
#define	TT_TLAT			(0x3C)	/*	tap time latency	*/
#define	TT_TW			(0x3D)	/*	tap time window		*/
/*	end CONTROL REGISTRES	*/


#define ENABLE_HIGH_RESOLUTION	1
#define ALL_ZEROES		(0x00)

#define LIS2DH_ACC_PM_OFF		(0x00)
#define LIS2DH_ACC_ENABLE_ALL_AXES	(0x07)


#define PMODE_MASK		(0x08)
#define ODR_MASK		(0XF0)

#define LIS2DH_ACC_ODR1	(0x10)  /* 1Hz output data rate */
#define LIS2DH_ACC_ODR10	(0x20)  /* 10Hz output data rate */
#define LIS2DH_ACC_ODR25	(0x30)  /* 25Hz output data rate */
#define LIS2DH_ACC_ODR50	(0x40)  /* 50Hz output data rate */
#define LIS2DH_ACC_ODR100	(0x50)  /* 100Hz output data rate */
#define LIS2DH_ACC_ODR200	(0x60)  /* 200Hz output data rate */
#define LIS2DH_ACC_ODR400	(0x70)  /* 400Hz output data rate */
#define LIS2DH_ACC_ODR1250	(0x90)  /* 1250Hz output data rate */



#define	IA			(0x40)
#define	ZH			(0x20)
#define	ZL			(0x10)
#define	YH			(0x08)
#define	YL			(0x04)
#define	XH			(0x02)
#define	XL			(0x01)
/* */
/* CTRL REG BITS*/
#define	CTRL_REG3_I1_AOI1	(0x40)
#define	CTRL_REG4_BDU_ENABLE	(0x80)
#define	CTRL_REG4_BDU_MASK	(0x80)
#define	CTRL_REG6_I2_TAPEN	(0x80)
#define	CTRL_REG6_HLACTIVE	(0x02)
/* */
#define NO_MASK			(0xFF)
#define INT1_DURATION_MASK	(0x7F)
#define	INT1_THRESHOLD_MASK	(0x7F)
#define TAP_CFG_MASK		(0x3F)
#define	TAP_THS_MASK		(0x7F)
#define	TAP_TLIM_MASK		(0x7F)
#define	TAP_TLAT_MASK		NO_MASK
#define	TAP_TW_MASK		NO_MASK


/* TAP_SOURCE_REG BIT */
#define	DTAP			(0x20)
#define	STAP			(0x10)
#define	SIGNTAP			(0x08)
#define	ZTAP			(0x04)
#define	YTAP			(0x02)
#define	XTAZ			(0x01)


#define	FUZZ			0
#define	FLAT			0
#define	I2C_RETRY_DELAY		5
#define	I2C_RETRIES		5
#define	I2C_AUTO_INCREMENT	(0x80)

/* RESUME STATE INDICES */
#define	RES_CTRL_REG1		0
#define	RES_CTRL_REG2		1
#define	RES_CTRL_REG3		2
#define	RES_CTRL_REG4		3
#define	RES_CTRL_REG5		4
#define	RES_CTRL_REG6		5

#define	RES_INT_CFG1		6
#define	RES_INT_THS1		7
#define	RES_INT_DUR1		8

#define	RES_TT_CFG		9
#define	RES_TT_THS		10
#define	RES_TT_LIM		11
#define	RES_TT_TLAT		12
#define	RES_TT_TW		13

#define	RES_TEMP_CFG_REG	14
#define	RES_REFERENCE_REG	15
#define	RES_FIFO_CTRL_REG	16

#define	RESUME_ENTRIES		17
/* end RESUME STATE INDICES */


struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} lis2dh_acc_odr_table[] = {
		{    1, LIS2DH_ACC_ODR1250 },
		{    3, LIS2DH_ACC_ODR400  },
		{    5, LIS2DH_ACC_ODR200  },
		{   10, LIS2DH_ACC_ODR100  },
		{   20, LIS2DH_ACC_ODR50   },
		{   40, LIS2DH_ACC_ODR25   },
		{  100, LIS2DH_ACC_ODR10   },
		{ 1000, LIS2DH_ACC_ODR1    },
};

static int int1_gpio = LIS2DH_ACC_DEFAULT_INT1_GPIO;
static int int2_gpio = LIS2DH_ACC_DEFAULT_INT2_GPIO;
module_param(int1_gpio, int, S_IRUGO);
module_param(int2_gpio, int, S_IRUGO);

struct lis2dh_acc_status {
	struct i2c_client *client;
	struct lis2dh_acc_platform_data *pdata;

	struct mutex lock;
	struct delayed_work input_work;

	struct input_dev *input_dev;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	int on_before_suspend;
	int use_smbus;

	u8 sensitivity;

	u8 resume_state[RESUME_ENTRIES];

	int irq1;
	struct work_struct irq1_work;
	struct workqueue_struct *irq1_work_queue;
	int irq2;
	struct work_struct irq2_work;
	struct workqueue_struct *irq2_work_queue;

#ifdef DEBUG
	u8 reg_addr;
#endif
};

static struct lis2dh_acc_platform_data default_lis2dh_acc_pdata = {
	.fs_range = LIS2DH_ACC_G_2G,
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,
	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,
	.poll_interval = 100,
	.min_interval = LIS2DH_ACC_MIN_POLL_PERIOD_MS,
	.gpio_int1 = LIS2DH_ACC_DEFAULT_INT1_GPIO,
	.gpio_int2 = LIS2DH_ACC_DEFAULT_INT2_GPIO,
};

#if 0
#define SENSOR_DEBUG
#endif

#ifdef SENSOR_DEBUG
#define DTRACE(format, args...)	\
	printk (KERN_CRIT " %s: " format, __func__, ##args)
#else
#define DTRACE(x...)
#endif


#ifdef CONFIG_OF

static int acc_power_on(struct device *dev)
{
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	struct device_pm_platdata *pm_platdata = stat->pdata->pm_platdata;
	int err = device_state_pm_set_state_by_name(dev,
			pm_platdata->pm_state_D0_name);
	if (err)
		dev_err(dev, "Power ON Failed: %d\n", err);

	return err;
}

static int acc_power_off(struct device *dev)
{
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	struct device_pm_platdata *pm_platdata = stat->pdata->pm_platdata;
	int err = device_state_pm_set_state_by_name(dev,
			pm_platdata->pm_state_D3_name);

	if (err)
		dev_err(dev, "Power OFF Failed:\n");

	return err;
}

static int acc_init(struct device *dev)
{
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	struct device_pm_platdata *pm_platdata = stat->pdata->pm_platdata;
	
	if (!pm_platdata || !pm_platdata->pm_state_D0_name ||
			!pm_platdata->pm_state_D3_name)
	{
		dev_err(dev, "jdy acc_init pm_platdata->pm_state_D0_name is NULL. \n");
		return -EINVAL;
	}

	
	return device_state_pm_set_class(dev, pm_platdata->pm_user_name);
}

static void acc_exit(void)
{
	return;
}

#define OF_AXIS_MAP		"intel,axis-map"
#define OF_NEGATE		"intel,negate"
#define OF_POLL_INTERVAL	"intel,poll-interval"

static struct lis2dh_acc_platform_data *lis2dh_acc_of_get_platdata(
		struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct lis2dh_acc_platform_data *acc_pdata;
	u32 out_values[3];
	int ret = 0;

	dev_err(dev, "lis2dh_acc_of_get_platdata \n");
	
	acc_pdata = kzalloc(sizeof(*acc_pdata), GFP_KERNEL);

	if (!acc_pdata)
		return ERR_PTR(-ENOMEM);

	/* pinctrl */
	acc_pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(acc_pdata->pinctrl))
		goto skip_pinctrl;

	acc_pdata->pins_default = pinctrl_lookup_state(acc_pdata->pinctrl,
			PINCTRL_STATE_DEFAULT);
	if (IS_ERR(acc_pdata->pins_default))
		dev_err(dev, "could not get default pinstate\n");

	acc_pdata->pins_sleep = pinctrl_lookup_state(acc_pdata->pinctrl,
			PINCTRL_STATE_SLEEP);
	if (IS_ERR(acc_pdata->pins_sleep))
		dev_err(dev, "could not get sleep pinstate\n");

	acc_pdata->pins_inactive = pinctrl_lookup_state(acc_pdata->pinctrl,
			"inactive");
	if (IS_ERR(acc_pdata->pins_inactive))
		dev_err(dev, "could not get inactive pinstate\n");

skip_pinctrl:
	/* Axis map properties */
	if (of_property_read_u32_array(np, OF_AXIS_MAP, out_values, 3) < 0) {

		dev_err(dev, "Error parsing %s property of node %s\n",
			OF_AXIS_MAP, np->name);
		goto out;
	}
	acc_pdata->axis_map_x = (u8)out_values[0];
	acc_pdata->axis_map_y = (u8)out_values[1];
	acc_pdata->axis_map_z = (u8)out_values[2];

	/* Negate properties */
	if (of_property_read_u32_array(np, OF_NEGATE, out_values, 3) < 0) {

		dev_err(dev, "Error parsing %s property of node %s\n",
			OF_NEGATE, np->name);
		goto out;
	}
	acc_pdata->negate_x = (u8)out_values[0];
	acc_pdata->negate_y = (u8)out_values[1];
	acc_pdata->negate_z = (u8)out_values[2];

	/* Poll interval property */
	if (of_property_read_u32(np, OF_POLL_INTERVAL,
				&acc_pdata->poll_interval) < 0) {

		dev_err(dev, "Error parsing %s property of node %s\n",
			OF_POLL_INTERVAL, np->name);
		goto out;
	}

	/* device pm */
	acc_pdata->pm_platdata = of_device_state_pm_setup(np);
	if (IS_ERR(acc_pdata->pm_platdata)) {
		dev_err(dev, "Error during device state pm init\n");
		ret = -EINVAL;
		goto out;
	}

	acc_pdata->init = acc_init;
	acc_pdata->exit = acc_exit;
	acc_pdata->power_on = acc_power_on;
	acc_pdata->power_off = acc_power_off;

	/* FIXME: set default values */
	acc_pdata->fs_range = LIS2DH_ACC_G_2G;
	acc_pdata->min_interval = LIS2DH_ACC_MIN_POLL_PERIOD_MS; /* 2ms */
	acc_pdata->gpio_int1 =
		LIS2DH_ACC_DEFAULT_INT1_GPIO;
	acc_pdata->gpio_int2 =
		LIS2DH_ACC_DEFAULT_INT2_GPIO;

	dev_err(dev, "lis2dh_acc_of_get_platdata done ok\n");

	return acc_pdata;

out:
	kfree(acc_pdata);
	return ERR_PTR(ret);
}

#endif

static inline int lis2dh_acc_set_pinctrl_state(struct device *dev,
		struct pinctrl_state *state)
{
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	struct lis2dh_acc_platform_data *pdata = stat->pdata;
	int ret = 0;

	if (!pdata) {
		dev_err(dev,
			"Unable to retrieve lsm303dlhc_acc platform data\n");
		return -EINVAL;
	}

	if (!IS_ERR_OR_NULL(state)) {
		ret = pinctrl_select_state(pdata->pinctrl, state);
		if (ret)
			dev_err(dev, "%d:could not set pins\n", __LINE__);
	}
	return ret;
}




static int lis2dh_acc_i2c_read(struct lis2dh_acc_status *stat, u8 *buf,
									int len)
{
	int ret;
	u8 reg = buf[0];
	u8 cmd = reg;

/*
	if (len > sizeof(buf))
			dev_err(&stat->client->dev,
				"read error insufficient buffer length: "
				"len:%d, buf size=%d\n",
				len, sizeof(buf));
*/
	if (len > 1)
		cmd = (I2C_AUTO_INCREMENT | reg);
	if (stat->use_smbus) {
		if (len == 1) {
			ret = i2c_smbus_read_byte_data(stat->client, cmd);
			buf[0] = ret & 0xff;
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_read_byte_data: ret=0x%02x, len:%d ,"
				"command=0x%02x, buf[0]=0x%02x\n",
				ret, len, cmd , buf[0]);
#endif
		} else if (len > 1) {
			ret = i2c_smbus_read_i2c_block_data(stat->client,
								cmd, len, buf);
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_read_i2c_block_data: ret:%d len:%d, "
				"command=0x%02x, ",
				ret, len, cmd);
			unsigned int ii;
			for (ii = 0; ii < len; ii++)
				printk(KERN_DEBUG "buf[%d]=0x%02x,",
								ii, buf[ii]);

			printk("\n");
#endif
		} else
			ret = -1;

		if (ret < 0) {
			dev_err(&stat->client->dev,
				"read transfer error: len:%d, command=0x%02x\n",
				len, cmd);
			return 0; /* failure */
		}
		return len; /* success */
	}

	ret = i2c_master_send(stat->client, &cmd, sizeof(cmd));
	if (ret != sizeof(cmd))
		return ret;

	return i2c_master_recv(stat->client, buf, len);
}

static int lis2dh_acc_i2c_write(struct lis2dh_acc_status *stat, u8 *buf,
									int len)
{
	int ret;
	u8 reg, value;

	if (len > 1)
		buf[0] = (I2C_AUTO_INCREMENT | buf[0]);

	reg = buf[0];
	value = buf[1];

	if (stat->use_smbus) {
		if (len == 1) {
			ret = i2c_smbus_write_byte_data(stat->client,
								reg, value);
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_write_byte_data: ret=%d, len:%d, "
				"command=0x%02x, value=0x%02x\n",
				ret, len, reg , value);
#endif
			return ret;
		} else if (len > 1) {
			ret = i2c_smbus_write_i2c_block_data(stat->client,
							reg, len, buf + 1);
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_write_i2c_block_data: ret=%d, "
				"len:%d, command=0x%02x, ",
				ret, len, reg);
			unsigned int ii;
			for (ii = 0; ii < (len + 1); ii++)
				printk(KERN_DEBUG "value[%d]=0x%02x,",
								ii, buf[ii]);

			printk("\n");
#endif
			return ret;
		}
	}

	ret = i2c_master_send(stat->client, buf, len+1);
	return (ret == len+1) ? 0 : ret;
}

static int lis2dh_acc_hw_init(struct lis2dh_acc_status *stat)
{
	int err = -1;
	u8 buf[7];

	pr_info("%s: hw init start\n", LIS2DH_ACC_DEV_NAME);

	buf[0] = WHO_AM_I;
	err = lis2dh_acc_i2c_read(stat, buf, 1);
	if (err < 0) {
		dev_warn(&stat->client->dev, "Error reading WHO_AM_I:"
				" is device available/working?\n");
		goto err_firstread;
	} else
		stat->hw_working = 1;

	if (buf[0] != WHOAMI_LIS2DH_ACC) {
		dev_err(&stat->client->dev,
			"device unknown. Expected: 0x%02x,"
			" Replies: 0x%02x\n",
			WHOAMI_LIS2DH_ACC, buf[0]);
		err = -1; /* choose the right coded error */
		goto err_unknown_device;
	}


	buf[0] = CTRL_REG1;
	buf[1] = stat->resume_state[RES_CTRL_REG1];
	err = lis2dh_acc_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TEMP_CFG_REG;
	buf[1] = stat->resume_state[RES_TEMP_CFG_REG];
	err = lis2dh_acc_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = FIFO_CTRL_REG;
	buf[1] = stat->resume_state[RES_FIFO_CTRL_REG];
	err = lis2dh_acc_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TT_THS;
	buf[1] = stat->resume_state[RES_TT_THS];
	buf[2] = stat->resume_state[RES_TT_LIM];
	buf[3] = stat->resume_state[RES_TT_TLAT];
	buf[4] = stat->resume_state[RES_TT_TW];
	err = lis2dh_acc_i2c_write(stat, buf, 4);
	if (err < 0)
		goto err_resume_state;
	buf[0] = TT_CFG;
	buf[1] = stat->resume_state[RES_TT_CFG];
	err = lis2dh_acc_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = INT_THS1;
	buf[1] = stat->resume_state[RES_INT_THS1];
	buf[2] = stat->resume_state[RES_INT_DUR1];
	err = lis2dh_acc_i2c_write(stat, buf, 2);
	if (err < 0)
		goto err_resume_state;
	buf[0] = INT_CFG1;
	buf[1] = stat->resume_state[RES_INT_CFG1];
	err = lis2dh_acc_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;


	buf[0] = CTRL_REG2;
	buf[1] = stat->resume_state[RES_CTRL_REG2];
	buf[2] = stat->resume_state[RES_CTRL_REG3];
	buf[3] = stat->resume_state[RES_CTRL_REG4];
	buf[4] = stat->resume_state[RES_CTRL_REG5];
	buf[5] = stat->resume_state[RES_CTRL_REG6];
	err = lis2dh_acc_i2c_write(stat, buf, 5);
	if (err < 0)
		goto err_resume_state;

	stat->hw_initialized = 1;
	pr_info("%s: hw init done\n", LIS2DH_ACC_DEV_NAME);
	return 0;

err_firstread:
	stat->hw_working = 0;
err_unknown_device:
err_resume_state:
	stat->hw_initialized = 0;
	dev_err(&stat->client->dev, "hw init error 0x%02x,0x%02x: %d\n", buf[0],
			buf[1], err);
	return err;
}

static void lis2dh_acc_device_power_off(struct lis2dh_acc_status *stat)
{
	int err;
	u8 buf[2] = { CTRL_REG1, LIS2DH_ACC_PM_OFF };

	err = lis2dh_acc_i2c_write(stat, buf, 1);
	if (err < 0)
		dev_err(&stat->client->dev, "soft power off failed: %d\n", err);

	if (stat->pdata->power_off) {
		if (stat->pdata->gpio_int1 >= 0)
			disable_irq_nosync(stat->irq1);
		if (stat->pdata->gpio_int2 >= 0)
			disable_irq_nosync(stat->irq2);
		stat->pdata->power_off(&stat->input_dev->dev);
		stat->hw_initialized = 0;
	}

	DTRACE("Notice gpio_int1=%d, gpio_int2=%d \n", stat->pdata->gpio_int1, stat->pdata->gpio_int2);
	if (stat->hw_initialized) {
		if (stat->pdata->gpio_int1 >= 0)
			disable_irq_nosync(stat->irq1);
		if (stat->pdata->gpio_int2 >= 0)
			disable_irq_nosync(stat->irq2);
		stat->hw_initialized = 0;
	}

}

static int lis2dh_acc_device_power_on(struct lis2dh_acc_status *stat)
{
	int err = -1;

	if (stat->pdata->power_on) {
		err = stat->pdata->power_on(&stat->input_dev->dev);
		if (err < 0) {
			dev_err(&stat->client->dev,
					"power_on failed: %d\n", err);
			return err;
		}
		if (stat->pdata->gpio_int1 >= 0)
			enable_irq(stat->irq1);
		if (stat->pdata->gpio_int2 >= 0)
			enable_irq(stat->irq2);
	}

	if (!stat->hw_initialized) {
		err = lis2dh_acc_hw_init(stat);
		if (stat->hw_working == 1 && err < 0) {
			lis2dh_acc_device_power_off(stat);
			return err;
		}
	}

	DTRACE("Notice gpio_int1=%d, gpio_int2=%d stat->hw_initialized=%d \n", stat->pdata->gpio_int1, stat->pdata->gpio_int2, stat->hw_initialized);
	if (stat->hw_initialized) {
		if (stat->pdata->gpio_int1 >= 0)
			enable_irq(stat->irq1);
		if (stat->pdata->gpio_int2 >= 0)
			enable_irq(stat->irq2);
	}
	return 0;
}

static irqreturn_t lis2dh_acc_isr1(int irq, void *dev)
{
	struct lis2dh_acc_status *stat = dev;

	disable_irq_nosync(irq);
	queue_work(stat->irq1_work_queue, &stat->irq1_work);
	pr_debug("%s: isr1 queued\n", LIS2DH_ACC_DEV_NAME);

	return IRQ_HANDLED;
}

static irqreturn_t lis2dh_acc_isr2(int irq, void *dev)
{
	struct lis2dh_acc_status *stat = dev;

	disable_irq_nosync(irq);
	queue_work(stat->irq2_work_queue, &stat->irq2_work);
	pr_debug("%s: isr2 queued\n", LIS2DH_ACC_DEV_NAME);

	return IRQ_HANDLED;
}

static void lis2dh_acc_irq1_work_func(struct work_struct *work)
{

	struct lis2dh_acc_status *stat =
	container_of(work, struct lis2dh_acc_status, irq1_work);
	/* TODO  add interrupt service procedure.
		 ie:lis2dh_acc_get_int1_source(stat); */

	/* ; */
	pr_debug("%s: IRQ1 triggered\n", LIS2DH_ACC_DEV_NAME);
/* exit: */
	enable_irq(stat->irq1);
}

static void lis2dh_acc_irq2_work_func(struct work_struct *work)
{

	struct lis2dh_acc_status *stat =
	container_of(work, struct lis2dh_acc_status, irq2_work);
	/* TODO  add interrupt service procedure.
		 ie:lis2dh_acc_get_tap_source(stat); */

	/* ; */

	pr_debug("%s: IRQ2 triggered\n", LIS2DH_ACC_DEV_NAME);
/* exit: */
	enable_irq(stat->irq2);
}

static int lis2dh_acc_update_fs_range(struct lis2dh_acc_status *stat,
							u8 new_fs_range)
{
	int err = -1;

	u8 sensitivity;
	u8 buf[2];
	u8 updated_val;
	u8 init_val;
	u8 new_val;
	u8 mask = LIS2DH_ACC_FS_MASK | HIGH_RESOLUTION;

	switch (new_fs_range) {
	case LIS2DH_ACC_G_2G:

		sensitivity = SENSITIVITY_2G;
		break;
	case LIS2DH_ACC_G_4G:

		sensitivity = SENSITIVITY_4G;
		break;
	case LIS2DH_ACC_G_8G:

		sensitivity = SENSITIVITY_8G;
		break;
	case LIS2DH_ACC_G_16G:

		sensitivity = SENSITIVITY_16G;
		break;
	default:
		dev_err(&stat->client->dev, "invalid fs range requested: %u\n",
				new_fs_range);
		return -EINVAL;
	}


	/* Updates configuration register 4,
	* which contains fs range setting */
	buf[0] = CTRL_REG4;
	err = lis2dh_acc_i2c_read(stat, buf, 1);
	if (err < 0)
		goto error;
	init_val = buf[0];
	stat->resume_state[RES_CTRL_REG4] = init_val;
	new_val = new_fs_range | HIGH_RESOLUTION;
	updated_val = ((mask & new_val) | ((~mask) & init_val));
	buf[1] = updated_val;
	buf[0] = CTRL_REG4;
	err = lis2dh_acc_i2c_write(stat, buf, 1);
	if (err < 0)
		goto error;
	stat->resume_state[RES_CTRL_REG4] = updated_val;
	stat->sensitivity = sensitivity;

	return err;
error:
	dev_err(&stat->client->dev,
			"update fs range failed 0x%02x,0x%02x: %d\n",
			buf[0], buf[1], err);

	return err;
}

static int lis2dh_acc_update_odr(struct lis2dh_acc_status *stat,
							int poll_interval_ms)
{
	int err = -1;
	int i;
	u8 config[2];

	/* Following, looks for the longest possible odr interval scrolling the
	 * odr_table vector from the end (shortest interval) backward (longest
	 * interval), to support the poll_interval requested by the system.
	 * It must be the longest interval lower then the poll interval.*/
	for (i = ARRAY_SIZE(lis2dh_acc_odr_table) - 1; i >= 0; i--) {
		if ((lis2dh_acc_odr_table[i].cutoff_ms <= poll_interval_ms)
								|| (i == 0))
			break;
	}
	config[1] = lis2dh_acc_odr_table[i].mask;

	config[1] |= LIS2DH_ACC_ENABLE_ALL_AXES;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&stat->enabled)) {
		config[0] = CTRL_REG1;
		err = lis2dh_acc_i2c_write(stat, config, 1);
		if (err < 0)
			goto error;
		stat->resume_state[RES_CTRL_REG1] = config[1];
	}

	return err;

error:
	dev_err(&stat->client->dev, "update odr failed 0x%02x,0x%02x: %d\n",
			config[0], config[1], err);

	return err;
}



static int lis2dh_acc_register_write(struct lis2dh_acc_status *stat,
					u8 *buf, u8 reg_address, u8 new_value)
{
	int err = -1;

		/* Sets configuration register at reg_address
		 *  NOTE: this is a straight overwrite  */
		buf[0] = reg_address;
		buf[1] = new_value;
		err = lis2dh_acc_i2c_write(stat, buf, 1);
		if (err < 0)
			return err;
	return err;
}

/*
static int lis2dh_acc_register_read(struct lis2dh_acc_status *stat,
							u8 *buf, u8 reg_address)
{

	int err = -1;
	buf[0] = (reg_address);
	err = lis2dh_acc_i2c_read(stat, buf, 1);
	return err;
}
*/

/*
static int lis2dh_acc_register_update(struct lis2dh_acc_status *stat,
		u8 *buf, u8 reg_address, u8 mask, u8 new_bit_values)
{
	int err = -1;
	u8 init_val;
	u8 updated_val;
	err = lis2dh_acc_register_read(stat, buf, reg_address);
	if (!(err < 0)) {
		init_val = buf[1];
		updated_val = ((mask & new_bit_values) | ((~mask) & init_val));
		err = lis2dh_acc_register_write(stat, buf, reg_address,
				updated_val);
	}
	return err;
}
*/

static int lis2dh_acc_get_acceleration_data(
				struct lis2dh_acc_status *stat, int *xyz)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	/* x,y,z hardware data */
	s16 hw_d[3] = { 0 };

	acc_data[0] = (AXISDATA_REG);
	err = lis2dh_acc_i2c_read(stat, acc_data, 6);
	if (err < 0)
		return err;

	hw_d[0] = (((s16) ((acc_data[1] << 8) | acc_data[0])) >> 4);
	hw_d[1] = (((s16) ((acc_data[3] << 8) | acc_data[2])) >> 4);
	hw_d[2] = (((s16) ((acc_data[5] << 8) | acc_data[4])) >> 4);



	hw_d[0] = hw_d[0] * stat->sensitivity;
	hw_d[1] = hw_d[1] * stat->sensitivity;
	hw_d[2] = hw_d[2] * stat->sensitivity;


	xyz[0] = ((stat->pdata->negate_x) ? (-hw_d[stat->pdata->axis_map_x])
		   : (hw_d[stat->pdata->axis_map_x]));
	xyz[1] = ((stat->pdata->negate_y) ? (-hw_d[stat->pdata->axis_map_y])
		   : (hw_d[stat->pdata->axis_map_y]));
	xyz[2] = ((stat->pdata->negate_z) ? (-hw_d[stat->pdata->axis_map_z])
		   : (hw_d[stat->pdata->axis_map_z]));

#ifdef DEBUG
/*
	pr_debug("%s read x=%d, y=%d, z=%d\n",
			LIS2DH_ACC_DEV_NAME, xyz[0], xyz[1], xyz[2]);
*/
#endif
	return err;
}

static void lis2dh_acc_report_values(struct lis2dh_acc_status *stat,
					int *xyz)
{
	input_report_abs(stat->input_dev, ABS_X, xyz[0]);
	input_report_abs(stat->input_dev, ABS_Y, xyz[1]);
	input_report_abs(stat->input_dev, ABS_Z, xyz[2]);
	input_sync(stat->input_dev);
}

static int lis2dh_acc_enable(struct lis2dh_acc_status *stat)
{
	int err;

	if (!atomic_cmpxchg(&stat->enabled, 0, 1)) {
		err = lis2dh_acc_device_power_on(stat);
		if (err < 0) {
			atomic_set(&stat->enabled, 0);
			return err;
		}
		schedule_delayed_work(&stat->input_work,
			msecs_to_jiffies(stat->pdata->poll_interval));
	}

	return 0;
}

static int lis2dh_acc_disable(struct lis2dh_acc_status *stat)
{
	if (atomic_cmpxchg(&stat->enabled, 1, 0)) {
		cancel_delayed_work_sync(&stat->input_work);
		lis2dh_acc_device_power_off(stat);
	}

	return 0;
}


static ssize_t read_single_reg(struct device *dev, char *buf, u8 reg)
{
	ssize_t ret;
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	int err;

	u8 data = reg;
	err = lis2dh_acc_i2c_read(stat, &data, 1);
	if (err < 0)
		return err;
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;

}

static int write_reg(struct device *dev, const char *buf, u8 reg,
		u8 mask, int resumeIndex)
{
	int err = -1;
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	u8 x[2];
	u8 new_val;
	unsigned long val;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;

	new_val = ((u8) val & mask);
	x[0] = reg;
	x[1] = new_val;
	err = lis2dh_acc_register_write(stat, x, reg, new_val);
	if (err < 0)
		return err;
	stat->resume_state[resumeIndex] = new_val;
	return err;
}

static ssize_t attr_get_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int val;
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	mutex_lock(&stat->lock);
	val = stat->pdata->poll_interval;
	mutex_unlock(&stat->lock);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (strict_strtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;
	interval_ms = max((unsigned int)interval_ms, stat->pdata->min_interval);
	mutex_lock(&stat->lock);
	stat->pdata->poll_interval = interval_ms;
	lis2dh_acc_update_odr(stat, interval_ms);
	mutex_unlock(&stat->lock);
	return size;
}

static ssize_t attr_get_range(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	char val;
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	char range = 2;
	mutex_lock(&stat->lock);
	val = stat->pdata->fs_range ;
	switch (val) {
	case LIS2DH_ACC_G_2G:
		range = 2;
		break;
	case LIS2DH_ACC_G_4G:
		range = 4;
		break;
	case LIS2DH_ACC_G_8G:
		range = 8;
		break;
	case LIS2DH_ACC_G_16G:
		range = 16;
		break;
	}
	mutex_unlock(&stat->lock);
	return sprintf(buf, "%d\n", range);
}

static ssize_t attr_set_range(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	unsigned long val;
	u8 range;
	int err;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	switch (val) {
	case 2:
		range = LIS2DH_ACC_G_2G;
		break;
	case 4:
		range = LIS2DH_ACC_G_4G;
		break;
	case 8:
		range = LIS2DH_ACC_G_8G;
		break;
	case 16:
		range = LIS2DH_ACC_G_16G;
		break;
	default:
		dev_err(&stat->client->dev, "invalid range request: %lu,"
				" discarded\n", val);
		return -EINVAL;
	}
	mutex_lock(&stat->lock);
	err = lis2dh_acc_update_fs_range(stat, range);
	if (err < 0) {
		mutex_unlock(&stat->lock);
		return err;
	}
	stat->pdata->fs_range = range;
	mutex_unlock(&stat->lock);
	dev_info(&stat->client->dev, "range set to: %lu g\n", val);

	return size;
}

static ssize_t attr_get_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	int val = atomic_read(&stat->enabled);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lis2dh_acc_enable(stat);
	else
		lis2dh_acc_disable(stat);

	return size;
}

static ssize_t attr_set_intconfig1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, INT_CFG1, NO_MASK, RES_INT_CFG1);
}

static ssize_t attr_get_intconfig1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, INT_CFG1);
}

static ssize_t attr_set_duration1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, INT_DUR1, INT1_DURATION_MASK, RES_INT_DUR1);
}

static ssize_t attr_get_duration1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, INT_DUR1);
}

static ssize_t attr_set_thresh1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, INT_THS1, INT1_THRESHOLD_MASK, RES_INT_THS1);
}

static ssize_t attr_get_thresh1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, INT_THS1);
}

static ssize_t attr_get_source1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return read_single_reg(dev, buf, INT_SRC1);
}

static ssize_t attr_set_click_cfg(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_CFG, TAP_CFG_MASK, RES_TT_CFG);
}

static ssize_t attr_get_click_cfg(struct device *dev,
		struct device_attribute *attr,	char *buf)
{

	return read_single_reg(dev, buf, TT_CFG);
}

static ssize_t attr_get_click_source(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_SRC);
}

static ssize_t attr_set_click_ths(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_THS, TAP_THS_MASK, RES_TT_THS);
}

static ssize_t attr_get_click_ths(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_THS);
}

static ssize_t attr_set_click_tlim(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_LIM, TAP_TLIM_MASK, RES_TT_LIM);
}

static ssize_t attr_get_click_tlim(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_LIM);
}

static ssize_t attr_set_click_tlat(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_TLAT, TAP_TLAT_MASK, RES_TT_TLAT);
}

static ssize_t attr_get_click_tlat(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_TLAT);
}

static ssize_t attr_set_click_tw(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_TLAT, TAP_TW_MASK, RES_TT_TLAT);
}

static ssize_t attr_get_click_tw(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_TLAT);
}


#ifdef DEBUG
/* PAY ATTENTION: These DEBUG functions don't manage resume_state */
static ssize_t attr_reg_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	int rc;
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	u8 x[2];
	unsigned long val;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&stat->lock);
	x[0] = stat->reg_addr;
	mutex_unlock(&stat->lock);
	x[1] = val;
	rc = lis2dh_acc_i2c_write(stat, x, 1);
	/*TODO: error need to be managed */
	return size;
}

static ssize_t attr_reg_get(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t ret;
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	int rc;
	u8 data;

	mutex_lock(&stat->lock);
	data = stat->reg_addr;
	mutex_unlock(&stat->lock);
	rc = lis2dh_acc_i2c_read(stat, &data, 1);
	/*TODO: error need to be managed */
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;
}

static ssize_t attr_addr_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct lis2dh_acc_status *stat = dev_get_drvdata(dev);
	unsigned long val;
	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&stat->lock);
	stat->reg_addr = val;
	mutex_unlock(&stat->lock);
	return size;
}
#endif

static struct device_attribute attributes[] = {

	__ATTR(pollrate_ms, 0664, attr_get_polling_rate, attr_set_polling_rate),
	__ATTR(range, 0664, attr_get_range, attr_set_range),
	__ATTR(enable_device, 0664, attr_get_enable, attr_set_enable),
	__ATTR(int1_config, 0664, attr_get_intconfig1, attr_set_intconfig1),
	__ATTR(int1_duration, 0664, attr_get_duration1, attr_set_duration1),
	__ATTR(int1_threshold, 0664, attr_get_thresh1, attr_set_thresh1),
	__ATTR(int1_source, 0444, attr_get_source1, NULL),
	__ATTR(click_config, 0664, attr_get_click_cfg, attr_set_click_cfg),
	__ATTR(click_source, 0444, attr_get_click_source, NULL),
	__ATTR(click_threshold, 0664, attr_get_click_ths, attr_set_click_ths),
	__ATTR(click_timelimit, 0664, attr_get_click_tlim, attr_set_click_tlim),
	__ATTR(click_timelatency, 0664, attr_get_click_tlat,
							attr_set_click_tlat),
	__ATTR(click_timewindow, 0664, attr_get_click_tw, attr_set_click_tw),

#ifdef DEBUG
	__ATTR(reg_value, 0600, attr_reg_get, attr_reg_set),
	__ATTR(reg_addr, 0200, NULL, attr_addr_set),
#endif
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto error;
	return 0;

error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return -1;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	return 0;
}

static void lis2dh_acc_input_work_func(struct work_struct *work)
{
	struct lis2dh_acc_status *stat;

	int xyz[3] = { 0 };
	int err;

	stat = container_of((struct delayed_work *)work,
			struct lis2dh_acc_status, input_work);

	mutex_lock(&stat->lock);
	err = lis2dh_acc_get_acceleration_data(stat, xyz);
	if (err < 0)
		dev_err(&stat->client->dev, "get_acceleration_data failed\n");
	else
		lis2dh_acc_report_values(stat, xyz);

	schedule_delayed_work(&stat->input_work, msecs_to_jiffies(
			stat->pdata->poll_interval));
	mutex_unlock(&stat->lock);
}

int lis2dh_acc_input_open(struct input_dev *input)
{
	struct lis2dh_acc_status *stat = input_get_drvdata(input);

	return lis2dh_acc_enable(stat);
}

void lis2dh_acc_input_close(struct input_dev *dev)
{
	struct lis2dh_acc_status *stat = input_get_drvdata(dev);

	lis2dh_acc_disable(stat);
}

static int lis2dh_acc_validate_pdata(struct lis2dh_acc_status *stat)
{
	/* checks for correctness of minimal polling period */
	stat->pdata->min_interval =
		max((unsigned int)LIS2DH_ACC_MIN_POLL_PERIOD_MS,
						stat->pdata->min_interval);

	stat->pdata->poll_interval = max(stat->pdata->poll_interval,
			stat->pdata->min_interval);

	if (stat->pdata->axis_map_x > 2 ||
		stat->pdata->axis_map_y > 2 ||
		 stat->pdata->axis_map_z > 2) {
		dev_err(&stat->client->dev, "invalid axis_map value "
			"x:%u y:%u z%u\n", stat->pdata->axis_map_x,
					stat->pdata->axis_map_y,
						stat->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (stat->pdata->negate_x > 1 || stat->pdata->negate_y > 1
			|| stat->pdata->negate_z > 1) {
		dev_err(&stat->client->dev, "invalid negate value "
			"x:%u y:%u z:%u\n", stat->pdata->negate_x,
				stat->pdata->negate_y, stat->pdata->negate_z);
		return -EINVAL;
	}

	/* Enforce minimum polling interval */
	if (stat->pdata->poll_interval < stat->pdata->min_interval) {
		dev_err(&stat->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lis2dh_acc_input_init(struct lis2dh_acc_status *stat)
{
	int err;

	INIT_DELAYED_WORK(&stat->input_work, lis2dh_acc_input_work_func);
	stat->input_dev = input_allocate_device();
	if (!stat->input_dev) {
		err = -ENOMEM;
		dev_err(&stat->client->dev, "input device allocation failed\n");
		goto err0;
	}

	stat->input_dev->open = lis2dh_acc_input_open;
	stat->input_dev->close = lis2dh_acc_input_close;
	stat->input_dev->name = /*LIS2DH_ACC_DEV_NAME*/"gsensor";
	/* stat->input_dev->name = "accelerometer"; */
	stat->input_dev->id.bustype = BUS_I2C;
	stat->input_dev->dev.parent = &stat->client->dev;

	input_set_drvdata(stat->input_dev, stat);

	set_bit(EV_ABS, stat->input_dev->evbit);
	/*	next is used for interruptA sources data if the case */
	set_bit(ABS_MISC, stat->input_dev->absbit);
	/*	next is used for interruptB sources data if the case */
	set_bit(ABS_WHEEL, stat->input_dev->absbit);

	input_set_abs_params(stat->input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(stat->input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(stat->input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);
	/*	next is used for interruptA sources data if the case */
	input_set_abs_params(stat->input_dev, ABS_MISC, INT_MIN, INT_MAX, 0, 0);
	/*	next is used for interruptB sources data if the case */
	input_set_abs_params(stat->input_dev, ABS_WHEEL, INT_MIN,
								INT_MAX, 0, 0);


	err = input_register_device(stat->input_dev);
	if (err) {
		dev_err(&stat->client->dev,
				"unable to register input device %s\n",
				stat->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(stat->input_dev);
err0:
	return err;
}

static void lis2dh_acc_input_cleanup(struct lis2dh_acc_status *stat)
{
	input_unregister_device(stat->input_dev);
	input_free_device(stat->input_dev);
}

static int adc_open(struct inode *inode, struct file *file)
{
    
    return 0;
}


static int adc_release(struct inode *inode, struct file *filp)
{
    return 0;
}


static struct lis2dh_acc_status *stat;



/* ioctl - I/O control */
static long lis2dh_gsensor_dev_ioctl(struct file *file,
						  unsigned int cmd, unsigned long arg)
{
	int result = 0;
	DTRACE("lis2dh_acc line=%d cmd=%x GSENSOR_IOCTL_START=%x  GSENSOR_IOCTL_CLOSE=%x\n", __LINE__, cmd, GSENSOR_IOCTL_START, GSENSOR_IOCTL_CLOSE);
 	if(stat != NULL)
 	{
	    switch (cmd) {
			 case GSENSOR_IOCTL_START:
			 	DTRACE("lis2dh GSENSOR_IOCTL_START \n");
			 	result = lis2dh_acc_enable(stat);
				DTRACE("Enable result=%d \n", result);
			 	break;
			
	         case GSENSOR_IOCTL_CLOSE:
			 	DTRACE("lis2dh GSENSOR_IOCTL_CLOSE \n");
			 	result = lis2dh_acc_disable(stat);
				DTRACE("Disable result=%d \n", result);
	 			break;
			 default:
                printk("Unknow cmd\n");
                return -EINVAL;
		}
 	}else
 	{
 		printk("no lis2dh acc \n");
		result = -EINVAL;
	}
	return result;
}



static struct file_operations adc_fops = 
{
    .owner    = THIS_MODULE,
    .open     = adc_open,
    .unlocked_ioctl     = lis2dh_gsensor_dev_ioctl,    
    .release  = adc_release,
};


static struct miscdevice adc_miscdev = 
{
    .minor   = MISC_DYNAMIC_MINOR, /*¡ä?¨¦¨¨¡À?o?¡ê??¡§¨°??¨²miscdevice.h?D¡ê??a255*/
    .name    = "mma8452_daemon",        /*¨¦¨¨¡À???3?*/
    .fops    = &adc_fops,          /*??ADC¨¦¨¨¡À????t2¨´¡Á¡Â*/
};


static int lis2dh_acc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	

	u32 smbus_func = (I2C_FUNC_SMBUS_BYTE_DATA |
			I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);

	int err = -1;

	dev_info(&client->dev, "probe start.\n");

	stat = kzalloc(sizeof(struct lis2dh_acc_status), GFP_KERNEL);
	if (stat == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for module data: "
					"%d\n", err);
		goto exit_check_functionality_failed;
	}

	/* Support for both I2C and SMBUS adapter interfaces. */
	stat->use_smbus = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_warn(&client->dev, "client not i2c capable\n");
		if (i2c_check_functionality(client->adapter, smbus_func)) {
			stat->use_smbus = 1;
			dev_warn(&client->dev, "client using SMBUS\n");
		} else {
			err = -ENODEV;
			dev_err(&client->dev, "client nor SMBUS capable\n");
			goto exit_check_functionality_failed;
		}
	}


	mutex_init(&stat->lock);
	mutex_lock(&stat->lock);

	stat->client = client;
	i2c_set_clientdata(client, stat);

	stat->pdata = kmalloc(sizeof(*stat->pdata), GFP_KERNEL);
	if (stat->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for pdata: %d\n",
				err);
		goto err_mutexunlock;
	}

#ifdef CONFIG_OF
		dev_info(&client->dev, "probe start. lis2dh_acc_of_get_platdata\n");
		client->dev.platform_data =
			lis2dh_acc_of_get_platdata(&client->dev);
		if (IS_ERR(client->dev.platform_data)) {
			err = PTR_ERR(client->dev.platform_data);
			goto err_mutexunlock;
		}
		dev_info(&client->dev, "probe start. lis2dh_acc_of_get_platdata is done\n");
#endif

	if (client->dev.platform_data == NULL) {
		default_lis2dh_acc_pdata.gpio_int1 = int1_gpio;
		default_lis2dh_acc_pdata.gpio_int2 = int2_gpio;
		memcpy(stat->pdata, &default_lis2dh_acc_pdata,
							sizeof(*stat->pdata));
		dev_info(&client->dev, "using default plaform_data\n");
	} else {
		memcpy(stat->pdata, client->dev.platform_data,
							sizeof(*stat->pdata));
	}

	err = lis2dh_acc_validate_pdata(stat);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}

	err = lis2dh_acc_input_init(stat);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto exit_kfree_pdata;
	}

	err = misc_register(&adc_miscdev);
	if (err) 
	{
		dev_err(&client->dev, "cannot register miscdev on minor=%d (%d)\n", MISC_DYNAMIC_MINOR, err);
		goto err_input_cleanup;
	}

	/* pcl */
	lis2dh_acc_set_pinctrl_state(&client->dev,
			stat->pdata->pins_default);
	
	if (stat->pdata->init) {
		err = stat->pdata->init(&stat->input_dev->dev);
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err_misc_register;
		}
	}

	DTRACE("Notice gpio_int1=%d gpio_int2=%d \n", stat->pdata->gpio_int1, stat->pdata->gpio_int2);
	
	if (stat->pdata->gpio_int1 >= 0) {
		stat->irq1 = gpio_to_irq(stat->pdata->gpio_int1);
		pr_info("%s: %s has set irq1 to irq: %d, "
							"mapped on gpio:%d\n",
			LIS2DH_ACC_DEV_NAME, __func__, stat->irq1,
							stat->pdata->gpio_int1);
	}

	if (stat->pdata->gpio_int2 >= 0) {
		stat->irq2 = gpio_to_irq(stat->pdata->gpio_int2);
		pr_info("%s: %s has set irq2 to irq: %d, "
							"mapped on gpio:%d\n",
			LIS2DH_ACC_DEV_NAME, __func__, stat->irq2,
							stat->pdata->gpio_int2);
	}

	if (stat->pdata->gpio_int1 >= 0) {
		INIT_WORK(&stat->irq1_work, lis2dh_acc_irq1_work_func);
		stat->irq1_work_queue =
			create_singlethread_workqueue("lis2dh_acc_wq1");
		if (!stat->irq1_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev,
					"cannot create work queue1: %d\n", err);
			goto err_misc_register;
		}
		err = request_irq(stat->irq1, lis2dh_acc_isr1,
			IRQF_TRIGGER_RISING, "lis2dh_acc_irq1", stat);
		if (err < 0) {
			dev_err(&client->dev, "request irq1 failed: %d\n", err);
			goto err_destoyworkqueue1;
		}
		disable_irq_nosync(stat->irq1);
	}

	if (stat->pdata->gpio_int2 >= 0) {
		INIT_WORK(&stat->irq2_work, lis2dh_acc_irq2_work_func);
		stat->irq2_work_queue =
			create_singlethread_workqueue("lis2dh_acc_wq2");
		if (!stat->irq2_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev,
					"cannot create work queue2: %d\n", err);
			goto err_free_irq1;
		}
		err = request_irq(stat->irq2, lis2dh_acc_isr2,
			IRQF_TRIGGER_RISING, "lis2dh_acc_irq2", stat);
		if (err < 0) {
			dev_err(&client->dev, "request irq2 failed: %d\n", err);
			goto err_destoyworkqueue2;
		}
		disable_irq_nosync(stat->irq2);
	}

	memset(stat->resume_state, 0, ARRAY_SIZE(stat->resume_state));

	stat->resume_state[RES_CTRL_REG1] = (ALL_ZEROES |
					LIS2DH_ACC_ENABLE_ALL_AXES);
	stat->resume_state[RES_CTRL_REG4] = (ALL_ZEROES | CTRL_REG4_BDU_ENABLE);

/*
	stat->resume_state[RES_CTRL_REG2] = ALL_ZEROES;
	stat->resume_state[RES_CTRL_REG3] = ALL_ZEROES;
	stat->resume_state[RES_CTRL_REG4] = ALL_ZEROES;
	stat->resume_state[RES_CTRL_REG5] = ALL_ZEROES;
	stat->resume_state[RES_CTRL_REG6] = ALL_ZEROES;

	stat->resume_state[RES_TEMP_CFG_REG] = ALL_ZEROES;
	stat->resume_state[RES_FIFO_CTRL_REG] = ALL_ZEROES;
	stat->resume_state[RES_INT_CFG1] = ALL_ZEROES;
	stat->resume_state[RES_INT_THS1] = ALL_ZEROES;
	stat->resume_state[RES_INT_DUR1] = ALL_ZEROES;

	stat->resume_state[RES_TT_CFG] = ALL_ZEROES;
	stat->resume_state[RES_TT_THS] = ALL_ZEROES;
	stat->resume_state[RES_TT_LIM] = ALL_ZEROES;
	stat->resume_state[RES_TT_TLAT] = ALL_ZEROES;
	stat->resume_state[RES_TT_TW] = ALL_ZEROES;
*/

	err = lis2dh_acc_device_power_on(stat);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err_destoyworkqueue2;
	}

	atomic_set(&stat->enabled, 1);

	err = lis2dh_acc_update_fs_range(stat, stat->pdata->fs_range);
	if (err < 0) {
		dev_err(&client->dev, "update_fs_range failed\n");
		goto  err_destoyworkqueue2;
	}

	err = lis2dh_acc_update_odr(stat, stat->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto  err_destoyworkqueue2;
	}

	


	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev,
		   "device LIS2DH_ACC_DEV_NAME sysfs register failed\n");
		goto err_destoyworkqueue2;
	}


	lis2dh_acc_device_power_off(stat);

	/* As default, do not report information */
	atomic_set(&stat->enabled, 0);

	

	mutex_unlock(&stat->lock);

	dev_info(&client->dev, "%s: probed\n", LIS2DH_ACC_DEV_NAME);

	return 0;


err_destoyworkqueue2:
	if (stat->pdata->gpio_int2 >= 0)
		destroy_workqueue(stat->irq2_work_queue);
err_free_irq1:
	if (stat->pdata->gpio_int1 >= 0)
		free_irq(stat->irq1, stat);
err_destoyworkqueue1:
	if (stat->pdata->gpio_int1 >= 0)
		destroy_workqueue(stat->irq1_work_queue);
err_misc_register:
	misc_deregister(&adc_miscdev);

err_input_cleanup:
	lis2dh_acc_input_cleanup(stat);
err_power_off:
	lis2dh_acc_device_power_off(stat);

err_pdata_init:
	if (stat->pdata->exit)
		stat->pdata->exit();
exit_kfree_pdata:
	kfree(stat->pdata);
err_mutexunlock:
	mutex_unlock(&stat->lock);
/* err_freedata: */
	kfree(stat);
	stat = NULL;
exit_check_functionality_failed:
	pr_err("%s: Driver Init failed\n", LIS2DH_ACC_DEV_NAME);
	return err;
}

static int /*__devexit*/ lis2dh_acc_remove(struct i2c_client *client)
{

	struct lis2dh_acc_status *stat = i2c_get_clientdata(client);

	if (stat->pdata->gpio_int1 >= 0) {
		free_irq(stat->irq1, stat);
		gpio_free(stat->pdata->gpio_int1);
		destroy_workqueue(stat->irq1_work_queue);
	}

	if (stat->pdata->gpio_int2 >= 0) {
		free_irq(stat->irq2, stat);
		gpio_free(stat->pdata->gpio_int2);
		destroy_workqueue(stat->irq2_work_queue);
	}

	misc_deregister(&adc_miscdev);
	lis2dh_acc_input_cleanup(stat);

	lis2dh_acc_set_pinctrl_state(&client->dev,
			stat->pdata->pins_inactive);
	
	
	
	lis2dh_acc_device_power_off(stat);
	remove_sysfs_interfaces(&client->dev);

	if (stat->pdata->exit)
		stat->pdata->exit();
	kfree(stat->pdata);
	kfree(stat);

	return 0;
}

#ifdef CONFIG_PM
static int lis2dh_acc_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lis2dh_acc_status *stat = i2c_get_clientdata(client);

	lis2dh_acc_set_pinctrl_state(dev,	stat->pdata->pins_default);
	
	if (stat->on_before_suspend)
		return lis2dh_acc_enable(stat);
	return 0;
}

static int lis2dh_acc_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lis2dh_acc_status *stat = i2c_get_clientdata(client);

	lis2dh_acc_set_pinctrl_state(dev, stat->pdata->pins_sleep);
	
	stat->on_before_suspend = atomic_read(&stat->enabled);
	return lis2dh_acc_disable(stat);
}
#else
#define lis2dh_acc_suspend	NULL
#define lis2dh_acc_resume	NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id lis2dh_acc_id[]
		= { { LIS2DH_ACC_DEV_NAME, 0 }, { }, };

MODULE_DEVICE_TABLE(i2c, lis2dh_acc_id);


static const struct dev_pm_ops lis2dh_acc_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(lis2dh_acc_suspend, lis2dh_acc_resume)
};


static struct i2c_driver lis2dh_acc_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = LIS2DH_ACC_DEV_NAME,
			.pm = &lis2dh_acc_pm,
		  },
	.probe = lis2dh_acc_probe,
	.remove = lis2dh_acc_remove,
	.id_table = lis2dh_acc_id,
};

static int __init lis2dh_acc_init(void)
{
	pr_info("%s accelerometer driver: init\n",
						LIS2DH_ACC_DEV_NAME);
	return i2c_add_driver(&lis2dh_acc_driver);
}

static void __exit lis2dh_acc_exit(void)
{

	pr_info("%s accelerometer driver exit\n",
						LIS2DH_ACC_DEV_NAME);

	i2c_del_driver(&lis2dh_acc_driver);
	return;
}

module_init(lis2dh_acc_init);
module_exit(lis2dh_acc_exit);

MODULE_DESCRIPTION("lis2dh accelerometer sysfs driver");
MODULE_AUTHOR("Matteo Dameno, Denis Ciocca, STMicroelectronics");
MODULE_LICENSE("GPL");
