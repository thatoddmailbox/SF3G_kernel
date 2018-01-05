/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#include <linux/bitops.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdhci.h>

#include "sdio_internal.h"
#include "sdio_tx.h"
#include "sdio_tx_policy.h"
#include "iwl-csr.h"
#include "iwl-io.h"
#include "iwl-op-mode.h"
#include "iwl-fh.h"
#include "iwl-agn-hw.h"
#include "iwl-debug.h"
#include "iwl-prph.h"
#ifdef CPTCFG_IWLWIFI_DEVICE_TESTMODE
#include "iwl-dnt-cfg.h"
#endif

#define IWL_SDIO_BUS_POLL_STALL_TIME_MIN	10
#define IWL_SDIO_BUS_POLL_STALL_TIME_MAX	100
#define IWL_SDIO_READ_VAL_ERR			0xFFFFFFFF
#define IWL_SDIO_CLOCK_STALL_TIME_MIN		50
#define IWL_SDIO_CLOCK_STALL_TIME_MAX		200
#define IWL_SDIO_H2D_ACK_WAIT_TIME		10 /* msec */
#define IWL_SDIO_POLL_BIT_MAX_TRIES		10 /* Times */
#define IWL_SDIO_DISABLE_SLEEP			10 /* msec */
#define HOST_COMPLETE_TIMEOUT			(2 * HZ)

static const struct iwl_sdio_sf_mem_addresses iwl8000_sf_addresses = {
	.tfd_base_addr = IWL_SDIO_8000_SF_MEM_TFD_BASE_ADDR,
	.tfdi_base_addr = IWL_SDIO_8000_SF_MEM_TFDI_BASE_ADDR,
	.bc_base_addr = IWL_SDIO_8000_SF_MEM_BC_BASE_ADDR,
	.tg_buf_base_addr = IWL_SDIO_8000_SF_MEM_TG_BUF_BASE_ADDR,
	.adma_dsc_mem_base = IWL_SDIO_8000_SF_MEM_ADMA_DSC_MEM_BASE,
	.tb_base_addr = IWL_SDIO_8000_SF_MEM_TB_BASE_ADDR,
};

static const struct iwl_sdio_sf_mem_addresses iwl7000_sf_addresses = {
	.tfd_base_addr = IWL_SDIO_7000_SF_MEM_TFD_BASE_ADDR,
	.tfdi_base_addr = IWL_SDIO_7000_SF_MEM_TFDI_BASE_ADDR,
	.bc_base_addr = IWL_SDIO_7000_SF_MEM_BC_BASE_ADDR,
	.tg_buf_base_addr = IWL_SDIO_7000_SF_MEM_TG_BUF_BASE_ADDR,
	.adma_dsc_mem_base = IWL_SDIO_7000_SF_MEM_ADMA_DSC_MEM_BASE,
	.tb_base_addr = IWL_SDIO_7000_SF_MEM_TB_BASE_ADDR,
};

/*
 * Returns the correct flag for the access control field in the target
 * access commnad according to the given address.
 */
static u32 iwl_sdio_get_addr_auto_inc_flag(u32 address)
{
#define NON_AUTO_INC_ADDRESS_1	(0x404)
#define NON_AUTO_INC_ADDRESS_2	(0x408)
#define NON_AUTO_INC_ADDRESS_3	(0x460)
	switch (address) {
	case NON_AUTO_INC_ADDRESS_1:
	case NON_AUTO_INC_ADDRESS_2:
	case NON_AUTO_INC_ADDRESS_3:
		return IWL_SDIO_CTRL_FIXED_ADDR_BIT;
	default:
		return 0;
	}
}

#ifdef CONFIG_X86_MRFLD
static void iwl_sdio_set_power(struct iwl_trans *trans, bool on)
{
	struct sdio_func *sdio_func = IWL_TRANS_SDIO_GET_FUNC(trans);

	if (on)
		mmc_power_restore_host(sdio_func->card->host);
	else
		mmc_power_save_host(sdio_func->card->host);
}
#else
static inline void iwl_sdio_set_power(struct iwl_trans *trans, bool on)
{}
#endif

/*
 * Sets the target access command's access_control field based on the
 * target address and target section.
*/
static u32 iwl_sdio_set_cmd_access_control(enum iwl_sdio_ta_ac_flags ac_mode,
					   u32 target_addr, bool write_mode)
{
	u32 access_control = 0;

	switch (ac_mode) {
	case IWL_SDIO_TA_AC_DIRECT:
		/* All zeroed */
		access_control |= iwl_sdio_get_addr_auto_inc_flag(target_addr);
		break;
	case IWL_SDIO_TA_AC_INDIRECT:
		access_control |= IWL_SDIO_CTRL_INDIRECT_BIT;
		if (write_mode) {
			access_control |= IWL_SDIO_HBUS_TARG_MEM_WADD;
			access_control |= (IWL_SDIO_HBUS_TARG_MEM_WDAT
					   << IWL_SDIO_CTRL_DATA_SHIFT);
		} else {
			access_control |= IWL_SDIO_HBUS_TARG_MEM_RADD;
			access_control |= (IWL_SDIO_HBUS_TARG_MEM_RDAT
					   << IWL_SDIO_CTRL_DATA_SHIFT);
		}
		break;
	case IWL_SDIO_TA_AC_PRPH:
		access_control |= IWL_SDIO_CTRL_INDIRECT_BIT;
		if (write_mode) {
			access_control |= IWL_SDIO_HBUS_TARG_PRPH_WADDR;
			access_control |= (IWL_SDIO_HBUS_TARG_PRPH_WDAT
					   << IWL_SDIO_CTRL_DATA_SHIFT);
		} else {
			access_control |= IWL_SDIO_HBUS_TARG_PRPH_RADDR;
			access_control |= (IWL_SDIO_HBUS_TARG_PRPH_RDAT
					   << IWL_SDIO_CTRL_DATA_SHIFT);
		}
		break;
	}

	return access_control;
}

/*
 * Returns a global command seq number.
 * Returns both for write and for erad depending on the given flag.
 */
u8 iwl_sdio_get_cmd_seq(struct iwl_trans_sdio *trans_sdio, bool write)
{
	if (write)
		return trans_sdio->ta_write_seq++;

	return trans_sdio->ta_read_seq++;
}

/*
 * Target Access write.
 *
 * The target access command uses the SDTM to write the data.
 * Assumes that the host is claimed.
 */
static int iwl_sdio_ta_write(struct iwl_trans *trans,
			     u32 target_addr, u32 length,
			     void *data, enum iwl_sdio_ta_ac_flags ac_mode)
{
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_ta_cmd *ta_write_cmd;
	u32 padding_length, access_control;
	int ret;

	/* Test that input is valid */
	if ((length > IWL_SDIO_MAX_PAYLOAD_SIZE) ||
	    (target_addr > IWL_SDIO_TA_MAX_ADDRESS)) {
		IWL_ERR(trans, "Cannot send buffer for TA command\n");
		return -EINVAL;
	}

	/* Fill the access command */
	access_control = iwl_sdio_set_cmd_access_control(ac_mode,
							 target_addr, true);

	/* Fill target acess command */
	ta_write_cmd = &trans_sdio->ta_buff.ta_cmd;
	ta_write_cmd->hdr.op_code = IWL_SDIO_OP_CODE_WRITE | IWL_SDIO_EOT_BIT;
	ta_write_cmd->hdr.seq_number = iwl_sdio_get_cmd_seq(trans_sdio, true);
	ta_write_cmd->hdr.signature =
			cpu_to_le16(IWL_SDIO_CMD_HEADER_SIGNATURE);
	ta_write_cmd->address = cpu_to_le32(target_addr);
	ta_write_cmd->address |= cpu_to_le32(IWL_SDIO_TA_AW_FOUR_BYTE);
	ta_write_cmd->length = cpu_to_le32(length);
	ta_write_cmd->access_control = cpu_to_le32(access_control);

	/* Copy the data to the cmd buffer */
	if (data)
		memcpy(trans_sdio->ta_buff.payload, data, length);
	else
		memset(trans_sdio->ta_buff.payload, 0, length);

	/* Pad as needed */
	padding_length = IWL_SDIO_MAX_PAYLOAD_SIZE - length;
	memset(trans_sdio->ta_buff.payload + length,
	       IWL_SDIO_CMD_PAD_BYTE, padding_length);

	IWL_DEBUG_INFO(trans,
		"### TA WRITE COMMAND: addr 0x%x, length 0x%x, seq %d, val %d",
		ta_write_cmd->address, ta_write_cmd->length,
		ta_write_cmd->hdr.seq_number,
		trans_sdio->ta_buff.payload[0]);

	/* Stream the data to the SDTM */
	ret = sdio_writesb(func, IWL_SDIO_DATA_ADDR,
			   &trans_sdio->ta_buff,
			   sizeof(struct iwl_sdio_cmd_buffer));
	if (ret)
		IWL_ERR(trans, "Cannot send buffer for TA command\n");

	return ret;
}

/*
 * Called from the interrupt handler when there is data ready after a
 * target access read command.
 * Wakes up the ta_read waitqueue that is waiting for this data.
 *
 * Since it's called from the ISR the host is already claimed.
 */
void iwl_sdio_handle_ta_read_ready(struct iwl_trans *trans,
				   struct iwl_sdio_cmd_buffer *ta_buff)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_ta_cmd *ta_cmd;

	IWL_DEBUG_INFO(trans,
		"TA READ addr 0x%.8x length 0x%x, access ctrl 0x%x seq %d\n",
		le32_to_cpu(ta_buff->ta_cmd.address),
		le32_to_cpu(ta_buff->ta_cmd.length),
		le32_to_cpu(ta_buff->ta_cmd.access_control),
		ta_buff->ta_cmd.hdr.seq_number);

	/* Mark that the TA transaction has ended */
	WARN_ON(!test_and_clear_bit(STATUS_TA_ACTIVE, &trans->status));

	/*
	 * We are under trans_sdio->target_access_mtx, but can't check it since
	 * it is held by another thread.
	 */

	/* Fetch data from buffer and test parameters validity against the
	 * sent command */
	ta_cmd = &trans_sdio->ta_buff.ta_cmd;
	if (WARN_ON(ta_cmd->length != ta_buff->ta_cmd.length) ||
	    WARN_ON((le32_to_cpu(ta_cmd->address) & IWL_SDIO_TA_MAX_ADDRESS) !=
		    (le32_to_cpu(ta_buff->ta_cmd.address) &
						IWL_SDIO_TA_MAX_ADDRESS)) ||
	    WARN_ON(ta_cmd->hdr.seq_number !=
		    ta_buff->ta_cmd.hdr.seq_number) ||
	    WARN_ON(IWL_SDIO_OP_CODE_READ !=
		    (ta_buff->ta_cmd.hdr.op_code & IWL_SDIO_OP_CODE_MASK))) {
		IWL_ERR(trans, "Target access read response is not valid\n");
		return;
	}

	/* Copy to the given read buffer */
	if (WARN_ON(!trans_sdio->ta_read_buff))
			return;
	memcpy(trans_sdio->ta_read_buff, ta_buff->payload,
	       le32_to_cpu(ta_buff->ta_cmd.length));

	wake_up(&trans_sdio->wait_target_access);
}

/*
 * Target Access read
 *
 * The target access command uses the SDTM to signal for a data
 * read request.
 * The actual data will be received through an interrupt and a data read
 * transaction.
 * Assumes that the host is claimed.
 */
static int iwl_sdio_ta_read(struct iwl_trans *trans,
			     u32 target_addr, u32 length, void *target_buff,
			     enum iwl_sdio_ta_ac_flags ac_mode)
{
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_ta_cmd *ta_read_cmd;
	u8 seq_number;
	u32 access_control;
	int ret;

	/* Test that input is valid */
	if ((length > IWL_SDIO_TA_MAX_LENGTH) ||
	    (target_addr > IWL_SDIO_TA_MAX_ADDRESS)) {
		IWL_ERR(trans, "Cannot send buffer for TA command\n");
		return -EINVAL;
	}

	/* Check that the given target buffer is valid */
	if (WARN_ON(!target_buff))
		return -ENOMEM;

	/* Fill the access command */
	access_control = iwl_sdio_set_cmd_access_control(ac_mode,
							 target_addr, false);

	/* Set the status, indicate that the transaction has started for TA read
	 * and shared resources with the TA write are in use */
	WARN_ON(test_and_set_bit(STATUS_TA_ACTIVE, &trans->status));

	/* Fill target acess command */
	ta_read_cmd = &trans_sdio->ta_buff.ta_cmd;
	ta_read_cmd->hdr.op_code = IWL_SDIO_OP_CODE_READ | IWL_SDIO_EOT_BIT;
	seq_number = iwl_sdio_get_cmd_seq(trans_sdio, false);
	ta_read_cmd->hdr.seq_number = seq_number;
	ta_read_cmd->hdr.signature =
			cpu_to_le16(IWL_SDIO_CMD_HEADER_SIGNATURE);
	ta_read_cmd->address = cpu_to_le32(target_addr);
	/* Direct access must be with bus width 0 all else 4B */
	if (ac_mode != IWL_SDIO_TA_AC_DIRECT)
		ta_read_cmd->address |= cpu_to_le32(IWL_SDIO_TA_AW_FOUR_BYTE);
	ta_read_cmd->length = cpu_to_le32(length);
	ta_read_cmd->access_control = cpu_to_le32(access_control);

	/* Pad the buffer */
	memset(trans_sdio->ta_buff.payload, IWL_SDIO_CMD_PAD_BYTE,
	       IWL_SDIO_MAX_PAYLOAD_SIZE);

	/* Set the given buffer as target for the read data */
	WARN_ON(trans_sdio->ta_read_buff);
	trans_sdio->ta_read_buff = target_buff;

	/* Stream the data to the SDTM */
	ret = sdio_writesb(func, IWL_SDIO_DATA_ADDR,
			   &trans_sdio->ta_buff,
			   sizeof(struct iwl_sdio_cmd_buffer));
	if (ret)
		IWL_ERR(trans, "Cannot send buffer for TA READ command\n");

	/* Wait for wake up event after the TA read buffer has been received */
	sdio_release_host(func);
	ret = wait_event_timeout(trans_sdio->wait_target_access,
				 !test_bit(STATUS_TA_ACTIVE,
					   &trans->status),
				 HOST_COMPLETE_TIMEOUT);
	sdio_claim_host(func);
	if (!ret) {
		if (test_bit(STATUS_TA_ACTIVE, &trans->status)) {
			IWL_ERR(trans,
				"Target access failed after %dms.\n",
				jiffies_to_msecs(HOST_COMPLETE_TIMEOUT));

			clear_bit(STATUS_TA_ACTIVE, &trans->status);
			trans_sdio->ta_read_buff = NULL;
			return -ETIMEDOUT;
		}
	} else {
		ret = 0; /* Clear the time left after waiting */

	IWL_DEBUG_INFO(trans,
		"### TA READ COMMAND: addr 0x%x, length 0x%x, seq %d, val %d",
		ta_read_cmd->address, ta_read_cmd->length,
		ta_read_cmd->hdr.seq_number,
		*((u32 *)trans_sdio->ta_read_buff));

	}

	/*
	 * Clear the buffer before next use.
	 * The actuall data has already been copied to the given buffer
	 */
	trans_sdio->ta_read_buff = NULL;
	IWL_DEBUG_INFO(trans, "Target access read has successfully finished\n");
	return ret;
}

/*
 * Checks if the card is powered up.
 * Assumes that the host is claimed.
 *
 * Returns true if the Nic is powered up, false else.
 */
bool iwl_sdio_is_func_enabled(struct iwl_trans *trans)
{
	int ret;
	u32 val;
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);

	/* Read the SDIO CCCR */
	val = iwl_sdio_read8(trans, SDIO_CCCR_IOEx, &ret);
	if (ret)
		return false;

	/* Check if CCCR bit enabled for this function */
	if (val & func->num)
		return true;

	return false;
}

/*
 * Write a byte to the given offset with the given val.
 * No return value. An error message will be printed in case of error.
 *
 * Since it's an API function assumes that the host is not claimed.
 */
static void iwl_trans_sdio_write8(struct iwl_trans *trans, u32 ofs, u8 val)
{
	int ret;
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);

	sdio_claim_host(func);

	/* Use target acces through the SDTM for indirect write */
	ret = iwl_sdio_ta_write(trans, ofs, sizeof(u8), &val,
				IWL_SDIO_TA_AC_INDIRECT);
	if (ret)
		/* TODO: Call op_mode nic_error if the operation failed
		 * and retry the operation. */
		IWL_ERR(trans, "Failed to write byte to SDIO bus\n");

	sdio_release_host(func);
}

/*
 * Write a dword to the given offset with the given val.
 * No return value. An error message will be printed in case of error.
 *
 * Since it's an API function assumes that the host is not claimed.
 */
static void iwl_trans_sdio_write32(struct iwl_trans *trans, u32 ofs, u32 val)
{
	int ret;
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);

	sdio_claim_host(func);

	/* Use target access through the SDTM for direct write */
	ret = iwl_sdio_ta_write(trans, ofs, sizeof(u32), &val,
				IWL_SDIO_TA_AC_DIRECT);
	if (ret)
		/* TODO: Call op_mode nic_error if the operation failed
		 * and retry the operation. */
		IWL_ERR(trans, "Failed to write dword to SDIO bus\n");

	sdio_release_host(func);
}

/*
 * Reads a dword from the given offset address.
 * Returns the value in the address or a negative value describing the error.
 *
 * Since it's an API function assumes that the host is not claimed.
 */
static u32 iwl_trans_sdio_read32(struct iwl_trans *trans, u32 ofs)
{
	int ret;
	u32 ret_val;
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);

	sdio_claim_host(func);

	/* Use target acces through the SDTM for indirect read */
	ret = iwl_sdio_ta_read(trans, ofs, sizeof(u32), &ret_val,
			       IWL_SDIO_TA_AC_DIRECT);
	if (ret) {
		IWL_ERR(trans, "Failed to read word from SDIO bus\n");
		ret_val = IWL_SDIO_READ_VAL_ERR;
	}

	sdio_release_host(func);
	return ret_val;
}

/*
 * Perform peripheral access write to the NIC.
 * Uses the SDTM in ordr to perform the access.
 * No need to grab NIC access before, it will be performed by the SDTM.
 *
 * Since it's an API function assumes that the host is not claimed.
 */
static void iwl_trans_sdio_write_prph(struct iwl_trans *trans, u32 addr,
				      u32 val)
{
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);
	int ret;

	sdio_claim_host(func);

	/* Use standard target access - The SDTM will perfrom the prph flow */
	ret = iwl_sdio_ta_write(trans, addr, sizeof(u32), &val,
				IWL_SDIO_TA_AC_PRPH);
	if (ret)
		/*
		 * TODO: Call op_mode nic_error if the operation failed
		 * and retry the operation.
		 */
		IWL_ERR(trans, "Failed to write to prph. address 0x%x, ret %d",
			addr , ret);

	sdio_release_host(func);
}

/*
 * Perform peripheral access write to the NIC.
 * Uses the SDTM in ordr to perform the access.
 * No need to grab NIC access before, it will be performed by the SDTM.
 */
static void iwl_sdio_write_prph_no_claim(struct iwl_trans *trans, u32 addr,
					 u32 val)
{
	/* Use standard target access - The SDTM will perfrom the prph flow */
	int ret = iwl_sdio_ta_write(trans, addr, sizeof(u32), &val,
				IWL_SDIO_TA_AC_PRPH);
	if (ret) {
		/*
		 * TODO: Call op_mode nic_error if the operation failed
		 * and retry the operation.
		 */
		IWL_ERR(trans, "Failed to write to prph. address 0x%x, ret %d",
			addr , ret);
	}
}

/*
 * Perform peripheral access read to the NIC.
 * Uses the SDTM in ordr to perform the access.
 * No need to grab NIC access before, it will be performed by the SDTM.
 *
 * Since it's an API function assumes that the host is not claimed.
 */
static u32 iwl_trans_sdio_read_prph(struct iwl_trans *trans, u32 addr)
{
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);
	u32 ret_val;
	int ret;

	sdio_claim_host(func);

	/* Use standard target access - The SDTM will perfrom the prph flow */
	ret = iwl_sdio_ta_read(trans, addr, sizeof(u32), &ret_val,
			       IWL_SDIO_TA_AC_PRPH);

	if (ret) {
		/*
		 * TODO: Call op_mode nic_error if the operation failed
		 * and retry the operation.
		 */
		IWL_ERR(trans, "Failed to read from prph. address 0x%x, ret %d",
			addr , ret);
		ret_val = IWL_SDIO_READ_VAL_ERR;
	}

	sdio_release_host(func);
	return ret_val;
}

/*
 * Perform peripheral access read to the NIC.
 * Uses the SDTM in ordr to perform the access.
 * No need to grab NIC access before, it will be performed by the SDTM.
 */
 static u32 iwl_sdio_read_prph_no_claim(struct iwl_trans *trans, u32 addr)
{
	u32 ret_val;
	int ret;

	/* Use standard target access - The SDTM will perfrom the prph flow */
	ret = iwl_sdio_ta_read(trans, addr, sizeof(u32), &ret_val,
			       IWL_SDIO_TA_AC_PRPH);

	if (ret) {
		/*
		 * TODO: Call op_mode nic_error if the operation failed
		 * and retry the operation.
		 */
		IWL_ERR(trans, "Failed to read from prph. address 0x%x, ret %d",
			addr , ret);
		ret_val = IWL_SDIO_READ_VAL_ERR;
	}
	return ret_val;
}

/*
 * Generic method to read and write consecutive memory in the NIC.
 *
 * The access is done in bytes accroding to the nubmer of DWORDS given.
 * The register lock should not be held when calling to this function.
 */
static int iwl_sdio_access_cons_mem(struct iwl_trans *trans, u32 addr,
				    void *buf, int dwords, bool write_access)
{
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	int length = dwords*sizeof(u32);
	u32 offset, copy_size;
	int ret = 0;

	/* Validate input */
	if (!buf)
		return -EINVAL;

	/*Lock register acccess lock */
	mutex_lock(&trans_sdio->target_access_mtx);
	sdio_claim_host(func);
	IWL_DEBUG_INFO(trans,
		       "TA access memory: length %d, addr 0x%x write %d",
		       length, addr, write_access);

	/* Read the requested memory by MAX_PAYLOAD size */
	for (offset = 0; offset < length;
	     offset += IWL_SDIO_MAX_PAYLOAD_SIZE) {
		copy_size = min_t(u32,
				  IWL_SDIO_MAX_PAYLOAD_SIZE,
				  length - offset);
		if (write_access) {
			ret = iwl_sdio_ta_write(trans, addr + offset, copy_size,
					       buf ? buf + offset : NULL,
					       IWL_SDIO_TA_AC_INDIRECT);
		} else {
			ret = iwl_sdio_ta_read(trans, addr + offset, copy_size,
					       buf + offset,
					       IWL_SDIO_TA_AC_INDIRECT);
		}
		if (ret)
			break;
	}

	if (ret)
		IWL_ERR(trans, "Failed to access mem in NIC, ret%d write %d",
			ret, write_access);
	else
		IWL_DEBUG_INFO(trans, "Successfully accessed memory in NIC");

	sdio_release_host(func);
	mutex_unlock(&trans_sdio->target_access_mtx);

	return ret;
}

/*
 * Read Memory from the NIC.
 * Reads a buffer in the length of given number of dwords.
 *
 * Uses the SDTM API, to read consecutive memory efficiently.
 */
static int iwl_trans_sdio_read_mem(struct iwl_trans *trans, u32 addr,
				   void *buf, int dwords)
{
	return iwl_sdio_access_cons_mem(trans, addr, buf, dwords, false);
}

static void iwl_trans_sdio_set_bits_mask(struct iwl_trans *trans, u32 reg,
					 u32 mask, u32 value)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	u32 v;

#ifdef CPTCFG_IWLWIFI_DEBUG
	WARN_ON_ONCE(value & ~mask);
#endif

	mutex_lock(&trans_sdio->target_access_mtx);
	v = iwl_read32(trans, reg);
	v &= ~mask;
	v |= value;
	iwl_write32(trans, reg, v);
	mutex_unlock(&trans_sdio->target_access_mtx);
}

/*
 * Writes Memory to the NIC.
 * Writes the given buffer in the length of given number of dwords.
 *
 * Uses the SDTM API, to write consecutive memory efficiently.
 */
static int iwl_trans_sdio_write_mem(struct iwl_trans *trans, u32 addr,
				    const void *buf, int dwords)
{
	return iwl_sdio_access_cons_mem(trans, addr, (void *)buf, dwords, true);
}

/*
 * Enables the interrupt receiving on the SDIO HW function.
 * Clears the mask on all interrupts.
 *
 *@trans - the generic transport layer.
 */
static int iwl_sdio_enable_interrupts(struct iwl_trans *trans)
{
	int ret;

	ret = iwl_sdio_write8(trans, IWL_SDIO_INTR_ENABLE_ALL,
			       IWL_SDIO_INTR_ENABLE_MASK_REG);
	if (ret)
		IWL_ERR(trans,
			"Failed to enable interrupts on the SDIO bus\n");

	/* Register the interrupts handler */
	ret = sdio_claim_irq(IWL_TRANS_SDIO_GET_FUNC(trans), iwl_sdio_isr);
	if (ret)
		IWL_ERR(trans, "Failed to claim IRQ on SDIO, ret %d\n", ret);

	return ret;
}

/*
 * Disables the interrupt receiving on the SDIO HW function.
 * Sets the masking on all interrupts.
 *
 *@trans - the generic transport layer.
 */
static int iwl_sdio_disable_interrupts(struct iwl_trans *trans)
{
	int ret;

	ret = iwl_sdio_write8(trans, IWL_SDIO_INTR_DISABLE_ALL,
			      IWL_SDIO_INTR_ENABLE_MASK_REG);
	if (ret)
		IWL_ERR(trans,
			"Failed to disable interrupts on the SDIO bus\n");

	return ret;
}

/*
 * Clears the interrupts in the AL.
 */
static int iwl_sdio_clear_interrupts(struct iwl_trans *trans)
{
	int ret;

	ret = iwl_sdio_write8(trans, IWL_SDIO_INTR_CAUSE_CLEAR_ALL_VAL,
			      IWL_SDIO_INTR_CAUSE_REG);
	if (ret)
		IWL_ERR(trans, "Failed to clear interrupts on the SDIO bus\n");

	return ret;
}

/*
 * Powers up and activates the SDIO function.
 * Register access is enable after this flow.
 * Enables the interrupts and registers the ISR handler.
 *
 *@trans - the generic transport layer.
 * If succesded returns 0, else return a negative value desribing the error.
 */
static int iwl_sdio_claim_hw(struct iwl_trans *trans)
{
	int ret;
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);

	/* Claim Bus function from bus driver */
	sdio_claim_host(func);

	/* Power up */
	ret = sdio_enable_func(IWL_TRANS_SDIO_GET_FUNC(trans));
	if (ret) {
		IWL_ERR(trans, "Failed to enable the SDIO bus");
		sdio_release_host(func);
		goto exit_error;
	}

	/* Enable interrupts */
	ret = iwl_sdio_enable_interrupts(trans);
	if (ret)
		goto disable_func;

	return 0;

disable_func:
	sdio_disable_func(func);
exit_error:
	IWL_ERR(trans, "Failed to claim SDIO HW");
	return ret;
}

/*
 * Powers down and deactivates the SDIO function.
 * Register access is disabled after this flow.
 * The host should be claimed before calling this function.
 *
 *@trans - the generic transport layer.
 * If succesded returns 0, else return a negative value desribing the error.
 */
static int iwl_sdio_release_hw(struct iwl_trans *trans)
{
	int ret = 0;
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	/* Power down */
	if (iwl_sdio_is_func_enabled(trans)) {
		IWL_DEBUG_INFO(trans, "Powering down the NIC");
		ret = sdio_disable_func(IWL_TRANS_SDIO_GET_FUNC(trans));
		if (ret)
			IWL_ERR(trans, "Failed to disable the SDIO bus\n");
	}

	/* Release the interrupts registration */
	sdio_release_irq(func);

	/* Cancel any work items initiated by the interrupt handler */
	cancel_work_sync(&trans_sdio->d2h_work);

	/* Release Bus function access from the driver */
	sdio_release_host(func);
	iwl_sdio_set_power(trans, false);

	return ret;
}

/*
 * Resets the hw by disabling and enabling the SDIO HW function.
 * The Caller should request sdio_claim_host before calling to this function.
 *
 *@trans - the generic transport layer.
 */
static int iwl_sdio_reset_hw(struct iwl_trans *trans)
{
	int ret;
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);

	/* Power down */
	ret = sdio_disable_func(func);
	if (ret) {
		IWL_ERR(trans, "Failed to disable the SDIO bus during reset\n");
		return ret;
	}

	/* Wait for 10ms for IOR to become 0 */
	msleep(IWL_SDIO_DISABLE_SLEEP);

	/* Power up */
	ret = sdio_enable_func(func);
	if (ret)
		IWL_ERR(trans, "Failed to enable the SDIO bus during reset\n");

	return ret;
}

/*
 * Wakes up the SDIO function HW clock.
 *
 * Setting the clock to manual causes the NIC and AL clocks to remain active
 * until a specific command to turn off. Every access to the SDIO Core will
 * cause the core to ask the device to keep the clock alive.
 * Meaning that the ADTM access is always enabled.
 *
 *@trans - the generic transport layer.
 */
static int iwl_sdio_func_clock_wakeup(struct iwl_trans *trans)
{
	int ret;

	ret = iwl_sdio_write8(trans, IWL_SDIO_CLK_MANUAL_VAL,
			      IWL_SDIO_CLK_WAKEUP_REG);
	if (ret)
		IWL_ERR(trans, "Failed to wakeup clock on the SDIO bus\n");
	usleep_range(IWL_SDIO_CLOCK_STALL_TIME_MIN,
		     IWL_SDIO_CLOCK_STALL_TIME_MAX);

	return ret;
}

/*
 * Set the SDIO function HW clock to go to auto mode.
 * While the clock is in auto mode when there is no activity on the bus
 * the core can allow the ARM to turn off the clock.
 * This will enable the retention flow.
 *
 *@trans - the generic transport layer.
 */
static __maybe_unused int iwl_sdio_func_clock_sleep(struct iwl_trans *trans)
{
	int ret;

	ret = iwl_sdio_write8(trans, IWL_SDIO_CLK_AUTO_VAL,
			      IWL_SDIO_CLK_WAKEUP_REG);
	if (ret)
		IWL_ERR(trans, "Failed to set the clock to sleep on SDIO\n");
	usleep_range(IWL_SDIO_CLOCK_STALL_TIME_MIN,
		     IWL_SDIO_CLOCK_STALL_TIME_MAX);

	return ret;
}

/*
 * Disable retention of the SDIO HW function.
 * Disables the HW from going to low power state.
 *
 *@trans - the generic transport layer.
 */
static int iwl_sdio_disable_retention(struct iwl_trans *trans)
{
	int ret, ret_val;

	/* Read current retention register value */
	ret_val = iwl_sdio_read8(trans, IWL_SDIO_RETENTION_REG, &ret);
	if (ret)
		goto exit_err;

	/* Write retention disable */
	ret = iwl_sdio_write8(trans, IWL_SDIO_DISABLE_RETENTION_VAL | ret_val,
			      IWL_SDIO_RETENTION_REG);
	if (ret)
		goto exit_err;

	IWL_DEBUG_INFO(trans, "Disabled retention on the SDIO bus\n");
	return 0;

exit_err:
	IWL_ERR(trans, "Failed to disable retention on the SDIO bus\n");
	return ret;
}

/*
 * Enable retention of the SDIO HW function.
 * Eable the HW to go to low power state in case it is idle.
 *
 *@trans - the generic transport layer.
 */
static __maybe_unused int iwl_sdio_enable_retention(struct iwl_trans *trans)
{
	int ret, ret_val;

	/* Read current retention register value */
	ret_val = iwl_sdio_read8(trans, IWL_SDIO_RETENTION_REG, &ret);
	if (ret)
		goto exit_err;

	/* Write retention enable */
	ret = iwl_sdio_write8(trans, ret_val & IWL_SDIO_ENABLE_RETENTION_MASK,
			      IWL_SDIO_RETENTION_REG);
	if (ret)
		goto exit_err;

	IWL_DEBUG_INFO(trans, "Enabled retention on the SDIO bus\n");
	return 0;

exit_err:
	IWL_ERR(trans, "Failed to enable retention on the SDIO bus\n");
	return ret;
}

/*
 * Configure the SDTM in the SDIO AL.
 *
 *@func - The SDIO HW function bus driver.
 */
static int iwl_sdio_config_sdtm(struct iwl_trans *trans)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	__le32 config_buf[7] = {
		[0] = cpu_to_le32(IWL_SDIO_PADDING_TERMINAL),
		[4] = cpu_to_le32(IWL_SDIO_WATCH_DOG_TIMER_TIMEOUT_VALUE),
		[5] = cpu_to_le32(IWL_SDIO_RB_SIZE_1K),
		[6] = cpu_to_le32(IWL_SDIO_RB_SIZE_16K),
	};
	int ret;

	if (trans->cfg->device_family == IWL_DEVICE_FAMILY_8000)
		trans_sdio->sf_mem_addresses = &iwl8000_sf_addresses;
	else
		trans_sdio->sf_mem_addresses = &iwl7000_sf_addresses;

	/*
	 * 0x00010050 - Padding Pattern [31:0] 0xACACACAC
	 * 0x00010054 - S/F Target Buffer Base Address 0x83000 (SIZE 512)
	 * 0x00010058 - S/F PTFD Base 0x81000 (SIZE 1280)
	 * 0x0001005C - S/F TFD  Base 0x80000 (TFD_BUFFER_SIZE 4096) TX data
	 *		will start from addr 0x83000
	 * 0x00010060 - watch dog timer timeout value 0x20 = 2msec
	 * 0x00010064 - min RB size, set to 1K
	 * 0x00010068 - Set the RB size to 16K since the RX FIFO is 24K
	 */
	config_buf[1] =
		cpu_to_le32(trans_sdio->sf_mem_addresses->tg_buf_base_addr);
	config_buf[2] =
		cpu_to_le32(trans_sdio->sf_mem_addresses->tfdi_base_addr);
	config_buf[3] =
		cpu_to_le32(trans_sdio->sf_mem_addresses->tfd_base_addr);

	/* Configure H2D GP:
	 * TODO: Check if we need to configure the SDTM with
	 * IWL_SDIO_MSG_SDTM_ALL */

	/* Target write the data to the SDTM */
	ret = iwl_sdio_ta_write(trans, IWL_SDIO_CONFIG_BASE_ADDRESS,
				sizeof(config_buf), config_buf,
				IWL_SDIO_TA_AC_DIRECT);
	if (ret)
		IWL_ERR(trans,
			"Failed to configure SDIO SDTM error msk, reason %d\n",
			ret);
	else
		IWL_DEBUG_INFO(trans, "SDTM init completed successfully\n");

	return ret;
}

/*
 * Init ADMA descriptors memory (due to ADMA-bug) :
 * By writing a non-zero value to S/F memory ADMA descriptors memory
 * base (0x82A00), 88 bytes (use pattern 0x47554232414D4441).
 *
 *@func - The SDIO HW function bus driver.
 */
static int iwl_sdio_config_adma(struct iwl_trans *trans)
{
	int ret, i;
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	__le64 config_buf[IWL_SDIO_SF_MEM_ADMA_DSC_LENGTH / sizeof(u64)];

	for (i = 0; i < ARRAY_SIZE(config_buf); i++) {
		u64 val = IWL_SDIO_SF_MEM_ADMA_DESC_MEM_PAD_LSB |
			((u64) IWL_SDIO_SF_MEM_ADMA_DESC_MEM_PAD_MSB << 32);
		config_buf[i] = cpu_to_le64(val);
	}

	/* Target write the data to the SDTM */
	ret = iwl_sdio_ta_write(trans,
				trans_sdio->sf_mem_addresses->adma_dsc_mem_base,
				sizeof(config_buf), config_buf,
				IWL_SDIO_TA_AC_DIRECT);

	if (ret)
		IWL_ERR(trans, "Failed to configure ADMA, reason %d\n", ret);
	else
		IWL_DEBUG_INFO(trans,
			 "SDIO ADMA initialization completed successfully\n");
	return ret;
}

/*
 * Polls the requested bits with the requested mask waiting for the
 * value to be changed to the given value.
 *
 * returns 0 if the polling was successfull, else the error value or ETIMEDOUT.
 */
static int iwl_sdio_poll_bits(struct iwl_trans *trans,
				    unsigned int reg_addr, u8 value,
				    u8 bit_mask, u8 poll_loops)
{
	int ret;
	u8 ret_val;

	do {
		ret_val = iwl_sdio_read8(trans, reg_addr, &ret);
		if (ret)
			goto exit_error;

		/* If value with mask matches */
		if (ret_val == (value & bit_mask))
			return 0;

		usleep_range(IWL_SDIO_BUS_POLL_STALL_TIME_MIN,
			     IWL_SDIO_BUS_POLL_STALL_TIME_MAX);
	} while (poll_loops--);
	IWL_ERR(trans,
		"Byte read poll of addr 0x%x for value %d TIMEOUT\n",
		reg_addr, value);
	ret = -ETIMEDOUT;

exit_error:
	IWL_ERR(trans, "Failed to read poll bits\n");
	return ret;
}

/*
 * Wait until GP change message will be acked by the device.
 * After receiving the ACK, clear the bit.
 * Since the ACK is given in the interrupt reason, interrupts must be disabled
 * while waiting for this ack.
 *
 * returns 0 if the message was received by the device, error value else.
 */
static int iwl_sdio_wait_h2d_gp_cmd_ack(struct iwl_trans *trans)
{
	int ret;

	msleep(IWL_SDIO_H2D_ACK_WAIT_TIME);
	ret = iwl_sdio_poll_bits(trans, IWL_SDIO_INTR_CAUSE_REG,
				 IWL_SDIO_INTR_H2D_GPR_MSG_ACK,
				 IWL_SDIO_INTR_H2D_GPR_MSG_ACK,
				 IWL_SDIO_POLL_BIT_MAX_TRIES);
	if (ret) {
		IWL_ERR(trans, "Failed to poll requested bit\n");
		return ret;
	}

	/* Clear the interrupt ACK */
	ret = iwl_sdio_write8(trans, IWL_SDIO_INTR_H2D_GPR_MSG_ACK,
			      IWL_SDIO_INTR_CAUSE_REG);

	if (ret) {
		IWL_ERR(trans, "Failed to clear inter bit\n");
		return ret;
	}

	IWL_DEBUG_INFO(trans, "Got H2D GP command flow ACK\n");
	return ret;
}

/*
 * Configures the FH to work with the AL.
 *
 * Configures the FH TSSR TX for SDIO.
 * Using the HW CONFIG registers configures the HW to work with SDIO.
 */
static int iwl_sdio_config_fh(struct iwl_trans *trans)
{
	u32 write_data;
	int ret;

	/* Configure the FH TSSR TX msg properties */
	write_data = IWL_SDIO_FH_TSSR_TX_CONFIG;
	ret = iwl_sdio_ta_write(trans, FH_TSSR_TX_MSG_CONFIG_REG, sizeof(u32),
				&write_data, IWL_SDIO_TA_AC_DIRECT);
	if (ret)
		goto exit_error;

	/* Configures the HW for SDIO */
	write_data = IWL_SDIO_CSR_HW_COFIG;
	ret = iwl_sdio_ta_write(trans, CSR_HW_IF_CONFIG_REG, sizeof(u32),
				&write_data, IWL_SDIO_TA_AC_DIRECT);
	if (ret)
		goto exit_error;

	IWL_DEBUG_INFO(trans, "Successfully configured the FH for SDIO");
	return 0;

exit_error:
	IWL_ERR(trans, "Failed to configure the FH");
	return ret;
}

/*
 * Exit retention flow
 *
 * Run a series of operations which when done the core is no longer able to go
 * into retention and shut down the NIC.
 *
 * This should be called after HW reset, during init INIT and after
 * system resume.
 *
 * Disables interrupts.
 *
 * @trans - the generic transport layer.
 */
static inline int iwl_sdio_exit_retention_flow(struct iwl_trans *trans)
{
	/* Disable interrupts */
	if (iwl_sdio_disable_interrupts(trans))
		return -EIO;

	/*
	 * Disable retention:
	 * Clear interrupts, Disable retention,
	 * Wait for h2d GP cmd ack interrupt.
	 */
	if (iwl_sdio_clear_interrupts(trans) ||
	    iwl_sdio_disable_retention(trans) ||
	    iwl_sdio_wait_h2d_gp_cmd_ack(trans))
		return -EIO;

	return 0;
}

/*
 * SDIO transport start HW.
 * This will enable the SDIO HW function and configure the SDIO AL.
 *
 *@trans - the generic transport layer.
 */
static int iwl_trans_sdio_start_hw(struct iwl_trans *trans)
{
	int ret;
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);

	iwl_sdio_set_power(trans, true);
	sdio_claim_host(func);
	ret = iwl_sdio_reset_hw(trans);
	if (ret)
		goto release_host;

	/* Wake up HW clock */
	ret = iwl_sdio_func_clock_wakeup(trans);
	if (ret)
		goto release_hw;

	/* Set the block size in the function */
	func->max_blksize = IWL_SDIO_BLOCK_SIZE;

	/* Set block size */
	ret = sdio_set_block_size(func, IWL_SDIO_BLOCK_SIZE);
	if (ret) {
		IWL_ERR(trans, "Failed to set SDIO block size\n");
		goto release_hw;
	}

	/* Exit retention flow after init */
	ret = iwl_sdio_exit_retention_flow(trans);
	if (ret)
		goto release_hw;

	/* Enable interrupts */
	ret = iwl_sdio_enable_interrupts(trans);
	if (ret)
		goto release_hw;

	/* Configure the SDTM  and ADMA in the SDIO AL */
	mutex_lock(&trans_sdio->target_access_mtx);

	ret = iwl_sdio_config_sdtm(trans);
	if (ret)
		goto release_irq;
	ret = iwl_sdio_config_adma(trans);
	if (ret)
		goto release_irq;

	/* Configure the LMAC FH */
	ret = iwl_sdio_config_fh(trans);
	if (ret)
		goto release_irq;
	mutex_unlock(&trans_sdio->target_access_mtx);

release_host:
	sdio_release_host(func);
	return ret;

release_irq:
	mutex_unlock(&trans_sdio->target_access_mtx);

	sdio_release_irq(func);
	cancel_work_sync(&trans_sdio->d2h_work);

release_hw:
	iwl_sdio_release_hw(trans);
	return ret;
}

/*
 * Configures the SDIO transport layer.
 *
 * Configures internal data variables required for the SDIO transport layer.
 * Should be called before initial use of the transport layer by the driver.
 * NULL transport configurations is not a valid option.
 */
static void iwl_trans_sdio_configure(struct iwl_trans *trans,
				     const struct iwl_trans_config *trans_cfg)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_trans_slv *trans_slv = IWL_TRANS_GET_SLV_TRANS(trans);

	WARN_ON(trans_cfg == NULL);

	iwl_slv_set_reclaim_cmds(trans_slv, trans_cfg);
	iwl_slv_set_rx_page_order(trans_slv, trans_cfg);

	/* Configure command names */
	trans_sdio->command_names = trans_cfg->command_names;

	/*Configure RX page order and size */
	trans_sdio->rx_buf_size_8k = trans_cfg->rx_buf_size_8k;
	trans_sdio->bc_table_dword = trans_cfg->bc_table_dword;

	trans_slv->cmd_queue = trans_cfg->cmd_queue;
	trans_slv->cmd_fifo = trans_cfg->cmd_fifo;
	trans_slv->command_names = trans_cfg->command_names;

	trans_slv->config.max_queues_num = IWL_SDIO_CB_QUEUES_NUM;
	trans_slv->config.queue_size = IWL_SDIO_CB_QUEUE_SIZE;
	trans_slv->config.tb_size = IWL_SDIO_TB_SIZE;
	trans_slv->config.max_data_desc_count = IWL_SDIO_MAX_TBS_NUM + 5;
	trans_slv->config.hcmd_headroom = 0;
	trans_slv->config.policy_trigger = iwl_sdio_tx_policy_trigger;
	trans_slv->config.clean_dtu = iwl_sdio_tx_clean_dtu;
	trans_slv->config.free_dtu_mem = iwl_sdio_tx_free_dtu_mem;
	trans_slv->config.calc_desc_num = iwl_sdio_tx_calc_desc_num;

	IWL_DEBUG_INFO(trans, "Configured the SDIO transport layer");
}

static int iwl_sdio_load_fw_chunk(struct iwl_trans *trans,
				  u32 *temp_fw_buff_t, u8 section_num,
				  const struct fw_desc *section,
				  u32 sram_address, u32 data_len,
				  u8 *section_data)
{
	int ret = 0, desc_idx;
	u32 *data_ptr;
	u32 send_len, send_len_aligned, block_pad_len;
	struct iwl_sdio_adma_desc *adma_list;
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	struct iwl_sdio_tx_dtu_hdr *dtu_hdr;
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);
	struct iwl_sdio_fh_desc *fh_desc;

	dtu_hdr = (struct iwl_sdio_tx_dtu_hdr *)temp_fw_buff_t;

	dtu_hdr->hdr.op_code = IWL_SDIO_OP_CODE_TX_DATA | IWL_SDIO_EOT_BIT;
	dtu_hdr->hdr.seq_number = iwl_sdio_get_cmd_seq(trans_sdio, true);
	dtu_hdr->hdr.signature = cpu_to_le16(IWL_SDIO_CMD_HEADER_SIGNATURE);
	dtu_hdr->dma_desc = cpu_to_le32(FDL_DMA_DESC_ADDRESS |
			    ((FDL_NUM_OF_DMA_DESC *
			    sizeof(struct iwl_sdio_adma_desc)) << 20));

	adma_list = dtu_hdr->adma_list;

	/* ADMA dlist */
	desc_idx = 0;
	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(data_len);
	adma_list[desc_idx].addr = cpu_to_le32(FDL_DATA_ADDRESS);
	desc_idx++;

	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(4);
	adma_list[desc_idx].addr = cpu_to_le32(FDL_FH_CONTROL_ADD1);
	desc_idx++;

	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(4);
	adma_list[desc_idx].addr = cpu_to_le32(FDL_FH_CONTROL_ADD2);
	desc_idx++;

	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(4);
	adma_list[desc_idx].addr = cpu_to_le32(FDL_FH_CONTROL_ADD3);
	desc_idx++;

	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(4);
	adma_list[desc_idx].addr = cpu_to_le32(FDL_FH_CONTROL_ADD4);
	desc_idx++;

	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(4);
	adma_list[desc_idx].addr = cpu_to_le32(FDL_FH_CONTROL_ADD5);
	desc_idx++;

	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(4);
	adma_list[desc_idx].addr = cpu_to_le32(FDL_FH_CONTROL_ADD6);
	desc_idx++;

	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_VALID | IWL_SDIO_ADMA_ATTR_ACT2;
	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(4);
	adma_list[desc_idx].addr = cpu_to_le32(FDL_FH_CONTROL_ADD1);
	desc_idx++;

	/* padding descriptor */
	send_len = (data_len + FDL_NUM_OF_FH_CONTROL_DESC +
		   sizeof(struct iwl_sdio_tx_dtu_hdr) +
		   FDL_NUM_OF_DMA_DESC * sizeof(struct iwl_sdio_adma_desc));

	IWL_DEBUG_FW(trans, "send_len %d\n", send_len);

	send_len_aligned = ALIGN(send_len, IWL_SDIO_BLOCK_SIZE);
	IWL_DEBUG_FW(trans, "send_len_aligned %d\n", send_len_aligned);

	block_pad_len = send_len_aligned - send_len;
	IWL_DEBUG_FW(trans, "block_pad_len %d\n", block_pad_len);

	adma_list[desc_idx].attr =
		IWL_SDIO_ADMA_ATTR_END | IWL_SDIO_ADMA_ATTR_VALID |
		IWL_SDIO_ADMA_ATTR_ACT2;

	adma_list[desc_idx].reserved = 0;
	adma_list[desc_idx].length = cpu_to_le16(block_pad_len);
	adma_list[desc_idx].addr = cpu_to_le32(FDL_FH_CONTROL_CMD2);
	desc_idx++;

	/* DATA */
	data_ptr = (u32 *)&adma_list[desc_idx].attr;
	memcpy((u8 *)data_ptr, section_data, data_len);

	/* fh control */
	fh_desc = (struct iwl_sdio_fh_desc *)(data_ptr + data_len/4);
	fh_desc->fh_first_word = cpu_to_le32(0);
	fh_desc->ptr_in_sf = cpu_to_le32(FDL_DATA_ADDRESS);
	fh_desc->image_size_in_byte = cpu_to_le32(data_len);
	fh_desc->ptr_in_sram = cpu_to_le32(sram_address);
	fh_desc->fh_cmd = cpu_to_le32(FDL_FH_CONTROL_CMD1);
	fh_desc->const_0 = cpu_to_le32(0);
	fh_desc->f_kick = cpu_to_le32(FDL_FH_KICK);

	/* pad */
	data_ptr = fh_desc->pad;
	memset(data_ptr, FDL_PAD_WORD, block_pad_len);

	IWL_DEBUG_FW(trans,
		     "send %d bytes to sdio\n", send_len + block_pad_len);

	ret = sdio_writesb(func, IWL_SDIO_DATA_ADDR,
			   temp_fw_buff_t,
			   send_len + block_pad_len);

	if (ret)
		IWL_ERR(trans, "Cannot send buffer\n");

	IWL_DEBUG_FW(trans,
		     "data buffer in SRAM; data len = %d, address = %x\n",
		     data_len, sram_address);

	return ret;
}

static int iwl_sdio_load_fw_section(struct iwl_trans *trans, u8 section_num,
				    const struct fw_desc *section)
{
	u32 data, offset, copy_size;
	int ret = 0;
	u32 *temp_fw_buff_t;

	IWL_DEBUG_FW(trans, "[%d] uCode section being loaded...\n",
		     section_num);

	temp_fw_buff_t = kzalloc(IWL_FDL_SDIO_MAX_BUFFER_SIZE, GFP_KERNEL);
	if (!temp_fw_buff_t)
		return -ENOMEM;

	/* Send FW in BLOCK SIZE chunks */
	for (offset = 0; offset < section->len;
	    offset += IWL_FDL_SDIO_MAX_PAYLOAD_SIZE) {
		copy_size = min_t(u32, IWL_FDL_SDIO_MAX_PAYLOAD_SIZE,
				  section->len - offset);

		ret = iwl_sdio_load_fw_chunk(trans, temp_fw_buff_t,
					     section_num, section,
					     section->offset + offset,
					     copy_size,
					     (u8 *)section->data + offset);

		if (ret) {
			IWL_ERR(trans, "Failed to download fw section %d ",
				section_num);
			break;
		}

		/* Let the internal memory transaction complete */
		mdelay(1);
	}

	/* W/A for WP B0 */
	if (section_num == IWL_UCODE_SECTION_INST) {
		data = CSR_INI_SET_MASK;
		ret = iwl_sdio_ta_write(trans, CSR_INT_MASK,
					sizeof(u32), &data,
					IWL_SDIO_TA_AC_DIRECT);
		if (ret)
			IWL_ERR(trans, "Failed to set interrupt mask.");
	}

	kfree(temp_fw_buff_t);

	return ret;
}

/*
 * Check the FW section after it was downloaded to the NICs SRAM.
 */
static int iwl_sdio_check_fw_section(struct iwl_trans *trans, u8 section_num,
				     const struct fw_desc *section)
{
	u32 offset, copy_size;
	void *temp_fw_buff;
	int ret = 0;

	/* Allocate the temp buffer for SRAM RX compare */
	temp_fw_buff = kzalloc(IWL_SDIO_MAX_PAYLOAD_SIZE, GFP_KERNEL);
	if (WARN_ON(!temp_fw_buff))
		return -ENOMEM;

	IWL_DEBUG_FW(trans, "[%d] uCode section being CHECKED...\n",
		     section_num);

	/* CHECK FW in BLOCK SIZE chunks using target access */
	for (offset = 0; offset < section->len;
	     offset += IWL_SDIO_MAX_PAYLOAD_SIZE) {
		copy_size = min_t(u32,
				  IWL_SDIO_MAX_PAYLOAD_SIZE,
				  section->len - offset);
		/* Read the SRAM from the relevant offset */
		ret = iwl_sdio_ta_read(trans, section->offset + offset,
				       copy_size, temp_fw_buff,
				       IWL_SDIO_TA_AC_INDIRECT);
		if (ret) {
			IWL_ERR(trans, "Failed to READ fw section %d\n",
				section_num);
			break;
		}
		/* Compare to stored image */
		if (WARN_ONCE(memcmp(temp_fw_buff, (u8 *)section->data + offset,
			       copy_size),
			       "FW sections Don't match on address 0x%x\n",
			       section->offset + offset)) {
			ret = iwl_sdio_ta_write(trans, section->offset + offset,
					copy_size, (u8 *)section->data + offset,
					 IWL_SDIO_TA_AC_INDIRECT);
			if (WARN_ON(ret))
				return ret;

			/* Zero the offset to perfrom read test again */
			offset -= IWL_SDIO_MAX_PAYLOAD_SIZE;
		} else
			IWL_DEBUG_INFO(trans, "FW sec in offset 0x%x MATCH\n",
				section->offset + offset);
	}

	kfree(temp_fw_buff);
	return ret;
}

static int iwl_sdio_load_cpu_secured_sections(struct iwl_trans *trans,
					      const struct fw_img *image,
					      int cpu)
{
	int shift_param;
	u32 first_idx, last_idx, load_status;
	int i, ret = 0;

	if (cpu == 1) {
		shift_param = 0;
		first_idx = 0;
		last_idx = 2;
	} else {
		shift_param = 16;
		first_idx = 3;
		last_idx = 5;
	}

	for (i = first_idx; i <= last_idx; i++) {
		if (!image->sec[i].data)
			break;
		if (i == first_idx + 1)
			/* set CPU to started */
			iwl_trans_set_bits_mask(trans,
						CSR_UCODE_LOAD_STATUS_ADDR,
						LMPM_CPU_HDRS_LOADING_COMPLETED
						<< shift_param, 1);

		ret = iwl_sdio_load_fw_section(trans, i, &image->sec[i]);
		if (ret)
			return ret;
		ret = iwl_sdio_check_fw_section(trans, i, &image->sec[i]);
		if (ret)
			return ret;
	}

	/* image loading complete */
	load_status = iwl_sdio_read_prph_no_claim(trans,
						  CSR_UCODE_LOAD_STATUS_ADDR);
	iwl_sdio_write_prph_no_claim(trans,
				     CSR_UCODE_LOAD_STATUS_ADDR,
				     load_status |
				     (LMPM_CPU_UCODE_LOADING_COMPLETED
				     << shift_param));
	return 0;
}

static int iwl_sdio_load_cpu_sections(struct iwl_trans *trans,
				      const struct fw_img *image,
				      int cpu)
{
	int shift_param;
	u32 first_idx, last_idx, load_status;
	int i, ret = 0;

	if (cpu == 1) {
		shift_param = 0;
		first_idx = 0;
		last_idx = 1;
	} else {
		shift_param = 16;
		first_idx = 2;
		last_idx = 3;
	}

	for (i = first_idx; i <= last_idx; i++) {
		if (!image->sec[i].data)
			break;
		ret = iwl_sdio_load_fw_section(trans, i, &image->sec[i]);
		if (ret)
			return ret;
		ret = iwl_sdio_check_fw_section(trans, i, &image->sec[i]);
		if (ret)
			return ret;
	}

	if (trans->cfg->device_family == IWL_DEVICE_FAMILY_8000) {
		load_status = iwl_sdio_read_prph_no_claim(trans, CSR_UCODE_LOAD_STATUS_ADDR);
		iwl_sdio_write_prph_no_claim(trans,
					     CSR_UCODE_LOAD_STATUS_ADDR,
					     load_status |
					     (LMPM_CPU_UCODE_LOADING_COMPLETED |
					      LMPM_CPU_HDRS_LOADING_COMPLETED |
					      LMPM_CPU_UCODE_LOADING_STARTED)
					     << shift_param);
	}
	return 0;
}

/*
 * Load the given FW image to the NIC.
 */
static int iwl_sdio_load_given_ucode(struct iwl_trans *trans,
				     const struct fw_img *image)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	int ret = 0;
	u32 write_data = 0;

	if (!image)
		return -EINVAL;

	IWL_DEBUG_FW(trans,
		     "working with %s image\n",
		     image->is_secure ? "Secured" : "Non Secured");
	IWL_DEBUG_FW(trans,
		     "working with %s CPU\n",
		     image->is_dual_cpus ? "Dual" : "Single");

	mutex_lock(&trans_sdio->target_access_mtx);

	/* configure the ucode to be ready to get the secured image */
	if (image->is_secure) {
		/* set secure boot inspector addresses */
		iwl_sdio_write32(trans, LMPM_SECURE_INSPECTOR_CODE_ADDR,
				 LMPM_SECURE_INSPECTOR_CODE_MEM_SPACE);

		iwl_sdio_write32(trans, LMPM_SECURE_INSPECTOR_DATA_ADDR,
				 LMPM_SECURE_INSPECTOR_DATA_MEM_SPACE);

		/* set CPU1 header address */
		iwl_sdio_write32(trans, LMPM_SECURE_UCODE_LOAD_CPU1_HDR_ADDR,
				 LMPM_SECURE_CPU1_HDR_MEM_SPACE);

		/* load to FW the binary Secured sections of CPU1 */
		ret = iwl_sdio_load_cpu_secured_sections(trans, image, 1);
		if (ret)
			goto exit_err;
	} else {
		/* load to FW the binary NoN secured sections of CPU1 */
		ret = iwl_sdio_load_cpu_sections(trans, image, 1);
		if (ret)
			goto exit_err;
	}

	if (image->is_dual_cpus) {
		/* set CPU2 header address */
		iwl_sdio_write32(trans, LMPM_SECURE_UCODE_LOAD_CPU2_HDR_ADDR,
				 LMPM_SECURE_CPU2_HDR_MEM_SPACE);

		/* load to FW the binary sections of CPU2 */
		if (image->is_secure)
			ret = iwl_sdio_load_cpu_secured_sections(trans,
								 image,
								 2);
		else
			ret = iwl_sdio_load_cpu_sections(trans, image, 2);
		if (ret)
			goto exit_err;
	}

#ifdef CPTCFG_IWLWIFI_DEVICE_TESTMODE
	/*
	 * The unlocking is required otherwise the writing to periphery
	 * registers will get stuck due to being unable to grab nic access
	 */
	mutex_unlock(&trans_sdio->target_access_mtx);
	iwl_dnt_configure(trans, image);
	mutex_lock(&trans_sdio->target_access_mtx);
#endif

	/* Remove CSR reset to allow NIC to operate */
	if (trans->cfg->device_family == IWL_DEVICE_FAMILY_8000) {
		iwl_trans_sdio_write_prph(trans, RELEASE_CPU_RESET,
					  RELEASE_CPU_RESET_BIT);
	} else {
		ret = iwl_sdio_ta_write(trans, CSR_RESET, sizeof(u32),
					&write_data, IWL_SDIO_TA_AC_DIRECT);
		if (ret)
			goto exit_err;
	}

	if (image->is_secure) {
		/* wait for image verification to complete  */
			/* polling on CSR_CPU_STATUS_LOADING_COMPLETED  */
		ret = iwl_poll_prph_bit(trans,
					LMPM_SECURE_BOOT_CPU1_STATUS_ADDR,
					LMPM_SECURE_BOOT_STATUS_SUCCESS,
					LMPM_SECURE_BOOT_STATUS_SUCCESS,
					LMPM_SECURE_TIME_OUT);

		if (ret < 0) {
			IWL_ERR(trans, "Time out on secure boot process\n");
			goto exit_err;
		}
	}

exit_err:
	mutex_unlock(&trans_sdio->target_access_mtx);
	return ret;
}

/*
 * SDIO start fw.
 * Performs fw download.
 */
static int iwl_trans_sdio_start_fw(struct iwl_trans *trans,
				   const struct fw_img *fw,
				   bool run_in_rfkill)
{
	int ret;

	ret = iwl_slv_init(trans);
	if (ret)
		goto exit_err;

	ret = iwl_sdio_tx_init(trans);
	if (ret)
		goto free_slv;

	/* Claim Host */
	sdio_claim_host(IWL_TRANS_SDIO_GET_FUNC(trans));

	/* Prepare card HW */

	/* Clear driver status */

	/* Enable RFKill interrupts */

	/* Test platform HW RF kill state */

	/* Clear interrupts */
	ret = iwl_sdio_clear_interrupts(trans);
	if (ret)
		goto free_tx;

	/* Nic init */

	/* Make sure rfkill handshake bits are cleared */

	/* Clear and enable interrutps */

	/* really make sure rfkill handshake bits are cleared */

	/* Load the given image to the HW */
	ret = iwl_sdio_load_given_ucode(trans, fw);
	if (ret) {
		IWL_ERR(trans, "Failed to load given FW Image");
		goto free_tx;
	}

	/* Release the host without powering down the NIC */
	sdio_release_host(IWL_TRANS_SDIO_GET_FUNC(trans));

	set_bit(STATUS_DEVICE_ENABLED, &trans->status);
	return 0;

free_tx:
	iwl_sdio_tx_free(trans);

free_slv:
	iwl_slv_free(trans);

exit_err:
	return ret;
}

/*
 * Generic method to handle grab/release nic access.
 * According to the grab_access bool flag grabs/releases the nic access.
 */
static bool iwl_sdio_change_nic_access(struct iwl_trans *trans,
				       bool silent, bool grab_access)
{
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	char *action = ((grab_access) ? "grabbed" : "released");
	int ret;

	if (grab_access)
		mutex_lock(&trans_sdio->target_access_mtx);

	sdio_claim_host(func);

	/* Wake up/ sleep HW clock */
	if (grab_access)
		ret = iwl_sdio_func_clock_wakeup(trans);
	else
		ret = iwl_sdio_func_clock_sleep(trans);
	if (ret)
		goto exit_error;

	sdio_release_host(func);

	if (!grab_access)
		mutex_unlock(&trans_sdio->target_access_mtx);

	IWL_DEBUG_INFO(trans, "Successfully %s nic access", action);
	return true;

exit_error:
	/* Cause an NMI due to failure to change NIC power state */
	iwl_write32(trans, CSR_RESET, CSR_RESET_REG_FLAG_FORCE_NMI);

	/* Read state and print it if requested */
	if (!silent) {
		u32 val = iwl_read32(trans, CSR_GP_CNTRL);
		WARN_ONCE(1,
			  "Timeout waiting for hardware access"\
			  "(CSR_GP_CNTRL 0x%08x)\n",
			  val);
	}

	sdio_release_host(func);

	mutex_unlock(&trans_sdio->target_access_mtx);

	IWL_ERR(trans, "Failed to %s nic access ret = %d", action, ret);
	return false;
}

/*
 * Grab nic access.
 * Make sure the device is powered up and ready to handle
 * undirect memory read/writes.
 */
static bool iwl_trans_sdio_grab_nic_access(struct iwl_trans *trans, bool silent,
					   unsigned long *flags)
{
	return iwl_sdio_change_nic_access(trans, silent, true);
}

/*
 * Release nic access.
 * Allow the device to go into power save in case it has nothing to perform.
 */
static void
iwl_trans_sdio_release_nic_access(struct iwl_trans *trans, unsigned long *flags)
{
	iwl_sdio_change_nic_access(trans, true, false);
}

static void iwl_trans_sdio_fw_alive(struct iwl_trans *trans, u32 scd_addr)
{
	IWL_DEBUG_FW(trans, "%s", __func__);
	iwl_sdio_tx_start(trans, scd_addr);
}

static void iwl_trans_sdio_stop_device(struct iwl_trans *trans)
{
	struct sdio_func *func = IWL_TRANS_SDIO_GET_FUNC(trans);

	IWL_DEBUG_INFO(trans, "%s", __func__);

	if (test_and_clear_bit(STATUS_DEVICE_ENABLED, &trans->status)) {
		iwl_slv_tx_stop(trans);
		iwl_sdio_tx_stop(trans);

		iwl_slv_free(trans);
		iwl_sdio_tx_free(trans);
	}

	/* Stop HW and power down */
	sdio_claim_host(func);
	iwl_sdio_release_hw(trans);
}

#ifdef CPTCFG_IWLWIFI_DEBUGFS
static int iwl_trans_sdio_dbgfs_register(struct iwl_trans *trans,
					 struct dentry *dir)
{
	return 0;
}
#else
static int iwl_trans_sdio_dbgfs_register(struct iwl_trans *trans,
					 struct dentry *dir)
{
	return 0;
}
#endif /*CPTCFG_IWLWIFI_DEBUGFS */

/*
 * The SDIO transport operations.
 */
static const struct iwl_trans_ops trans_ops_sdio = {
	.start_hw = iwl_trans_sdio_start_hw,
	.start_fw = iwl_trans_sdio_start_fw,
	.stop_device = iwl_trans_sdio_stop_device,
	.configure = iwl_trans_sdio_configure,
	.fw_alive = iwl_trans_sdio_fw_alive,

	/* Target Access */
	.write8 = iwl_trans_sdio_write8,
	.write32 = iwl_trans_sdio_write32,
	.read32 = iwl_trans_sdio_read32,
	.write_prph = iwl_trans_sdio_write_prph,
	.read_prph = iwl_trans_sdio_read_prph,
	.read_mem = iwl_trans_sdio_read_mem,
	.write_mem = iwl_trans_sdio_write_mem,
	.set_bits_mask = iwl_trans_sdio_set_bits_mask,

	/* TX */
	.reclaim = iwl_trans_slv_tx_data_reclaim,
	.send_cmd = iwl_trans_slv_send_cmd,
	.tx = iwl_trans_slv_tx_data_send,
	.txq_enable = iwl_trans_sdio_txq_enable,
	.txq_disable = iwl_trans_sdio_txq_disable,

	/* NIC Access */
	.grab_nic_access = iwl_trans_sdio_grab_nic_access,
	.release_nic_access = iwl_trans_sdio_release_nic_access,

	.dbgfs_register = iwl_trans_sdio_dbgfs_register,

	.ref = iwl_trans_slv_ref,
	.unref = iwl_trans_slv_unref,
};

/*
 * Allocate the transport of the SDIO bus.
 * Allocate fields required for the general transport layer.
 * Perform basic configurations of the transport.
 *
 * Returns the newly allocated generic transport if successfull,
 * or NULL otherwise.
 */
struct iwl_trans *iwl_trans_sdio_alloc(struct sdio_func *func,
				       const struct sdio_device_id *id,
				       const struct iwl_cfg *cfg)
{
	int ret;
	struct iwl_trans *trans;
	struct iwl_trans_sdio *trans_sdio;

	/* Alloc general + SDIO specific transport */
	trans = kzalloc(sizeof(struct iwl_trans) +
			sizeof(struct iwl_trans_slv) +
			sizeof(struct iwl_trans_sdio),
			GFP_KERNEL);
	if (!trans) {
		ret = -ENOMEM;
		goto exit_err;
	}

	trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);
	trans_sdio->func = func;
	trans_sdio->trans = trans;

	trans->ops = &trans_ops_sdio;
	trans->cfg = cfg;
	trans->dev = &func->dev;

	trans_lockdep_init(trans);

	/*
	 * SDIO can't access registers from atomic context. But our current MVM
	 * op_mode handles LED from atomic context.
	 * OTOH, SDIO devices don't really care about LEDs. So just ignore LED
	 * here for now.
	 */
	iwlwifi_mod_params.led_mode = IWL_LED_DISABLE;

	/* Init internal data variables */
	mutex_init(&trans_sdio->target_access_mtx);
	init_waitqueue_head(&trans_sdio->wait_target_access);

	/* Init RX data structs */
	INIT_LIST_HEAD(&trans_sdio->rx_mem_buff_list);
	mutex_init(&trans_sdio->rx_buff_mtx);
	INIT_WORK(&trans_sdio->d2h_work, iwl_sdio_d2h_work);

	snprintf(trans_sdio->rx_mem_desc_pool_name,
		 sizeof(trans_sdio->rx_mem_desc_pool_name),
		 "iwl_sdio_rx_mem_desc:%s\n", dev_name(trans->dev));
	trans_sdio->rx_mem_desc_pool =
		kmem_cache_create(trans_sdio->rx_mem_desc_pool_name,
				  sizeof(struct iwl_sdio_rx_mem_desc),
				  sizeof(void *),
				  SLAB_HWCACHE_ALIGN,
				  NULL);
	if (!trans_sdio->rx_mem_desc_pool) {
		ret = -ENOMEM;
		goto free_trans;
	}

	/* Read revision register */
	ret = iwl_sdio_claim_hw(trans);
	if (ret)
		goto free_rx_desc;

	trans->hw_rev = iwl_sdio_read8(trans, CSR_HW_REV, &ret);
	if (ret)
		goto free_rx_desc;

	ret = iwl_sdio_release_hw(trans);
	if (ret)
		goto free_rx_desc;

	/* Device identification */
	trans->hw_id = func->device;
	snprintf(trans->hw_id_str,
		 sizeof(trans->hw_id_str),
		 "SDIO ID: 0x%04X\n", func->device);

	/* Command pool creation */
	snprintf(trans->dev_cmd_pool_name,
		 sizeof(trans->dev_cmd_pool_name),
		 "iwl_cmd_pool:%s\n", dev_name(trans->dev));
	trans->dev_cmd_pool =
		kmem_cache_create(trans->dev_cmd_pool_name,
				  sizeof(struct iwl_device_cmd)
				  + trans->dev_cmd_headroom,
				  sizeof(void *),
				  SLAB_HWCACHE_ALIGN,
				  NULL);
	if (!trans->dev_cmd_pool) {
		ret = -ENOMEM;
		goto free_rx_desc;
	}

	IWL_DEBUG_INFO(trans,
		 "Allocated SDIO trans: Device %s\n"
		 "iwlwifi-SDIO: HW_REV 0x%x HW ID 0x%x\n",
		 trans->hw_id_str, trans->hw_rev, trans->hw_id);
	return trans;

free_rx_desc:
	kmem_cache_destroy(trans_sdio->rx_mem_desc_pool);
free_trans:
	kfree(trans);
exit_err:
	return ERR_PTR(ret);
}

/*
 * Free the Generic transport layer and the sdio transport layer as part of it.
 * Frees internal generic transport layer fields if were allocated.
 *
 *@trans - The generic transport layer.
 */
void iwl_trans_sdio_free(struct iwl_trans *trans)
{
	struct iwl_trans_sdio *trans_sdio = IWL_TRANS_GET_SDIO_TRANS(trans);

	/* Free command pool */
	kmem_cache_destroy(trans->dev_cmd_pool);

	/* Free all of the SDIO RX  memory */
	iwl_sdio_free_rx_mem(trans);

	/* Free rx buffers memory descriptors pools*/
	kmem_cache_destroy(trans_sdio->rx_mem_desc_pool);

	/* free generic + specific transport */
	kfree(trans);
}

/*
 * INTERNAL API
 *
 * Writes a BYTE to a given offset using the SDIO transport.
 * Uses trace points for logging.
 * Assumes that the host is claimed.
 *
 * Returns 0 on success, error value otherwise.
 */
int iwl_sdio_write8(struct iwl_trans *trans, u8 val, u32 ofs)
{
	int ret;

	trace_iwlwifi_dev_iowrite8(trans->dev, ofs, val);
	sdio_writeb(IWL_TRANS_SDIO_GET_FUNC(trans), val, ofs, &ret);
	IWL_DEBUG_INFO(trans, "%s: return value: %d, addr 0x%x, val 0x%x",
		 __func__, ret, ofs, val);
	return ret;
}

/*
 * INTERNAL API
 *
 * Writes a DWORD to a given offset using the SDIO transport.
 * Uses trace points for logging.
 * Assumes that the host is claimed.
 *
 * Returns 0 on success, error value otherwise.
 */
int iwl_sdio_write32(struct iwl_trans *trans, u32 val, u32 ofs)
{
	int ret;
	trace_iwlwifi_dev_iowrite32(trans->dev, ofs, val);
	sdio_writel(IWL_TRANS_SDIO_GET_FUNC(trans), val, ofs, &ret);
	IWL_DEBUG_INFO(trans, "%s: return value: %d, addr 0x%x, val 0x%x",
		 __func__, ret, ofs, val);
	return ret;
}

/*
 * INTERNAL API
 *
 * Reads a BYTE from a given offset using the SDIO transport.
 * Uses trace points for logging.
 * Assumes that the host is claimed.
 *
 * Returns the value read and the read operation result in the given ret value
 * 0 on success, error value otherwise.
 */
u8 iwl_sdio_read8(struct iwl_trans *trans, u32 ofs, int *ret)
{
	u8 ret_val;
	ret_val = sdio_readb(IWL_TRANS_SDIO_GET_FUNC(trans), ofs, ret);
	trace_iwlwifi_dev_ioread32(trans->dev, ofs, (u32)ret_val);
	IWL_DEBUG_INFO(trans, "%s: return value: %d, addr 0x%x, val 0x%x",
		       __func__, *ret, ofs, ret_val);
	return ret_val;
}

/*
 * INTERNAL API
 *
 * Reads a DWORD from a given offset using the SDIO transport.
 * Uses trace points for logging.
 * Assumes that the host is claimed.
 *
 * Returns the value read and the read operation result in the given ret value
 * 0 on success, error value otherwise.
 */
u32 iwl_sdio_read32(struct iwl_trans *trans, u32 ofs, int *ret)
{
	u32 ret_val;
	ret_val = sdio_readl(IWL_TRANS_SDIO_GET_FUNC(trans), ofs, ret);
	trace_iwlwifi_dev_ioread32(trans->dev, ofs, ret_val);
	IWL_DEBUG_INFO(trans, "%s: return value: %d, addr 0x%x, val 0x%x",
		       __func__, *ret, ofs, ret_val);
	return ret_val;
}
