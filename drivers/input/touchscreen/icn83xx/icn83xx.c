/*
*
* Copyright (c) 2012 Zhimin Tian <zmtian@chiponeic.com>
* Copyright (c) 2012 ChipOne Technology (Beijing) Co., Ltd.
* Copyright (C) 2015 ROCKCHIP, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*/

#include "icn83xx.h"

#if COMPILE_FW_WITH_DRIVER
#include "icn83xx_fw.h"
#endif

static struct i2c_client *this_client;
#if SUPPORT_FW_UPDATE
static short log_rawdata[28][16];
static short log_diffdata[28][16];

//static char firmware[128] = {"/system/fw.bin"};
//if file system not ready,you can use inner array 
static char firmware[128] = "icn83xx_firmware";
#endif

//static void icn_delayedwork_fun(struct work_struct *work);

#if SUPPORT_SYSFS
static enum hrtimer_restart chipone_timer_func(struct hrtimer *timer);
static ssize_t icn83xx_show_update(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t icn83xx_store_update(struct device* cd, struct device_attribute *attr, const char* buf, size_t len);
static ssize_t icn83xx_show_process(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t icn83xx_store_process(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);

static DEVICE_ATTR(update, S_IRUGO | S_IWUSR, icn83xx_show_update, icn83xx_store_update);
static DEVICE_ATTR(process, S_IRUGO | S_IWUSR, icn83xx_show_process, icn83xx_store_process);

static ssize_t icn83xx_show_process(struct device* cd,struct device_attribute *attr, char* buf)
{
    ssize_t ret = 0;
    sprintf(buf, "icn83xx process\n");
    ret = strlen(buf) + 1;
    return ret;
}

static ssize_t icn83xx_store_process(struct device* cd, struct device_attribute *attr,
               const char* buf, size_t len)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    unsigned long on_off = simple_strtoul(buf, NULL, 10); 
    if(on_off == 0)
    {
        icn83xx_ts->work_mode = on_off;
    }
    else if((on_off == 1) || (on_off == 2))
    {
        if((icn83xx_ts->work_mode == 0) && (icn83xx_ts->use_irq == 1))
        {
            hrtimer_init(&icn83xx_ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
            icn83xx_ts->timer.function = chipone_timer_func;
            hrtimer_start(&icn83xx_ts->timer, ktime_set(CTP_START_TIMER/1000, (CTP_START_TIMER%1000)*1000000), HRTIMER_MODE_REL);
        }
        icn83xx_ts->work_mode = on_off;
    }
    return len;
}

static ssize_t icn83xx_show_update(struct device* cd,
                     struct device_attribute *attr, char* buf)
{
    ssize_t ret = 0;     
    sprintf(buf, "icn83xx firmware\n");
    ret = strlen(buf) + 1;
    return ret;
}

static ssize_t icn83xx_store_update(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
    //unsigned long on_off = simple_strtoul(buf, NULL, 10); 
    return len;
}

static int icn83xx_create_sysfs(struct i2c_client *client)
{
    int err;
    struct device *dev = &(client->dev);
    icn83xx_trace("%s: \n",__func__);    
    err = device_create_file(dev, &dev_attr_update);
    err = device_create_file(dev, &dev_attr_process);
    return err;
}

#endif

#if SUPPORT_PROC_FS

static pack_head cmd_head;
static struct proc_dir_entry *icn83xx_proc_entry;
static int  DATA_LENGTH = 0;
//static ssize_t icn83xx_tool_write(struct file *filp, const char __user *buff, unsigned long len, void *data)
static ssize_t icn83xx_tool_write(struct file * filp, const char __user *buff, size_t len, loff_t *off)

{
    int ret = 0;
    
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    proc_info("%s \n",__func__);  
    if(down_interruptible(&icn83xx_ts->sem)) {  
        return -1;   
    }     
    ret = copy_from_user(&cmd_head, buff, CMD_HEAD_LENGTH);
    if(ret) {
        proc_error("copy_from_user failed.\n");
        goto write_out;
    }  
    else {
        ret = CMD_HEAD_LENGTH;
    }
    
    proc_info("wr  :0x%02x.\n", cmd_head.wr);
    proc_info("flag:0x%02x.\n", cmd_head.flag);
    proc_info("circle  :%d.\n", (int)cmd_head.circle);
    proc_info("times   :%d.\n", (int)cmd_head.times);
    proc_info("retry   :%d.\n", (int)cmd_head.retry);
    proc_info("data len:%d.\n", (int)cmd_head.data_len);
    proc_info("addr len:%d.\n", (int)cmd_head.addr_len);
    proc_info("addr:0x%02x%02x.\n", cmd_head.addr[0], cmd_head.addr[1]);
    proc_info("len:%d.\n", (int)len);
    proc_info("data:0x%02x%02x.\n", buff[CMD_HEAD_LENGTH], buff[CMD_HEAD_LENGTH+1]);
    if (1 == cmd_head.wr) {// write iic
        if(1 == cmd_head.addr_len) {
            ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
            if(ret) {
                proc_error("copy_from_user failed.\n");
                goto write_out;
            }
            ret = icn83xx_i2c_txdata(cmd_head.addr[0], &cmd_head.data[0], cmd_head.data_len);
            if (ret < 0) {
                proc_error("write iic failed! ret: %d\n", ret);
                goto write_out;
            }
            ret = cmd_head.data_len + CMD_HEAD_LENGTH;
            goto write_out;
        }
    }
    else if(3 == cmd_head.wr) {
        ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
        if(ret) {
            proc_error("copy_from_user failed.\n");
            goto write_out;
        }
        ret = cmd_head.data_len + CMD_HEAD_LENGTH;
        memset(firmware, 0, 128);
        memcpy(firmware, &cmd_head.data[0], cmd_head.data_len);
        proc_info("firmware : %s\n", firmware);
    }
    else if(5 == cmd_head.wr) {
        icn83xx_update_status(1);  
        ret = kernel_thread(icn83xx_fw_update, firmware, CLONE_KERNEL);
        icn83xx_trace("the kernel_thread result is:%d\n", ret);    
    }
    else if(7 == cmd_head.wr) {//write reg 
        if(2 == cmd_head.addr_len) {
            ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
            if(ret) {
                proc_error("copy_from_user failed.\n");
                goto write_out;
            }
            ret = icn83xx_writeReg((cmd_head.addr[0]<<8)|cmd_head.addr[1], cmd_head.data[0]);
            if (ret < 0) {
                proc_error("write reg failed! ret: %d\n", ret);
                goto write_out;
            }
            ret = cmd_head.data_len + CMD_HEAD_LENGTH;
            goto write_out;
        }
    }
write_out:
    up(&icn83xx_ts->sem); 
    return len;
    
}
//static int icn83xx_tool_read( char *page, char **start, off_t off, int count, int *eof, void *data )
static ssize_t icn83xx_tool_read(struct file *filp, char __user *page, size_t count, loff_t *off)
{
    int i;
    int ret = 0;
    int data_len = 0;
    int len = 0;
    int loc = 0;
    char retvalue;
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    if(down_interruptible(&icn83xx_ts->sem)) {  
        return -1;   
    }     
    proc_info("%s: count:%d, off:%d, cmd_head.data_len: %d\n",__func__, count, off, cmd_head.data_len); 
    if (cmd_head.wr % 2) {
        ret = 0;
        goto read_out;
    }
    else if (0 == cmd_head.wr) {//read iic
        if(1 == cmd_head.addr_len) {
            data_len = cmd_head.data_len;
            if(cmd_head.addr[0] == 0xff) {                
                page[0] = 83;
                proc_info("read ic type: %d\n", page[0]);
            }
            else {
                while(data_len>0) {
                    if (data_len > DATA_LENGTH) {
                        len = DATA_LENGTH;
                    }
                    else {
                        len = data_len;
                    }
                    data_len -= len;   
                    memset(&cmd_head.data[0], 0, len+1);
                    ret = icn83xx_i2c_rxdata(cmd_head.addr[0]+loc, &cmd_head.data[0], len);
                    //proc_info("cmd_head.data[0]: 0x%02x\n", cmd_head.data[0]);
                    //proc_info("cmd_head.data[1]: 0x%02x\n", cmd_head.data[1]);
                    if(ret < 0) {
                        icn83xx_error("read iic failed: %d\n", ret);
                        goto read_out;
                    }
                    else {
                        //proc_info("iic read out %d bytes, loc: %d\n", len, loc);
                        memcpy(&page[loc], &cmd_head.data[0], len);
                    }
                    loc += len;
                }
                proc_info("page[0]: 0x%02x\n", page[0]);
                proc_info("page[1]: 0x%02x\n", page[1]);
            }
        }
    }
    else if(2 == cmd_head.wr) {//read rawdata
        //scan tp rawdata
        icn83xx_write_reg(4, 0x20); 
        mdelay(cmd_head.times);
        icn83xx_read_reg(2, &retvalue);
        while(retvalue != 1) {
            mdelay(cmd_head.times);
            icn83xx_read_reg(2, &retvalue);
        }            

        if(2 == cmd_head.addr_len) {
            for(i=0; i<cmd_head.addr[1]; i++) {
                icn83xx_write_reg(3, i);
                mdelay(cmd_head.times);
                ret = icn83xx_i2c_rxdata(128, &cmd_head.data[0], cmd_head.addr[0]*2);
                if (ret < 0) {
                    icn83xx_error("read rawdata failed: %d\n", ret);            
                    goto read_out;           
                }
                else {
                    //proc_info("read rawdata out %d bytes, loc: %d\n", cmd_head.addr[0]*2, loc);                    
                    memcpy(&page[loc], &cmd_head.data[0], cmd_head.addr[0]*2);
                }
                loc += cmd_head.addr[0]*2;
            }  
            for(i=0; i<cmd_head.data_len; i=i+2) {
                swap_ab(page[i], page[i+1]);
            }
            //icn83xx_rawdatadump(&page[0], cmd_head.data_len/2, cmd_head.addr[0]);
        }

        //finish scan tp rawdata
        icn83xx_write_reg(2, 0x0);
    }
    else if(4 == cmd_head.wr) {//get update status
        page[0] = icn83xx_get_status();
    }
    else if(6 == cmd_head.wr) {//read reg
        if(2 == cmd_head.addr_len) {
            ret = icn83xx_readReg((cmd_head.addr[0]<<8)|cmd_head.addr[1], &cmd_head.data[0]);
            if (ret < 0) {
                proc_error("reg reg failed! ret: %d\n", ret);
                goto read_out;
            }
            page[0] = cmd_head.data[0];
            goto read_out;   
        }
    }
read_out:
    up(&icn83xx_ts->sem);   
    proc_info("%s out: %d, cmd_head.data_len: %d\n\n",__func__, count, cmd_head.data_len); 
    return cmd_head.data_len;
}

static const struct file_operations icn83xx_proc_fops = {
	.owner = THIS_MODULE,
	.read = icn83xx_tool_read,
	.write = icn83xx_tool_write,
};

static int init_proc_node(void)
{
    int i;
    memset(&cmd_head, 0, sizeof(cmd_head));
    cmd_head.data = NULL;

    i = 5;
    while ((!cmd_head.data) && i) {
        cmd_head.data = kzalloc(i * DATA_LENGTH_UINT, GFP_KERNEL);
        if (NULL != cmd_head.data) {
            break;
        }
        i--;
    }
    if (i) {
        //DATA_LENGTH = i * DATA_LENGTH_UINT + GTP_ADDR_LENGTH;
        DATA_LENGTH = i * DATA_LENGTH_UINT;
        icn83xx_trace("alloc memory size:%d.\n", DATA_LENGTH);
    }
    else {
        proc_error("alloc for memory failed.\n");
        return 0;
    }

	icn83xx_proc_entry = proc_create(ICN83XX_ENTRY_NAME, 0666, NULL,&icn83xx_proc_fops);
    if (icn83xx_proc_entry == NULL) {
        proc_error("Couldn't create proc entry!\n");
        return 0;
    }
    else {
        icn83xx_trace("Create proc entry success!\n");
    }

    return 1;
}

static void uninit_proc_node(void)
{
    kfree(cmd_head.data);
    cmd_head.data = NULL;
    remove_proc_entry(ICN83XX_ENTRY_NAME, NULL);
}
    
#endif


#if TOUCH_VIRTUAL_KEYS
static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf,
     __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":100:1030:50:60"
     ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":280:1030:50:60"
     ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":470:1030:50:60"
     ":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH) ":900:1030:50:60"
     "\n");
}

static struct kobj_attribute virtual_keys_attr = {
    .attr = {
        .name = "virtualkeys.chipone-ts",
        .mode = S_IRUGO,
    },
    .show = &virtual_keys_show,
};

static struct attribute *properties_attrs[] = {
    &virtual_keys_attr.attr,
    NULL
};

static struct attribute_group properties_attr_group = {
    .attrs = properties_attrs,
};

static void icn83xx_ts_virtual_keys_init(void)
{
    int ret = 0;
    struct kobject *properties_kobj;      
    properties_kobj = kobject_create_and_add("board_properties", NULL);
    if (properties_kobj)
        ret = sysfs_create_group(properties_kobj,
                     &properties_attr_group);
    if (!properties_kobj || ret)
        pr_err("failed to create board_properties\n");    
}
#endif


/* ---------------------------------------------------------------------
*
*   Chipone panel related driver
*
*
----------------------------------------------------------------------*/
/***********************************************************************************************
Name    :   icn83xx_ts_wakeup 
Input   :   void
Output  :   ret
function    : this function is used to wakeup tp
***********************************************************************************************/
void icn83xx_ts_wakeup(void)
{
//#if def TOUCH_RESET_PIN

}

/***********************************************************************************************
Name    :   icn83xx_ts_reset 
Input   :   void
Output  :   ret
function    : this function is used to reset tp, you should not delete it
***********************************************************************************************/
void icn83xx_ts_reset(void)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    gpio_set_value(icn83xx_ts->board_info->reset_pin, 0);
    mdelay(30);
    gpio_set_value(icn83xx_ts->board_info->reset_pin, 1);
    mdelay(50);
 }

/***********************************************************************************************
Name    :   icn83xx_irq_disable 
Input   :   void
Output  :   ret
function    : this function is used to disable irq
***********************************************************************************************/
void icn83xx_irq_disable(void)
{
    unsigned long irqflags;
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);

    spin_lock_irqsave(&icn83xx_ts->irq_lock, irqflags);
    if (!icn83xx_ts->irq_is_disable) {
        icn83xx_ts->irq_is_disable = 1; 
        disable_irq_nosync(icn83xx_ts->irq);
        //disable_irq(icn83xx_ts->irq);
    }
    spin_unlock_irqrestore(&icn83xx_ts->irq_lock, irqflags);
}

/***********************************************************************************************
Name    :   icn83xx_irq_enable 
Input   :   void
Output  :   ret
function    : this function is used to enable irq
***********************************************************************************************/
void icn83xx_irq_enable(void)
{
    unsigned long irqflags = 0;
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);

    spin_lock_irqsave(&icn83xx_ts->irq_lock, irqflags);
    if (icn83xx_ts->irq_is_disable) {
        enable_irq(icn83xx_ts->irq);
        icn83xx_ts->irq_is_disable = 0; 
    }
    spin_unlock_irqrestore(&icn83xx_ts->irq_lock, irqflags);

}

/***********************************************************************************************
Name    :   icn83xx_prog_i2c_rxdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : read data from icn83xx, prog mode 
***********************************************************************************************/
int icn83xx_prog_i2c_rxdata(unsigned short addr, char *rxdata, int length)
{
    int ret = -1;
    int retries = 0;
#if 0   
    struct i2c_msg msgs[] = {   
        {
            .addr   = ICN83XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
            //.scl_rate = ICN83XX_I2C_SCL,
        },
    };
        
    icn83xx_prog_i2c_txdata(addr, NULL, 0);
    while(retries < IIC_RETRY_NUM)
    {    
        ret = i2c_transfer(this_client->adapter, msgs, 1);
        if(ret == 1)break;
        retries++;
    }
    if (retries >= IIC_RETRY_NUM)
    {
        icn83xx_error("%s i2c read error: %d\n", __func__, ret); 
//        icn83xx_ts_reset();
    }    
#else
    unsigned char tmp_buf[2];
    struct i2c_msg msgs[] = {
        {
            .addr   = ICN83XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = 0,
            .len    = 2,
            .buf    = tmp_buf,
            //.scl_rate = ICN83XX_I2C_SCL,
        },
        {
            .addr   = ICN83XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
            //.scl_rate = ICN83XX_I2C_SCL,
        },
    };
    tmp_buf[0] = U16HIBYTE(addr);
    tmp_buf[1] = U16LOBYTE(addr);  

    while(retries < IIC_RETRY_NUM) {
        ret = i2c_transfer(this_client->adapter, msgs, 2);
        if(ret == 2)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM) {
        icn83xx_error("%s i2c read error: %d\n", __func__, ret); 
//        icn83xx_ts_reset();
    }
#endif      
    return ret;
}
/***********************************************************************************************
Name    :   icn83xx_prog_i2c_txdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : send data to icn83xx , prog mode
***********************************************************************************************/
int icn83xx_prog_i2c_txdata(unsigned short addr, char *txdata, int length)
{
    int ret = -1;
    char tmp_buf[128];
    int retries = 0; 
    struct i2c_msg msg[] = {
        {
            .addr   = ICN83XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = 0,
            .len    = length + 2,
            .buf    = tmp_buf,
            //.scl_rate = ICN83XX_I2C_SCL,
        },
    };
    
    if (length > 125) {
        icn83xx_error("%s too big datalen = %d!\n", __func__, length);
        return -1;
    }
    
    tmp_buf[0] = U16HIBYTE(addr);
    tmp_buf[1] = U16LOBYTE(addr);

    if (length != 0 && txdata != NULL) {
        memcpy(&tmp_buf[2], txdata, length);
    }   
    
    while(retries < IIC_RETRY_NUM) {
        ret = i2c_transfer(this_client->adapter, msg, 1);
        if(ret == 1)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM) {
        icn83xx_error("%s i2c write error: %d\n", __func__, ret); 
//       icn83xx_ts_reset();
    }
    return ret;
}
/***********************************************************************************************
Name    :   icn83xx_prog_write_reg
Input   :   addr -- address
            para -- parameter
Output  :   
function    :   write register of icn83xx, prog mode
***********************************************************************************************/
int icn83xx_prog_write_reg(unsigned short addr, char para)
{
    char buf[3];
    int ret = -1;

    buf[0] = para;
    ret = icn83xx_prog_i2c_txdata(addr, buf, 1);
    if (ret < 0) {
        icn83xx_error("write reg failed! %#x ret: %d\n", buf[0], ret);
        return -1;
    }
    
    return ret;
}


/***********************************************************************************************
Name    :   icn83xx_prog_read_reg 
Input   :   addr
            pdata
Output  :   
function    :   read register of icn83xx, prog mode
***********************************************************************************************/
int icn83xx_prog_read_reg(unsigned short addr, char *pdata)
{
    int ret = -1;
    ret = icn83xx_prog_i2c_rxdata(addr, pdata, 1);  
    return ret;    
}

/***********************************************************************************************
Name    :   icn83xx_i2c_rxdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : read data from icn83xx, normal mode   
***********************************************************************************************/
int icn83xx_i2c_rxdata(unsigned char addr, unsigned char *rxdata, int length)
{
    int ret = -1;
    int retries = 0;
#if 0
    struct i2c_msg msgs[] = {   
        {
            .addr   = this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
            //.scl_rate = ICN83XX_I2C_SCL,
        },
    };
        
    icn83xx_i2c_txdata(addr, NULL, 0);
    while(retries < IIC_RETRY_NUM) {

        ret = i2c_transfer(this_client->adapter, msgs, 1);
        if(ret == 1)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM) {
        icn83xx_error("%s i2c read error: %d\n", __func__, ret); 
//        icn83xx_ts_reset();
    }
#else
    unsigned char tmp_buf[1];
    struct i2c_msg msgs[] = {
        {
            .addr   = this_client->addr,
            .flags  = 0,
            .len    = 1,
            .buf    = tmp_buf,
            //.scl_rate = ICN83XX_I2C_SCL,
        },
        {
            .addr   = this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
            //.scl_rate = ICN83XX_I2C_SCL,
        },
    };
    tmp_buf[0] = addr; 
    
    while(retries < IIC_RETRY_NUM) {
        ret = i2c_transfer(this_client->adapter, msgs, 2);
        if(ret == 2)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM) {
        icn83xx_error("%s i2c read error: %d\n", __func__, ret); 
        icn83xx_ts_reset();
    }    
#endif

    return ret;
}
/***********************************************************************************************
Name    :   icn83xx_i2c_txdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : send data to icn83xx , normal mode
***********************************************************************************************/
int icn83xx_i2c_txdata(unsigned char addr, unsigned char *txdata, int length)
{
    int ret = -1;
    unsigned char tmp_buf[128];
    int retries = 0;

    struct i2c_msg msg[] = {
        {
            .addr   = this_client->addr,
            .flags  = 0,
            .len    = length + 1,
            .buf    = tmp_buf,
            //.scl_rate = ICN83XX_I2C_SCL,
        },
    };
    
    if (length > 125) {
        icn83xx_error("%s too big datalen = %d!\n", __func__, length);
        return -1;
    }
    
    tmp_buf[0] = addr;

    if (length != 0 && txdata != NULL) {
        memcpy(&tmp_buf[1], txdata, length);
    }   
    
    while(retries < IIC_RETRY_NUM) {
        ret = i2c_transfer(this_client->adapter, msg, 1);
        if(ret == 1)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM) {
        icn83xx_error("%s i2c write error: %d\n", __func__, ret); 
        icn83xx_ts_reset();
    }

    return ret;
}

/***********************************************************************************************
Name    :   icn83xx_write_reg
Input   :   addr -- address
            para -- parameter
Output  :   
function    :   write register of icn83xx, normal mode
***********************************************************************************************/
int icn83xx_write_reg(unsigned char addr, unsigned char para)
{
    char buf[3];
    int ret = -1;

    buf[0] = para;
    ret = icn83xx_i2c_txdata(addr, buf, 1);
    if (ret < 0) {
        icn83xx_error("write reg failed! %#x ret: %d\n", buf[0], ret);
        return -1;
    }
    
    return ret;
}


/***********************************************************************************************
Name    :   icn83xx_read_reg 
Input   :   addr
            pdata
Output  :   
function    :   read register of icn83xx, normal mode
***********************************************************************************************/
int icn83xx_read_reg(unsigned char addr, unsigned char *pdata)
{
    int ret = -1;
    ret = icn83xx_i2c_rxdata(addr, pdata, 1);  
    return ret;    
}

#if SUPPORT_FW_UPDATE
/***********************************************************************************************
Name    :   icn83xx_log
Input   :   0: rawdata, 1: diff data
Output  :   err type
function    :   calibrate param
***********************************************************************************************/
int  icn83xx_log(char diff)
{
    char row = 0;
    char column = 0;
    int i, j;
    icn83xx_read_reg(160, &row);
    icn83xx_read_reg(161, &column);

    if(diff == 1) {
        icn83xx_readTP(row, column, (char *)(&log_diffdata[0][0]));

        for(i=0; i<row; i++) {       
            for(j=0; j<column; j++) {
                log_diffdata[i][j] = log_diffdata[i][j] - log_rawdata[i][j];
            }
        }   
        icn83xx_rawdatadump(&log_diffdata[0][0], row*16, 16);
    }
    else {
        icn83xx_readTP(row, column, (char *)(&log_rawdata[0][0]));    
        icn83xx_rawdatadump(&log_rawdata[0][0], row*16, 16);
    }
	return 0;
}
#endif

/***********************************************************************************************
Name    :   icn83xx_iic_test 
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_iic_test(void)
{
    int  ret = -1;
    unsigned char value = 0;
    int  retry = 0;
	
    while(retry++ < 3) {        
        ret = icn83xx_read_reg(0, &value);
        if(ret > 0) {
            return ret;
        }
        icn83xx_error("iic test error! %d\n", retry);
        msleep(3);
    }
    return ret;    
}
/***********************************************************************************************
Name    :   icn83xx_ts_release 
Input   :   void
Output  :   
function    : touch release
***********************************************************************************************/
#if !CTP_REPORT_PROTOCOL
static void icn83xx_ts_release(void)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    icn83xx_info("==icn83xx_ts_release ==\n");
    input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
    input_sync(icn83xx_ts->input_dev);
}

/***********************************************************************************************
Name    :   icn83xx_report_value_A
Input   :   void
Output  :   
function    : reprot touch ponit
***********************************************************************************************/
static void icn83xx_report_value_A(void)
{
    icn83xx_info("==icn83xx_report_value_A ==\n");
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    char buf[POINT_NUM*POINT_SIZE+3]={0};
    int ret = -1;
    int i = 0, j = 0, nBytes = 0;
#if TOUCH_VIRTUAL_KEYS
    unsigned char button;
    static unsigned char button_last;
#endif

	nBytes = POINT_NUM*POINT_SIZE+2;
	while (nBytes) {
		i = nBytes > 32 ? 32 : nBytes;
		ret = icn83xx_i2c_rxdata(16+j, buf+j, i);
		if (ret < 0) {
			icn83xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
			return ret;
		}
		j = j + i;
		nBytes = nBytes - i;
	}

#if TOUCH_VIRTUAL_KEYS    
    button = buf[0];    
    icn83xx_info("%s: button=%d\n",__func__, button);

    if((button_last != 0) && (button == 0)) {
        icn83xx_ts_release();
        button_last = button;
        return 1;       
    }
    if(button != 0) {
        switch(button) {
        case ICN_VIRTUAL_BUTTON_HOME:
            icn83xx_info("ICN_VIRTUAL_BUTTON_HOME down\n");
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 200);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_X, 280);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, 1030);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
            input_mt_sync(icn83xx_ts->input_dev);
            input_sync(icn83xx_ts->input_dev);            
            break;
        case ICN_VIRTUAL_BUTTON_BACK:
            icn83xx_info("ICN_VIRTUAL_BUTTON_BACK down\n");
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 200);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_X, 470);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, 1030);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
            input_mt_sync(icn83xx_ts->input_dev);
            input_sync(icn83xx_ts->input_dev);                
            break;
        case ICN_VIRTUAL_BUTTON_MENU:
            icn83xx_info("ICN_VIRTUAL_BUTTON_MENU down\n");
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 200);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_X, 100);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, 1030);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
            input_mt_sync(icn83xx_ts->input_dev);
            input_sync(icn83xx_ts->input_dev);            
            break;                      
        default:
            icn83xx_info("other gesture\n");
            break;          
        }
        button_last = button;
        return 1;
    }        
#endif
 
    icn83xx_ts->point_num = buf[1]; 
    if (icn83xx_ts->point_num > POINT_NUM) {
        //icn83xx_ts_release();
        return -1; 
    }    
    if (icn83xx_ts->point_num == 0) {
        icn83xx_ts_release();
        return 1; 
    }   
    for(i=0;i<icn83xx_ts->point_num;i++) {
        if(buf[8 + POINT_SIZE*i] != 4)
			break ;
    }
    
    if(i == icn83xx_ts->point_num) {
        icn83xx_ts_release();
        return 1; 
    }   

    for(i=0; i<icn83xx_ts->point_num; i++) {
        icn83xx_ts->point_info[i].u8ID = buf[2 + POINT_SIZE*i];
        icn83xx_ts->point_info[i].u16PosX = (buf[3 + POINT_SIZE*i]<<8) + buf[4 + POINT_SIZE*i];
        icn83xx_ts->point_info[i].u16PosY = (buf[5 + POINT_SIZE*i]<<8) + buf[6 + POINT_SIZE*i];
        icn83xx_ts->point_info[i].u8Pressure = 200;//buf[7 + POINT_SIZE*i];
        icn83xx_ts->point_info[i].u8EventId = buf[8 + POINT_SIZE*i];    

        if(1 == icn83xx_ts->revert_x_flag) {
            icn83xx_ts->point_info[i].u16PosX = icn83xx_ts->screen_max_x- icn83xx_ts->point_info[i].u16PosX;
        }
        if(1 == icn83xx_ts->revert_y_flag) {
            icn83xx_ts->point_info[i].u16PosY = icn83xx_ts->screen_max_y- icn83xx_ts->point_info[i].u16PosY;
        }
        
        icn83xx_info("u8ID %d\n", icn83xx_ts->point_info[i].u8ID);
        icn83xx_info("u16PosX %d\n", icn83xx_ts->point_info[i].u16PosX);
        icn83xx_info("u16PosY %d\n", icn83xx_ts->point_info[i].u16PosY);
        icn83xx_info("u8Pressure %d\n", icn83xx_ts->point_info[i].u8Pressure);
        icn83xx_info("u8EventId %d\n", icn83xx_ts->point_info[i].u8EventId);  

        input_report_abs(icn83xx_ts->input_dev, ABS_MT_TRACKING_ID, icn83xx_ts->point_info[i].u8ID);    
        input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, icn83xx_ts->point_info[i].u8Pressure);
        input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_X, icn83xx_ts->point_info[i].u16PosX);
        input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, icn83xx_ts->point_info[i].u16PosY);
        input_report_abs(icn83xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
        input_mt_sync(icn83xx_ts->input_dev);
        icn83xx_point_info("point: %d ===x = %d,y = %d, press = %d ====\n",i, icn83xx_ts->point_info[i].u16PosX,icn83xx_ts->point_info[i].u16PosY, icn83xx_ts->point_info[i].u8Pressure);
    }

    input_sync(icn83xx_ts->input_dev);
  
}
#endif
/***********************************************************************************************
Name    :   icn83xx_report_value_B
Input   :   void
Output  :   
function    : reprot touch ponit
***********************************************************************************************/
#if CTP_REPORT_PROTOCOL
static int icn83xx_report_value_B(void)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    unsigned char buf[POINT_NUM*POINT_SIZE+3]={0};
    static unsigned char finger_last[POINT_NUM + 1]={0};
    unsigned char  finger_current[POINT_NUM + 1] = {0};
    unsigned int position = 0;
    int temp = 0;
    int ret = -1;
	int i = 0, j = 0, nBytes = 0;
    icn83xx_info("==icn83xx_report_value_B ==\n");
   // icn83xx_trace("==icn83xx_report_value_B ==\n"); 

   nBytes = POINT_NUM*POINT_SIZE+2;
   while (nBytes) {
	   i = nBytes > 32 ? 32 : nBytes;
	   ret = icn83xx_i2c_rxdata(16+j, buf+j, i);
	   if (ret < 0) {
		   icn83xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		   return ret;
	   }
	   j = j + i;
	   nBytes = nBytes - i;
   }

    icn83xx_ts->point_num = buf[1]; 
    if (icn83xx_ts->point_num > POINT_NUM) {
        //icn83xx_ts_release();
        return -1; 
    }     
    if(icn83xx_ts->point_num > 0) {
        for(position = 0; position<icn83xx_ts->point_num; position++) {       
            temp = buf[2 + POINT_SIZE*position] + 1;
			if (temp > POINT_NUM) {
				printk("ERROR: %s ,%d\n", __func__, __LINE__);
				return -1;			
			}
            finger_current[temp] = 1;
            icn83xx_ts->point_info[temp].u8ID = buf[2 + POINT_SIZE*position];
            icn83xx_ts->point_info[temp].u16PosX = (buf[3 + POINT_SIZE*position]<<8) + buf[4 + POINT_SIZE*position];
            icn83xx_ts->point_info[temp].u16PosY = (buf[5 + POINT_SIZE*position]<<8) + buf[6 + POINT_SIZE*position];
            icn83xx_ts->point_info[temp].u8Pressure = buf[7 + POINT_SIZE*position];
            icn83xx_ts->point_info[temp].u8EventId = buf[8 + POINT_SIZE*position];
            
            if(icn83xx_ts->point_info[temp].u8EventId == 4)
                finger_current[temp] = 0;
                            
            if(1 == icn83xx_ts->revert_x_flag) {
                icn83xx_ts->point_info[temp].u16PosX = icn83xx_ts->screen_max_x- icn83xx_ts->point_info[temp].u16PosX;
            }
            if(1 == icn83xx_ts->revert_y_flag) {
                icn83xx_ts->point_info[temp].u16PosY = icn83xx_ts->screen_max_y- icn83xx_ts->point_info[temp].u16PosY;
            }
            icn83xx_info("temp %d\n", temp);
            icn83xx_info("u8ID %d\n", icn83xx_ts->point_info[temp].u8ID);
            icn83xx_info("u16PosX %d\n", icn83xx_ts->point_info[temp].u16PosX);
            icn83xx_info("u16PosY %d\n", icn83xx_ts->point_info[temp].u16PosY);
            icn83xx_info("u8Pressure %d\n", icn83xx_ts->point_info[temp].u8Pressure);
            icn83xx_info("u8EventId %d\n", icn83xx_ts->point_info[temp].u8EventId);             
            //icn83xx_info("u8Pressure %d\n", icn83xx_ts->point_info[temp].u8Pressure*16);
        }
    }   
    else {
        for(position = 1; position < POINT_NUM+1; position++) {
            finger_current[position] = 0;
        }
        icn83xx_info("no touch\n");
    }

    for(position = 1; position < POINT_NUM + 1; position++) {
        if((finger_current[position] == 0) && (finger_last[position] != 0)) {
            input_mt_slot(icn83xx_ts->input_dev, position-1);
            input_mt_report_slot_state(icn83xx_ts->input_dev, MT_TOOL_FINGER, false);
            icn83xx_point_info("one touch up: %d\n", position);
        }
        else if(finger_current[position]) {
            input_mt_slot(icn83xx_ts->input_dev, position-1);
            input_mt_report_slot_state(icn83xx_ts->input_dev, MT_TOOL_FINGER, true);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 1);
            //input_report_abs(icn83xx_ts->input_dev, ABS_MT_PRESSURE, icn83xx_ts->point_info[position].u8Pressure);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_PRESSURE, 200);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_X, icn83xx_ts->point_info[position].u16PosX);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, icn83xx_ts->point_info[position].u16PosY);
            icn83xx_point_info("===position: %d, x = %d,y = %d, press = %d ====\n", position, icn83xx_ts->point_info[position].u16PosX,icn83xx_ts->point_info[position].u16PosY, icn83xx_ts->point_info[position].u8Pressure);
          //  icn83xx_trace("===position: %d, x = %d,y = %d, press = %d ====\n", position, icn83xx_ts->point_info[position].u16PosX,icn83xx_ts->point_info[position].u16PosY, icn83xx_ts->point_info[position].u8Pressure);
		}
    }
    input_sync(icn83xx_ts->input_dev);

    for(position = 1; position < POINT_NUM + 1; position++) {
        finger_last[position] = finger_current[position];
    }
    return 0;
}
#endif

/***********************************************************************************************
Name    :   icn83xx_ts_pen_irq_work
Input   :   void
Output  :   
function    : work_struct
***********************************************************************************************/
static void icn83xx_ts_pen_irq_work(struct work_struct *work)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);  
#if SUPPORT_PROC_FS
    if(down_interruptible(&icn83xx_ts->sem)) {  
        return;   
    }  
#endif
      
    if(icn83xx_ts->work_mode == 0) {
#if CTP_REPORT_PROTOCOL
        icn83xx_report_value_B();
#else
        icn83xx_report_value_A();
#endif
    }
#if SUPPORT_FW_UPDATE    
    else if(icn83xx_ts->work_mode == 1) {
        printk("log raw data\n");
        icn83xx_log(0);   //raw data
    }
    else if(icn83xx_ts->work_mode == 2) {
        printk("log diff data\n");
        icn83xx_log(1);   //diff data
    }
#endif

#if SUPPORT_PROC_FS
    up(&icn83xx_ts->sem);
#endif
}
/***********************************************************************************************
Name    :   chipone_timer_func
Input   :   void
Output  :   
function    : Timer interrupt service routine.
***********************************************************************************************/
static enum hrtimer_restart chipone_timer_func(struct hrtimer *timer)
{
    struct icn83xx_ts_data *icn83xx_ts = container_of(timer, struct icn83xx_ts_data, timer);
    queue_work(icn83xx_ts->ts_workqueue, &icn83xx_ts->pen_event_work);

    if(icn83xx_ts->use_irq == 1) {
        if((icn83xx_ts->work_mode == 1) || (icn83xx_ts->work_mode == 2)) {
            hrtimer_start(&icn83xx_ts->timer, ktime_set(CTP_POLL_TIMER/1000, (CTP_POLL_TIMER%1000)*1000000), HRTIMER_MODE_REL);
        }
    }
    else {
        hrtimer_start(&icn83xx_ts->timer, ktime_set(CTP_POLL_TIMER/1000, (CTP_POLL_TIMER%1000)*1000000), HRTIMER_MODE_REL);
    }
    return HRTIMER_NORESTART;
}
/***********************************************************************************************
Name    :   icn83xx_ts_interrupt
Input   :   void
Output  :   
function    : interrupt service routine
***********************************************************************************************/
static irqreturn_t icn83xx_ts_interrupt(int irq, void *dev_id)
{
    struct icn83xx_ts_data *icn83xx_ts = dev_id;
       
    icn83xx_info("==========------icn83xx_ts TS Interrupt-----============\n"); 
    if(icn83xx_ts->work_mode != 0) {
        return IRQ_HANDLED;
    }
	
    //icn83xx_irq_disable();
    if (!work_pending(&icn83xx_ts->pen_event_work)) {
        //icn83xx_info("Enter work\n");
        queue_work(icn83xx_ts->ts_workqueue, &icn83xx_ts->pen_event_work);
    }

    return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name    :   icn83xx_ts_suspend
Input   :   void
Output  :   
function    : tp enter sleep mode
***********************************************************************************************/
static void icn83xx_ts_suspend(struct early_suspend *handler)
{
	  int retry = 0;
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    icn83xx_trace("icn83xx_ts_suspend: write ICN83XX_REG_PMODE .\n");
    if (icn83xx_ts->use_irq) {
        icn83xx_irq_disable();
	icn83xx_trace("icn83xx_ts_suspend:disable irq .\n");
    }
    else {
        hrtimer_cancel(&icn83xx_ts->timer);
    }    
    for(retry = 0;retry <3; retry++ ) {
     icn83xx_write_reg(ICN83XX_REG_PMODE, PMODE_HIBERNATE); 
    }  
}

/***********************************************************************************************
Name    :   icn83xx_ts_resume
Input   :   void
Output  :   
function    : wakeup tp or reset tp
***********************************************************************************************/
static void icn83xx_ts_resume(struct early_suspend *handler)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    int i;
    printk("==icn83xx_ts_resume== \n");
   // icn83xx_ts_reset();
//report touch release    
#if CTP_REPORT_PROTOCOL
    for (i = 0; i < POINT_NUM; i++) {
        input_mt_slot(icn83xx_ts->input_dev, i);
        input_mt_report_slot_state(icn83xx_ts->input_dev, MT_TOOL_FINGER, false);
    }
#else
	icn83xx_ts_release();
#endif 
   	icn83xx_ts_wakeup();
   	icn83xx_ts_reset();
    if (icn83xx_ts->use_irq) {
    	printk("icn83xx_irq_enable\n");
        icn83xx_irq_enable();
    }
    else {   
		printk("icn83xx_ts_resume  hrtimer_start\n");
        hrtimer_start(&icn83xx_ts->timer, ktime_set(CTP_START_TIMER/1000, (CTP_START_TIMER%1000)*1000000), HRTIMER_MODE_REL);
    }
    
}
#else
/***********************************************************************************************
Name	:	icn83xx_ts_suspend
Input	:	void
Output	:	
function	: tp enter sleep mode
***********************************************************************************************/
static int icn83xx_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int retry = 0;
	struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(client);
	icn83xx_trace("icn83xx_ts_suspend: write ICN83XX_REG_PMODE .\n");
	if (icn83xx_ts->use_irq) {
		icn83xx_irq_disable();
		icn83xx_trace("icn83xx_ts_suspend:disable irq .\n");
	}
	else {
		hrtimer_cancel(&icn83xx_ts->timer);
	}
	
	cancel_work_sync(&icn83xx_ts->pen_event_work);

	for(retry = 0;retry <3; retry++ ) {
		icn83xx_write_reg(ICN83XX_REG_PMODE, PMODE_HIBERNATE); 
	}
	return 0;
}

/***********************************************************************************************
Name	:	icn83xx_ts_resume
Input	:	void
Output	:	
function	: wakeup tp or reset tp
***********************************************************************************************/
static int icn83xx_ts_resume(struct i2c_client *client)
{
	struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(client);
	int i;
	printk("==icn83xx_ts_resume== \n");
   // icn83xx_ts_reset();
//report touch release	  
#if CTP_REPORT_PROTOCOL
	for (i = 0; i < POINT_NUM; i++) {
		input_mt_slot(icn83xx_ts->input_dev, i);
		input_mt_report_slot_state(icn83xx_ts->input_dev, MT_TOOL_FINGER, false);
	}
#else
	icn83xx_ts_release();
#endif 
	icn83xx_ts_wakeup();
	icn83xx_ts_reset();
	if (icn83xx_ts->use_irq) {
		printk("icn83xx_irq_enable\n");
		icn83xx_irq_enable();
	}
	else {	 
		printk("icn83xx_ts_resume  hrtimer_start\n");
		hrtimer_start(&icn83xx_ts->timer, ktime_set(CTP_START_TIMER/1000, (CTP_START_TIMER%1000)*1000000), HRTIMER_MODE_REL);
	}
	return 0;
}

static int rk_ts_early_suspend(struct tp_device *tp_d)
{
    struct icn83xx_ts_data *ts = container_of(tp_d, struct icn83xx_ts_data, tp);
	//printk("----%s---\n", __func__);
	if (ts->is_suspend == 0) {
    	icn83xx_ts_suspend(ts->client, PMSG_SUSPEND);
		ts->is_suspend = 1;
	}
    return 0;
}

static int rk_ts_early_resume(struct tp_device *tp_d)
{
    struct icn83xx_ts_data *ts = container_of(tp_d, struct icn83xx_ts_data, tp);
	//printk("----%s---\n", __func__);
	if (ts->is_suspend == 1) {
		icn83xx_ts_resume(ts->client);
		ts->is_suspend = 0;
	}
    return 0;
}
#endif

/***********************************************************************************************
Name    :   icn83xx_request_io_port
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_request_io_port(struct icn83xx_ts_data *icn83xx_ts)
{
    icn83xx_ts->screen_max_x = SCREEN_MAX_X;
    icn83xx_ts->screen_max_y = SCREEN_MAX_Y;
    icn83xx_ts->irq = gpio_to_irq(icn83xx_ts->board_info->irq_pin);
    return 0;
}

/***********************************************************************************************
Name    :   icn83xx_free_io_port
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_free_io_port(struct icn83xx_ts_data *icn83xx_ts)
{    
	return 0;   
}

/***********************************************************************************************
Name    :   icn83xx_request_irq
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_request_irq(struct icn83xx_ts_data *icn83xx_ts)
{
    int err = -1;
#if 0
    err = gpio_request(icn83xx_ts->irq, "TS_INT"); //Request IO
    if (err < 0) {
        icn83xx_error("Failed to request GPIO:%d, ERRNO:%d\n", (int)icn83xx_ts->irq, err);
        return err;
    }
    gpio_direction_input(icn83xx_ts->irq);
#endif
    err = request_irq(icn83xx_ts->irq, icn83xx_ts_interrupt, IRQ_TYPE_EDGE_FALLING, "icn83xx_ts", icn83xx_ts);
    if (err < 0) {
        icn83xx_error("icn83xx_ts_probe: request irq failed\n");
        return err;
    } 
    else {
        icn83xx_irq_disable();
        icn83xx_ts->use_irq = 1;        
    } 

    return 0;
}


/***********************************************************************************************
Name    :   icn83xx_free_irq
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_free_irq(struct icn83xx_ts_data *icn83xx_ts)
{
    if (icn83xx_ts) {
        if (icn83xx_ts->use_irq) {
            free_irq(icn83xx_ts->irq, icn83xx_ts);
        }
        else {
            hrtimer_cancel(&icn83xx_ts->timer);
        }
    }
	return 0;
}

/***********************************************************************************************
Name    :   icn83xx_request_input_dev
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_request_input_dev(struct icn83xx_ts_data *icn83xx_ts)
{
    int ret = -1;    
    struct input_dev *input_dev;

    input_dev = input_allocate_device();
    if (!input_dev) {
        icn83xx_error("failed to allocate input device\n");
        return -ENOMEM;
    }
    icn83xx_ts->input_dev = input_dev;

    icn83xx_ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
#if CTP_REPORT_PROTOCOL
    __set_bit(INPUT_PROP_DIRECT, icn83xx_ts->input_dev->propbit);
    input_mt_init_slots(icn83xx_ts->input_dev, POINT_NUM*2, 0);
#else
    set_bit(ABS_MT_TOUCH_MAJOR, icn83xx_ts->input_dev->absbit);
    set_bit(ABS_MT_POSITION_X, icn83xx_ts->input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y, icn83xx_ts->input_dev->absbit);
    set_bit(ABS_MT_WIDTH_MAJOR, icn83xx_ts->input_dev->absbit); 
#endif
    input_set_abs_params(icn83xx_ts->input_dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
    input_set_abs_params(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(icn83xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);  
    input_set_abs_params(icn83xx_ts->input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

    __set_bit(KEY_MENU,  input_dev->keybit);
    __set_bit(KEY_BACK,  input_dev->keybit);
    __set_bit(KEY_HOME,  input_dev->keybit);
    __set_bit(KEY_SEARCH,  input_dev->keybit);

    input_dev->name = CTP_NAME;
    ret = input_register_device(input_dev);
    if (ret) {
        icn83xx_error("Register %s input device failed\n", input_dev->name);
        input_free_device(input_dev);
        return -ENODEV;        
    }
    
#ifdef CONFIG_HAS_EARLYSUSPEND
    icn83xx_trace("==register_early_suspend =\n");
    icn83xx_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    icn83xx_ts->early_suspend.suspend = icn83xx_ts_suspend;
    icn83xx_ts->early_suspend.resume  = icn83xx_ts_resume;
    register_early_suspend(&icn83xx_ts->early_suspend);
#endif

    return 0;
}

#if SUPPORT_DELAYED_WORK
static void icn_delayedwork_fun(struct delayed_work *icn_delayed_work)
{
    int retry;
    short fwVersion = 0;
    short curVersion = 0;
    icn83xx_trace("====%s begin1111=====.  \n", __func__);
 
#if SUPPORT_FW_UPDATE 
    #if COMPILE_FW_WITH_DRIVER
    icn83xx_set_fw(sizeof(icn83xx_fw), &icn83xx_fw[0]);
    #endif 
	 fwVersion = icn83xx_read_fw_Ver(firmware);
	 curVersion = icn83xx_readVersion();
	 icn83xx_trace("fwVersion : 0x%x\n", fwVersion); 
	 icn83xx_trace("current version: 0x%x\n", curVersion);	
 
 
#if FORCE_UPDATA_FW
	 retry = 5;
	 while(retry > 0) {
		 if(R_OK == icn83xx_fw_update(firmware)) {
			 break;
		 }
		 retry--;
		 icn83xx_error("icn83xx_fw_update failed.\n");		  
	 }
#else
	 if(fwVersion > curVersion) {
		retry = 5;
		 while(retry > 0) {
			 if(R_OK == icn83xx_fw_update(firmware)) {
				 break;
			 }
			 retry--;
			 icn83xx_error("icn83xx_fw_update failed.\n");	 
		 }
	}
#endif
 
#endif
 
   
     icn83xx_irq_enable();
     icn83xx_trace("====%s over1111=====.  \n", __func__);
}
#endif


char FbCap[4][16]={
            {0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14},
            {0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},           
            {0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08},
};  

#ifdef CONFIG_OF
#define OF_TP_SUPPLY  "i2c"
#define OF_TP_SUPPLY_A  "i2ca"
#define OF_TP_PIN_RESET "intel,ts-gpio-reset"
#define OF_TP_PIN_IRQ   "intel,ts-gpio-irq"

static struct tp_board_info *tp_of_get_platdata(
	struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct tp_board_info *pdata;
	struct regulator *pow_reg;
	int ret;
	pdata = devm_kzalloc(&client->dev,
		sizeof(struct  tp_board_info), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	/* regulator */
	pow_reg = regulator_get(&client->dev, OF_TP_SUPPLY);
	pdata->power = pow_reg;
	if (IS_ERR(pow_reg)) {
		dev_warn(&client->dev, "%s can't get %s-supply handle\n",
			 np->name, OF_TP_SUPPLY);
		pdata->power = NULL;
	}

	pow_reg = regulator_get(&client->dev, OF_TP_SUPPLY_A);
	pdata->power2 = pow_reg;
	if (IS_ERR(pow_reg)) {
		dev_warn(&client->dev, "%s can't get %s-supply handle\n",
			 np->name, OF_TP_SUPPLY_A);
		pdata->power2 = NULL;
	}

	/* pinctrl */
	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl)) {
		ret = PTR_ERR(pdata->pinctrl);
		goto out;
	}

	pdata->pins_default = pinctrl_lookup_state(
		pdata->pinctrl, PINCTRL_STATE_DEFAULT);
	if (IS_ERR(pdata->pins_default))
		dev_err(&client->dev, "could not get default pinstate\n");

	pdata->pins_sleep = pinctrl_lookup_state(
		pdata->pinctrl, PINCTRL_STATE_SLEEP);
	if (IS_ERR(pdata->pins_sleep))
		dev_err(&client->dev, "could not get sleep pinstate\n");

	pdata->pins_inactive = pinctrl_lookup_state(
		pdata->pinctrl, "inactive");
	if (IS_ERR(pdata->pins_inactive))
		dev_err(&client->dev, "could not get inactive pinstate\n");

	/* gpio reset */
	pdata->reset_pin =
		of_get_named_gpio_flags(client->dev.of_node,
			OF_TP_PIN_RESET, 0, NULL);
	if (pdata->reset_pin <= 0) {
		dev_err(&client->dev,
			"error getting gpio for %s\n",  OF_TP_PIN_RESET);
		ret = -EINVAL;
		goto out;
	}
	/* gpio irq */
	pdata->irq_pin =
		of_get_named_gpio_flags(client->dev.of_node,
			OF_TP_PIN_IRQ, 0, NULL);
	if (pdata->irq_pin <= 0) {
		dev_err(&client->dev,
			"error getting gpio for %s\n",  OF_TP_PIN_IRQ);
		ret = -EINVAL;
		goto out;
	}

	/* pm */
	pdata->pm_platdata = of_device_state_pm_setup(np);
	if (IS_ERR(pdata->pm_platdata)) {
		dev_warn(&client->dev, "Error during device state pm init\n");
		ret = PTR_ERR(pdata->pm_platdata);
		goto out;
	}
	return pdata;
out:
	return ERR_PTR(ret);
}
#endif

//static struct wake_lock touch_wakelock;

static int icn83xx_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct icn83xx_ts_data *icn83xx_ts;
    int err = 0;
#if SUPPORT_FW_CALIB
    char value;
	int average;
#endif
#if SUPPORT_FW_UPDATE
    short fwVersion = 0;
    short curVersion = 0;
    int retry;
#endif
    icn83xx_trace("====%s begin=====.  \n", __func__);

	//wake_lock_init(&touch_wakelock, WAKE_LOCK_SUSPEND, "touch");
	//wake_lock(&touch_wakelock); //system do not enter deep sleep
	
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        icn83xx_error("I2C check functionality failed.\n");
        return -ENODEV;
    }

    icn83xx_ts = kzalloc(sizeof(*icn83xx_ts), GFP_KERNEL);
    if (!icn83xx_ts) {
        icn83xx_error("Alloc icn83xx_ts memory failed.\n");
        return -ENOMEM;
    }
    memset(icn83xx_ts, 0, sizeof(*icn83xx_ts));
	
	icn83xx_ts->board_info = tp_of_get_platdata(client);

    this_client = client;
    this_client->addr = client->addr;
    i2c_set_clientdata(client, icn83xx_ts);
	icn83xx_ts->client = client;
	  
    icn83xx_ts->work_mode = 0;
    spin_lock_init(&icn83xx_ts->irq_lock);
//    icn83xx_ts->irq_lock = SPIN_LOCK_UNLOCKED;

	err = gpio_request(icn83xx_ts->board_info->reset_pin, "tp reset");
	if (err) {
        icn83xx_error("icn83xx reset gpio request failed.\n");
		return -1;
	}

    icn83xx_ts_reset();

    err = icn83xx_iic_test();
    if (err < 0) {
        icn83xx_error("icn83xx_iic_test  failed.\n");
		 goto err_free_mem;
#if SUPPORT_FW_UPDATE
    #if COMPILE_FW_WITH_DRIVER
        icn83xx_set_fw(sizeof(icn83xx_fw), &icn83xx_fw[0]);
    #endif
        if(icn83xx_check_progmod() == 0) {
        		
            retry = 5;
            icn83xx_trace("in prog mode\n");  
            while(retry > 0) {
                if(R_OK == icn83xx_fw_update(firmware)) {
                    break;
                }
                retry--;
                icn83xx_error("icn83xx_fw_update failed.\n");
            }
        }
        else {
            icn83xx_error("I2C communication failed.\n");
            kfree(icn83xx_ts);
            return -1;
        }
#endif        
    }
    else {
        icn83xx_trace("iic communication ok\n"); 
    }

#if SUPPORT_FW_UPDATE 
    #if COMPILE_FW_WITH_DRIVER
    icn83xx_set_fw(sizeof(icn83xx_fw), &icn83xx_fw[0]);
    #endif 
    fwVersion = icn83xx_read_fw_Ver(firmware);
    curVersion = icn83xx_readVersion();
    icn83xx_trace("fwVersion : 0x%x\n", fwVersion); 
    icn83xx_trace("current version: 0x%x\n", curVersion);  


#if FORCE_UPDATA_FW
    retry = 5;
    while(retry > 0) {
        if(R_OK == icn83xx_fw_update(firmware)) {
            break;
        }
        retry--;
        icn83xx_error("icn83xx_fw_update failed.\n");        
    }
#else
    if(fwVersion > curVersion) {
    	        retry = 5;
        while(retry > 0) {
            if(R_OK == icn83xx_fw_update(firmware)) {
                break;
            }
            retry--;
            icn83xx_error("icn83xx_fw_update failed.\n");   
        }
	}
#endif

#endif

#if SUPPORT_FW_CALIB
    err = icn83xx_read_reg(0, &value);
    if (err > 0) {
//auto calib fw
        average = icn83xx_calib(0, NULL);
//fix FbCap
//      average = icn83xx_calib(0, FbCap[1]);
        icn83xx_trace("average : %d\n", average); 
        icn83xx_setPeakGroup(250, 150);
        icn83xx_setDownUp(400, 300);
    }
#endif

#if SUPPORT_SYSFS
		icn83xx_create_sysfs(client);
#endif
	
#if SUPPORT_PROC_FS
		sema_init(&icn83xx_ts->sem, 1);
		init_proc_node();
#endif

	INIT_WORK(&icn83xx_ts->pen_event_work, icn83xx_ts_pen_irq_work);
	icn83xx_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!icn83xx_ts->ts_workqueue) {
	   icn83xx_error("create_singlethread_workqueue failed.\n");
	   kfree(icn83xx_ts);
	   return -ESRCH;
	}

	err= icn83xx_request_input_dev(icn83xx_ts);
	if (err < 0) {
	   icn83xx_error("request input dev failed\n");
	   kfree(icn83xx_ts);
	   return err;		  
	}

	err = icn83xx_request_io_port(icn83xx_ts);
	if (err != 0) {
	   icn83xx_error("icn83xx_request_io_port failed.\n");
	   kfree(icn83xx_ts);
	   return err;
	}

#if TOUCH_VIRTUAL_KEYS
    icn83xx_ts_virtual_keys_init();
#endif
    err = icn83xx_request_irq(icn83xx_ts);
    if (err != 0) {
        printk("request irq error, use timer\n");
        icn83xx_ts->use_irq = 0;
        hrtimer_init(&icn83xx_ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        icn83xx_ts->timer.function = chipone_timer_func;
        hrtimer_start(&icn83xx_ts->timer, ktime_set(CTP_START_TIMER/1000, (CTP_START_TIMER%1000)*1000000), HRTIMER_MODE_REL);
    }

    #if SUPPORT_DELAYED_WORK
    INIT_DELAYED_WORK(&icn83xx_ts->icn_delayed_work, icn_delayedwork_fun);
    schedule_delayed_work(&icn83xx_ts->icn_delayed_work, msecs_to_jiffies(8000));
	#else
    icn83xx_irq_enable();
    #endif

	icn83xx_ts->is_suspend = 0;
	
	icn83xx_ts->tp.tp_resume = rk_ts_early_resume;
	icn83xx_ts->tp.tp_suspend = rk_ts_early_suspend;
	tp_register_fb(&icn83xx_ts->tp);

    icn83xx_trace("==%s over =\n", __func__);    
    return 0;

	err_free_mem:
	//input_free_device(input_dev);
	gpio_free(icn83xx_ts->board_info->reset_pin);
	kfree(icn83xx_ts);
	return err;
}

static int icn83xx_ts_remove(struct i2c_client *client)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(client);  
    icn83xx_trace("==icn83xx_ts_remove=\n");
    icn83xx_irq_disable();
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&icn83xx_ts->early_suspend);
#endif
	tp_unregister_fb(&icn83xx_ts->tp);
#if SUPPORT_PROC_FS
    uninit_proc_node();
#endif

    input_unregister_device(icn83xx_ts->input_dev);
    input_free_device(icn83xx_ts->input_dev);
    cancel_work_sync(&icn83xx_ts->pen_event_work);
    destroy_workqueue(icn83xx_ts->ts_workqueue);
    icn83xx_free_irq(icn83xx_ts);
    icn83xx_free_io_port(icn83xx_ts);
    kfree(icn83xx_ts);    
    i2c_set_clientdata(client, NULL);
    return 0;
}

static const struct i2c_device_id icn83xx_ts_id[] = {
    { "chipone-icn83xx", 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, icn83xx_ts_id);

static struct i2c_driver icn83xx_ts_driver = {
    .class      = I2C_CLASS_HWMON,
    .probe      = icn83xx_ts_probe,
    .remove     = icn83xx_ts_remove,
    .id_table   = icn83xx_ts_id,
    .driver = {
        .name   = "chipone-icn83xx",
        .owner  = THIS_MODULE,
    },
};


static int __init icn83xx_ts_init(void)
{ 
    int ret = -1;
    icn83xx_trace("===========================%s=====================\n", __func__);
    ret = i2c_add_driver(&icn83xx_ts_driver);
    return ret;
}

static void __exit icn83xx_ts_exit(void)
{
    icn83xx_trace("==icn83xx_ts_exit==\n");
    i2c_del_driver(&icn83xx_ts_driver);
}

late_initcall(icn83xx_ts_init);
module_exit(icn83xx_ts_exit);

MODULE_AUTHOR("Zhimin Tian <zmtian@chiponeic.com>");
MODULE_DESCRIPTION("Chipone icn83xx TouchScreen driver");
MODULE_LICENSE("GPL v2");
