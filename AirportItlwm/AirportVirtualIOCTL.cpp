//
//  AirportVirtualIOCTL.cpp
//  AirportItlwm
//
//  Created by qcwap on 2020/9/4.
//  Copyright © 2020 钟先耀. All rights reserved.
//

#include "AirportItlwm.hpp"
#include <Airport/LegacyAwdlChannels.hpp>


SInt32 AirportItlwm::
apple80211VirtualRequest(UInt request_type, int request_number, IO80211VirtualInterface *interface, void *data)
{
    if (beAwdlEnabled && getWorkLoop() && !getWorkLoop()->inGate()) {
        BeAwdlIoctl call = {request_type, request_number, interface, data, kIOReturnError};
        const IOReturn status = getWorkLoop()->runAction(&AirportItlwm::beAwdlIoctlAction, this, &call);
        return status == kIOReturnSuccess ? call.result : status;
    }
    if (request_type != SIOCGA80211 && request_type != SIOCSA80211)
        return kIOReturnError;
    IOReturn ret = kIOReturnError;
    
    switch (request_number) {
        case APPLE80211_IOC_CARD_CAPABILITIES:
            IOCTL_GET(request_type, CARD_CAPABILITIES, apple80211_capability_data);
            break;
        case APPLE80211_IOC_POWER:
            IOCTL_GET(request_type, POWER, apple80211_power_data);
            break;
        case APPLE80211_IOC_SUPPORTED_CHANNELS:
            IOCTL_GET(request_type, SUPPORTED_CHANNELS, apple80211_sup_channel_data);
            break;
        case APPLE80211_IOC_DRIVER_VERSION:
            IOCTL_GET(request_type, DRIVER_VERSION, apple80211_version_data);
            break;
        case APPLE80211_IOC_OP_MODE:
            IOCTL_GET(request_type, OP_MODE, apple80211_opmode_data);
            break;
        case APPLE80211_IOC_PHY_MODE:
            IOCTL_GET(request_type, PHY_MODE, apple80211_phymode_data);
            break;
        case APPLE80211_IOC_RSSI:
            IOCTL_GET(request_type, RSSI, apple80211_rssi_data);
            break;
        case APPLE80211_IOC_STATE:
            IOCTL_GET(request_type, STATE, apple80211_state_data);
            break;
        case APPLE80211_IOC_BSSID:
            IOCTL(request_type, BSSID, apple80211_bssid_data);
            break;
        case APPLE80211_IOC_RATE:
            if(beAwdlEnabled && interface==beAwdlVif && request_type==SIOCGA80211 && data){
                // No qualified per-AWDL link-rate telemetry exists. Never
                // label the infrastructure BSS rate as an AWDL measurement.
                auto *rate=static_cast<apple80211_rate_data *>(data);memset(rate,0,sizeof(*rate));
                rate->version=APPLE80211_VERSION;ret=kIOReturnNotReady;
            } else {IOCTL_GET(request_type, RATE, apple80211_rate_data);}
            break;
        case APPLE80211_IOC_CHANNEL:
            if (beAwdlEnabled && request_type == SIOCSA80211) {
                auto *requested = static_cast<apple80211_channel_data *>(data);
                auto *ic = fHalService->get80211Controller();
                auto *ni = ic->ic_bss;
                ret = requested && requested->channel.channel == 44 && ic->ic_state == IEEE80211_S_RUN &&
                    ni && ni->ni_chan && ni->ni_chan != IEEE80211_CHAN_ANYC &&
                    ieee80211_chan2ieee(ic, ni->ni_chan) == 44 ? kIOReturnSuccess : kIOReturnUnsupported;
                ++beAwdlChannelStats[3];
                beAwdlChannelStats[4]=requested?requested->channel.channel:0;
                beAwdlChannelStats[5]=uint32_t(ret);
                absolutetime_to_nanoseconds(mach_absolute_time(),&beAwdlChannelStats[6]);
                break; // acknowledge the already-active channel; never retune STA
            }
            IOCTL(request_type, CHANNEL, apple80211_channel_data);
            break;
        case APPLE80211_IOC_AUTH_TYPE:
            IOCTL(request_type, AUTH_TYPE, apple80211_authtype_data);
            break;
        case APPLE80211_IOC_ROAM_PROFILE:
            IOCTL(request_type, ROAM_PROFILE, apple80211_roam_profile_band_data);
            break;
        case APPLE80211_IOC_SSID:
            IOCTL(request_type, SSID, apple80211_ssid_data);
            break;
        case APPLE80211_IOC_AWDL_PEER_TRAFFIC_REGISTRATION:
            IOCTL(request_type, AWDL_PEER_TRAFFIC_REGISTRATION, apple80211_awdl_peer_traffic_registration);
            break;
        case APPLE80211_IOC_AWDL_SYNC_ENABLED:
            IOCTL(request_type, SYNC_ENABLED, apple80211_awdl_sync_enabled);
            break;
        case APPLE80211_IOC_AWDL_SYNC_FRAME_TEMPLATE:
            IOCTL(request_type, SYNC_FRAME_TEMPLATE, apple80211_awdl_sync_frame_template);
            break;
        case APPLE80211_IOC_HT_CAPABILITY:
            IOCTL_GET(request_type, AWDL_HT_CAPABILITY, apple80211_ht_capability);
            break;
        case APPLE80211_IOC_VHT_CAPABILITY:
            IOCTL_GET(request_type, AWDL_VHT_CAPABILITY, apple80211_vht_capability);
            break;
        case APPLE80211_IOC_AWDL_ELECTION_METRIC:
            IOCTL(request_type, AWDL_ELECTION_METRIC, apple80211_awdl_election_metric);
            break;
        case APPLE80211_IOC_AWDL_BSSID:
            IOCTL(request_type, AWDL_BSSID, apple80211_awdl_bssid);
            break;
        case APPLE80211_IOC_CHANNELS_INFO:
            IOCTL_GET(request_type, CHANNELS_INFO, apple80211_channels_info);
            break;
        case APPLE80211_IOC_PEER_CACHE_MAXIMUM_SIZE:
            IOCTL(request_type, PEER_CACHE_MAXIMUM_SIZE, apple80211_peer_cache_maximum_size);
            break;
        case APPLE80211_IOC_AWDL_ELECTION_ID:
            IOCTL(request_type, AWDL_ELECTION_ID, apple80211_awdl_election_id);            
            break;
        case APPLE80211_IOC_AWDL_MASTER_CHANNEL:
            IOCTL(request_type, AWDL_MASTER_CHANNEL, apple80211_awdl_master_channel);
            break;
        case APPLE80211_IOC_AWDL_SECONDARY_MASTER_CHANNEL:
            IOCTL(request_type, AWDL_SECONDARY_MASTER_CHANNEL, apple80211_awdl_secondary_master_channel);
            break;
        case APPLE80211_IOC_AWDL_MIN_RATE:
            IOCTL(request_type, AWDL_MIN_RATE, apple80211_awdl_min_rate);
            break;
        case APPLE80211_IOC_AWDL_ELECTION_RSSI_THRESHOLDS:
            IOCTL(request_type, AWDL_ELECTION_RSSI_THRESHOLDS, apple80211_awdl_election_rssi_thresholds);
            break;
        case APPLE80211_IOC_AWDL_SYNCHRONIZATION_CHANNEL_SEQUENCE:
            IOCTL(request_type, AWDL_SYNCHRONIZATION_CHANNEL_SEQUENCE, apple80211_awdl_sync_channel_sequence);
            break;
        case APPLE80211_IOC_AWDL_PRESENCE_MODE:
            IOCTL(request_type, AWDL_PRESENCE_MODE, apple80211_awdl_presence_mode);
            break;
        case APPLE80211_IOC_AWDL_EXTENSION_STATE_MACHINE_PARAMETERS:
            IOCTL(request_type, AWDL_EXTENSION_STATE_MACHINE_PARAMETERS, apple80211_awdl_extension_state_machine_parameter);
            break;
        case APPLE80211_IOC_AWDL_SYNC_STATE:
            IOCTL(request_type, AWDL_SYNC_STATE, apple80211_awdl_sync_state);
            break;
        case APPLE80211_IOC_AWDL_SYNC_PARAMS:
            IOCTL(request_type, AWDL_SYNC_PARAMS, apple80211_awdl_sync_params);
            break;
        case APPLE80211_IOC_AWDL_CAPABILITIES:
            IOCTL_GET(request_type, AWDL_CAPABILITIES, apple80211_awdl_cap);
            break;
        case APPLE80211_IOC_AWDL_AF_TX_MODE:
            IOCTL(request_type, AWDL_AF_TX_MODE, apple80211_awdl_af_tx_mode);
            break;
        case APPLE80211_IOC_PEER_CACHE_CONTROL:
            if (beAwdlEnabled && request_type==SIOCSA80211)
                ret=beAwdlCacheRequest(interface,data);
            else if(beAwdlEnabled)ret=kIOReturnUnsupported;
            break;
        case APPLE80211_IOC_AWDL_OOB_REQUEST:
            // REA sendUnicastMI0x41e0c uses149, not the periodic225 contract.
            // Copy its borrowed body and destination; never turn UMI into an
            // accepted-but-unsent broadcast service-template update.
            if(beAwdlEnabled)ret=request_type==SIOCSA80211 ?
                beAwdlOobRequest(interface,static_cast<apple80211_awdl_oob_request *>(data)) : kIOReturnUnsupported;
            else {IOCTL_SET(request_type, AWDL_OOB_AUTO_REQUEST, apple80211_awdl_oob_request);}
            break;
        case APPLE80211_IOC_AWDL_AWDL_ADVERTISERS:
            if(beAwdlEnabled) {
                // Internal Legacy caller passes an unversioned borrowed prefix.
                // No asynchronous pointer retention and no duplicated election.
                if(request_type!=SIOCSA80211)ret=kIOReturnUnsupported;
                else if(!data || interface!=beAwdlVif || !getWorkLoop()->inGate())ret=kIOReturnBadArgument;
                else {
                    uint64_t now=0;absolutetime_to_nanoseconds(mach_absolute_time(),&now);
                    ret=beAwdlTransport.acceptNativeAdvertiser(data,now) ? kIOReturnSuccess:kIOReturnNotReady;
                }
            }
            break;
        case APPLE80211_IOC_AWDL_OOB_AUTO_REQUEST:
            IOCTL_SET(request_type, AWDL_OOB_AUTO_REQUEST, apple80211_awdl_oob_request);
            break;
        case APPLE80211_IOC_IE:
            IOCTL(request_type, IE, apple80211_ie_data);
            break;
        case APPLE80211_IOC_P2P_LISTEN:
            IOCTL_SET(request_type, P2P_LISTEN, apple80211_p2p_listen_data);
            break;
        case APPLE80211_IOC_P2P_SCAN:
            IOCTL_SET(request_type, P2P_SCAN, apple80211_scan_data);
            break;
        case APPLE80211_IOC_P2P_GO_CONF:
            IOCTL_SET(request_type, P2P_GO_CONF, apple80211_p2p_go_conf_data);
            break;
        default:
        unhandled:
            if (!ml_at_interrupt_context())
                XYLog("%s Unhandled IOCTL %s (%d) %s\n", __FUNCTION__, IOCTL_NAMES[request_number >= ARRAY_SIZE(IOCTL_NAMES) ? 0: request_number],
                      request_number, request_type == SIOCGA80211 ? "get" : (request_type == SIOCSA80211 ? "set" : "other"));
            break;
    }
    
    if (beAwdlEnabled) {
        beAwdlStats[24] = uint32_t(request_number); beAwdlStats[25] = request_type;
        beAwdlStats[26] = uint32_t(ret); ++beAwdlStats[27];
    }
    if(beAwdlEnabled) {
        ++beAwdlFlowStats[19];beAwdlFlowStats[20]=uint32_t(request_number);
        beAwdlFlowStats[21]=uint32_t(ret);absolutetime_to_nanoseconds(mach_absolute_time(),&beAwdlFlowStats[22]);
        if(request_number==241)++beAwdlFlowStats[23];
        if(request_number==242)++beAwdlFlowStats[24];
        if(request_number==243)++beAwdlFlowStats[25];
        if(request_number==266)++beAwdlFlowStats[26];
        if(request_number==143)++beAwdlFlowStats[27];
    }
    return ret;
}

IOReturn AirportItlwm::
setAWDL_PEER_TRAFFIC_REGISTRATION(OSObject *object, struct apple80211_awdl_peer_traffic_registration *data)
{
    if (!data || data->name_len > sizeof(data->name)) return kIOReturnBadArgument;
    return kIOReturnSuccess; // bounded registration metadata, no fabricated peer
}

IOReturn AirportItlwm::
getAWDL_PEER_TRAFFIC_REGISTRATION(OSObject *object, struct apple80211_awdl_peer_traffic_registration *)
{
    XYLog("%s\n", __FUNCTION__);
    if (fAWDLInterface)
        return 45;
    return 22;
}

IOReturn AirportItlwm::
setAWDL_ELECTION_METRIC(OSObject *object, struct apple80211_awdl_election_metric *data)
{
    XYLog("%s metric=%d\n", __FUNCTION__, data->metric);
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_ELECTION_METRIC(OSObject *object, struct apple80211_awdl_election_metric *data)
{
    XYLog("%s\n", __FUNCTION__);
    return kIOReturnError;
}

IOReturn AirportItlwm::
getSYNC_ENABLED(OSObject *object, struct apple80211_awdl_sync_enabled *data)
{
    XYLog("%s\n", __FUNCTION__);
    data->version = APPLE80211_VERSION;
    data->enabled = this->awdlSyncEnable;
    data->unk1 = 0;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setSYNC_ENABLED(OSObject *object, struct apple80211_awdl_sync_enabled *data)
{
    if (!data) return kIOReturnBadArgument;
    if (beAwdlEnabled && bool(data->enabled)!=awdlSyncEnable) ++beAwdlWindowNativeStats[15];
    XYLog("%s sync_enabled=%d\n", __FUNCTION__, data->enabled);
    this->awdlSyncEnable = data->enabled;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getSYNC_FRAME_TEMPLATE(OSObject *object, struct apple80211_awdl_sync_frame_template *data)
{
    if (beAwdlEnabled && !beAwdlTransport.templateLength()) return kIOReturnNotReady;
    if (!data) return kIOReturnBadArgument;
    uint32_t capacity = data->payload_len;
    const auto result = beAwdlTemplate.copy(data->payload, capacity);
    data->version = APPLE80211_VERSION; data->payload_len = capacity;
    if (result == awdl_p9::Template::OK) return kIOReturnSuccess;
    if (result == awdl_p9::Template::Busy) return kIOReturnBusy;
    return result == awdl_p9::Template::Short ? kIOReturnNoSpace : kIOReturnNotReady;
}

IOReturn AirportItlwm::
setSYNC_FRAME_TEMPLATE(OSObject *object, struct apple80211_awdl_sync_frame_template *data)
{
    if (!data) return kIOReturnBadArgument;
    // Validate before replacing either owned representation. Native HT bytes
    // are NOT padded into an invented capability advertisement in this port.
    if (beAwdlEnabled && !itlwm_awdl::TransportState::validTemplate(data->payload, data->payload_len))
        return kIOReturnBadArgument;
    auto result = beAwdlTemplate.replace(data->payload, data->payload_len);
    if (result == awdl_p9::Template::OK) {
        // Do not replace the transmitted representation if the authoritative
        // P9R2 getter store rejected Busy. Validation above is mutation-free;
        // under the native gate this deterministic CPU copy cannot now fail.
        if(beAwdlEnabled)beAwdlTransport.setTemplate(data->payload,data->payload_len,false);
        ++beAwdlStats[0]; return kIOReturnSuccess;
    }
    return result == awdl_p9::Template::Busy ? kIOReturnBusy : kIOReturnBadArgument;
}

IOReturn AirportItlwm::
getAWDL_HT_CAPABILITY(OSObject *object, struct apple80211_ht_capability *data)
{
    if (!data) return kIOReturnBadArgument;
    memset(data, 0, sizeof(*data));
    data->version = APPLE80211_VERSION;
    data->hc_id = IEEE80211_ELEMID_HTCAPS;
    data->hc_cap = (IEEE80211_HTCAP_SGI20 | IEEE80211_HTCAP_CBW20_40 | IEEE80211_HTCAP_SGI40);
    if (beAwdlEnabled) {
        auto *ic = fHalService ? fHalService->get80211Controller() : nullptr;
        if (!ic || !ic->ic_sup_mcs[0]) return kIOReturnNotReady;
        // Legacy caches this IE and trims its trailing zeros into TLV 7.
        // A caps-only reply produces five fixed bytes with no MCS data.
        // The matching MBP decoder then throws on the empty remainder.
        // Supply the initialized radio's actual IE, including its MCS set.
        data->hc_len = 26;
        data->hc_cap = ic->ic_htcaps;
        data->hc_param = ic->ic_ampdu_params;
        memcpy(data->hc_mcsset, ic->ic_sup_mcs, sizeof(ic->ic_sup_mcs));
        data->hc_mcsset[10] = uint8_t(ic->ic_max_rxrate);
        data->hc_mcsset[11] = uint8_t(ic->ic_max_rxrate >> 8);
        data->hc_mcsset[12] = ic->ic_tx_mcs_set;
    }
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_VHT_CAPABILITY(OSObject *object, struct apple80211_vht_capability *data)
{
    if (!data) return kIOReturnBadArgument;
    memset(data, 0, sizeof(*data));
    data->version = APPLE80211_VERSION;
    data->cap = 3263;
    if (beAwdlEnabled) {
        auto *ic = fHalService ? fHalService->get80211Controller() : nullptr;
        if (!ic || !ic->ic_sup_mcs[0]) return kIOReturnNotReady;
        // The 14 bytes following version are the VHT IE, as consumed by
        // Legacy and the reference Skywalk getter. A zero map advertises
        // eight supported streams. Preserve the actual radio's RX/TX maps.
        data->unk1 = ic->ic_vhtcaps;
        data->unk2 = ic->ic_vht_rx_mcs_map;
        data->unk3 = ic->ic_vht_rx_highest;
        data->unk4 = ic->ic_vht_tx_mcs_map;
        data->unk5 = ic->ic_vht_tx_highest;
    }
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_BSSID(OSObject *object, struct apple80211_awdl_bssid *data)
{
    XYLog("%s\n", __FUNCTION__);
    data->version = APPLE80211_VERSION;
    memcpy(data->bssid, awdlBSSID, 6);
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setAWDL_BSSID(OSObject *object, struct apple80211_awdl_bssid *data)
{
    if (!data) return kIOReturnBadArgument;
    XYLog("%s bssid=%s unk_mac=%s\n", __FUNCTION__, ether_sprintf(data->bssid), ether_sprintf(data->unk_mac));
    if (!data) return kIOReturnBadArgument;
    if (beAwdlEnabled && !beAwdlTransport.setBssid(data->bssid)) return kIOReturnBadArgument;
    memcpy(awdlBSSID, data->bssid, 6);
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getCHANNELS_INFO(OSObject *object, struct apple80211_channels_info *data)
{
    XYLog("%s\n", __FUNCTION__);
    if (!data) return kIOReturnBadArgument;
    struct ieee80211com *ic = fHalService->get80211Controller();
#if __IO80211_TARGET == __MAC_13_0
    if (beAwdlEnabled && object==beAwdlVif) {
        ++beAwdlChannelStats[7];
        // This is the native PM's virtual GET207, not an infrastructure query.
        // PM ioctl_internal_gated0x6ad2c routes its +b8 VIF through +cb8;
        // configureValidAWDLChannels0x16fa8 owns the full 0x1a9c buffer.
        // The old 128-entry type put chan_num at+10a, where this binary reads
        // chan_spec: a success return still left the real channel array empty.
        auto *native=reinterpret_cast<be_awdl::LegacyChannelInfo *>(data);
        native->clear();
        for (unsigned i=0;i<IEEE80211_CHAN_MAX && native->count()<native->Capacity;++i) {
            const auto *channel=&ic->ic_channels[i];
            if (!channel->ic_freq) continue;
            const unsigned number=ieee80211_chan2ieee(ic,channel);
            if (!number || number>255) continue;
            native->add(uint8_t(number),bool(channel->ic_flags&IEEE80211_CHAN_DFS),
                bool(channel->ic_flags&IEEE80211_CHAN_PASSIVE),
                IEEE80211_IS_CHAN_HT40(channel)||IEEE80211_IS_CHAN_VHT40(channel),
                IEEE80211_IS_CHAN_VHT80(channel));
        }
        return kIOReturnSuccess;
    }
#endif
    memset(data, 0, sizeof(*data));
    data->version = APPLE80211_VERSION;
    for (int i = 0; i < IEEE80211_CHAN_MAX; i++) {
        struct ieee80211_channel *channel = &ic->ic_channels[i];
        if (channel->ic_freq != 0) {
            int chanNum = ieee80211_chan2ieee(ic, channel);
            data->chan_num[data->num_chan_specs] = chanNum;
            data->support_80Mhz[data->num_chan_specs] = IEEE80211_IS_CHAN_VHT80(channel);
            data->support_40Mhz[data->num_chan_specs] = IEEE80211_IS_CHAN_HT40(channel) || IEEE80211_IS_CHAN_VHT40(channel);
            data->num_chan_specs++;
            if (data->num_chan_specs >= APPLE80211_MAX_CHANNELS)
                break;
        }
    }
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_ELECTION_ID(OSObject *object, struct apple80211_awdl_election_id *data)
{
    XYLog("%s\n", __FUNCTION__);
    data->version = APPLE80211_VERSION;
    data->election_id = awdlElectionId;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setAWDL_ELECTION_ID(OSObject *object, struct apple80211_awdl_election_id *data)
{
    XYLog("%s election_id=%d\n", __FUNCTION__, data->election_id);
    awdlElectionId = data->election_id;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getPEER_CACHE_MAXIMUM_SIZE(OSObject *object, struct apple80211_peer_cache_maximum_size *data)
{
    XYLog("%s\n", __FUNCTION__);
    data->version = APPLE80211_VERSION;
    data->max_peers = beAwdlEnabled ? 4 : 255; // real bounded R5 reservation capacity, not installed count
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setPEER_CACHE_MAXIMUM_SIZE(OSObject *object, struct apple80211_peer_cache_maximum_size *data)
{
    XYLog("%s max_peers=%d\n", __FUNCTION__, data->max_peers);
    
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_MASTER_CHANNEL(OSObject *object, struct apple80211_awdl_master_channel *data)
{
    data->version = APPLE80211_VERSION;
    data->master_channel = awdlMasterChannel;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setAWDL_MASTER_CHANNEL(OSObject *object, struct apple80211_awdl_master_channel *data)
{
    XYLog("%s master_channel=%d\n", __FUNCTION__, data->master_channel);
    awdlMasterChannel = data->master_channel;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_SECONDARY_MASTER_CHANNEL(OSObject *object, struct apple80211_awdl_secondary_master_channel *data)
{
    XYLog("%s temporary return channel 0\n", __FUNCTION__);
    data->version = APPLE80211_VERSION;
    data->secondary_master_channel = awdlSecondaryMasterChannel;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setAWDL_SECONDARY_MASTER_CHANNEL(OSObject *object, struct apple80211_awdl_secondary_master_channel *data)
{
    XYLog("%s secondary_master_channel=%d\n", __FUNCTION__, data->secondary_master_channel);
    awdlSecondaryMasterChannel = data->secondary_master_channel;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setAWDL_MIN_RATE(OSObject *object, struct apple80211_awdl_min_rate *data)
{
    if (beAwdlEnabled) {
        if (!data) return kIOReturnBadArgument;
        // Native default12 is 6Mbps in 500kbps units; this is the actual
        // private GL92 command-rate path. No unimplemented rate change ACK.
        return data->min_rate==12 ? kIOReturnSuccess : kIOReturnUnsupported;
    }
    XYLog("%s min_rate=%d plus=%d\n", __FUNCTION__, data->min_rate, 2 *data->min_rate);
    
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_MIN_RATE(OSObject *object, struct apple80211_awdl_min_rate *data)
{
    if (beAwdlEnabled) {
        if (!data) return kIOReturnBadArgument;
        data->version=APPLE80211_VERSION; data->min_rate=12;
        return kIOReturnSuccess;
    }
    XYLog("%s\n", __FUNCTION__);
    data->version = APPLE80211_VERSION;
    
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_ELECTION_RSSI_THRESHOLDS(OSObject *object, struct apple80211_awdl_election_rssi_thresholds *data)
{
    XYLog("%s\n", __FUNCTION__);
    return kIOReturnError;
}

IOReturn AirportItlwm::
setAWDL_ELECTION_RSSI_THRESHOLDS(OSObject *object, struct apple80211_awdl_election_rssi_thresholds *data)
{
    XYLog("%s unk1=%d unk2=%d unk3=%d\n", __FUNCTION__, data->unk1, data->unk2, data->unk3);
    
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_SYNCHRONIZATION_CHANNEL_SEQUENCE(OSObject *object, struct apple80211_awdl_sync_channel_sequence *data)
{
    XYLog("%s\n", __FUNCTION__);
    if (!data) return kIOReturnBadArgument;
    if (beAwdlEnabled) {
        if (!beAwdlSequenceValid) return kIOReturnNotReady;
        *data = beAwdlNativeSequence; // requested sequence; RF override is explicitly channel 44
    }
    data->version = APPLE80211_VERSION;
    
    return kIOReturnSuccess;
}

#if 0
static void dumpAWDLChannelSeqs(struct apple80211_awdl_sync_channel_sequence *data)
{
    if (data == nullptr)
        return;
    XYLog("%s length %u step count %u duplicate %u fill %d encoding %u\n", __FUNCTION__, data->length, data->step_count, data->duplicate_count, data->fill_channel, data->encoding);
    for (int i = 0; i < data->length; i++) {
        struct apple80211_channel_sequence seq = data->seqs[i];
        uint16_t band = seq.flags & 0xC00;
        uint16_t channel = seq.flags & 0x300;

        XYLog("%s %d 0x%04x=%d%s%s%s\n", __FUNCTION__, i, seq.flags, (uint8_t)seq.flags, band != 0x800 ? (band == 0xC00 ? ",20MHz" : ",unknown") : ",20MHz", channel != 0x200 ? (channel == 0x100 ? ",-1" : ",none") : ",1", (seq.flags & 0xF000) == 4096 ? ",5GHz" : ",unknown");
    }
}
#endif

IOReturn AirportItlwm::
setAWDL_SYNCHRONIZATION_CHANNEL_SEQUENCE(OSObject *object, struct apple80211_awdl_sync_channel_sequence *data)
{
    if (!data) return kIOReturnBadArgument;
    if (beAwdlEnabled) {
        const size_t count = data->length;
        if (!count || count > sizeof(data->seqs) / sizeof(data->seqs[0]) || data->encoding > 1)
            return kIOReturnBadArgument;
        uint16_t channels[itlwm_awdl::kMaximumChannelSequence] = {};
        for (size_t i = 0; i < count; ++i) channels[i] = uint16_t(data->seqs[i].flags);
        if (!beAwdlTransport.setChannelSequence(channels, count, data->encoding,
            data->step_count, data->duplicate_count, data->fill_channel)) return kIOReturnBadArgument;
        if (!beAwdlSequenceValid || memcmp(&beAwdlNativeSequence,data,sizeof(*data))) ++beAwdlWindowNativeStats[15];
        beAwdlNativeSequence = *data; beAwdlSequenceValid = true;
    }
#if 0
    dumpAWDLChannelSeqs(data);
#endif
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_PRESENCE_MODE(OSObject *object, struct apple80211_awdl_presence_mode *data)
{
    XYLog("%s\n", __FUNCTION__);
    data->version = APPLE80211_VERSION;
    data->mode = awdlPresenceMode;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setAWDL_PRESENCE_MODE(OSObject *object, struct apple80211_awdl_presence_mode *data)
{
    if (!data) return kIOReturnBadArgument;
    XYLog("%s mode=%d\n", __FUNCTION__, data->mode);
    if (!data || data->mode > 255) return kIOReturnBadArgument;
    if (beAwdlEnabled && awdlPresenceMode!=data->mode) ++beAwdlWindowNativeStats[15];
    awdlPresenceMode = data->mode;
    if (beAwdlEnabled) beAwdlTransport.setPresenceMode(data->mode);
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_EXTENSION_STATE_MACHINE_PARAMETERS(OSObject *object, struct apple80211_awdl_extension_state_machine_parameter *data)
{
    // The finite whole-EAW lease does not implement native dynamic extension
    // thresholds. GET must not feed caller's unfilled bytes back into +ceb..cee;
    // SET must not acknowledge an unexecuted firmware state-machine request.
    if (beAwdlEnabled) return data ? kIOReturnUnsupported : kIOReturnBadArgument;
    XYLog("%s\n", __FUNCTION__);
    data->version = APPLE80211_VERSION;
    
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setAWDL_EXTENSION_STATE_MACHINE_PARAMETERS(OSObject *object, struct apple80211_awdl_extension_state_machine_parameter *data)
{
    // The finite whole-EAW lease does not implement native dynamic extension
    // thresholds. GET must not feed caller's unfilled bytes back into +ceb..cee;
    // SET must not acknowledge an unexecuted firmware state-machine request.
    if (beAwdlEnabled) return data ? kIOReturnUnsupported : kIOReturnBadArgument;
    XYLog("%s unk1=%d unk2=%d unk3=%d unk4=%d\n", __FUNCTION__, data->unk1, data->unk2, data->unk3, data->unk4);
    
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_SYNC_STATE(OSObject *object, struct apple80211_awdl_sync_state *data)
{
    if (!data) return kIOReturnBadArgument;
    if (beAwdlEnabled && beAwdlNativeSyncValid) *data = beAwdlNativeSyncRequest;
    XYLog("%s\n", __FUNCTION__);
    data->version = APPLE80211_VERSION;
    data->state = awdlSyncState;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setAWDL_SYNC_STATE(OSObject *object, struct apple80211_awdl_sync_state *data)
{
    if (!data) return kIOReturnBadArgument;
    if (beAwdlEnabled) {
        if (data->version > APPLE80211_VERSION || data->state>3) return kIOReturnBadArgument;
        if (!beAwdlNativeSyncValid || memcmp(&beAwdlNativeSyncRequest,data,sizeof(*data))) ++beAwdlWindowNativeStats[15];
        beAwdlNativeSyncRequest = *data; // all24 framework-marshalled bytes
        beAwdlNativeSyncValid = true;
    }
    XYLog("%s state=%d\n", __FUNCTION__, data->state);
    awdlSyncState = data->state;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_SYNC_PARAMS(OSObject *object, struct apple80211_awdl_sync_params *data)
{
    XYLog("%s\n", __FUNCTION__);
    if (!data) return kIOReturnBadArgument;
    if (beAwdlEnabled) {
        if (!beAwdlParamsValid) return kIOReturnNotReady;
        *data = beAwdlNativeParams;
    }
    data->version = APPLE80211_VERSION;

    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setAWDL_SYNC_PARAMS(OSObject *object, struct apple80211_awdl_sync_params *data)
{
    if (!data) return kIOReturnBadArgument;
    if (beAwdlEnabled) {
        const uint32_t values[] = {data->availability_window_length, data->availability_window_period, data->extension_length, data->synchronization_frame_period};
        for (unsigned i = 0; i < 4; ++i) if (!values[i] || values[i] > 65535) return kIOReturnBadArgument;
        beAwdlTransport.setSyncParameters(data->availability_window_length, data->availability_window_period,
            data->extension_length, data->synchronization_frame_period);
        if (!beAwdlParamsValid || memcmp(&beAwdlNativeParams,data,sizeof(*data))) ++beAwdlWindowNativeStats[15];
        beAwdlNativeParams = *data; beAwdlParamsValid = true;
    }
    XYLog("%s availability_window_length=%d availability_window_period=%d extension_length=%d synchronization_frame_period=%d\n", __FUNCTION__, data->availability_window_length, data->availability_window_period, data->extension_length, data->synchronization_frame_period);
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_CAPABILITIES(OSObject *object, struct apple80211_awdl_cap *data)
{
    if (!data) return kIOReturnBadArgument;
    XYLog("%s\n", __FUNCTION__);
    data->version = APPLE80211_VERSION;
#if __IO80211_TARGET == __MAC_13_0
    data->length=beAwdlEnabled ? sizeof(*data) : 0; // preserve non-opt-in bytes
#endif
    // The experimental BE200 bridge does not implement native CCA telemetry.
    // Keep the historical non-opt-in path unchanged; never fabricate CCA in43.
    data->cap = beAwdlEnabled ? 0 : APPLE80211_AWDL_CAP_CCA_STATS;
    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
getAWDL_AF_TX_MODE(OSObject *object, struct apple80211_awdl_af_tx_mode *data)
{
    if(beAwdlEnabled) {
        auto *vif=OSDynamicCast(IO80211VirtualInterface,object);OSObject *manager=nullptr;be_awdl::NativeConfig cfg={};
        if(!data || !vif || vif!=beAwdlVif || !getWorkLoop()->inGate())return kIOReturnBadArgument;
        memcpy(&manager,reinterpret_cast<const uint8_t *>(vif)+0xb0,sizeof(manager));
        if(!manager || !manager->metaCast("IO80211AWDLPeerManager") ||
           !be_awdl::readConfig(manager,manager->getMetaClass()->getClassSize(),cfg))return kIOReturnNotReady;
        data->version=APPLE80211_VERSION;data->mode=beAwdlAfTxModeKnown?beAwdlAfTxMode:cfg.afTxMode;
        return (data->mode==0 || data->mode==3)?kIOReturnSuccess:kIOReturnUnsupported;
    }
    XYLog("%s\n", __FUNCTION__);
    data->version = APPLE80211_VERSION;

    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setAWDL_AF_TX_MODE(OSObject *object, struct apple80211_awdl_af_tx_mode *data)
{
    if(beAwdlEnabled) {
        if(!data || data->version>1 || object!=beAwdlVif || !getWorkLoop()->inGate())return kIOReturnBadArgument;
        // REA monitorDataPath0x2c7cc:0 enables PSFs,3 disables PSFs.
        // It does not cancel explicit149 requests or the separate MI template.
        if(data->mode!=0 && data->mode!=3)return kIOReturnUnsupported;
        beAwdlAfTxMode=data->mode;beAwdlAfTxModeKnown=true;return kIOReturnSuccess;
    }
    XYLog("%s mode=%llu\n", __FUNCTION__, static_cast<unsigned long long>(data->mode));

    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setAWDL_OOB_AUTO_REQUEST(OSObject *object, struct apple80211_awdl_oob_request *data)
{
    if (!data) return kIOReturnBadArgument;
#if __IO80211_TARGET == __MAC_13_0
    // P9R2: synchronous, borrowed CPU pointer at byte 48 of a 56-byte header.
    // The transport copies accepted TLVs now; no pointer survives this call.
    if (data->data_len > 1200 || (data->data_len && !data->data)) return kIOReturnBadArgument;
    if (beAwdlEnabled && data->data_len) {
        auto *vif=OSDynamicCast(IO80211VirtualInterface,object);
        auto *iwx=OSDynamicCast(ItlIwx,fHalService);
        if(vif!=beAwdlVif || !vif || !iwx || !getWorkLoop()->inGate())return kIOReturnNotReady;
        // Exact Legacy updatePrimaryPayloadMI0x42327 supplies the complete
        // current MI TLV set, including DataPathState, not just Services.
        // Keep it independently of the Sync template; dropping native TLVs
        // silently freezes the peer's view of data/UMI/identity prerequisites.
        const size_t bytes=be_awdl::NativeActions::miBody(beAwdlAutoMi.body,
            sizeof(beAwdlAutoMi.body),data->data,data->data_len,&beAwdlAutoMi.needsMiTiming);
        if(!bytes)return kIOReturnBadArgument;
        const int kept=beAwdlTransport.setServiceTlvsFromFrame(data->data, data->data_len);
        beAwdlFlowStats[28]=data->data_len;beAwdlFlowStats[29]=uint32_t(kept);
        beAwdlFlowStats[30]=0;memcpy(&beAwdlFlowStats[30],data->data,data->data_len<8?data->data_len:8);
        if (kept < 0) return kIOReturnBadArgument;
        ++beAwdlStats[1];
        beAwdlAutoMi.length=bytes;beAwdlAutoMi.oob=false;
        memset(beAwdlAutoMi.destination,255,6);beAwdlTransport.copyBssid(beAwdlAutoMi.bssid);
        beAwdlAutoMi.key.generation=uint32_t(iwx->com.sc_generation);
        beAwdlAutoMi.key.epoch=iwx->fPc1ResetEpoch;beAwdlAutoMi.key.publication=beAwdlStats[21];
        beAwdlAutoMiValid=true;
    }
    return kIOReturnSuccess;
#else
    return kIOReturnUnsupported;
#endif
}

IOReturn AirportItlwm::beAwdlIoctlAction(OSObject *owner, void *arg, void *, void *, void *)
{
    auto *self = OSDynamicCast(AirportItlwm, owner);
    auto *call = static_cast<BeAwdlIoctl *>(arg);
    if (!self || !call) return kIOReturnBadArgument;
    call->result = self->apple80211VirtualRequest(call->type, call->number, call->vif, call->data);
    return kIOReturnSuccess; // borrowed payload remains valid throughout synchronous call
}
