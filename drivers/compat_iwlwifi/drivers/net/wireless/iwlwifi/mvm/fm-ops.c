/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2013 - 2014 Intel Corporation. All rights reserved.
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
 * Copyright(c) 2013 - 2014 Intel Corporation. All rights reserved.
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
#include <linux/fm/iui_fm.h>
#include "mvm.h"

/*
 * Used in the FM callback and is declared here since according
 * to the FM API, the callback does not receive an mvm pointer
 */
static struct iwl_mvm *g_mvm;

struct chan_ifaces {
	struct iui_fm_wlan_channel_tx_power *chan_txpwr;
	int num_of_vif; /* for statistics */
};

struct chan_list {
	struct iui_fm_wlan_info *winfo;
	struct ieee80211_chanctx_conf *ctx_ignore;
	enum iwl_fm_chan_change_action action;
};

/*
 * Search for an interface with a given frequency
 */
static void iwl_mvm_fm_iface_iterator(void *_data, u8 *mac,
				      struct ieee80211_vif *vif)
{
	struct chan_ifaces *data = _data;
	struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);
	struct ieee80211_chanctx_conf *chanctx_conf;

	/* P2P device is never assigned a channel */
	if (vif->type == NL80211_IFTYPE_P2P_DEVICE)
		return;

	rcu_read_lock();

	chanctx_conf = rcu_dereference(vif->chanctx_conf);
	/* make sure the channel context is assigned */
	if (!chanctx_conf) {
		rcu_read_unlock();
		return;
	}

	if (chanctx_conf->def.chan->center_freq ==
	    KHZ_TO_MHZ(data->chan_txpwr->frequency)) {
		mvmvif->phy_ctxt->fm_tx_power_limit =
			data->chan_txpwr->max_tx_pwr;
		data->num_of_vif++;
		rcu_read_unlock();
		/* FM requests to remove Tx power limitation */
		if (data->chan_txpwr->max_tx_pwr == IUI_FM_WLAN_NO_TX_PWR_LIMIT)
			data->chan_txpwr->max_tx_pwr =
				IWL_DEFAULT_MAX_TX_POWER;

		iwl_mvm_fm_set_tx_power(g_mvm, vif,
					data->chan_txpwr->max_tx_pwr);
		return;
	}

	rcu_read_unlock();
}

static enum iui_fm_wlan_bandwidth
iwl_mvm_fm_get_bandwidth(enum nl80211_chan_width bandwidth)
{
	switch (bandwidth) {
	case NL80211_CHAN_WIDTH_20_NOHT:
		return IUI_FM_WLAN_BW_20MHZ;
	case NL80211_CHAN_WIDTH_20:
		return IUI_FM_WLAN_BW_20MHZ;
	case NL80211_CHAN_WIDTH_40:
		return  IUI_FM_WLAN_BW_40MHZ;
	case NL80211_CHAN_WIDTH_80:
		return IUI_FM_WLAN_BW_80MHZ;
	default:
		return IUI_FM_WLAN_BW_INVALID;
	}
}

/*
 * Search for all channels used by interfaces
 */
static void iwl_mvm_fm_chan_iterator(struct ieee80211_hw *hw,
				     struct ieee80211_chanctx_conf *ctx,
				     void *_data)
{
	int i;
	enum nl80211_chan_width band;
	u32 freq;
	struct chan_list *data = _data;
	struct iui_fm_wlan_info *winfo = data->winfo;
	/* if a chanctx was freed, ignore the channel */
	struct ieee80211_chanctx_conf *ctx_ignore = data->ctx_ignore;
	enum iwl_fm_chan_change_action action = data->action;

	if (winfo->num_channels == IUI_FM_WLAN_MAX_CHANNELS)
		return;

	freq = MHZ_TO_KHZ(ctx->def.chan->center_freq);
	band = ctx->def.width;

	for (i = 0; i < winfo->num_channels; i++)
		if (winfo->channel_info[i].frequency == freq) {
			/*
			 * channel exists - but bandwidth maybe invalid since
			 * we are removing a ctx that operates on this channel
			 */
			winfo->channel_info[winfo->num_channels].bandwidth =
				iwl_mvm_fm_get_bandwidth(band);
			return; /* channel already exists in list */
		}

	winfo->channel_info[winfo->num_channels].frequency = freq;
	winfo->channel_info[winfo->num_channels].bandwidth =
		iwl_mvm_fm_get_bandwidth(band);

	/*
	 * if notifying the FM about freeing a channel ctx we need to report
	 * this channel's bandwidth invalid.
	 */
	if (action == IWL_FM_REMOVE_CHANCTX && ctx == ctx_ignore)
		winfo->channel_info[winfo->num_channels].bandwidth =
			IUI_FM_WLAN_BW_INVALID;

	winfo->num_channels++;
}

/*
 * Handling Frequency Managers Tx power mitigation Request.
 * Reducing the Tx power of all interfaces that use a specific channel.
 */
static enum iui_fm_mitigation_status
iwl_mvm_fm_mitig_txpwr(struct iui_fm_wlan_channel_tx_power *chan_txpwr_list,
		       u32 num_channels)
{
	int i;
	struct chan_ifaces chan_ifaces;

	if (IUI_FM_WLAN_MAX_CHANNELS < num_channels)
		return IUI_FM_MITIGATION_ERROR_INVALID_PARAM;

	for (i = 0; i < num_channels; i++) {
		chan_ifaces.chan_txpwr = &chan_txpwr_list[i];
		chan_ifaces.num_of_vif = 0;
		/* find all interfaces that use this channel */
		ieee80211_iterate_active_interfaces(g_mvm->hw,
						    IEEE80211_IFACE_ITER_NORMAL,
						    iwl_mvm_fm_iface_iterator,
						    &chan_ifaces);
		IWL_DEBUG_EXTERNAL(g_mvm,
				   "FM: Changed Tx power to %d for %d"
				   " vifs on channel %d\n",
				   chan_ifaces.chan_txpwr->max_tx_pwr,
				   chan_ifaces.num_of_vif,
				   chan_ifaces.chan_txpwr->frequency);
	}

	return IUI_FM_MITIGATION_COMPLETE_OK;
}

/*
 * Handling Frequency Managers ADC/DAC mitigation Request
 * TODO: implement
 */
static enum iui_fm_mitigation_status
iwl_mvm_fm_mitig_adc_dac_freq(u32 adc_dac_freq)
{
	if (adc_dac_freq != 0)
		return IUI_FM_MITIGATION_ERROR_INVALID_PARAM;

	IWL_DEBUG_EXTERNAL(g_mvm, "FM: adc - dac mitigation\n");

	return IUI_FM_MITIGATION_COMPLETE_OK;
}

/*
 * Handling Frequency Managers Rx Gain mitigation Request
 * TODO: implement
 */
static enum iui_fm_mitigation_status
iwl_mvm_fm_mitig_rxgain_behavior(enum iui_fm_wlan_rx_gain_behavior rx_gain)
{
	if (rx_gain != IUI_FM_WLAN_RX_GAIN_NORMAL)
		return IUI_FM_MITIGATION_ERROR_INVALID_PARAM;

	IWL_DEBUG_EXTERNAL(g_mvm,
			   "FM: rxgain behaviour mitigation - not implemented\n");
	return IUI_FM_MITIGATION_COMPLETE_OK;
}

/*
 * Enables/Disable 2G coex mode -  aggregation limiting.
 */
static enum iui_fm_mitigation_status
iwl_mvm_fm_2g_coex(struct iui_fm_wlan_channel_tx_power *chan_txpwr_list,
		   u32 num_channels)
{
	int i;
	struct iui_fm_wlan_channel_tx_power *chan;
	enum iui_fm_mitigation_status ret = IUI_FM_MITIGATION_COMPLETE_OK;

	if (IUI_FM_WLAN_MAX_CHANNELS < num_channels)
		return IUI_FM_MITIGATION_ERROR_INVALID_PARAM;

	for (i = 0; i < num_channels; i++) {
		chan = &chan_txpwr_list[i];
		if (chan->frequency == FM_2G_COEX_ENABLE_DISABLE) {
			mutex_lock(&g_mvm->mutex);
			if (chan->max_tx_pwr == FM_2G_COEX_ENABLE) {
				g_mvm->coex_2g_enabled = true;
			} else if (chan->max_tx_pwr == FM_2G_COEX_DISABLE) {
				g_mvm->coex_2g_enabled = false;
			} else {
				IWL_DEBUG_EXTERNAL(g_mvm,
						   "FM 2G coex: ERROR: Invalid paramters for enable/disable(%d)\n",
						   chan->max_tx_pwr);
				ret = IUI_FM_MITIGATION_ERROR_INVALID_PARAM;
			}
			IWL_DEBUG_EXTERNAL(g_mvm,
					   "FM 2G coex: %sabling 2G coex mode\n",
					   g_mvm->coex_2g_enabled ?
					   "en" : "dis");
			mutex_unlock(&g_mvm->mutex);
			break;
		}
	}

	return ret;
}

/*
 * Frequency Mitigation Callback function implementation.
 * Frequency Manager can ask to:
 * 1. Limit Tx power on specific channels
 * 2. Change ADC/DAC frequency - Currently not supported
 * 3. Request Rx Gain behaviour - Currently not supported
 */
static enum iui_fm_mitigation_status
iwl_mvm_fm_wlan_mitigation(const enum iui_fm_macro_id macro_id,
			   const struct iui_fm_mitigation *mitigation,
			   const uint32_t sequence)
{
	enum iui_fm_mitigation_status ret;
	struct iui_fm_wlan_mitigation *mit;

	if (macro_id != IUI_FM_MACRO_ID_WLAN ||
	    mitigation->type != IUI_FM_MITIGATION_TYPE_WLAN) {
		if (WARN_ON(!g_mvm))
			return IUI_FM_MITIGATION_ERROR_INVALID_PARAM;
		ret = IUI_FM_MITIGATION_ERROR_INVALID_PARAM;
		goto end;
	}

	if (WARN_ON(!g_mvm))
		return IUI_FM_MITIGATION_ERROR;

	mit = mitigation->info.wlan_mitigation;

	/* Enable/Disable 2G coex mode */
	ret = iwl_mvm_fm_2g_coex(mit->channel_tx_pwr, mit->num_channels);
	if (ret)
		goto end;

	/*
	 * Going to mitigate the Tx power of all stations using the channels in
	 * the channel list mit->channel_tx_pwr received from the FM.
	 */
	ret = iwl_mvm_fm_mitig_txpwr(mit->channel_tx_pwr, mit->num_channels);
	if (ret)
		goto end;

	ret = iwl_mvm_fm_mitig_adc_dac_freq(mit->wlan_adc_dac_freq);

	if (ret)
		goto end;

	ret = iwl_mvm_fm_mitig_rxgain_behavior(mit->rx_gain_behavior);
end:
	IWL_DEBUG_EXTERNAL(g_mvm, "FM: fm mitigation callback %s\n",
			   ret ? "failed" : "succeeded");
	return ret;
}

/*
 * Notify Frequency Manager that an interface changed a channel.
 */
int iwl_mvm_fm_notify_channel_change(struct ieee80211_chanctx_conf *ctx,
				     enum iwl_fm_chan_change_action action)
{
	int ret, i;
	struct iui_fm_freq_notification notification;
	struct iui_fm_wlan_info winfo = {
		.num_channels  = 0,
	};
	struct chan_list chan_info = {
		.winfo = &winfo,
		.ctx_ignore = ctx,
		.action = action,
	};

	/*
	 * if notifying the FM about adding a channel ctx we need to add this
	 * channel to the list before iterating over the channel list since
	 * the list is updated only after this function is called.
	 */
	if (action == IWL_FM_ADD_CHANCTX) {
		winfo.channel_info[0].frequency =
			MHZ_TO_KHZ(ctx->def.chan->center_freq);
		winfo.channel_info[0].bandwidth =
			iwl_mvm_fm_get_bandwidth(ctx->def.width);
		winfo.num_channels++;
	}

	/* finding all bandwidths of used channels for FM notification */
	ieee80211_iter_chan_contexts_atomic(g_mvm->hw,
					    iwl_mvm_fm_chan_iterator,
					    &chan_info);

	notification.type = IUI_FM_FREQ_NOTIFICATION_TYPE_WLAN;
	notification.info.wlan_info = &winfo;
	/* parameter not yet supported */
	notification.info.wlan_info->wlan_adc_dac_freq = 0;

	for (i = 0; i < winfo.num_channels; i++)
		IWL_DEBUG_EXTERNAL(g_mvm,
				   "FM: notifying fm about: channel=%d bandwith=%d\n",
				   winfo.channel_info[i].frequency,
				   winfo.channel_info[i].bandwidth);

	ret =  iui_fm_notify_frequency(IUI_FM_MACRO_ID_WLAN, &notification);

	IWL_DEBUG_EXTERNAL(g_mvm,
			   "FM: notified fm about channel change (fail = %d)\n",
			   ret);

	return ret ? -EINVAL : ret;
}

/*
 * Register the Frequency Mitigation Callback function implementation with
 * Frequency Manager to receive Frequency Mitigation messages from Frequency
 * Manager.
 */
int iwl_mvm_fm_register(struct iwl_mvm *mvm)
{
	int ret;

	if (g_mvm)
		return -EINVAL;

	g_mvm = mvm;

	ret = iui_fm_register_mitigation_callback(IUI_FM_MACRO_ID_WLAN,
						  iwl_mvm_fm_wlan_mitigation);

	IWL_DEBUG_EXTERNAL(g_mvm,
			   "FM: registering fm callback function (fail = %d)\n",
			   ret);
	if (ret)
		g_mvm = NULL;

	return ret ? -EINVAL : ret;
}

/*
 * Unregister the Frequency Mitigation Callback function implementation
 */
int iwl_mvm_fm_unregister(struct iwl_mvm *mvm)
{
	int ret;

	if (g_mvm != mvm)
		return 0;

	ret = iui_fm_register_mitigation_callback(IUI_FM_MACRO_ID_WLAN, NULL);

	IWL_DEBUG_EXTERNAL(g_mvm,
			   "FM: unregistering fm callback function (fail = %d)\n",
			   ret);

	g_mvm = NULL;

	return ret ? -EINVAL : ret;
}
