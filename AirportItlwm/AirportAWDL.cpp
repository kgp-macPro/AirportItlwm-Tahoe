//
//  AirportAWDL.cpp
//  AirportItlwm
//
//  Created by qcwap on 2020/9/4.
//  Copyright © 2020 钟先耀. All rights reserved.
//

#include <Airport/BeAwdlData.hpp>
#include <Airport/BeAwdlDataTransport.hpp>
#include <Airport/BeAwdlDpStateExperiment.hpp>
#include "AirportItlwm.hpp"
#include <Airport/AwdlP9.hpp>

#include <net80211/ieee80211_priv.h>
#include <net80211/ieee80211_var.h>

#define INTERFACE_NAME(object) \
 OSDynamicCast(IO80211Interface, object) == nullptr ? (OSDynamicCast(IO80211P2PInterface, object) == nullptr ? "???" : OSDynamicCast(IO80211P2PInterface, object)->getBSDName()) : OSDynamicCast(IO80211Interface, object)->getBSDName()

IOReturn AirportItlwm::
getIE(OSObject *object, struct apple80211_ie_data *data)
{
    XYLog("%s %s Error\n", __FUNCTION__,  INTERFACE_NAME(object));
    return kIOReturnError;
}

IOReturn AirportItlwm::
setIE(OSObject *object, struct apple80211_ie_data *data)
{
    XYLog("%s %s frame_type_flags %x add %d signature_len %d ie_len %d\n", __FUNCTION__, INTERFACE_NAME(object), data->frame_type_flags, data->add, data->signature_len, data->ie_len);
    if (data->frame_type_flags == APPLE80211_IE_FLAG_ASSOC_REQ && data->add && data->ie_len && *(uint8_t*)data->ie == 68) {
        XYLog("%s setCustomAssocIE\n", __FUNCTION__);
        return kIOReturnSuccess;
    }

    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setP2P_SCAN(OSObject *object, struct apple80211_scan_data *data)
{
    XYLog("%s %s ssid=%s bssid=%s channel=%d phy_mode=%d scan_type=%d\n", __FUNCTION__, INTERFACE_NAME(object), data->ssid, ether_sprintf(data->bssid.octet), data->num_channels, data->phy_mode, data->scan_type);

    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setP2P_LISTEN(OSObject *object, struct apple80211_p2p_listen_data *data)
{
    XYLog("%s %s channel=%d pad1=%d flags=%d duration=%d\n", __FUNCTION__, INTERFACE_NAME(object), data->channel, data->pad1, data->flags, data->duration);

    return kIOReturnSuccess;
}

IOReturn AirportItlwm::
setP2P_GO_CONF(OSObject *object, struct apple80211_p2p_go_conf_data *data)
{
    if (beAwdlEnabled) return kIOReturnUnsupported; // no GL92 GO port is claimed
    XYLog("%s %s auth_upper=%d auth_lower=%d channel=%d bcn_len=%d ssid=%s suppress_beacon=%d\n", __FUNCTION__, INTERFACE_NAME(object), data->auth_upper, data->auth_lower, data->channel, data->bcn_len, data->ssid, data->suppress_beacon);

    return kIOReturnSuccess;
}

// BE200 first discovery candidate. Native ABI is pinned to Ventura Legacy
// 4C07538B-62EB-3D6D-AEF9-93BCDCF45FAB by the installation preflight/ABI test.
// REA 0xb2baa -> 0x3db24 proves bpfTapInput may consume asynchronously.
// Use the exported synchronous borrower (0x3ddb2), not that ambiguous wrapper.
// REA 0x12b34/0x5ee2e attaches its native Action event source, timers and
// command gate to this controller getWorkLoop (vtable +0x688, ABI-tested).
// This callback runs on that same loop; do not move it to the HAL RX loop.
#if __IO80211_TARGET == __MAC_13_0
extern "C" bool kgpNativeAwdlProcess(void *, mbuf_t, UInt, void *, size_t)
    __asm__("__ZN22IO80211AWDLPeerManager23actionFrameProcessInputEP6__mbufjPvm");
// REA Legacy0xb54c4 consumes the mbuf on every path. The optional metadata
// pointer is checked by base0x69adc; no invented packet_info_tag is supplied.
// Exact Legacy0x1259e is a non-consuming lookup; result is only tested for
// presence under the retained manager/main gate, never dereferenced/owned.
extern "C" void *kgpNativeFindAwdlPeer(void *,uint8_t *)
    __asm__("__ZN22IO80211AWDLPeerManager12findAWDLPeerEPh");
extern "C" void kgpNativeAwdlData(void *,mbuf_t,void *)
    __asm__("__ZN23IO80211VirtualInterface11inputPacketEP6__mbufP15packet_info_tag");
struct KgpAwdlActionMetadata {
    uint8_t source[6], reserved0[6];
    apple80211_channel channel;
    uint8_t reserved1[16];
    uint32_t rate;
    int32_t rssi;
    uint32_t gp2;
    uint8_t reserved2[20];
};
static_assert(sizeof(KgpAwdlActionMetadata) == 72, "matched native Action metadata");
static_assert(offsetof(KgpAwdlActionMetadata, rssi) == 44, "native RSSI");
#endif

void AirportItlwm::beAwdlTimerAction(OSObject *owner, IOTimerEventSource *)
{
    auto *self = OSDynamicCast(AirportItlwm, owner);
    if (!self || !self->beAwdlEnabled) return;
    self->beAwdlReceiveData(); // bounded CPU copies; never on the RX credit loop
    self->beAwdlServiceOob(); // actual correlated statuses, outside RX completion
    self->beAwdlAvailabilityStep();
    self->beAwdlReceiveActions(); // bounded CPU mailbox on the actual native loop
    self->beAwdlServiceData(); // only after current native plan/window revalidation
    if ((self->beAwdlFastTick++ % 40)==0) self->beAwdlStep(); // context preparation/publication only
    if (self->beAwdlTimer) self->beAwdlTimer->setTimeoutMS(5);
}

void AirportItlwm::beAwdlAvailabilityStep()
{
#if __IO80211_TARGET == __MAC_13_0
    auto *iwx=OSDynamicCast(ItlIwx,fHalService);
    if (!beAwdlEnabled || !iwx || !getWorkLoop()->inGate() || beAwdlAvailabilityBusy) return;
    struct Busy { bool &v; Busy(bool &b):v(b){v=true;} ~Busy(){v=false;} } busy(beAwdlAvailabilityBusy);
    ++beAwdlWindowNativeStats[1];
    iwx->beAwdlWindowPoll();
    uint64_t w[32]; iwx->beAwdlWindowSnapshot(w);
    auto *vif=beAwdlVif;
    if (!vif) return; // finite co-channel lease drains; no new TX/native calls
    vif->retain(); // synchronous native stack-payload call, original ABI pattern
    OSObject *manager=nullptr;
    memcpy(&manager,reinterpret_cast<const uint8_t *>(vif)+0xb0,sizeof(manager));
    if (!manager || !manager->metaCast("IO80211AWDLPeerManager")) {
        ++beAwdlWindowNativeStats[4]; vif->release(); return;
    }
    manager->retain();
    be_awdl::NativeConfig cfg={}; be_awdl::SyncRequest policy={};
    memcpy(&policy,&beAwdlNativeSyncRequest,sizeof(policy));
    const bool nativeReady=be_awdl::readConfig(manager,manager->getMetaClass()->getClassSize(),cfg) &&
        beAwdlNativeSyncValid && be_awdl::ready(cfg,policy);
    beAwdlWindowNativeStats[16]=policy.state; beAwdlWindowNativeStats[17]=policy.mode16;
    beAwdlWindowNativeStats[18]=nativeReady; beAwdlWindowNativeStats[19]=!memcmp(cfg.master,cfg.self,6);
    beAwdlWindowNativeStats[31]=!memcmp(cfg.self,beAwdlAddress,6);
    beAwdlWindowNativeStats[26]=beAwdlNativeSyncValid; beAwdlWindowNativeStats[30]=policy.version;
    uint32_t duration=0; uint64_t interval=0;
    const bool wanted=beAwdlVirtualEnabled && awdlSyncEnable && iwx->beAwdlCurrent() &&
        beAwdlParamsValid && beAwdlSequenceValid && nativeReady &&
        be_awdl::timing(beAwdlNativeParams.availability_window_length,
            beAwdlNativeParams.availability_window_period,awdlPresenceMode,duration,interval);
    beAwdlWindowNativeStats[22]=duration; beAwdlWindowNativeStats[23]=interval;
    const bool sameEpoch=beAwdlWindowGeneration==w[13] && beAwdlWindowEpoch==w[14];
    bool nativeInvalidated=false;
    auto clearNative=[&]() {
        if (!beAwdlWindowAnnounced) return;
        // Native46 can postSyncStateChanged and reenter configuration.
        // Withdraw the old CPU authority before invoking that callback.
        beAwdlWindowAnnounced=false; beAwdlPostedWindow=0;
        nativeInvalidated=true;
        uint32_t inactive=0; // actual stopped/failed own hardware context, not a forced enable
        vif->postMessage(be_awdl::SyncChangedMessage,&inactive,sizeof(inactive));
        ++beAwdlWindowNativeStats[14];
    };
    bool invalidatedAnnounced=false;
    if (!sameEpoch) {
        invalidatedAnnounced=beAwdlWindowAnnounced;
        clearNative(); beAwdlWindowGeneration=w[13]; beAwdlWindowEpoch=w[14];
        beAwdlWindowNativeStats[12]=beAwdlWindowNativeStats[13]=0;
        beAwdlNextWindowNs=0; beAwdlLastActionNs=0; ++beAwdlWindowNativeStats[7];
    }
    if (nativeInvalidated) { manager->release(); vif->release(); return; }
    const bool samePlan=beAwdlWindowPlanSerial==beAwdlWindowNativeStats[15] &&
        beAwdlWindowPlanPublication==beAwdlStats[21] &&
        cfg.monitorPeriod==beAwdlWindowPlanConfig.monitorPeriod &&
        cfg.sequenceLength==beAwdlWindowPlanConfig.sequenceLength &&
        cfg.duplicateCount==beAwdlWindowPlanConfig.duplicateCount &&
        !memcmp(cfg.self,beAwdlWindowPlanConfig.self,6) &&
        !memcmp(cfg.master,beAwdlWindowPlanConfig.master,6) &&
        !memcmp(cfg.topMaster,beAwdlWindowPlanConfig.topMaster,6);
    beAwdlWindowNativeStats[27]=samePlan; beAwdlWindowNativeStats[28]=beAwdlStats[21];
    const uint64_t mismatch=
        (uint64_t(beAwdlWindowPlanSerial!=beAwdlWindowNativeStats[15])<<0) |
        (uint64_t(beAwdlWindowPlanPublication!=beAwdlStats[21])<<1) |
        (uint64_t(cfg.monitorPeriod!=beAwdlWindowPlanConfig.monitorPeriod)<<2) |
        (uint64_t(cfg.sequenceLength!=beAwdlWindowPlanConfig.sequenceLength)<<3) |
        (uint64_t(cfg.duplicateCount!=beAwdlWindowPlanConfig.duplicateCount)<<4) |
        (uint64_t(memcmp(cfg.self,beAwdlWindowPlanConfig.self,6)!=0)<<5) |
        (uint64_t(memcmp(cfg.master,beAwdlWindowPlanConfig.master,6)!=0)<<6) |
        (uint64_t(memcmp(cfg.topMaster,beAwdlWindowPlanConfig.topMaster,6)!=0)<<7) |
        (uint64_t(!wanted)<<16);
    if (mismatch!=beAwdlWindowMismatch) {
        iwx->beAwdlWindowEvent(8,mismatch,beAwdlWindowNativeStats[15],beAwdlWindowPlanSerial);
        beAwdlWindowMismatch=mismatch;
    }
    // A real end is processed once, after its real start. Firmware clocks are
    // RX-anchored estimates at notification receipt, never claimed on-air TSF.
    if (w[17] && w[17]!=beAwdlWindowNativeStats[13]) {
        const bool announcedEnd=beAwdlPostedWindow==w[17] && sameEpoch &&
            iwx->beAwdlQueueOwned() && !iwx->fBeAwdlDetail[6] && beAwdlVif==vif &&
            beAwdlWindowPlanPublication==beAwdlStats[21];
        // Consume the old CPU ticket BEFORE native43: it can run election,
        // change policy, detach/recreate the VIF or synchronously request work.
        beAwdlPostedWindow=0; beAwdlWindowNativeStats[13]=w[17];
        if (announcedEnd) {
            // Native43 closes an actually announced window at its actual END.
            // Interrupted protection is not global sync loss, nor a TX success.
            if (w[26] && w[1]==0) {
                be_awdl::WindowEnd end={};
                end.firmwareTimeBE=be_awdl::big32(uint32_t(w[24]));
                end.observedInfraBeacons=iwx->beAwdlObservedInfraBeacons();
                ++beAwdlWindowNativeStats[3]; beAwdlWindowNativeStats[11]=w[24];
                vif->postMessage(be_awdl::EndMessage,&end,sizeof(end));
            } else { ++beAwdlWindowNativeStats[5]; clearNative(); }
            // cfg/wanted/samePlan predate the synchronous callback. Do not
            // request a new lease or transmit with these stale authorities.
            manager->release(); vif->release(); return;
        }
    }
    // A native plan/enable change closes software admission immediately.
    // Drain the already finite <=128TU co-channel lease to its actual END;
    // do not REMOVE/reuse the same ID or fabricate another native window.
    // Missing natural END still quarantines through the phase-specific poll.
    if (!wanted) { ++beAwdlWindowNativeStats[4]; if (!iwx->beAwdlCurrent()) clearNative(); }
    if (w[1]==4) { clearNative(); ++beAwdlWindowNativeStats[7]; }
    if (nativeInvalidated) { manager->release(); vif->release(); return; }
    if (w[16] && w[16]!=beAwdlWindowNativeStats[12]) {
        // No replay of a window that ended before native delivery, no success
        // from command ACK alone, no start for changed/native-incomplete state.
        uint64_t delivery=0; absolutetime_to_nanoseconds(mach_absolute_time(),&delivery);
        const bool withinLease=w[10] && delivery>=w[10] && delivery-w[10]<uint64_t(w[12])*1024000ULL;
        if (wanted && samePlan && w[1]==2 && w[25] && withinLease && !iwx->fBeAwdlWindowCommand) {
            const uint32_t gp2=uint32_t(w[23]);
            uint16_t sequence=0; bool knownSequence=false;
            if (!memcmp(cfg.master,cfg.self,6)) {
                sequence=uint16_t(gp2/(beAwdlNativeParams.availability_window_period*1024U));
                knownSequence=true; // local/master epoch, same GP2 convention as CPU composer
            } else {
                uint64_t now=0; absolutetime_to_nanoseconds(mach_absolute_time(),&now);
                itlwm_awdl::FollowState follow={}; uint16_t down=0;
                // Window clock holdover is not permission to emit stale Sync.
                // Native election expires a parent after ~3s (Legacy0x29bb2).
                // Stopping all41/43 at2s prevented that regular election path.
                // Use the existing bounded30s GP2 extrapolation for REAL
                // firmware windows; composer still requires a fresh2s anchor.
                if (beAwdlTransport.copyNativeAnchor(&follow,cfg.master,cfg.topMaster,now,
                        uint64_t(itlwm_awdl::kMaximumSyncExtrapolationUs)*1000ULL))
                    knownSequence=itlwm_awdl::TransportState::masterSyncAt(follow,gp2,&down,&sequence);
            }
            if (knownSequence && beAwdlVif==vif) {
                be_awdl::WindowStart start={};
                start.sequence=sequence; start.state=policy.state; // actual native SET134, never default2
                memcpy(start.master,cfg.master,6); memcpy(start.topMaster,cfg.topMaster,6);
                start.firmwareTimeBE=be_awdl::big32(gp2);
                start.channelVersion=APPLE80211_VERSION; start.channel=44;
                start.channelFlags=APPLE80211_C_FLAG_5GHZ;
                vif->postMessage(be_awdl::StartMessage,&start,sizeof(start));
                awdl_p9::NativeInputGates after={};
                if (awdl_p9::readNativeInputGates(manager,manager->getMetaClass()->getClassSize(),after)) {
                    beAwdlWindowNativeStats[20]=after.cd4; beAwdlWindowNativeStats[21]=after.flags8528;
                }
                absolutetime_to_nanoseconds(mach_absolute_time(),&beAwdlWindowNativeStats[25]);
                // Native postMessage can reenter VIF teardown. Never attach
                // the old manager's announcement to a new VIF publication.
                if (beAwdlVif==vif && beAwdlWindowPlanPublication==beAwdlStats[21]) {
                    beAwdlPostedWindow=w[16]; beAwdlWindowAnnounced=true;
                } else { beAwdlPostedWindow=0; beAwdlWindowAnnounced=false; ++beAwdlWindowNativeStats[7]; }
                ++beAwdlWindowNativeStats[2]; beAwdlWindowNativeStats[8]=policy.state;
                beAwdlWindowNativeStats[9]=sequence; beAwdlWindowNativeStats[10]=gp2;
            } else ++beAwdlWindowNativeStats[6];
        } else if (!w[25]) ++beAwdlWindowNativeStats[5];
        else ++beAwdlWindowNativeStats[7];
        beAwdlWindowNativeStats[12]=w[16];
    }
    uint64_t now=0; absolutetime_to_nanoseconds(mach_absolute_time(),&now);
    beAwdlWindowNativeStats[29]=now;
    if (wanted && samePlan && w[1]==2 && beAwdlVif==vif &&
        be_awdl::txWindow(w[1],w[10],w[12],now)) {
        // postMessage above is a native reentry boundary. The retained object
        // is safe to read, but its old plan/window is not current authority.
        OSObject *liveManager=nullptr;
        memcpy(&liveManager,reinterpret_cast<const uint8_t *>(vif)+0xb0,sizeof(liveManager));
        uint64_t live[32]; iwx->beAwdlWindowSnapshot(live);
        be_awdl::NativeConfig current={};
        const bool currentPlan=beAwdlVirtualEnabled && awdlSyncEnable &&
            beAwdlNativeSyncValid && beAwdlParamsValid && beAwdlSequenceValid &&
            beAwdlWindowPlanPublication==beAwdlStats[21] && liveManager==manager &&
            beAwdlWindowPlanSerial==beAwdlWindowNativeStats[15] &&
            live[13]==w[13] && live[14]==w[14] && live[15]==w[15] &&
            live[1]==2 && iwx->beAwdlCurrent() &&
            be_awdl::readConfig(manager,manager->getMetaClass()->getClassSize(),current) &&
            be_awdl::ready(current,policy) && current.monitorPeriod==cfg.monitorPeriod &&
            current.sequenceLength==cfg.sequenceLength && current.duplicateCount==cfg.duplicateCount &&
            !memcmp(current.self,cfg.self,6) && !memcmp(current.master,cfg.master,6) &&
            !memcmp(current.topMaster,cfg.topMaster,6);
        if (currentPlan) beAwdlSendAction(); // HAL still validates the live lease/ownership
        else iwx->beAwdlWindowEvent(10,beAwdlWindowNativeStats[15],beAwdlWindowPlanSerial,live[1]);
    }
    if (wanted && !invalidatedAnnounced && !w[1] && !iwx->beAwdlDataSetupPending() && !(iwx->beAwdlDataOptIn && beAwdlDataStopWanted) && now>=beAwdlNextWindowNs && beAwdlVif==vif) {
        beAwdlWindowPlanSerial=beAwdlWindowNativeStats[15];
        beAwdlWindowPlanPublication=beAwdlStats[21]; beAwdlWindowPlanConfig=cfg;
        beAwdlNextWindowNs=now+interval;
        const int err=iwx->beAwdlWindowRequest(duration);
        beAwdlWindowNativeStats[24]=uint32_t(err);
        if (err && err!=EAGAIN && err!=EBUSY) ++beAwdlWindowNativeStats[7];
    }
    manager->release(); vif->release();
#endif
}

// Exact Legacy0xb2664 passes the actual VIF to controller::bpfOutputPacket.
// DLT_RAW carries an AWDL Action body, not an infrastructure IO80211Interface.
// Copy while the caller owns the body; release its mbuf exactly once. A zero
// return acknowledges bounded host admission, NEVER firmware/peer delivery.
int AirportItlwm::beAwdlOutputAction(OSObject *object, mbuf_t packet)
{
    if (!packet) return 1;
    struct Call { OSObject *object; mbuf_t packet; int result; } call={object,packet,1};
    auto *hal=OSDynamicCast(ItlIwx,fHalService);
    auto *gate=hal ? hal->beAwdlCommandGate():nullptr;
    if (beAwdlEnabled && object && gate && !ml_at_interrupt_context()) {
        object->retain();
        const IOReturn entered=gate->attemptAction([](OSObject *owner,void *arg,void *,void *,void *) -> IOReturn {
            auto *self=static_cast<AirportItlwm *>(owner);
            auto *c=static_cast<Call *>(arg);
            auto *vif=OSDynamicCast(IO80211VirtualInterface,c->object);
            auto *iwx=OSDynamicCast(ItlIwx,self->fHalService);
            ++self->beAwdlTxStats[3];
            if (!self->beAwdlEnabled || !vif || vif!=self->beAwdlVif ||
                !self->beAwdlVirtualEnabled || self->beAwdlCancelBusy || !iwx || !iwx->beAwdlCurrent()) {
                ++self->beAwdlTxStats[5];return kIOReturnNotReady;
            }
            const size_t length=mbuf_pkthdr_len(c->packet);
            if (length<16 || length>be_awdl::NativeActions::MaximumBody) {
                ++self->beAwdlTxStats[6];return kIOReturnBadArgument;
            }
            // Admission is a separate producer, including during native
            // reentry. Never borrow the consumer's active Item as scratch:
            // its body, ticket and destination must remain one transaction.
            uint8_t body[be_awdl::NativeActions::MaximumBody];
            if (mbuf_copydata(c->packet,0,length,body) ||
                !self->beAwdlTransport.isAwdlActionBody(body,length)) {
                ++self->beAwdlTxStats[6];return kIOReturnBadArgument;
            }
            uint64_t w[32];iwx->beAwdlWindowSnapshot(w);
            if (w[1]==4 || w[1]==5) {++self->beAwdlTxStats[5];return kIOReturnNotReady;}
            uint64_t now=0;absolutetime_to_nanoseconds(mach_absolute_time(),&now);
            be_awdl::NativeActions::Key key={w[13],w[14],self->beAwdlStats[21],self->beAwdlWindowNativeStats[15],self->beAwdlStats[0],self->beAwdlStats[1]};
            self->beAwdlNativeActions.discardStale(key,now,self->beAwdlTxStats[11]);
            if (!self->beAwdlNativeActions.push(body,length,key,now,self->beAwdlTxStats[4]+1)) {
                ++self->beAwdlTxStats[7];return kIOReturnNoResources;
            }
            iwx->beAwdlWindowEvent(12,length,body[6],self->beAwdlTxStats[4]+1);
            ++self->beAwdlTxStats[4];self->beAwdlTxStats[8]=self->beAwdlNativeActions.pending();
            self->beAwdlTxStats[9]=length;self->beAwdlTxStats[10]=body[6];
            c->result=0;return kIOReturnSuccess;
        },&call);
        if (entered==kIOReturnCannotLock) __atomic_fetch_add(&beAwdlTxGateDrops,1ULL,__ATOMIC_RELAXED);
        object->release();
    }
    mbuf_freem(packet);
    return call.result;
}

void AirportItlwm::beAwdlRequestTx(OSObject *object,UInt flags)
{
    auto *hal=OSDynamicCast(ItlIwx,fHalService);
    auto *gate=hal ? hal->beAwdlCommandGate():nullptr;if (!object || !gate || ml_at_interrupt_context()) return;
    struct Call {OSObject *object;UInt flags;} call={object,flags};object->retain();
    const IOReturn entered=gate->attemptAction([](OSObject *owner,void *arg,void *,void *,void *) -> IOReturn {
        auto *self=static_cast<AirportItlwm *>(owner);auto *c=static_cast<Call *>(arg);
        auto *vif=OSDynamicCast(IO80211VirtualInterface,c->object);
        auto *iwx=OSDynamicCast(ItlIwx,self->fHalService);
        if (!self->beAwdlEnabled || !vif || vif!=self->beAwdlVif || !iwx || self->beAwdlCancelBusy ||
            self->beAwdlDataStopWanted || !self->beAwdlVirtualEnabled || !self->awdlSyncEnable || vif->isInactive()) return kIOReturnNotReady;
        const uint64_t previous=self->beAwdlTxStats[17];++self->beAwdlTxStats[16];
        self->beAwdlTxStats[17]=c->flags;
        self->beAwdlDataWanted|=c->flags&31;
        uint64_t prerequisites[16];iwx->beAwdlDataPrerequisites(prerequisites);
        const bool ready=iwx->beAwdlDataReady();
        const uint64_t mask=(ready?0:1)|(prerequisites[12]?0:2)|((ready && iwx->beAwdlDataQueueOwned(iwx->fBeAwdlDataQueue))?0:4);
        self->beAwdlTxStats[19]=mask;
        if(mask&5)++self->beAwdlTxStats[18];
        if(self->beAwdlTxStats[16]==1 || previous!=c->flags)
            iwx->beAwdlWindowEvent(15,c->flags,mask,self->beAwdlTxStats[16]);
        return (mask&5)?kIOReturnNotReady:kIOReturnSuccess; // schedules work, never a packet completion
    },&call);
    if (entered==kIOReturnCannotLock) __atomic_fetch_add(&beAwdlTxGateDrops,1ULL,__ATOMIC_RELAXED);
    object->release();
}

static uint64_t beAwdlDataNowNs() {uint64_t n=0;absolutetime_to_nanoseconds(mach_absolute_time(),&n);return n;}

IOReturn AirportItlwm::beAwdlOobRequest(IO80211VirtualInterface *vif,const apple80211_awdl_oob_request *request)
{
#if __IO80211_TARGET == __MAC_13_0
    ++beAwdlOobStats[3];beAwdlOobStats[11]=beAwdlDataNowNs();
    auto reject=[&](IOReturn result){++beAwdlOobStats[5];return result;};
    auto *iwx=OSDynamicCast(ItlIwx,fHalService);
    if(!request || !beAwdlEnabled || beAwdlCancelBusy || !vif || vif!=beAwdlVif || !getWorkLoop()->inGate() ||
       !beAwdlVirtualEnabled || !awdlSyncEnable || !iwx || !iwx->beAwdlCurrent())return reject(kIOReturnNotReady);
    // Exact Legacy sendUnicastMI0x41e0c writes destination+4, BSSID+22,
    // length+40 and borrowed CPU pointer+48 in its56-byte request. The scalar
    // at20 is always6 in that producer; do NOT interpret it as channel6 or
    // retune the infrastructure. This port remains explicitly cochannel44.
    const auto *raw=reinterpret_cast<const uint8_t *>(request);
    uint8_t destination[6],bssid[6],currentBssid[6];
    memcpy(destination,raw+4,6);memcpy(bssid,raw+22,6);beAwdlTransport.copyBssid(currentBssid);
    const bool broadcast=!memcmp(destination,"\xff\xff\xff\xff\xff\xff",6);
    uint8_t body[1216];const size_t bytes=be_awdl::NativeActions::miBody(body,sizeof(body),request->data,request->data_len);
    if(request->version>1 || (!broadcast && !be_awdl::unicast(destination)) || !memcmp(destination,beAwdlAddress,6) ||
       memcmp(bssid,currentBssid,6) || !bytes)return reject(kIOReturnBadArgument);
    OSObject *manager=nullptr;memcpy(&manager,reinterpret_cast<const uint8_t *>(vif)+0xb0,sizeof(manager));
    if(!manager || !manager->metaCast("IO80211AWDLPeerManager"))return reject(kIOReturnNotReady);
    for(const auto &v:beAwdlOob)if(v.ticket && !memcmp(v.destination,destination,6))return reject(kIOReturnBusy);
    BeAwdlOob *slot=nullptr;for(auto &v:beAwdlOob)if(!v.ticket){slot=&v;break;}
    if(!slot || beAwdlOobTicket==0x7fffffffffffffffULL)return reject(kIOReturnNoResources);
    uint64_t w[32];iwx->beAwdlWindowSnapshot(w);if(w[1]==4 || w[1]==5)return reject(kIOReturnNotReady);
    const uint64_t now=beAwdlDataNowNs(),ticket=(1ULL<<63)|++beAwdlOobTicket;
    const be_awdl::NativeActions::Key key={w[13],w[14],beAwdlStats[21],beAwdlWindowNativeStats[15],beAwdlStats[0],beAwdlStats[1]};
    beAwdlNativeActions.discardStale(key,now,beAwdlTxStats[11]);
    if(!beAwdlNativeActions.push(body,bytes,key,now,ticket,destination,bssid))return reject(kIOReturnNoResources);
    // Capacity/owned copies are real admission only. Message69 is delivered
    // later from an identified unicast TX result; no immediate/fake ACK.
    vif->retain();manager->retain();slot->vif=vif;slot->manager=manager;slot->ticket=ticket;
    slot->queuedNs=now;slot->publication=beAwdlStats[21];slot->generation=iwx->com.sc_generation;
    slot->epoch=iwx->fPc1ResetEpoch;memcpy(slot->destination,destination,6);
    ++beAwdlOobStats[4];beAwdlOobStats[12]=0;memcpy(beAwdlOobStats+12,destination,6);
    beAwdlOobStats[14]=ticket;
    return kIOReturnSuccess;
#else
    return kIOReturnUnsupported;
#endif
}

void AirportItlwm::beAwdlServiceOob()
{
#if __IO80211_TARGET == __MAC_13_0
    if(!beAwdlEnabled || !getWorkLoop()->inGate() || beAwdlOobBusy)return;
    auto *iwx=OSDynamicCast(ItlIwx,fHalService);if(!iwx)return;
    struct HalHold {ItlIwx *p;explicit HalHold(ItlIwx *v):p(v){p->retain();}~HalHold(){p->release();}} halHold(iwx);
    struct Busy {bool &b;Busy(bool &v):b(v){b=true;}~Busy(){b=false;}} busy(beAwdlOobBusy);
    be_awdl::ActionResults::Result result;
    while(iwx->fBeAwdlActionResults.pop(result))for(auto &v:beAwdlOob)
        if(v.ticket==result.ticket && v.submitted && v.generation==result.generation && v.epoch==result.epoch) {
            v.status=int(result.status);++beAwdlOobStats[7];break;
        }
    beAwdlOobStats[15]=0;
    for(auto &v:beAwdlOob)if(v.ticket) {
        const uint64_t now=beAwdlDataNowNs();OSObject *manager=nullptr;
        const bool same=v.vif==beAwdlVif && v.publication==beAwdlStats[21] &&
            v.generation==uint32_t(iwx->com.sc_generation) && v.epoch==iwx->fPc1ResetEpoch;
        if(same)memcpy(&manager,reinterpret_cast<const uint8_t *>(v.vif)+0xb0,sizeof(manager));
        const bool current=same && manager==v.manager && !v.vif->isInactive() &&
            beAwdlVirtualEnabled && awdlSyncEnable && iwx->beAwdlCurrent();
        const bool expired=now<v.queuedNs || now-v.queuedNs>(v.submitted?4000000000ULL:200000000ULL);
        if(current && !expired && v.status==-1){++beAwdlOobStats[15];continue;}
        const auto completed=v;v=BeAwdlOob{}; // native callback may submit another request
        if(current && !(completed.destination[0]&1)) {
            // REA reportDataPathEvents0x21214 case0x45 -> processUnicastAfStatus
            // reads MAC bytes0..5 and status byte6:0=ACK,nonzero=failure.
            // Consumers inspect that prefix; all bytes sent are initialized.
            struct {uint8_t address[6],failed;} __packed status={};
            static_assert(sizeof(status)==7,"exact Legacy UMI status consumer prefix");
            memcpy(status.address,completed.destination,6);
            const bool acked=completed.submitted && completed.status==IWX_TX_STATUS_SUCCESS;
            status.failed=acked?0:1;
            if(acked)++beAwdlOobStats[8];else ++beAwdlOobStats[9];
            beAwdlOobStats[13]=status.failed;
            completed.vif->postMessage(69,&status,sizeof(status));
        }
        if(expired || !current)++beAwdlOobStats[10];
        completed.manager->release();completed.vif->release();
    }
#endif
}

// Exact Legacy: peer::inputPacket0x11366 gates multicast on native PPP-done
// +0xce6, which pppDone0x12330 sets via native IOCTL wrapper0x8d42e.
// Observe only under the actual manager's recursive list lock. Never call
// pppDone, alter route/cache flags or infer a private identity success.
void AirportItlwm::beAwdlObserveAdmission(OSObject *manager,const uint8_t *address,unsigned event)
{
    if(!manager || !address || !manager->metaCast("IO80211AWDLPeerManager") ||
       manager->getMetaClass()->getClassSize()<0x378)return;
    IORecursiveLock *lock=nullptr;memcpy(&lock,reinterpret_cast<const uint8_t *>(manager)+0x370,sizeof(lock));
    if(!lock)return;
    IORecursiveLockLock(lock);
    auto *peer=static_cast<OSObject *>(kgpNativeFindAwdlPeer(manager,const_cast<uint8_t *>(address)));
    be_awdl::NativePeerEvidence p={};
    auto *t=beAwdlPeerAdmission;++t[3];
    // R14 registry proves a wifip2pd AsyncEventUserClient is attached. Native
    // AWDL routeEventViaBSD returns0; its async route additionally requires
    // this actual client and !controllerLostPower(+0x168). Read, never force.
    if(beAwdlVif){OSObject *current=nullptr;memcpy(&current,reinterpret_cast<const uint8_t *>(beAwdlVif)+0xb0,sizeof(current));
        if(current==manager && beAwdlVif->getMetaClass()->getClassSize()>=0x169){
            uintptr_t client=0;const auto *v=reinterpret_cast<const uint8_t *>(beAwdlVif);
            memcpy(&client,v+0xa0,sizeof(client));t[8]=1;t[9]=client!=0;t[10]=v[0x168];
            uint32_t role=0;memcpy(&role,v+0xc0,4);t[11]=role;t[12]=beAwdlDataNowNs();}}

    if(peer && peer->metaCast("IO80211AWDLPeer") && peer->getMetaClass()->getClassSize()>=0xce7 &&
       be_awdl::readNativePeer(peer,peer->getMetaClass()->getClassSize(),p) && !memcmp(p.address,address,6)) {
        uint64_t mac=0;memcpy(&mac,address,6);unsigned slot=0;
        for(;slot<4;++slot)if(t[16+12*slot]==mac)break;
        if(slot==4)slot=unsigned(t[4]++%4);
        auto *r=t+16+12*slot;const auto *raw=reinterpret_cast<const uint8_t *>(peer);
        r[0]=mac;r[1]=beAwdlDataNowNs();r[2]=event;r[3]=p.reachability;r[4]=raw[0xce6];
        r[5]=uint64_t(raw[0x4e3])|(uint64_t(raw[0x4e4])<<8)|(uint64_t(raw[0x4e5])<<16)|(uint64_t(raw[0x4e6])<<24)|(uint64_t(p.cached)<<32);
        r[6]=raw[0x1cc];r[7]=raw[0x1db];r[8]=p.overlap;memcpy(r+9,raw+0xa0,8);r[10]=beAwdlStats[21];
        r[11]=(beAwdlRadioGeneration<<32)|beAwdlRadioEpoch;
        if(event==2 && !r[4])++t[5]; // native PPP closed before data invocation, NOT a proven packet drop
        if(event==2 && r[3]!=1)++t[6];
    }else ++t[7];
    IORecursiveLockUnlock(lock);
}

// Read-only bounded transaction history. Native delete reasons are NOT in the
// cache IOCTL; do not infer them from a FW command or successful removal.
void AirportItlwm::beAwdlCacheEvent(unsigned event,const uint8_t *address,unsigned op,uint32_t result,uint64_t ticket)
{
    auto *t=beAwdlCacheTrace;const uint64_t count=++t[3];
    auto *r=t+8+12*((count-1)%16);memset(r,0,12*sizeof(*r));
    r[0]=event;r[1]=beAwdlDataNowNs();if(address)memcpy(r+2,address,6);
    r[3]=op;r[4]=result;r[5]=ticket;r[6]=beAwdlStats[21];
    auto *iwx=OSDynamicCast(ItlIwx,fHalService);
    if(iwx){r[7]=uint32_t(iwx->com.sc_generation);r[8]=iwx->fPc1ResetEpoch;
        for(unsigned i=0;i<4;++i)r[9]|=uint64_t(iwx->fBeAwdlDataPeers[i].state)<<(8*i);
        // Previously reserved evidence words: atomically observed in this
        // gate, not reconstructed from a later periodic ready sample.
        r[10]=uint64_t(iwx->beAwdlDataReady()) |
            (uint64_t(iwx->beAwdlCurrent())<<1) | (uint64_t(iwx->fBeAwdlPreparing)<<2) |
            (uint64_t(iwx->fBeAwdlDataPreparing)<<3) | (uint64_t(iwx->fBeAwdlError!=0)<<4) |
            (uint64_t(iwx->fBeAwdlDetail[6]!=0)<<5) | (uint64_t(iwx->beAwdlDataRemovalComplete())<<6) |
            (uint64_t((iwx->com.sc_flags&IWX_FLAG_BGSCAN)!=0)<<7) |
            (uint64_t((iwx->com.sc_flags&IWX_FLAG_SCANNING)!=0)<<8) |
            (uint64_t((iwx->com.sc_flags&IWX_FLAG_SHUTDOWN)!=0)<<9);
        r[11]=uint64_t(iwx->fBeAwdlDataStage&255) | (uint64_t(iwx->fBeAwdlDataPhyStage&255)<<8) |
            (uint64_t(iwx->fBeAwdlDataError)<<32);}
    // Keep the first genuine failed operation/callback even after ring rollover.
    if(!beAwdlCacheFault[3] && result && (event==3 || event==6) && result!=uint32_t(EBUSY)){
        beAwdlCacheFault[3]=r[1];beAwdlCacheFault[4]=event;beAwdlCacheFault[5]=op;
        beAwdlCacheFault[6]=result;beAwdlCacheFault[7]=r[2];beAwdlCacheFault[8]=ticket;
        beAwdlCacheFault[9]=r[6];beAwdlCacheFault[10]=r[7];beAwdlCacheFault[11]=r[8];
    }
    // A matching callback/publication is not proof of native cache acceptance.
    t[4]=r[1];t[5]=count%16;
}

IOReturn AirportItlwm::beAwdlCacheRequest(IO80211VirtualInterface *vif,const void *data)
{
#if __IO80211_TARGET == __MAC_13_0
    const uint8_t *traceAddress=nullptr;uint32_t traceOp=UINT32_MAX;
    auto reply=[&](IOReturn value){beAwdlTxStats[29]=uint32_t(value);
        if(value!=kIOReturnSuccess)beAwdlCacheEvent(2,traceAddress,traceOp,uint32_t(value),0);return value;};
    if(!data || !vif || vif!=beAwdlVif || beAwdlDataStopWanted || beAwdlCancelBusy || !getWorkLoop()->inGate())return reply(kIOReturnBadArgument);
    uint32_t version,op;memcpy(&version,data,4);memcpy(&op,static_cast<const uint8_t *>(data)+4,4);
    ++beAwdlTxStats[27];beAwdlTxStats[28]=op;traceOp=op;traceAddress=static_cast<const uint8_t *>(data)+8;
    if(version>1 || (op!=0 && op!=2))return reply(kIOReturnUnsupported);
    auto *iwx=OSDynamicCast(ItlIwx,fHalService);
    // Add requires current installation readiness. Delete can be queued while
    // this exact healthy lifetime owns its resources but RF admission is
    // temporarily closed. Existing serviceCache performs the real operation
    // later and posts51 only after resource-state/fence verification.
    if(!iwx || memcmp(iwx->fBeAwdlAddress,beAwdlAddress,6) ||
       !(op==2?iwx->beAwdlDataCanQueueRemoval():iwx->beAwdlDataReady()))return reply(kIOReturnNotReady);
    const auto *address=static_cast<const uint8_t *>(data)+8;
    if((address[0]&1)||!memcmp(address,"\0\0\0\0\0\0",6)||!memcmp(address,beAwdlAddress,6))return reply(kIOReturnBadArgument);
    // A successful return reserves an owned request and a bounded real peer
    // slot. No blocking firmware work or Message51 inside native locks.
    unsigned installed=0;for(const auto &p:iwx->fBeAwdlDataPeers)if(p.state)++installed;
    unsigned pendingAdds=0;for(const auto &r:beAwdlCache)if(r.state){if(!memcmp(r.bytes+8,address,6))return reply(kIOReturnBusy);uint32_t o;memcpy(&o,r.bytes+4,4);if(o==0)++pendingAdds;}
    if(op==0 && installed+pendingAdds>=4)return reply(kIOReturnNoResources);
    for(auto &r:beAwdlCache)if(!r.state){memcpy(r.bytes,data,sizeof(r.bytes));r.vif=vif;vif->retain();
        r.serial=++beAwdlCacheTicket;r.publication=beAwdlStats[21];r.generation=iwx->com.sc_generation;r.epoch=iwx->fPc1ResetEpoch;r.state=1;
        ++beAwdlDataNative[3];beAwdlTxStats[29]=0;beAwdlCacheEvent(1,address,op,0,r.serial);return reply(kIOReturnSuccess);}
    return reply(kIOReturnNoResources);
#else
    return kIOReturnUnsupported;
#endif
}
void AirportItlwm::beAwdlCancelNativeData(IO80211VirtualInterface *only)
{
    // Exact Legacy reportTransmitStatus0xb16e2 can wake native output and
    // enter peer completion logic. Withdraw admission BEFORE those callbacks,
    // so a newly submitted request cannot survive an already-completed scan
    // of the cache/OOB arrays. This is CPU cancellation, not a DMA fence.
    if(beAwdlCancelBusy)return;
    struct Cancel {bool &flag;Cancel(bool &f):flag(f){flag=true;}~Cancel(){flag=false;}}cancel(beAwdlCancelBusy);
    if(!only || only==beAwdlVif){beAwdlDataWanted=0;beAwdlMulticastDeferredSinceNs=0;}
    if(!only || only==beAwdlVif){beAwdlAutoMiValid=false;beAwdlAfTxModeKnown=false;}
    for(auto &r:beAwdlOob)if(r.ticket && (!only || r.vif==only)){
        const auto old=r;r=BeAwdlOob{};old.manager->release();old.vif->release();++beAwdlOobStats[10];}
    for(auto &r:beAwdlCache)if(r.state && (!only || r.vif==only)){
        auto *v=r.vif;r.state=0;r.vif=nullptr;if(v)v->release();++beAwdlDataNative[6];}
    for(auto &o:beAwdlOriginals)if(o.packet && (!only || o.vif==only)){
        auto *v=o.vif;auto packet=o.packet;o=BeAwdlOriginal{};
        if(v && v==beAwdlVif && !v->isInactive())v->reportTransmitStatus(packet,kIOReturnOutputDropped,nullptr);
        else mbuf_freem(packet);
        if(v)v->release();++beAwdlDataNative[17];}
}
void AirportItlwm::beAwdlServiceData()
{
#if __IO80211_TARGET == __MAC_13_0
    if(!beAwdlEnabled || !getWorkLoop()->inGate() || beAwdlDataBusy)return;
    auto *iwx=OSDynamicCast(ItlIwx,fHalService);if(!iwx)return;
    struct HalLifetime {ItlIwx *p;HalLifetime(ItlIwx *v):p(v){p->retain();}~HalLifetime(){p->release();}}halLifetime(iwx);
    struct Busy {bool &b;Busy(bool &v):b(v){b=true;}~Busy(){b=false;}}busy(beAwdlDataBusy);
    ++beAwdlDataNative[7];auto *vif=beAwdlVif;const uint64_t entryPublication=beAwdlStats[21];
    struct VifLifetime {IO80211VirtualInterface *p;VifLifetime(IO80211VirtualInterface *v):p(v){if(p)p->retain();}~VifLifetime(){if(p)p->release();}}vifLifetime(vif);
    for(auto &r:beAwdlCache)if(r.state && (r.generation!=uint32_t(iwx->com.sc_generation)||r.epoch!=iwx->fPc1ResetEpoch||r.vif!=vif)){
        auto *old=r.vif;r.state=0;r.vif=nullptr;if(old)old->release();++beAwdlDataNative[6];}
    for(auto &o:beAwdlOriginals)if(o.packet && (o.generation!=uint32_t(iwx->com.sc_generation)||o.epoch!=iwx->fPc1ResetEpoch||o.vif!=vif)){
        auto *target=o.vif;auto packet=o.packet;o=BeAwdlOriginal{};
        if(target==vif && !target->isInactive())target->reportTransmitStatus(packet,kIOReturnOutputDropped,nullptr);else mbuf_freem(packet);
        target->release();++beAwdlDataNative[17];}
    // Native completion is outside RX/firmware callbacks and consumes each
    // original exactly once. Unknown per-packet result is not success.
    auto finishOriginals=[&]() {
    ItlIwx::BeAwdlDataResult result;
    for(unsigned count=0;count<16 && iwx->beAwdlDataPopResult(result);++count){
        for(auto &o:beAwdlOriginals)if(o.packet && o.ticket==result.ticket){
            auto *target=o.vif;auto packet=o.packet;const bool current=target==beAwdlVif && !target->isInactive() && o.publication==beAwdlStats[21] && o.generation==uint32_t(iwx->com.sc_generation) && o.epoch==iwx->fPc1ResetEpoch;
            uint8_t destination[6]={};
            const bool hasDestination=!mbuf_copydata(packet,0,sizeof(destination),destination);
            static_assert(IWX_TX_STATUS_SUCCESS==0x01 && IWX_TX_STATUS_DIRECT_DONE==0x02,
                          "GL92 data completion status encoding");
            const bool success=current && be_awdl::dataTxSucceeded(result.status,hasDestination && (destination[0]&1));
            beAwdlDataCompletions.record(result.ticket,o.hostNs,beAwdlDataNowNs(),
                hasDestination?destination:nullptr,result.status,result.firmwareDetail,
                o.generation,o.epoch,current,success);
            o=BeAwdlOriginal{};
            if(current)target->reportTransmitStatus(packet,success?kIOReturnSuccess:kIOReturnOutputDropped,nullptr);
            else mbuf_freem(packet);
            ++beAwdlDataNative[success?14:15];target->release();break;}
    }
    };
    auto serviceCache=[&](bool stoppedOnly) {
    bool serviced=false;
    for(auto &r:beAwdlCache)if(r.state==1 || r.state==4){
        if(r.vif!=vif || r.publication!=beAwdlStats[21] || r.generation!=uint32_t(iwx->com.sc_generation) || r.epoch!=iwx->fPc1ResetEpoch){auto *old=r.vif;r=BeAwdlCacheRecord{};old->release();++beAwdlDataNative[6];break;}
        uint32_t op;memcpy(&op,r.bytes+4,4);
        if(stoppedOnly && op!=2)continue;
        serviced=true;
        uint8_t peer[6];memcpy(peer,r.bytes+8,6);const uint64_t serial=r.serial;
        if(r.state==1){
            r.state=2;auto *held=r.vif;held->retain();
            const int err=iwx->beAwdlDataPeer(peer,op==0);beAwdlDataNative[4]=uint32_t(err);beAwdlCacheEvent(3,peer,op,uint32_t(err),serial);
            if(r.state!=2 || r.serial!=serial || r.vif!=held){held->release();break;}
            held->release();r.lastError=uint32_t(err);
            if(err==EBUSY){r.state=1;break;}
            if(err){++beAwdlDataNative[5];r.state=3;break;} // ambiguous FW outcomes stay quarantined
            r.state=4; // actual resource operation complete; native callback still owed
        }
        // A completed operation can outlive a native manager or a whole-role
        // teardown/rebuild. Reapply the request if its real resource state was
        // invalidated; never report an add that no longer exists in firmware.
        if(!iwx->beAwdlDataPeerMatches(peer,op==0)){r.state=1;break;}
        // A removal flush may have retired DMA copies while this transaction
        // slept. Deliver those CPU-original results before native Message51
        // clears/unpauses/re-adds the old peer's queues (Legacy0x9f2bc).
        // Firmware flush/retirement is NOT the native packet completion.
        if(op==2)finishOriginals();
        if(r.state!=4 || r.serial!=serial || r.vif!=vif)break;
        if(!iwx->beAwdlDataPeerMatches(peer,op==0)){r.state=1;break;}
        // The exact Legacy handlePeerAddIndication(0x9ef02) may immediately
        // delete a peer; handlePeerDeleteIndication(0x9f2bc) may immediately
        // re-add it. Complete the owned CPU transaction BEFORE Message51.
        // Otherwise the synchronous next IOCTL for this MAC sees our old
        // state2 and is rejected Busy, stranding native pending-delete state.
        // This releases no firmware peer, queue, DMA object or packet credit.
        OSObject *pm=nullptr;memcpy(&pm,reinterpret_cast<const uint8_t *>(r.vif)+0xb0,sizeof(pm));
        if(!pm || !pm->metaCast("IO80211AWDLPeerManager")){
            if(r.lastError!=uint32_t(kIOReturnNotReady))beAwdlCacheEvent(7,peer,op,uint32_t(kIOReturnNotReady),serial);
            r.lastError=uint32_t(kIOReturnNotReady);break; // keep real completion, no fabricated native success
        }
        pm->retain();
        auto *target=r.vif;
        uint8_t notification[sizeof(r.bytes)];memcpy(notification,r.bytes,sizeof(notification));
        const uint64_t completedPublication=r.publication;
        const uint32_t completedGeneration=r.generation,completedEpoch=r.epoch;
        r=BeAwdlCacheRecord{}; // move its retained VIF ownership to this scope
        const uint32_t completed=op==2?3:1;memcpy(notification+4,&completed,4);
        beAwdlCacheEvent(4,peer,op,0,serial);
        if(target==beAwdlVif && completedPublication==beAwdlStats[21] && completedGeneration==uint32_t(iwx->com.sc_generation) && completedEpoch==iwx->fPc1ResetEpoch){
            target->postMessage(51,notification,sizeof(notification));++beAwdlDataNative[9];
            beAwdlCacheEvent(5,peer,op,0,serial);}
        else {++beAwdlDataNative[6];beAwdlCacheEvent(6,peer,op,uint32_t(kIOReturnAborted),serial);}
        pm->release();target->release();break;
    }
    return serviced;
    };
    finishOriginals();
    if(vif!=beAwdlVif || entryPublication!=beAwdlStats[21])return; // consuming native callbacks may reenter detach/recreate
    // R28: an accepted delete is still owed when native Sync switches off.
    // Full firmware teardown already removed every peer; waiting for a new
    // data role kept Legacy pending-delete set for16s/68s in the originals.
    // No add is completed here and no new firmware command is claimed.
    auto completeStoppedDelete=[&]() {
        if(!iwx->beAwdlDataRemovalComplete())return false;
        for(const auto &o:beAwdlOriginals)if(o.packet)return false;
        return serviceCache(true);
    };
    if(completeStoppedDelete())return; // Message51 can reenter policy/identity
    const uint64_t reasons=(!vif?1U:0U)|(!beAwdlVirtualEnabled?2U:0U)|(!awdlSyncEnable?4U:0U)|
        (memcmp(iwx->fBeAwdlAddress,beAwdlAddress,6)?8U:0U);
    auto *life=iwx->fBeAwdlDataLife;
    if(life[4]!=reasons || life[5]!=beAwdlWindowNativeStats[15] || life[6]!=beAwdlStats[21]){
        ++life[3];life[4]=reasons;life[5]=beAwdlWindowNativeStats[15];life[6]=beAwdlStats[21];life[7]=beAwdlDataNowNs();
        life[8]=iwx->com.sc_generation;life[9]=iwx->fPc1ResetEpoch;life[10]=iwx->fBeAwdlDataStage;
        // One bounded transition record in the existing ring; raw first-fault stays separate.
        iwx->beAwdlWindowEvent(16,reasons,life[5],0);
    }
    const bool nativeWanted=!reasons;
    if(!nativeWanted)beAwdlDataStopWanted=true;
    else if(iwx->beAwdlDataReady() || iwx->fBeAwdlDataStage==0 || iwx->fBeAwdlDataStage==8 || iwx->fBeAwdlDataEpoch!=iwx->fPc1ResetEpoch)
        beAwdlDataStopWanted=beAwdlDataStopTried=false;
    life[11]=beAwdlDataStopWanted;life[12]=beAwdlDataStopTried;
    if(beAwdlDataStopWanted){
        if(!beAwdlDataStopTried){life[13]=beAwdlDataNowNs();life[14]=reasons;
            if(!life[32] && iwx->fBeAwdlDataStage && iwx->fBeAwdlDataStage!=8 && iwx->fBeAwdlDataEpoch==iwx->fPc1ResetEpoch){life[32]=life[13];life[33]=reasons;life[34]=life[5];life[35]=life[6];life[36]=iwx->com.sc_generation;life[37]=iwx->fPc1ResetEpoch;life[38]=iwx->fBeAwdlDataStage;}const int e=iwx->beAwdlDataTeardown();beAwdlDataNative[27]=uint32_t(e);beAwdlDataStopTried=e!=EBUSY && e!=EAGAIN;life[15]=uint32_t(e);}
        // A qualified flush may have produced originals during teardown.
        // Consume them before a native deletion can unpause/re-add queues.
        finishOriginals();
        if(vif==beAwdlVif && entryPublication==beAwdlStats[21])completeStoppedDelete();
        return;
    }
    if(!vif || !beAwdlVirtualEnabled || !iwx->beAwdlCurrent())return;
    beAwdlDataNative[25]=beAwdlDataNative[28]=0;for(unsigned i=21;i<25;++i)beAwdlDataNative[i]=UINT64_MAX;
    OSObject *manager=nullptr;memcpy(&manager,reinterpret_cast<const uint8_t *>(vif)+0xb0,sizeof(manager));
    if(manager && manager->metaCast("IO80211AWDLPeerManager")){
        manager->retain();be_awdl::NativeDataState state;
        beAwdlDataNative[28]=manager->getMetaClass()->getClassSize();
        if(be_awdl::readNativeDataState(manager,beAwdlDataNative[28],state)){
            beAwdlDataNative[21]=state.mode;beAwdlDataNative[22]=state.available;beAwdlDataNative[23]=state.open;beAwdlDataNative[24]=state.presenceMode;beAwdlDataNative[25]=1;}
        manager->release();}
    beAwdlDataNative[26]=beAwdlDataNowNs();
    // Prepare actual firmware resources at a quiet lease boundary, after
    // native configuration is genuinely available, without forcing START/gates.
    if(!iwx->beAwdlDataReady() && (!beAwdlNativeSyncValid || !beAwdlParamsValid ||
       !beAwdlSequenceValid || !beAwdlWindowNativeStats[18]))return;
    const int prepared=iwx->beAwdlDataPrepare();beAwdlDataNative[8]=uint32_t(prepared);
    if(prepared || vif!=beAwdlVif || entryPublication!=beAwdlStats[21] || !beAwdlVirtualEnabled || !awdlSyncEnable)return;
    serviceCache(false);
    if(vif!=beAwdlVif || entryPublication!=beAwdlStats[21] || !beAwdlVirtualEnabled || !awdlSyncEnable)return;
    unsigned free=0;uint64_t oldest=0;for(const auto &o:beAwdlOriginals){if(!o.packet)++free;else if(!oldest||o.hostNs<oldest)oldest=o.hostNs;}
    beAwdlDataNative[18]=8-free;beAwdlDataNative[19]=oldest;
    // Native callbacks may change master/identity without a new firmware
    // generation. The old lease may drain, but must not admit old-plan data.
    auto nativePlanCurrent=[&]() {
        OSObject *pm=nullptr;memcpy(&pm,reinterpret_cast<const uint8_t *>(vif)+0xb0,sizeof(pm));
        be_awdl::NativeConfig cfg={};
        return pm && pm->metaCast("IO80211AWDLPeerManager") &&
            be_awdl::readConfig(pm,pm->getMetaClass()->getClassSize(),cfg) &&
            be_awdl::sameNativePlan(cfg,beAwdlWindowPlanConfig);
    };
    uint64_t w[32];iwx->beAwdlWindowSnapshot(w);
    if(!nativePlanCurrent() || !beAwdlWindowAnnounced || beAwdlPostedWindow!=w[16] || beAwdlWindowPlanSerial!=beAwdlWindowNativeStats[15] || !beAwdlDataWanted || !free || !be_awdl::txWindow(w[1],w[10],w[12],beAwdlDataNowNs()))return;
    // Native dequeue transfers packet ownership. Do not take packets from
    // Apple while the HAL already knows this native channel opportunity has
    // ended, even if the asynchronously started firmware lease is still live.
    if(!iwx->beAwdlRadioTxAllowed()) { iwx->beAwdlTxAdmissionEvent(1,0,EAGAIN);return; }
    const UInt wanted=beAwdlDataWanted;
    uint32_t gp2;
    if (wanted==16 && iwx->beAwdlClock(gp2)) {
        const uint64_t now=beAwdlDataNowNs();
        if (beAwdlTransport.deferMulticastRequest(now,gp2,beAwdlMulticastDeferredSinceNs,
                                                  wanted,beAwdlAddress)) {
            if (!beAwdlMulticastDeferredSinceNs) {
                beAwdlMulticastDeferredSinceNs=now;
                iwx->beAwdlWindowEvent(17,gp2,w[16],wanted);
            }
            return; // request stays pending; no mbuf or firmware resource taken
        }
    }
    beAwdlMulticastDeferredSinceNs=0;
    TxPacketRequest request={};beAwdlDataWanted=0;
    unsigned budget=free;
    if((wanted&16)&&budget){request.mM=1;--budget;}
    for(unsigned ac=0;ac<4&&budget;++ac)if(wanted&(1U<<ac)){request.packetCounts[ac]=1;++request.mU;--budget;}
    request.t=request.mU+request.mM;if(!request.t)return;
    vif->retain();const uint32_t generation=iwx->com.sc_generation,epoch=iwx->fPc1ResetEpoch;const uint64_t publication=beAwdlStats[21],control=beAwdlWindowNativeStats[15],window=w[15];
    const UInt32 returned=vif->dequeueTxPackets(&request);++beAwdlDataNative[10];beAwdlDataNative[11]=returned;beAwdlDataNative[20]=request.reqTx;beAwdlDataWanted|=request.reqTx&31;
    uint64_t after[32];iwx->beAwdlWindowSnapshot(after);
    for(unsigned list=0;list<8;++list){mbuf_t packet=request.bufs[list];
        while(packet){mbuf_t next=mbuf_nextpkt(packet);mbuf_setnextpkt(packet,nullptr);++beAwdlDataNative[12];
            BeAwdlOriginal *slot=nullptr;for(auto &o:beAwdlOriginals)if(!o.packet){slot=&o;break;}
            uint8_t ether[be_awdl::DataMaxNativeBytes]={};const size_t bytes=mbuf_pkthdr_len(packet);size_t length=0;
            const bool current=vif==beAwdlVif && nativePlanCurrent() && beAwdlVirtualEnabled && awdlSyncEnable && publication==beAwdlStats[21] && control==beAwdlWindowNativeStats[15] && control==beAwdlWindowPlanSerial && after[1]==2 && after[15]==window && generation==uint32_t(iwx->com.sc_generation)&&epoch==iwx->fPc1ResetEpoch&&iwx->beAwdlDataReady();
            uint8_t dataBssid[6];beAwdlTransport.copyBssid(dataBssid);
            if(slot && current && bytes<=sizeof(ether) && !mbuf_copydata(packet,0,bytes,ether))
                length=be_awdl::dataFrame(beAwdlDataTxBytes,sizeof(beAwdlDataTxBytes),ether,bytes,beAwdlAddress,++beAwdlSequence,dataBssid);
            if(length && !be_awdl::assignMulticastSequence(beAwdlDataTxBytes,length,
                unsigned(mbuf_get_traffic_class(packet)),beAwdlMulticastSequence))length=0;
            beAwdlDataNative[29]=bytes;beAwdlDataNative[30]=list;
            beAwdlDataNative[31]=(uint64_t(bytes>=14?((uint16_t(ether[12])<<8)|ether[13]):0)<<32)|length;
            if(length) {
                const auto prepared=iwx->fBeAwdlWire.record(be_awdl::WireEvidence::PreparedData,
                    beAwdlDataTxBytes,length,beAwdlDataNowNs(),iwx->fBeAwdlGp2,44,generation,epoch);
                iwx->fBeAwdlWire.complete(prepared,0,8); // constructed, not enqueued or ACKed
            }
            mbuf_t frame=length?allocatePacket(uint32_t(length)):nullptr;int err=EINVAL;
            if(frame){
                // As on the Action path, allocation is not a lifetime fence.
                // Re-read the lease AFTER allocation; the earlier snapshot
                // cannot authorize a prepared packet in a replaced native
                // plan, publication, firmware epoch or data role.
                uint64_t live[32];iwx->beAwdlWindowSnapshot(live);
                const bool admitted=vif==beAwdlVif && !vif->isInactive() &&
                    publication==beAwdlStats[21] && beAwdlVirtualEnabled && awdlSyncEnable &&
                    control==beAwdlWindowNativeStats[15] && control==beAwdlWindowPlanSerial &&
                    beAwdlWindowAnnounced && beAwdlPostedWindow==live[16] &&
                    live[15]==window && be_awdl::txWindow(live[1],live[10],live[12],beAwdlDataNowNs()) &&
                    generation==uint32_t(iwx->com.sc_generation) && epoch==iwx->fPc1ResetEpoch &&
                    iwx->beAwdlDataReady() && iwx->beAwdlRadioTxAllowed() && nativePlanCurrent() && slot && !slot->packet;
                if(!admitted){iwx->beAwdlTxAdmissionEvent(2,0,ESTALE,beAwdlDataTxBytes,length);mbuf_freem(frame);frame=nullptr;err=ESTALE;}
            }
            if(frame){if(!mbuf_copyback(frame,0,length,beAwdlDataTxBytes,MBUF_DONTWAIT)){
                    slot->packet=packet;slot->vif=vif;vif->retain();slot->ticket=++beAwdlDataTicket;slot->hostNs=beAwdlDataNowNs();slot->publication=publication;slot->generation=generation;slot->epoch=epoch;
                    err=iwx->beAwdlDataTransmit(frame,slot->ticket);
                    if(!err){++beAwdlDataNative[13];packet=next;continue;}
                    slot->vif->release();*slot=BeAwdlOriginal{};
                }else mbuf_freem(frame);}
            beAwdlDataNative[16]=uint32_t(err);++beAwdlDataNative[15];
            // A rejected frame is explicitly failed, never claimed transmitted.
            if(vif==beAwdlVif && !vif->isInactive())vif->reportTransmitStatus(packet,kIOReturnOutputDropped,nullptr);else mbuf_freem(packet);
            packet=next;
        }
    }
    vif->release();
#endif
}

void AirportItlwm::beAwdlSendAction()
{
    auto *iwx=OSDynamicCast(ItlIwx,fHalService);
    if (!iwx || !beAwdlEnabled || !getWorkLoop()->inGate() || beAwdlActionTxBusy || !beAwdlVif ||
        !beAwdlVirtualEnabled || !awdlSyncEnable || !beAwdlTransport.templateLength() ||
        !beAwdlParamsValid || !beAwdlSequenceValid || !awdlPresenceMode) return;
    // Leave native queued Actions and their throttle/sequence state untouched
    // until the same HAL radio admission used by actual TX is available.
    if(!iwx->beAwdlRadioTxAllowed()){iwx->beAwdlTxAdmissionEvent(6,0,EAGAIN);return;}
    struct Busy {bool &b;explicit Busy(bool &v):b(v){b=true;}~Busy(){b=false;}} busy(beAwdlActionTxBusy);
    struct HalHold {ItlIwx *p;explicit HalHold(ItlIwx *v):p(v){p->retain();}~HalHold(){p->release();}} halHold(iwx);
    auto *vif=beAwdlVif;vif->retain();
    struct VifHold {IO80211VirtualInterface *v;~VifHold(){v->release();}} vifHold{vif};
    const uint64_t publication=beAwdlStats[21];
    uint64_t now=0; absolutetime_to_nanoseconds(mach_absolute_time(),&now);
    OSObject *policyManager=nullptr;be_awdl::NativeConfig policy={};
    memcpy(&policyManager,reinterpret_cast<const uint8_t *>(vif)+0xb0,sizeof(policyManager));
    if(!policyManager || !policyManager->metaCast("IO80211AWDLPeerManager") ||
       !be_awdl::readConfig(policyManager,policyManager->getMetaClass()->getClassSize(),policy))return;
    const uint32_t afMode=beAwdlAfTxModeKnown?beAwdlAfTxMode:policy.afTxMode;
    if(afMode!=0 && afMode!=3)return; // unknown policy, never assume permission
    uint8_t currentBssid[6];beAwdlTransport.copyBssid(currentBssid);
    const bool autoCurrent=beAwdlAutoMiValid && beAwdlAutoMi.key.generation==uint32_t(iwx->com.sc_generation) &&
        beAwdlAutoMi.key.epoch==iwx->fPc1ResetEpoch && beAwdlAutoMi.key.publication==publication &&
        !memcmp(beAwdlAutoMi.bssid,currentBssid,6);
    const unsigned selected=be_awdl::NativeActions::select(now,beAwdlLastActionNs,
        beAwdlLastNativeActionNs,beAwdlNativeActions.pending() || autoCurrent,beAwdlNativeActions.hasImmediate(),afMode==0);
    if (!selected) return;
    if (selected==1) beAwdlLastActionNs=now; // preserve periodic Sync even with native backlog
    uint32_t gp2;
    if (!iwx->beAwdlClock(gp2)) {
        if (selected==2) beAwdlLastNativeActionNs=now;
        ++beAwdlStats[13]; return;
    }
    beAwdlStats[16]=gp2;
    beAwdlTransport.setSyncEnabled(awdlSyncEnable); beAwdlTransport.setSyncState(awdlSyncState);
    beAwdlTransport.setOverrideChannel(44);
    uint64_t window[32];iwx->beAwdlWindowSnapshot(window);
    const be_awdl::NativeActions::Key key={window[13],window[14],beAwdlStats[21],beAwdlWindowNativeStats[15],beAwdlStats[0],beAwdlStats[1]};
    bool native=selected==2 && beAwdlNativeActions.pop(beAwdlNativeAction,key,now,beAwdlTxStats[11]);
    const bool repeatingMi = selected==2 && !native && autoCurrent;
    beAwdlTxStats[8]=beAwdlNativeActions.pending();
    if (repeatingMi && beAwdlTransport.deferRepeatingMi(now,gp2,beAwdlLastNativeActionNs,beAwdlAddress)) {
        ++beAwdlFlowStats[31]; // observations at eligible service ticks, not dropped frames
        return; // never delays a real native queued Action, UMI or data
    }
    if(selected==2) beAwdlLastNativeActionNs=now; // throttle attempts, not deferrals
    if(repeatingMi) {
        beAwdlNativeAction=beAwdlAutoMi;beAwdlNativeAction.ticket=++beAwdlAutoMiTicket;native=true;
    }
    if (selected==2 && !native) return; // never manufacture a second periodic Sync
    if(native && beAwdlNativeAction.oob) {
        bool live=false;for(const auto &v:beAwdlOob)if(v.ticket==beAwdlNativeAction.ticket &&
            v.vif==beAwdlVif && v.publication==beAwdlStats[21]){live=true;break;}
        if(!live)return; // a cancelled/detached request must never be transmitted
    }
    size_t length=0;
    if (native) {
        // Management injection needs no invented data peer: Linux6.12 tx.c
        // uses the P2P_DEVICE's BCAST_MGMT station for non-data frames.
        // Keep native TLVs/subtype and the request's actual destination.
        uint8_t bssid[6];beAwdlTransport.copyBssid(bssid);
        length=be_awdl::NativeActions::frame(beAwdlTxBytes,sizeof(beAwdlTxBytes),
            beAwdlNativeAction,beAwdlAddress,bssid,uint16_t(beAwdlSequence++<<4),gp2);
        be_awdl::NativeConfig cfg={};OSObject *manager=nullptr;
        memcpy(&manager,reinterpret_cast<const uint8_t *>(beAwdlVif)+0xb0,sizeof(manager));
        if(!manager || !manager->metaCast("IO80211AWDLPeerManager") ||
           !be_awdl::readConfig(manager,manager->getMetaClass()->getClassSize(),cfg) ||
           memcmp(cfg.self,beAwdlAddress,6) ||
           !beAwdlTransport.nativePlanMatches(beAwdlTxBytes,length,cfg.master,cfg.topMaster)) {
            length=0;++beAwdlTxStats[11]; // stale queued plan, not a failed firmware transmission
        } else if (repeatingMi && beAwdlNativeAction.needsMiTiming) {
            length=beAwdlTransport.completeNativeMi(beAwdlTxBytes,length,sizeof(beAwdlTxBytes),
                gp2,cfg.master,cfg.self,now,cfg.topMaster);
        }
        ++beAwdlTxStats[12];
    } else {
        // The native plan and emitted election must name the same master.
        // Observed peer timing is admitted only for that fresh native master.
        be_awdl::NativeConfig cfg={};
        OSObject *manager=nullptr;memcpy(&manager,reinterpret_cast<const uint8_t *>(beAwdlVif)+0xb0,sizeof(manager));
        ++beAwdlFlowStats[3];
        if(manager && manager->metaCast("IO80211AWDLPeerManager") &&
           be_awdl::readConfig(manager,manager->getMetaClass()->getClassSize(),cfg)) {
            const bool local=!memcmp(cfg.master,cfg.self,6);
            ++beAwdlFlowStats[local?4:5];
            length=beAwdlTransport.composeNativeActionFrame(beAwdlTxBytes,sizeof(beAwdlTxBytes),
                beAwdlAddress,uint16_t(beAwdlSequence++<<4),gp2,cfg.master,cfg.self,now,cfg.topMaster);
            beAwdlFlowStats[7]=local;beAwdlFlowStats[8]=now;
            memcpy(&beAwdlFlowStats[9],cfg.master,6);memcpy(&beAwdlFlowStats[10],cfg.self,6);
        }
        if(!length)++beAwdlFlowStats[6];
        beAwdlFlowStats[11]=length;
    }
    if (!length || length>sizeof(beAwdlRxFrame.bytes)) { ++beAwdlStats[9]; return; }
    // R29_DPSTATE_TEST_BEGIN: optional advertisement experiment, not a gate.
    // Native objects and OOB/UMI requests remain untouched. This edits only
    // the completed own broadcast copy, before evidence/allocation/handoff.
    if(beAwdlDpStateNoInfra && !(native && beAwdlNativeAction.oob)) {
        be_awdl::DpStateTestInfo edit;
        const size_t before=length;
        const auto outcome=be_awdl::omitDpStateInfra(beAwdlTxBytes,sizeof(beAwdlTxBytes),length,beAwdlAddress,edit);
        ++beAwdlDpStateTest[4];
        if(outcome==be_awdl::DpStateTestResult::Changed)++beAwdlDpStateTest[5];
        else if(outcome==be_awdl::DpStateTestResult::Unchanged || outcome==be_awdl::DpStateTestResult::NotEligible)++beAwdlDpStateTest[6];
        else ++beAwdlDpStateTest[7]; // refuse only the edit; preserve the original frame
        beAwdlDpStateTest[8]=uint32_t(outcome);
        beAwdlDpStateTest[9]=edit.beforeFlags;beAwdlDpStateTest[10]=edit.afterFlags;
        beAwdlDpStateTest[11]=before;beAwdlDpStateTest[12]=length;
        beAwdlDpStateTest[13]=edit.beforeValueBytes;beAwdlDpStateTest[14]=edit.afterValueBytes;
        beAwdlDpStateTest[15]=now;
    }
    // R29_DPSTATE_TEST_END
    const auto prepared=iwx->fBeAwdlWire.record(be_awdl::WireEvidence::PreparedAction,
        beAwdlTxBytes,length,now,gp2,44,beAwdlRadioGeneration,beAwdlRadioEpoch);
    iwx->fBeAwdlWire.complete(prepared,0,8); // CPU composition, no allocation/FW proof
    mbuf_t action=allocatePacket(length);
    if (!action) { ++beAwdlStats[8]; return; }
    if (mbuf_copyback(action,0,length,beAwdlTxBytes,MBUF_DONTWAIT)) {
        mbuf_freem(action); ++beAwdlStats[8]; return;
    }
    // Allocation may reenter VIF/plan teardown. Neither retained objects nor
    // a prepared CPU frame authorize transmission in a replaced lifetime.
    uint64_t live[32];iwx->beAwdlWindowSnapshot(live);
    const be_awdl::NativeActions::Key liveKey={live[13],live[14],beAwdlStats[21],beAwdlWindowNativeStats[15],beAwdlStats[0],beAwdlStats[1]};
    bool requestCurrent=true;
    if(native && beAwdlNativeAction.oob) {
        requestCurrent=false;for(const auto &v:beAwdlOob)if(v.ticket==beAwdlNativeAction.ticket &&
            v.vif==vif && v.publication==publication && !v.submitted &&
            v.generation==live[13] && v.epoch==live[14]){requestCurrent=true;break;}
    }
    OSObject *liveManager=nullptr;be_awdl::NativeConfig livePolicy={};
    if(vif==beAwdlVif)memcpy(&liveManager,reinterpret_cast<const uint8_t *>(vif)+0xb0,sizeof(liveManager));
    const bool livePlan=liveManager && liveManager->metaCast("IO80211AWDLPeerManager") &&
        be_awdl::readConfig(liveManager,liveManager->getMetaClass()->getClassSize(),livePolicy) &&
        !memcmp(livePolicy.self,beAwdlAddress,6) &&
        beAwdlTransport.nativePlanMatches(beAwdlTxBytes,length,livePolicy.master,livePolicy.topMaster);
    if(vif!=beAwdlVif || vif->isInactive() || publication!=beAwdlStats[21] ||
       !beAwdlVirtualEnabled || !awdlSyncEnable || !requestCurrent || !livePlan || !be_awdl::NativeActions::same(key,liveKey)) {
        mbuf_freem(action);++beAwdlTxStats[11];return;
    }
    const int err=iwx->beAwdlTransmit(action,native ? beAwdlNativeAction.ticket:0); // consumes every result, authoritative lease admission
    if(native && beAwdlNativeAction.oob)for(auto &v:beAwdlOob)if(v.ticket==beAwdlNativeAction.ticket) {
        if(err)v.status=-2;else{v.submitted=true;++beAwdlOobStats[6];}break;
    }
    beAwdlStats[3]=uint32_t(err); if (!err) ++beAwdlStats[4];
    if (native) {++beAwdlTxStats[err ? 14 : 13];beAwdlTxStats[15]=uint32_t(err);
        iwx->beAwdlWindowEvent(13,beAwdlNativeAction.ticket,uint32_t(err),length);}
}

void AirportItlwm::beAwdlReceiveData()
{
    auto *iwx=OSDynamicCast(ItlIwx,fHalService);
    if (!beAwdlEnabled || !iwx || !getWorkLoop()->inGate() || beAwdlDataInputBusy) return;
    struct HalHold {ItlIwx *p;explicit HalHold(ItlIwx *v):p(v){p->retain();}~HalHold(){p->release();}} halHold(iwx);
    struct Busy {bool &value;Busy(bool &v):value(v){value=true;}~Busy(){value=false;}} busy(beAwdlDataInputBusy);
#if __IO80211_TARGET == __MAC_13_0
    for (unsigned budget=0;budget<8 && iwx->beAwdlPopData(beAwdlRxFrame);++budget) {
        ++beAwdlTxStats[21];
        auto *vif=beAwdlVif;
        const uint64_t publication=beAwdlStats[21];
        uint64_t now=0;absolutetime_to_nanoseconds(mach_absolute_time(),&now);
        if (!vif || !beAwdlVirtualEnabled || now<beAwdlRxFrame.hostNs ||
            now-beAwdlRxFrame.hostNs>200000000ULL) {++beAwdlTxStats[23];continue;}
        // Exact Legacy inputPacket consumes the copy and can call back into
        // cache/driver state. Hold both owners BEFORE allocation as well:
        // allocatePacket itself is a reentry boundary, not an ownership fence.
        struct VifHold {IO80211VirtualInterface *p;
            explicit VifHold(IO80211VirtualInterface *v):p(v){p->retain();}
            ~VifHold(){p->release();}
        } hold(vif);
        uint8_t bssid[6];beAwdlTransport.copyBssid(bssid);
        const size_t length=be_awdl::nativeData(beAwdlDataRxBytes,sizeof(beAwdlDataRxBytes),
            beAwdlRxFrame.bytes,beAwdlRxFrame.length,beAwdlAddress,bssid);
        if (!length) {++beAwdlTxStats[23];continue;}
        OSObject *manager=nullptr;
        memcpy(&manager,reinterpret_cast<const uint8_t *>(vif)+0xb0,sizeof(manager));
        awdl_p9::NativeInputGates native={};
        beAwdlTxStats[30]=UINT64_MAX;beAwdlTxStats[31]=now;
        if (!manager || !manager->metaCast("IO80211AWDLPeerManager") ||
            !awdl_p9::readNativeInputGates(manager,manager->getMetaClass()->getClassSize(),native)) {
            ++beAwdlTxStats[23];continue; // never bypass a missing/unknown peer manager
        }
        beAwdlTxStats[30]=native.flags8528; // fresh pre-input value, not an acceptance flag
        manager->retain();
        uint64_t radio[16];iwx->beAwdlSnapshot(radio);
        mbuf_t copy=allocatePacket(length);
        if (!copy) {manager->release();++beAwdlTxStats[24];continue;}
        if (mbuf_copyback(copy,0,length,beAwdlDataRxBytes,MBUF_DONTWAIT)) {
            mbuf_freem(copy);manager->release();++beAwdlTxStats[24];continue;
        }
        OSObject *currentManager=nullptr;
        if(beAwdlVif==vif)
            memcpy(&currentManager,reinterpret_cast<const uint8_t *>(vif)+0xb0,sizeof(currentManager));
        uint64_t currentRadio[16];iwx->beAwdlSnapshot(currentRadio);
        if(beAwdlVif!=vif || publication!=beAwdlStats[21] || !beAwdlVirtualEnabled ||
           !iwx->beAwdlCurrent() || currentManager!=manager ||
           radio[4]!=currentRadio[4] || radio[5]!=currentRadio[5]) {
            mbuf_freem(copy);manager->release();++beAwdlTxStats[23];break;
        }
        const auto evidence=iwx->fBeAwdlWire.record(be_awdl::WireEvidence::NativeRxData,
            beAwdlDataRxBytes,length,now,beAwdlRxFrame.gp2,beAwdlRxFrame.channel,
            radio[4],radio[5]);
        beAwdlObserveAdmission(manager,beAwdlDataRxBytes+6,2);
        uint8_t nativeSource[6];memcpy(nativeSource,beAwdlDataRxBytes+6,6);
        kgpNativeAwdlData(vif,copy,nullptr); // consumes: never caller-free afterward
        if(beAwdlVif==vif && publication==beAwdlStats[21])beAwdlObserveAdmission(manager,nativeSource,3);
        iwx->fBeAwdlWire.complete(evidence,0,4); // invocation only, not native acceptance
        ++beAwdlTxStats[22];
        if (beAwdlTxStats[22]<=4) iwx->beAwdlWindowEvent(14,length,beAwdlTxStats[22],0);
        manager->release(); // VIF guard releases after the consuming callback
        if(beAwdlVif!=vif || publication!=beAwdlStats[21])break;
    }
    beAwdlTxStats[25]=iwx->fBeAwdlDataCopied;beAwdlTxStats[26]=iwx->fBeAwdlDataOverflow;
#endif
}

// REA ac2dc15b actionFrameProcessInput is synchronous on the controller's
// native loop; its parser/timeline/peer state must not wait for the unrelated
// 5Hz context/publication service. R11/R12 exact GP2/MAC/length joins measured
// ingress->native delays up to222ms. Preserve raw arrival GP2 and ownership;
// remove the fixed polling delay, but measure actual latency on hardware. No
// peer admission, radio delivery or AirDrop result follows from this change.
void AirportItlwm::beAwdlReceiveActions()
{
    auto *iwx=OSDynamicCast(ItlIwx,fHalService);
    if(!beAwdlEnabled || !iwx || !getWorkLoop()->inGate() || !iwx->beAwdlCurrent())return;
    if(beAwdlStepBusy || !beAwdlVif || !beAwdlVirtualEnabled || !awdlSyncEnable) {
        ++beAwdlRxServiceStats[4];return;
    }
    uint64_t radio[16];iwx->beAwdlSnapshot(radio);
    if(radio[4]!=beAwdlRadioGeneration || radio[5]!=beAwdlRadioEpoch) {
        ++beAwdlRxServiceStats[4];return; // context service must reconcile first
    }
    beAwdlStepBusy=true;const uint64_t serviceStart=beAwdlDataNowNs();
    struct Done {bool &busy;uint64_t *stats;uint64_t start;
        ~Done(){const uint64_t now=beAwdlDataNowNs();stats[13]=now>=start?now-start:0;
            if(stats[13]>stats[11])stats[11]=stats[13];busy=false;}
    } done{beAwdlStepBusy,beAwdlRxServiceStats,serviceStart};
    ++beAwdlRxServiceStats[3];
    {
        // Bounded CPU mailbox. Nothing here retains an RX descriptor or credit.
        for (unsigned i = 0; i < 4; ++i) {
            // Do not keep a CPU burst ahead of native timers or infrastructure.
            // A single native call is non-preemptible; bound the next dequeue.
            if (beAwdlDataNowNs()-serviceStart>=2000000ULL) {
                ++beAwdlRxServiceStats[12]; break;
            }
            if (!beAwdlVif || !beAwdlVirtualEnabled) break;
            if (!iwx->beAwdlPop(beAwdlActionRxFrame)) break;
            ++beAwdlRxServiceStats[5];
            const auto &frame = beAwdlActionRxFrame;
            // Native PeerManager keeps a raw VIF pointer (REA base init
            // 0x5ee2e,+0xb8). Retaining the manager does not itself establish
            // our VIF ownership across callbacks; mirror the data/window path.
            auto *vif=beAwdlVif;
            struct VifHold {IO80211VirtualInterface *v;
                explicit VifHold(IO80211VirtualInterface *p):v(p){v->retain();}
                ~VifHold(){v->release();}
            } hold(vif);
            be_awdl::NativeConfig selected={}; bool selectedValid=false;
#if __IO80211_TARGET == __MAC_13_0
            OSObject *selectionManager=nullptr;
            memcpy(&selectionManager,reinterpret_cast<const uint8_t *>(vif)+0xb0,sizeof(selectionManager));
            selectedValid=selectionManager && selectionManager->metaCast("IO80211AWDLPeerManager") &&
                be_awdl::readConfig(selectionManager,selectionManager->getMetaClass()->getClassSize(),selected) &&
                !memcmp(selected.self,beAwdlAddress,6);
#endif
            const auto peer = beAwdlTransport.observe80211(frame.bytes, frame.length,
                uint8_t(frame.channel), int8_t(frame.rssi), frame.gp2, frame.hostNs,
                selectedValid?selected.master:nullptr,selectedValid?selected.topMaster:nullptr);
            if (!peer.valid) { ++beAwdlStats[6]; continue; }
            ++beAwdlStats[7]; beAwdlStats[17] = frame.gp2;
#if __IO80211_TARGET == __MAC_13_0
            // The same +0xb0 peer pointer used by native bpfTapInput; read only.
            // No PM offsets, fake presence, forced availability or object writes.
            OSObject *manager = nullptr;
            memcpy(&manager, reinterpret_cast<const uint8_t *>(vif) + 0xb0, sizeof(manager));
            if (!manager || !manager->metaCast("IO80211AWDLPeerManager")) {
                ++beAwdlStats[12]; continue;
            }
            manager->retain(); // hold both native owners before allocation/callbacks
            mbuf_t body = allocatePacket(frame.length - 24);
            if (!body) { manager->release(); ++beAwdlStats[8]; continue; }
            if (mbuf_copyback(body, 0, frame.length - 24, frame.bytes + 24, MBUF_DONTWAIT)) {
                manager->release();mbuf_freem(body); ++beAwdlStats[8]; continue;
            }
            if(beAwdlVif!=vif || !beAwdlVirtualEnabled || !awdlSyncEnable || !iwx->beAwdlCurrent()) {
                ++beAwdlRxServiceStats[4];manager->release();mbuf_freem(body);break;
            }
            KgpAwdlActionMetadata metadata = {};
            memcpy(metadata.source, frame.bytes + 10, 6);
            metadata.channel.version = APPLE80211_VERSION;
            metadata.channel.channel = frame.channel;
            metadata.channel.flags = APPLE80211_C_FLAG_5GHZ; // band known, no invented PHY rate/width
            metadata.rssi = frame.rssi; metadata.gp2 = frame.gp2;
            ++beAwdlNativeStats[1];
            const size_t nativeSize = manager->getMetaClass()->getClassSize();
            awdl_p9::NativeInputGates gates = {};
            if (awdl_p9::readNativeInputGates(manager, nativeSize, gates)) {
                ++beAwdlNativeStats[2];
                if (!gates.cd4) ++beAwdlNativeStats[4];
                if (!(gates.flags8528 & 2)) ++beAwdlNativeStats[5];
                beAwdlNativeStats[6] = gates.cd4;
                beAwdlNativeStats[7] = gates.flags8528;
                beAwdlNativeStats[8] = gates.bypass8950;
            } else ++beAwdlNativeStats[3];
            beAwdlNativeStats[9] = nativeSize;
            beAwdlNativeStats[10] = frame.length - 24;
            beAwdlNativeStats[11] = mbuf_next(body) != nullptr;
            beAwdlNativeStats[12] = uint32_t(frame.rssi);
            absolutetime_to_nanoseconds(mach_absolute_time(), &beAwdlNativeStats[14]);
            const uint64_t invoke=beAwdlDataNowNs();
            ++beAwdlRxServiceStats[6];
            if(invoke>=frame.hostNs) {
                beAwdlRxServiceStats[7]=invoke-frame.hostNs;
                if(beAwdlRxServiceStats[7]>beAwdlRxServiceStats[8])
                    beAwdlRxServiceStats[8]=beAwdlRxServiceStats[7];
            } else {++beAwdlRxServiceStats[14];beAwdlRxServiceStats[7]=UINT64_MAX;}
            beAwdlRxServiceStats[9]=frame.gp2;
            beAwdlRxServiceStats[10]=0;memcpy(beAwdlRxServiceStats+10,frame.bytes+10,6);
            const uint64_t serviceBefore=beAwdlServiceWord(manager,metadata.source);
            const uint64_t servicePublication=beAwdlStats[21];
            const uint32_t serviceGeneration=uint32_t(iwx->com.sc_generation),serviceEpoch=iwx->fPc1ResetEpoch;
            const bool returnedTrue = kgpNativeAwdlProcess(manager, body, DLT_RAW, &metadata, sizeof(metadata));
            beAwdlServiceEvidence.record(frame.bytes,frame.length,frame.hostNs,beAwdlDataNowNs(),
                servicePublication,serviceGeneration,serviceEpoch,serviceBefore,
                beAwdlServiceWord(manager,metadata.source),returnedTrue);
            beAwdlObserveAdmission(manager,metadata.source,1);
            ++beAwdlFlowStats[12];
            // REA ac2dc15b findPeer 0x66af8 and base init 0x5ee2e:
            // its list is protected by this recursive native lock. Retain the
            // manager and hold the same lock across lookup and bounded reads;
            // never retain a borrowed peer or call its assessment routines.
            IORecursiveLock *peerLock=nullptr;
            if(nativeSize>=0x378)
                memcpy(&peerLock,reinterpret_cast<const uint8_t *>(manager)+0x370,sizeof(peerLock));
            bool peerPresent=false; ++beAwdlPeerStats[3];
            if(peerLock && nativeSize>=0x378) {
                IORecursiveLockLock(peerLock);
                auto *object=static_cast<OSObject *>(kgpNativeFindAwdlPeer(manager,metadata.source));
                peerPresent=object!=nullptr;
                be_awdl::NativePeerEvidence p={};
                if(object && object->metaCast("IO80211AWDLPeer") &&
                   be_awdl::readNativePeer(object,object->getMetaClass()->getClassSize(),p)) {
                    if(!memcmp(p.address,metadata.source,6)) {
                        ++beAwdlPeerStats[4];
                        uint64_t mac=0;memcpy(&mac,p.address,6);
                        unsigned slot=0;
                        for(;slot<4;++slot)if(beAwdlPeerStats[16+12*slot]==mac)break;
                        if(slot==4)slot=unsigned(beAwdlPeerStats[10]++%4);
                        auto *r=beAwdlPeerStats+16+12*slot;
                        r[0]=mac;r[1]=beAwdlNativeStats[14];r[2]=p.reachability;
                        r[3]=p.syncFrames;r[4]=p.observations;r[5]=p.posted;
                        r[6]=p.pending;r[7]=p.cached;r[8]=p.overlap;r[9]=p.sequence;
                        r[10]=uint64_t(p.version)|(uint64_t(p.parsedSequence)<<8);
                        r[11]=(beAwdlRadioGeneration<<32)|beAwdlRadioEpoch;
                    } else ++beAwdlPeerStats[5]; // default/other-address object, not admission
                } else ++beAwdlPeerStats[6]; // absent or unknown class/layout
                IORecursiveLockUnlock(peerLock);
            } else ++beAwdlPeerStats[7];
            beAwdlPeerStats[8]=beAwdlNativeStats[14];
            beAwdlPeerStats[9]=(beAwdlRadioGeneration<<32)|beAwdlRadioEpoch;
            ++beAwdlFlowStats[peerPresent?13:14];
            beAwdlFlowStats[15]=uint32_t(metadata.rssi);beAwdlFlowStats[16]=frame.length;
            beAwdlFlowStats[17]=0;memcpy(&beAwdlFlowStats[17],metadata.source,6);
            beAwdlFlowStats[18]=beAwdlNativeStats[14];
            beAwdlNativeStats[13] = returnedTrue;
            manager->release();
            // REA: original is a synchronous borrow. Boolean drives the native
            // caller's optional BPF tap; false is NOT a validation reject and
            // true is NOT a peer/session acceptance. Keep neutral raw counts.
            mbuf_freem(body);
            ++beAwdlStats[returnedTrue ? 10 : 11];
#endif
        }
    }
}

// Exact Legacy ac2dc15b findPeer0x66af8/list-lock+370 and service_response
// SUI+752, changed+891, offload-count+8a0. Copy while locked; never retain a
// peer, manager or VIF beyond the existing RX call, or mutate native state.
uint64_t AirportItlwm::beAwdlServiceWord(OSObject *manager,const uint8_t *mac)
{
#if __IO80211_TARGET == __MAC_13_0
    if(!mac || !manager || !manager->metaCast("IO80211AWDLPeerManager") ||
       manager->getMetaClass()->getClassSize()<0x378)return UINT64_MAX;
    IORecursiveLock *lock=nullptr;
    memcpy(&lock,reinterpret_cast<const uint8_t *>(manager)+0x370,sizeof(lock));
    if(!lock)return UINT64_MAX;
    IORecursiveLockLock(lock);
    auto *peer=static_cast<OSObject *>(kgpNativeFindAwdlPeer(manager,const_cast<uint8_t *>(mac)));
    be_awdl::NativePeerEvidence e={};uint64_t word=UINT64_MAX;
    if(peer && peer->metaCast("IO80211AWDLPeer") && peer->getMetaClass()->getClassSize()>=0x8a1 &&
       be_awdl::readNativePeer(peer,peer->getMetaClass()->getClassSize(),e) && !memcmp(e.address,mac,6)) {
        const auto *p=reinterpret_cast<const uint8_t *>(peer);
        uint16_t sui=0;memcpy(&sui,p+0x752,2);
        word=uint64_t(sui)|(uint64_t(p[0x891])<<16)|(uint64_t(p[0x8a0])<<24);
    }
    IORecursiveLockUnlock(lock);return word;
#else
    return UINT64_MAX;
#endif
}

void AirportItlwm::beAwdlStep()
{
    auto *iwx = OSDynamicCast(ItlIwx, fHalService);
    if (!iwx || !beAwdlEnabled) return;
    if (beAwdlStepBusy) return;
    beAwdlStepBusy = true; // firmware commands can release/reenter the gate
    int err = EAGAIN;
    if (!beAwdlVif || !beAwdlVirtualEnabled || !awdlSyncEnable) goto publish;
    err = iwx->beAwdlPrepare(beAwdlAddress);
    beAwdlStats[2] = uint32_t(err);
    if (err || !beAwdlVif || !beAwdlVirtualEnabled) goto publish;
    {
        uint64_t current[16]; iwx->beAwdlSnapshot(current);
        if (current[4] != beAwdlRadioGeneration || current[5] != beAwdlRadioEpoch) {
            beAwdlTransport.resetRadioHistory();
            beAwdlMulticastDeferredSinceNs=0;
            beAwdlRadioGeneration = current[4]; beAwdlRadioEpoch = current[5];
        }
    }
    beAwdlTransport.setSyncEnabled(awdlSyncEnable);
    beAwdlTransport.setSyncState(awdlSyncState);


    beAwdlTransport.setOverrideChannel(44); // truth: radio never leaves the AP's primary channel

publish:
    beAwdlStats[14] = beAwdlTransport.peerCount();
    beAwdlStats[15] = beAwdlTransport.serviceTlvsLength();
    beAwdlStats[18] = (beAwdlEnabled ? 1 : 0) | (beAwdlVif ? 2 : 0) |
        (beAwdlVirtualEnabled ? 4 : 0) | (awdlSyncEnable ? 8 : 0) |
        (beAwdlTransport.templateLength() ? 16 : 0) | (beAwdlParamsValid ? 32 : 0) |
        (beAwdlSequenceValid ? 64 : 0) | (awdlPresenceMode ? 128 : 0);
    beAwdlStats[19] = beAwdlTransport.serviceTlvsDropped();
    iwx->beAwdlHostGate(beAwdlStats[18]);
    uint64_t firmware[16]; iwx->beAwdlSnapshot(firmware);
    // At most once a second, outside the RX completion service. R11 evidence
    // includes bounded private frame prefixes/MACs, never kernel pointers.
    if ((++beAwdlPublishTick % 5) == 0) {
        uint64_t clock[8]; iwx->beAwdlClockSnapshot(clock);
        setProperty("KGP_BE_AWDL_CLOCK", clock, sizeof(clock));
        setProperty("KGP_BE_AWDL_NATIVE", beAwdlNativeStats, sizeof(beAwdlNativeStats));
        setProperty("KGP_BE_AWDL_RX_SERVICE",beAwdlRxServiceStats,sizeof(beAwdlRxServiceStats));
        setProperty("KGP_BE_AWDL_OOB",beAwdlOobStats,sizeof(beAwdlOobStats));
        setProperty("KGP_BE_AWDL_CACHE_TRACE",beAwdlCacheTrace,sizeof(beAwdlCacheTrace));
        setProperty("KGP_BE_AWDL_CACHE_FAULT",beAwdlCacheFault,sizeof(beAwdlCacheFault));
        uint64_t cacheSlots[48]={223,1,384,beAwdlCacheTicket};
        for(unsigned i=0;i<4;++i){const auto &r=beAwdlCache[i];auto *v=cacheSlots+8+i*10;
            v[0]=r.state;v[1]=r.serial;memcpy(v+2,r.bytes+8,6);memcpy(v+3,r.bytes+4,4);
            v[4]=r.lastError;v[5]=r.publication;v[6]=r.generation;v[7]=r.epoch;
            v[8]=r.vif && r.vif==beAwdlVif; // no private pointers exported
        }
        setProperty("KGP_BE_AWDL_CACHE_SLOTS",cacheSlots,sizeof(cacheSlots));
        setProperty("KGP_BE_AWDL_DATA_TERMINALS",&beAwdlDataCompletions,sizeof(beAwdlDataCompletions));
        setProperty("KGP_BE_AWDL_ADMISSION",beAwdlPeerAdmission,sizeof(beAwdlPeerAdmission));
        beAwdlTxStats[20]=__atomic_load_n(&beAwdlTxGateDrops,__ATOMIC_RELAXED);
        setProperty("KGP_BE_AWDL_TX",beAwdlTxStats,sizeof(beAwdlTxStats));
        setProperty("KGP_BE_AWDL_DPSTATE_TEST",beAwdlDpStateTest,sizeof(beAwdlDpStateTest));
        // Native channel choices are distinct from the private radio's current
        // cochannel constraint. Do not call the constant below RF-time proof.
        auto *ch=beAwdlChannelStats;
        memset(ch+8,0,16*sizeof(*ch));
        ch[8]=awdlMasterChannel;ch[9]=awdlSecondaryMasterChannel;
        ch[10]=beAwdlSequenceValid;ch[11]=beAwdlNativeSequence.length;
        ch[14]=44; // configured private PHY channel, not a firmware readback
        auto *ic=fHalService->get80211Controller();
        if(ic->ic_bss && ic->ic_bss->ni_chan && ic->ic_bss->ni_chan!=IEEE80211_CHAN_ANYC)
            ch[13]=ieee80211_chan2ieee(ic,ic->ic_bss->ni_chan);
        OSObject *channelManager=nullptr;
        if(beAwdlVif)memcpy(&channelManager,reinterpret_cast<uint8_t *>(beAwdlVif)+0xb0,sizeof(channelManager));
        if(channelManager && channelManager->metaCast("IO80211AWDLPeerManager") &&
           channelManager->getMetaClass()->getClassSize()>=0x8a5a) {
            const auto *p=reinterpret_cast<const uint8_t *>(channelManager);
            ch[15]=1;ch[12]=p[0x8a35];
            uint32_t valid[3];memcpy(valid,p+0xcf0,sizeof(valid));
            for(unsigned i=0;i<3;++i)ch[16+i]=valid[i];
            memcpy(ch+19,p+0x8a4a,16);
        }
        ch[21]=beAwdlTransport.overrideChannel();
        setProperty("KGP_BE_AWDL_CHANNEL_CONTRACT",ch,sizeof(beAwdlChannelStats));
        setProperty("KGP_BE_AWDL_FLOW",beAwdlFlowStats,sizeof(beAwdlFlowStats));
        setProperty("KGP_AWDL_R42_COMMANDS",iwx->fAwdlR42Commands,sizeof(iwx->fAwdlR42Commands));
        setProperty("KGP_AWDL_R42_ACTIVATION",iwx->fAwdlR42Activation,sizeof(iwx->fAwdlR42Activation));
        setProperty("KGP_BE_AWDL_SERVICE_EVIDENCE",&beAwdlServiceEvidence,sizeof(beAwdlServiceEvidence));
        setProperty("KGP_BE_AWDL_PEERS",beAwdlPeerStats,sizeof(beAwdlPeerStats));
        setProperty("KGP_BE_AWDL_WIRE",&iwx->fBeAwdlWire,sizeof(iwx->fBeAwdlWire));
        // Diagnostic property only on changes; never per-packet IOKit work.
        // This gate also serializes all recorder updates. OSData copies bytes.
        if(beAwdlTxDetailPublished!=iwx->fBeAwdlTxDetail.h[5] &&
           setProperty("KGP_BE_AWDL_TX_DETAIL",&iwx->fBeAwdlTxDetail,sizeof(iwx->fBeAwdlTxDetail)))
            beAwdlTxDetailPublished=iwx->fBeAwdlTxDetail.h[5];
        uint64_t dataRole[64];iwx->beAwdlDataSnapshot(dataRole);
        setProperty("KGP_BE_AWDL_DATA_ROLE",dataRole,sizeof(dataRole));
        uint64_t dataConfig[64];iwx->beAwdlDataConfigSnapshot(dataConfig);
        setProperty("KGP_BE_AWDL_DATA_CONFIG",dataConfig,sizeof(dataConfig));
        setProperty("KGP_BE_AWDL_DATA_LIFECYCLE",iwx->fBeAwdlDataLife,sizeof(iwx->fBeAwdlDataLife));
        setProperty("KGP_BE_AWDL_NATIVE_DATA",beAwdlDataNative,sizeof(beAwdlDataNative));
        uint64_t prerequisites[16];iwx->beAwdlDataPrerequisites(prerequisites);
        setProperty("KGP_BE_AWDL_DATA_PREREQ",prerequisites,sizeof(prerequisites));
        setProperty("KGP_BE_AWDL_TX_STATUS",iwx->fBeAwdlNativeStatus,sizeof(iwx->fBeAwdlNativeStatus));
        setProperty("KGP_BE_AWDL_TX_ADMISSION",&iwx->fBeAwdlTxAdmission,sizeof(iwx->fBeAwdlTxAdmission));
        uint64_t infraRate[32];iwx->beAwdlInfraRateSnapshot(infraRate);
        setProperty("KGP_BE_AWDL_INFRA_RATE",infraRate,sizeof(infraRate));
        uint64_t infraCoex[64];iwx->beAwdlInfraCoexSnapshot(infraCoex);
        setProperty("KGP_BE_AWDL_INFRA_COEX",infraCoex,sizeof(infraCoex));
        setProperty("KGP_BE_AWDL_INFRA_RX_OWNER",iwx->fBeAwdlInfraRxOwner,sizeof(iwx->fBeAwdlInfraRxOwner));
        setProperty("KGP_BE_AWDL_TX_RESOURCES",iwx->fBeAwdlTxResources,sizeof(iwx->fBeAwdlTxResources));
        uint64_t trace[272];iwx->beAwdlQueueSnapshot(trace);
        setProperty("KGP_BE_AWDL_QUEUE_STATE",trace,sizeof(trace)); // OSData copies; reuse existing trace scratch
        uint64_t window[64]; iwx->beAwdlWindowSnapshot(window);
        memcpy(window+32,beAwdlWindowNativeStats,sizeof(beAwdlWindowNativeStats));
        setProperty("KGP_BE_AWDL_WINDOW",window,sizeof(window));
        iwx->beAwdlWindowTraceSnapshot(trace);
        setProperty("KGP_BE_AWDL_WINDOW_TRACE",trace,sizeof(trace));
        setProperty("KGP_BE_AWDL_WINDOW_LIVE",iwx->fBeAwdlWindowLife,sizeof(iwx->fBeAwdlWindowLife));
        uint64_t detail[336]; iwx->beAwdlDetailSnapshot(detail);
        setProperty("KGP_BE_AWDL_DETAIL", detail, sizeof(detail));
        setProperty("KGP_BE_AWDL_SCHEMA", 1U, 32);
        setProperty("KGP_BE_AWDL_FW", firmware, sizeof(firmware));
        setProperty("KGP_BE_AWDL_HOST", beAwdlStats, sizeof(beAwdlStats));
    }
    beAwdlStepBusy = false;
}

IOReturn AirportItlwm::beAwdlVifAction(OSObject *owner, void *arg, void *, void *, void *)
{
    auto *self = OSDynamicCast(AirportItlwm, owner);
    auto *call = static_cast<BeAwdlVifCall *>(arg);
    if (!self || !call) return kIOReturnBadArgument;
    if (call->action == 0) call->vif = self->createVirtualInterface(call->ether, call->role);
    else if (call->action == 1) call->result = self->enableVirtualInterface(call->vif);
    else if (call->action == 2) call->result = self->disableVirtualInterface(call->vif);
    else return kIOReturnBadArgument;
    return kIOReturnSuccess;
}

#if __IO80211_TARGET == __MAC_13_0
OSDefineMetaClassAndStructors(BeAwdlInterface, IO80211P2PInterface)
void BeAwdlInterface::detach(IOService *provider)
{
    auto *controller = OSDynamicCast(AirportItlwm, provider);
    auto *loop = controller ? controller->getWorkLoop() : nullptr;
    retain(); // native registry detach may release its attachment references
    if (loop) {
        loop->runAction([](OSObject *owner, void *arg, void *, void *, void *) -> IOReturn {
            auto *ctl = static_cast<AirportItlwm *>(owner);
            auto *vif = static_cast<BeAwdlInterface *>(arg);
            const bool busy = ctl->beAwdlVifBusy; ctl->beAwdlVifBusy = true;
            if (ctl->beAwdlVif == vif) {
                ctl->beAwdlDataStopWanted=true;ctl->beAwdlDataStopTried=false;
                ctl->beAwdlCancelNativeData(vif);
                ctl->beAwdlVif = nullptr; ctl->beAwdlVirtualEnabled = false;
                ctl->beAwdlTransport.reset(); ctl->beAwdlParamsValid = ctl->beAwdlSequenceValid = false;
                ctl->awdlPresenceMode = 0; ++ctl->beAwdlStats[23];
                ctl->beAwdlNativeSyncValid=false;
                ctl->beAwdlWindowAnnounced=false; ctl->beAwdlPostedWindow=0;
            }
            vif->IO80211P2PInterface::detach(ctl);
            ctl->beAwdlVifBusy = busy; return kIOReturnSuccess;
        }, controller, this);
    } else {
        if (controller && controller->beAwdlVif == this) controller->beAwdlVif = nullptr;
        IO80211P2PInterface::detach(provider);
    }
    release();
}
#endif

IOReturn AirportItlwm::beAwdlCreate(apple80211_virt_if_create_data *data)
{
#if __IO80211_TARGET == __MAC_13_0
    if (!data) return kIOReturnBadArgument;
    if (data->role != APPLE80211_VIF_AWDL || !beAwdlEnabled) return kIOReturnUnsupported;
    auto *loop = getWorkLoop(); if (!loop) return kIOReturnNotReady;
    return loop->runAction([](OSObject *owner, void *arg, void *, void *, void *) -> IOReturn {
        auto *ctl = static_cast<AirportItlwm *>(owner);
        auto *request = static_cast<apple80211_virt_if_create_data *>(arg);
        ++ctl->beAwdlStats[20];
        if (ctl->isInactive()) return kIOReturnNotReady;
        if (ctl->beAwdlVifBusy) return kIOReturnBusy;
        if (auto *existing = ctl->beAwdlVif) {
            auto *vif = OSDynamicCast(BeAwdlInterface, existing);
            unsigned char address[6];
            if (!vif || vif->deleteRequested || vif->isInactive() ||
                !vif->getInterfaceAddress(address) || memcmp(address, request->mac, 6)) return kIOReturnBusy;
            strlcpy(reinterpret_cast<char *>(request->bsd_name), vif->getBSDName(), sizeof(request->bsd_name));
            return kIOReturnSuccess;
        }
        struct BusyScope { bool &busy; BusyScope(bool &b):busy(b){busy=true;} ~BusyScope(){busy=false;} } scope(ctl->beAwdlVifBusy);
        ether_addr address; memcpy(address.octet, request->mac, 6);
        IO80211VirtualInterface *created = nullptr;
        if (!ctl->attachVirtualInterface(&created, &address, APPLE80211_VIF_AWDL, false)) return kIOReturnError;
        // Checked factory is the only producer for this opt-in role. Do not
        // keep an extra permanent VIF reference: init already retains its controller.
        auto *vif = OSDynamicCast(BeAwdlInterface, created);
        if (!vif) { ctl->detachVirtualInterface(created, false); created->release(); return kIOReturnError; }
        ctl->beAwdlVif = vif; ctl->beAwdlVirtualEnabled = vif->enabledBySystem();
        ctl->beAwdlNativeSyncValid=false;
        ctl->beAwdlWindowAnnounced=false; ctl->beAwdlPostedWindow=0;
        memcpy(ctl->beAwdlAddress, request->mac, 6);
        strlcpy(reinterpret_cast<char *>(request->bsd_name), vif->getBSDName(), sizeof(request->bsd_name));
        vif->registerService(); // exactly one publication, alias already visible
        ++ctl->beAwdlStats[21];
        vif->release(); // registry attachment owns lifetime; not the driver alias
        return kIOReturnSuccess;
    }, this, data);
#else
    return kIOReturnUnsupported;
#endif
}

IOReturn AirportItlwm::beAwdlDelete(apple80211_virt_if_delete_data *data)
{
#if __IO80211_TARGET == __MAC_13_0
    if (!data || !data->bsd_name[0] || strnlen(reinterpret_cast<const char *>(data->bsd_name), sizeof(data->bsd_name)) == sizeof(data->bsd_name)) return kIOReturnBadArgument;
    auto *loop = getWorkLoop(); if (!loop) return kIOReturnNotReady;
    BeAwdlInterface *vif = nullptr;
    auto status = loop->runAction([](OSObject *owner, void *arg, void *out, void *, void *) -> IOReturn {
        auto *ctl = static_cast<AirportItlwm *>(owner);
        auto *request = static_cast<apple80211_virt_if_delete_data *>(arg);
        if (ctl->beAwdlVifBusy) return kIOReturnBusy;
        auto *vif = OSDynamicCast(BeAwdlInterface, ctl->beAwdlVif);
        if (!vif || strcmp(reinterpret_cast<const char *>(request->bsd_name), vif->getBSDName())) return kIOReturnNotFound;
        ++ctl->beAwdlStats[22];
        if (!vif->deleteRequested && !vif->isInactive()) {
            vif->deleteRequested = true; vif->retain();
            *static_cast<BeAwdlInterface **>(out) = vif;
        }
        return kIOReturnSuccess;
    }, this, data, &vif);
    if (vif) {
        status = detachVirtualInterface(vif, false) ? kIOReturnSuccess : kIOReturnError;
        if (status != kIOReturnSuccess) loop->runAction([](OSObject *, void *v, void *, void *, void *) -> IOReturn {
            static_cast<BeAwdlInterface *>(v)->deleteRequested = false; return kIOReturnSuccess;
        }, this, vif);
        vif->release(); // outside the newly introduced gate action, as in KGP 2O
    }
    return status; // request result, never a fabricated completed detach
#else
    return kIOReturnUnsupported;
#endif
}
