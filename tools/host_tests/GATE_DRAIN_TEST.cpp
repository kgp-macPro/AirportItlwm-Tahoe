#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <memory>
#include <cstdio>
#include <cerrno>
#include <atomic>
#include <mutex>
#include <thread>
#include <future>
#include <functional>
#include <chrono>
#include <algorithm>
using IOReturn=int;
using OSObject=void;
#define AIRPORT 1
#define NBPFILTER 0
#define nitems(x) (sizeof(x)/sizeof((x)[0]))
#define XYLog(...) ((void)0)
#define IWX_DEVICE_FAMILY_AX210 210
#define IEEE80211_S_AUTH 2
#define IEEE80211_S_ASSOC 3
#define IEEE80211_S_RUN 4
#define IFF_UP 1
#define IFF_RUNNING 64
#define IWX_FLAG_TXFLUSH 16
#define IEEE80211_F_TX_MGMT_ONLY 32
#define EDCA_AC_BE 0
constexpr IOReturn kIOReturnSuccess=0,kIOReturnError=0x1234,kIOReturnCannotLock=int(0xe00002cc),kIOReturnNoMemory=int(0xe00002bd),kIOReturnNotPermitted=int(0xe00002e2);
struct ieee80211_node {unsigned releases=0;};
struct Packet { uint8_t bytes[32]={0xb0};size_t len=30;ieee80211_node node;bool consumed=false;bool encapsulated=false;};
using mbuf_t=Packet*;
struct Queue {std::vector<mbuf_t> packets;std::recursive_mutex mutex;bool tryAllowed=true,held=false;unsigned drops=0;};
struct mbuf_queue {Queue store;Queue *mq_mtx=&store;};
bool IORecursiveLockTryLock(Queue *q){if(!q->tryAllowed)return false;if(!q->mutex.try_lock())return false;q->held=true;return true;}
void IORecursiveLockUnlock(Queue*q){assert(q->held);q->held=false;q->mutex.unlock();}
unsigned mq_len(mbuf_queue*q){return q->store.packets.size();}unsigned mq_drops(mbuf_queue*q){return q->store.drops;}
mbuf_t mq_dequeue(mbuf_queue*q){std::lock_guard<std::recursive_mutex> l(q->store.mutex);if(q->store.packets.empty())return nullptr;auto m=q->store.packets.front();q->store.packets.erase(q->store.packets.begin());return m;}
struct _ifqueue {unsigned ifq_oactive=0;std::vector<mbuf_t> packets;};
unsigned ifq_is_oactive(_ifqueue*q){return q->ifq_oactive;}void ifq_set_oactive(_ifqueue*q){q->ifq_oactive=1;}
mbuf_t ifq_dequeue(_ifqueue*q){if(q->packets.empty())return nullptr;auto m=q->packets.front();q->packets.erase(q->packets.begin());return m;}
struct Stats {unsigned outputErrors=0,outputPackets=0;};
struct _ifnet {void*if_softc;uint32_t if_flags=IFF_UP|IFF_RUNNING;_ifqueue if_snd;Stats stats;Stats*netStat=&stats;unsigned if_timer=0;};
struct ieee80211com {struct{_ifnet ac_if;}ic_ac;uint32_t ic_state=IEEE80211_S_AUTH,ic_xflags=0;mbuf_queue ic_mgtq;};
#define ic_if ic_ac.ac_if
struct Ring {unsigned ring_count=128,queued=0;};
struct iwx_softc {void*owner;int sc_device_family=IWX_DEVICE_FAMILY_AX210;ieee80211com sc_ic;uint32_t sc_flags=0,qfullmsk=0;int first_data_qid=4,sc_generation=1;Ring txq[32];int sc_tx_timer=0;};
#define container_of(sc,type,member) static_cast<type*>((sc)->owner)
struct ether_header {uint8_t data[14];};
size_t mbuf_len(mbuf_t m){assert(!m->consumed);return m->len;}
void*mbuf_data(mbuf_t m){assert(!m->consumed);return m->bytes;}
void*mbuf_pkthdr_rcvif(mbuf_t m){assert(!m->consumed);return &m->node;}
int pullError=0;bool encapFails=false;
int mbuf_pullup(mbuf_t*m,size_t length){if(pullError){(*m)->consumed=true;*m=nullptr;return pullError;}(*m)->len=length;return 0;}
mbuf_t ieee80211_encap(_ifnet*,mbuf_t m,ieee80211_node**n){if(encapFails){m->consumed=true;return nullptr;}*n=&m->node;m->encapsulated=true;return m;}
void ieee80211_release_node(ieee80211com*,ieee80211_node*n){++n->releases;}
uint64_t time2GNow(){static std::atomic<uint64_t> t{0};return ++t;}

thread_local bool inAttempt=false;
thread_local int spinHeld=0;
using IOInterruptState=int;
struct IOSimpleLock {std::mutex mutex;};
bool failLock=false,failEvent=false;int liveLocks=0,liveEvents=0;
IOSimpleLock*IOSimpleLockAlloc(){if(failLock)return nullptr;++liveLocks;return new IOSimpleLock;}
void IOSimpleLockFree(IOSimpleLock*l){--liveLocks;delete l;}
IOInterruptState IOSimpleLockLockDisableInterrupt(IOSimpleLock*l){assert(!spinHeld);l->mutex.lock();++spinHeld;return 0;}
void IOSimpleLockUnlockEnableInterrupt(IOSimpleLock*l,int){--spinHeld;l->mutex.unlock();}
struct IOInterruptEventSource {
 using Action=void(*)(OSObject*,IOInterruptEventSource*,int);void*owner;Action action;
 std::atomic<unsigned> signals{0};std::atomic<bool> enabled{false};unsigned refs=1;
 IOInterruptEventSource(void*o,Action a):owner(o),action(a){++liveEvents;}
 ~IOInterruptEventSource(){--liveEvents;}
 static IOInterruptEventSource*interruptEventSource(void*o,Action a){return failEvent?nullptr:new IOInterruptEventSource(o,a);}
 void retain(){++refs;}void release(){assert(refs);if(!--refs)delete this;}
 void enable(){enabled=true;}void disable(){enabled=false;}
 void interruptOccurred(void*,void*,int){assert(spinHeld);++signals;}
};
struct Loop {
 std::recursive_mutex gate;IOInterruptEventSource*event=nullptr;int addError=0,removeError=0;
 using Action=IOReturn(*)(OSObject*,void*,void*,void*,void*);
 IOReturn addEventSource(IOInterruptEventSource*s){assert(!spinHeld);std::lock_guard<std::recursive_mutex> l(gate);if(addError)return addError;assert(!event);event=s;s->retain();return 0;}
 IOReturn removeEventSource(IOInterruptEventSource*s){assert(!spinHeld);std::lock_guard<std::recursive_mutex> l(gate);if(removeError)return removeError;assert(event==s);event=nullptr;s->release();return 0;}
 IOReturn runAction(Action f,void*o){assert(!spinHeld);std::lock_guard<std::recursive_mutex> l(gate);return f(o,nullptr,nullptr,nullptr,nullptr);}
 void deliver(){assert(!spinHeld);std::lock_guard<std::recursive_mutex> l(gate);if(event&&event->enabled){auto n=event->signals.exchange(0);if(n)event->action(event->owner,event,n);}}
};
struct Gate {
 Loop*loop;std::atomic<unsigned> calls{0};int forced=0;std::atomic<IOReturn> last{0};
 using Action=IOReturn(*)(OSObject*,void*,void*,void*,void*);
 IOReturn attemptAction(Action f,void*a,void*b){assert(!spinHeld);++calls;if(forced)return last=forced;if(!loop->gate.try_lock())return last=kIOReturnCannotLock;bool before=inAttempt;inAttempt=true;int result=f(nullptr,a,b,nullptr,nullptr);inAttempt=before;loop->gate.unlock();last=result;return result;}
};
struct Provider {unsigned calls=0;bool success=true;std::vector<uint8_t> wire,drain;bool setProperty(const char*k,const void*p,size_t n){assert(!inAttempt&&!spinHeld);++calls;auto&v=!strcmp(k,"KGP_DRAIN_2M")?drain:wire;assert(n==(v.data()==drain.data()&& !strcmp(k,"KGP_DRAIN_2M")?96:416));v.assign((const uint8_t*)p,(const uint8_t*)p+n);return success;}};
struct ItlIwx {
 iwx_softc com;struct{Provider*pa_tag=nullptr;}pci;Loop loop;Gate gate{&loop};uint32_t fTx2IBlocked=0,fTx2IStats[32]={};unsigned txCalls=0,refs=1;int txResult=0;std::function<void()> duringTx;std::vector<Packet*> sent;
 ItlIwx(){com.owner=this;com.sc_ic.ic_if.if_softc=&com;}
 Gate*getMainCommandGate(){return &gate;}Loop*getMainWorkLoop(){return &loop;}
 void retain(){++refs;}void release(){assert(refs>1);--refs;}
 int iwx_tx(iwx_softc*,mbuf_t m,ieee80211_node*n,int ac){assert(inAttempt);assert(!m->consumed&&n==&m->node&&ac==0);++txCalls;sent.push_back(m);if(duringTx)duringTx();m->consumed=true;if(!txResult){++fTx2IStats[3];++fTx2IStats[4];++fTx2IStats[5];}return txResult;}
 static IOReturn _iwx_start_task(OSObject*,void*,void*,void*,void*);static void iwx_start(_ifnet*);
    struct Start2LContext { bool entered; uint32_t reason; };
    struct Path2LRecord {
        uint64_t ns;
        uint32_t event, result, state, ifflags, driverflags, qfull;
        uint32_t mgtlen, mgtdrops, qid, ringcount, ringqueued;
        uint32_t blocked, oactive, generation, detail, fc0;
    };
    struct Path2LData {
        uint32_t word[32];
        Path2LRecord firstAttempt, firstFailure, firstTx, latest;
    } fPath2L = {};
    static_assert(sizeof(Path2LRecord) == 72 && sizeof(Path2LData) == 416, "2L wire layout");
    uint32_t fPath2LBusy = 0, fPath2LDropped = 0;
    void path2L(unsigned event, uint32_t result = 0, uint32_t detail = 0, uint32_t fc0 = 0);
    void path2LPublish();

    // 2M: a coalesced software event on the SAME workloop as mainCommandGate.
    // The lock protects event lifetime/signalling only; never held across gates.
    IOSimpleLock *fDrain2MLock = nullptr;
    IOInterruptEventSource *fDrain2MEvent = nullptr;
    bool fDrain2MPending = false, fDrain2MPaused = true, fDrain2MClosed = false;
    uint32_t fDrain2MEpoch = 0;
    uint32_t fDrain2MStats[24] = {}, fDrain2MWire[24] = {};
    bool drain2MSetup();
    void drain2MRequest();
    uint32_t drain2MPause();
    void drain2MArm(uint32_t);
    void drain2MRemove();
    static IOReturn drain2MFence(OSObject *, void *, void *, void *, void *);
    static void drain2MAction(OSObject *, IOInterruptEventSource *, int);

};
void ItlIwx::path2L(unsigned event, uint32_t result, uint32_t detail, uint32_t fc0)
{
    if (com.sc_device_family < IWX_DEVICE_FAMILY_AX210) return;
    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&fPath2LBusy, &expected, 1U, false,
                                      __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        __atomic_fetch_add(&fPath2LDropped, 1U, __ATOMIC_RELAXED); return;
    }
    auto &ic = com.sc_ic;
    Path2LRecord row = {};
    row.ns = time2GNow(); row.event = event; row.result = result;
    row.state = ic.ic_state; row.ifflags = ic.ic_if.if_flags;
    row.driverflags = com.sc_flags; row.qfull = com.qfullmsk;
    row.mgtlen = row.mgtdrops = UINT32_MAX;
    // Never wait for another thread's mbuf-queue lock just for telemetry.
    if (ic.ic_mgtq.mq_mtx && IORecursiveLockTryLock(ic.ic_mgtq.mq_mtx)) {
        row.mgtlen = mq_len(&ic.ic_mgtq); row.mgtdrops = mq_drops(&ic.ic_mgtq);
        IORecursiveLockUnlock(ic.ic_mgtq.mq_mtx);
    } else ++fPath2L.word[23];
    row.qid = uint32_t(com.first_data_qid);
    row.ringcount = row.ringqueued = UINT32_MAX;
    if (row.qid < nitems(com.txq)) {
        row.ringcount = com.txq[row.qid].ring_count;
        row.ringqueued = com.txq[row.qid].queued;
    }
    row.blocked = __atomic_load_n(&fTx2IBlocked, __ATOMIC_ACQUIRE);
    row.oactive = ic.ic_if.if_snd.ifq_oactive;
    row.generation = com.sc_generation; row.detail = detail; row.fc0 = fc0;
    fPath2L.word[0] = 1; ++fPath2L.word[2];
    const bool pending = row.mgtlen != UINT32_MAX && row.mgtlen != 0;
    const bool joining = row.state == IEEE80211_S_AUTH || row.state == IEEE80211_S_ASSOC;
    bool failure = false;
    switch (event) {
    case 1:
        ++fPath2L.word[4];
        if (!fPath2L.word[20] && (pending || joining)) {
            fPath2L.firstAttempt = row; fPath2L.word[20] = 1;
        }
        break;
    case 2:
        if (result) ++fPath2L.word[26];
        if (!detail) { ++fPath2L.word[6]; failure = pending || joining; }
        break;
    case 3: ++fPath2L.word[5]; break;
    case 4:
        if (detail >= 1 && detail <= 4) ++fPath2L.word[6 + detail];
        if (detail == 5) ++fPath2L.word[25];
        failure = detail >= 1 && detail <= 4 && (pending || joining);
        break;
    case 5: ++fPath2L.word[11]; break;
    case 6: ++fPath2L.word[12]; break;
    case 7:
        ++fPath2L.word[13];
        if (!fPath2L.word[22]) { fPath2L.firstTx = row; fPath2L.word[22] = 1; }
        break;
    case 8:
        ++fPath2L.word[result ? 15 : 14]; failure = result != 0;
        break;
    case 9: ++fPath2L.word[24]; break;
    }
    if (failure && !fPath2L.word[21]) {
        fPath2L.firstFailure = row; fPath2L.word[21] = 1;
    }
    fPath2L.latest = row;
    __atomic_store_n(&fPath2LBusy, 0U, __ATOMIC_RELEASE);
}
void ItlIwx::path2LPublish()
{
    if (com.sc_device_family < IWX_DEVICE_FAMILY_AX210) return;
    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&fPath2LBusy, &expected, 1U, false,
                                      __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return;
    // No snapshots for idle pre-join calls; preserve budget for the actual gap.
    if (!fPath2L.word[20] || fPath2L.word[1] >= 64) {
        __atomic_store_n(&fPath2LBusy, 0U, __ATOMIC_RELEASE); return;
    }
    ++fPath2L.word[1];
    fPath2L.word[3] = __atomic_load_n(&fPath2LDropped, __ATOMIC_RELAXED);
    fPath2L.word[17] = __atomic_load_n(&fTx2IStats[3], __ATOMIC_RELAXED);
    fPath2L.word[18] = __atomic_load_n(&fTx2IStats[5], __ATOMIC_RELAXED);
    fPath2L.word[19] = __atomic_load_n(&fTx2IStats[6], __ATOMIC_RELAXED);
    fPath2L.word[27] = __atomic_load_n(&fTx2IStats[4], __ATOMIC_RELAXED);
    fPath2L.word[28] = __atomic_load_n(&fTx2IStats[18], __ATOMIC_RELAXED);
    fPath2L.word[29] = __atomic_load_n(&fTx2IStats[19], __ATOMIC_RELAXED);
    fPath2L.word[30] = __atomic_load_n(&fTx2IBlocked, __ATOMIC_ACQUIRE);
    fPath2L.word[31] = 64;
    // Publish only after the original attemptAction returns. No packet or node
    // pointers are retained; the copied property survives normal HAL cleanup.
    if (fDrain2MLock) {
        for (unsigned i = 0; i < 24; ++i)
            fDrain2MWire[i] = __atomic_load_n(&fDrain2MStats[i], __ATOMIC_RELAXED);
        fDrain2MWire[0] = 1;
        const IOInterruptState irq = IOSimpleLockLockDisableInterrupt(fDrain2MLock);
        fDrain2MWire[14] = fDrain2MPending;
        fDrain2MWire[15] = fDrain2MPaused;
        fDrain2MWire[16] = fDrain2MClosed;
        fDrain2MWire[20] = fDrain2MEpoch;
        IOSimpleLockUnlockEnableInterrupt(fDrain2MLock, irq);
        fDrain2MWire[21] = fPath2L.word[1];
        if (!pci.pa_tag || !pci.pa_tag->setProperty("KGP_DRAIN_2M", fDrain2MWire, sizeof(fDrain2MWire)))
            __atomic_fetch_add(&fDrain2MStats[22], 1U, __ATOMIC_RELAXED);
    }
    const bool ok = pci.pa_tag && pci.pa_tag->setProperty("KGP_PATH_2L", &fPath2L, sizeof(fPath2L));
    if (!ok) ++fPath2L.word[16];
    const uint32_t snapshots = fPath2L.word[1], gate = fPath2L.word[6];
    const uint32_t dequeued = fPath2L.word[11], tx = fPath2L.word[13], errors = fPath2L.word[15];
    __atomic_store_n(&fPath2LBusy, 0U, __ATOMIC_RELEASE);
    XYLog("KGP_PATH_2L snapshot=%u gate_not_entered=%u mgmt_dequeued=%u tx_calls=%u tx_errors=%u\n",
          snapshots, gate, dequeued, tx, errors);
}
bool ItlIwx::drain2MSetup()
{
    if (com.sc_device_family < IWX_DEVICE_FAMILY_AX210) return true;
    fDrain2MLock = IOSimpleLockAlloc();
    if (!fDrain2MLock) { fDrain2MStats[17] = kIOReturnNoMemory; return false; }
    fDrain2MEvent = IOInterruptEventSource::interruptEventSource(this, drain2MAction);
    if (!fDrain2MEvent) { fDrain2MStats[17] = kIOReturnNoMemory; return false; }
    const IOReturn result = getMainWorkLoop()->addEventSource(fDrain2MEvent);
    fDrain2MStats[17] = uint32_t(result);
    if (result != kIOReturnSuccess) {
        fDrain2MEvent->release(); fDrain2MEvent = nullptr; return false;
    }
    retain(); // callback owner stays alive until synchronous removal succeeds
    fDrain2MEvent->enable();
    return true;
}
void ItlIwx::drain2MRequest()
{
    if (!fDrain2MLock) return;
    __atomic_fetch_add(&fDrain2MStats[1], 1U, __ATOMIC_RELAXED);
    const IOInterruptState irq = IOSimpleLockLockDisableInterrupt(fDrain2MLock);
    if (!fDrain2MEvent || fDrain2MClosed || fDrain2MPaused) {
        __atomic_fetch_add(&fDrain2MStats[4], 1U, __ATOMIC_RELAXED);
    } else if (fDrain2MPending) {
        __atomic_fetch_add(&fDrain2MStats[3], 1U, __ATOMIC_RELAXED);
    } else {
        fDrain2MPending = true;
        __atomic_fetch_add(&fDrain2MStats[2], 1U, __ATOMIC_RELAXED);
        // Interrupt-safe producer signalling, not a gate acquisition or callout.
        // Removal first takes this lock, so the source cannot disappear here.
        fDrain2MEvent->interruptOccurred(nullptr, nullptr, 0);
    }
    IOSimpleLockUnlockEnableInterrupt(fDrain2MLock, irq);
}
IOReturn ItlIwx::drain2MFence(OSObject *, void *, void *, void *, void *)
{
    return kIOReturnSuccess;
}
uint32_t ItlIwx::drain2MPause()
{
    if (!fDrain2MLock) return 0;
    const IOInterruptState irq = IOSimpleLockLockDisableInterrupt(fDrain2MLock);
    fDrain2MPaused = true;
    const uint32_t epoch = ++fDrain2MEpoch;
    if (fDrain2MPending) __atomic_fetch_add(&fDrain2MStats[10], 1U, __ATOMIC_RELAXED);
    fDrain2MPending = false;
    IOSimpleLockUnlockEnableInterrupt(fDrain2MLock, irq);
    __atomic_fetch_add(&fDrain2MStats[11], 1U, __ATOMIC_RELAXED);
    // Lifecycle context only, BEFORE existing stop/admission/queue operations.
    // This empty workloop action fences an already-running deferred drain.
    // It never waits on a disabled command gate and holds no new lock while waiting.
    const IOReturn result = getMainWorkLoop()->runAction(drain2MFence, this);
    __atomic_store_n(&fDrain2MStats[18], uint32_t(result), __ATOMIC_RELAXED);
    return epoch;
}
void ItlIwx::drain2MArm(uint32_t epoch)
{
    if (!fDrain2MLock) return;
    const IOInterruptState irq = IOSimpleLockLockDisableInterrupt(fDrain2MLock);
    if (fDrain2MEvent && !fDrain2MClosed && epoch == fDrain2MEpoch)
        fDrain2MPaused = false;
    else __atomic_fetch_add(&fDrain2MStats[23], 1U, __ATOMIC_RELAXED);
    IOSimpleLockUnlockEnableInterrupt(fDrain2MLock, irq);
    __atomic_fetch_add(&fDrain2MStats[12], 1U, __ATOMIC_RELAXED);
}
void ItlIwx::drain2MRemove()
{
    if (!fDrain2MLock) return;
    const IOInterruptState irq = IOSimpleLockLockDisableInterrupt(fDrain2MLock);
    fDrain2MClosed = fDrain2MPaused = true;
    ++fDrain2MEpoch;
    if (fDrain2MPending) __atomic_fetch_add(&fDrain2MStats[10], 1U, __ATOMIC_RELAXED);
    fDrain2MPending = false;
    IOInterruptEventSource *source = fDrain2MEvent;
    fDrain2MEvent = nullptr;
    IOSimpleLockUnlockEnableInterrupt(fDrain2MLock, irq);
    if (!source) return;
    __atomic_fetch_add(&fDrain2MStats[13], 1U, __ATOMIC_RELAXED);
    source->disable();
    // SDK removal is synchronous with the workloop; no callback survives return.
    const IOReturn result = getMainWorkLoop()->removeEventSource(source);
    __atomic_store_n(&fDrain2MStats[19], uint32_t(result), __ATOMIC_RELAXED);
    if (result != kIOReturnSuccess) {
        // Unexpected contract failure: keep both owner/source alive, closed.
        // Never free an owner whose callback removal was not established.
        XYLog("KGP_DRAIN_2M removal failed=0x%x; closed owner retained\n", result);
        return;
    }
    source->release();
    release(); // paired setup retain; lifecycle caller still owns the HAL
}
void ItlIwx::drain2MAction(OSObject *owner, IOInterruptEventSource *sender, int)
{
    ItlIwx *that = static_cast<ItlIwx *>(owner);
    __atomic_fetch_add(&that->fDrain2MStats[5], 1U, __ATOMIC_RELAXED);
    const IOInterruptState irq = IOSimpleLockLockDisableInterrupt(that->fDrain2MLock);
    const bool claimed = sender == that->fDrain2MEvent && that->fDrain2MPending &&
                         !that->fDrain2MPaused && !that->fDrain2MClosed;
    if (claimed) that->fDrain2MPending = false;
    IOSimpleLockUnlockEnableInterrupt(that->fDrain2MLock, irq);
    if (!claimed) return;
    __atomic_fetch_add(&that->fDrain2MStats[6], 1U, __ATOMIC_RELAXED);
    Start2LContext context = {};
    // Already on the same recursive workloop gate: transient external gate
    // contention has ended. Still respect the original command-gate action.
    const IOReturn result = that->getMainCommandGate()->attemptAction(
        _iwx_start_task, &that->com.sc_ic.ic_ac.ac_if, &context);
    __atomic_fetch_add(&that->fDrain2MStats[context.entered ? 7 : 8], 1U, __ATOMIC_RELAXED);
    __atomic_store_n(&that->fDrain2MStats[9], uint32_t(result), __ATOMIC_RELAXED);
    that->path2L(2, uint32_t(result), context.entered ? 1U : 0U);
    that->path2LPublish();
    // No self-rescheduling loop. Another enqueue can signal new work; normal
    // backpressure remains governed by the unchanged queue/completion paths.
}
IOReturn ItlIwx::
_iwx_start_task(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    struct _ifnet *ifp = (struct _ifnet *)arg0;
    struct iwx_softc *sc = (struct iwx_softc *)ifp->if_softc;
    ItlIwx *that = container_of(sc, ItlIwx, com);
    struct ieee80211com *ic = &sc->sc_ic;
    struct ieee80211_node *ni;
    struct ether_header *eh;
    mbuf_t m;
    int ac = EDCA_AC_BE; /* XXX */
    Start2LContext *context2L = static_cast<Start2LContext *>(arg1);
    if (context2L) context2L->entered = true;
    that->path2L(3);
    uint32_t exit2L = 0;
    
    if (!(ifp->if_flags & IFF_RUNNING) ||  ifq_is_oactive(&ifp->if_snd)) {
        const uint32_t reason2L = !(ifp->if_flags & IFF_RUNNING) ? 1U : 2U;
        if (context2L) context2L->reason = reason2L;
        that->path2L(4, uint32_t(kIOReturnError), reason2L);
        return kIOReturnError;
    }
    
    for (;;) {
        /* why isn't this done per-queue? */
        if (sc->qfullmsk != 0) {
            ifq_set_oactive(&ifp->if_snd);
            exit2L = 3;
            break;
        }

        /* Don't queue additional frames while flushing Tx queues. */
        if (sc->sc_flags & IWX_FLAG_TXFLUSH) {
            exit2L = 4;
            break;
        }
        
        /* need to send management frames even if we're not RUNning */
        m = mq_dequeue(&ic->ic_mgtq);
        if (m) {
            that->path2L(5);
            //            ni = m->m_pkthdr.ph_cookie;
            ni = (struct ieee80211_node *)mbuf_pkthdr_rcvif(m);
            goto sendit;
        }
        
        that->path2L(9);
        if (
#ifndef AIRPORT
            ic->ic_state != IEEE80211_S_RUN ||
#endif
            (ic->ic_xflags & IEEE80211_F_TX_MGMT_ONLY)) {
            exit2L = 5;
            break;
        }
        
        m = ifq_dequeue(&ifp->if_snd);
        if (!m)
            break;
        that->path2L(6);
        if (mbuf_len(m) < sizeof (*eh) &&
            mbuf_pullup(&m, sizeof (*eh)) != 0) {
            ifp->netStat->outputErrors++;
            continue;
        }
#if NBPFILTER > 0
        if (ifp->if_bpf != NULL)
            bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
        if ((m = ieee80211_encap(ifp, m, &ni)) == NULL) {
            ifp->netStat->outputErrors++;
            continue;
        }
        
    sendit:
#if NBPFILTER > 0
        if (ic->ic_rawbpf != NULL)
            bpf_mtap(ic->ic_rawbpf, m, BPF_DIRECTION_OUT);
#endif
        // Capture only the frame-control byte while this call still owns m.
        // Never touch m/ni after iwx_tx; its existing disposition is unchanged.
        uint8_t fc2L = 0;
        if (mbuf_len(m) >= 1) memcpy(&fc2L, mbuf_data(m), sizeof(fc2L));
        that->path2L(7, 0, 0, fc2L);
        const int txResult2L = that->iwx_tx(sc, m, ni, ac);
        that->path2L(8, uint32_t(txResult2L), 0, fc2L);
        if (txResult2L != 0) {
            ieee80211_release_node(ic, ni);
            ifp->netStat->outputErrors++;
            continue;
        }
        ifp->netStat->outputPackets++;
        
        if (ifp->if_flags & IFF_UP) {
            sc->sc_tx_timer = 15;
            ifp->if_timer = 1;
        }
    }
    
    if (context2L) context2L->reason = exit2L;
    that->path2L(4, uint32_t(kIOReturnSuccess), exit2L);
    return kIOReturnSuccess;
}
void ItlIwx::
iwx_start(struct _ifnet *ifp)
{
    struct iwx_softc *sc = (struct iwx_softc*)ifp->if_softc;
    ItlIwx *that = container_of(sc, ItlIwx, com);
    Start2LContext context2L = {};
    that->path2L(1);
    const IOReturn result2L = that->getMainCommandGate()->attemptAction(
        _iwx_start_task, &that->com.sc_ic.ic_ac.ac_if, &context2L);
    that->path2L(2, uint32_t(result2L), context2L.entered ? 1U : 0U);
    if (!context2L.entered && result2L == kIOReturnCannotLock)
        that->drain2MRequest();
    that->path2LPublish();
}

struct Test {
 ItlIwx x;Provider p;Test(){x.pci.pa_tag=&p;assert(x.drain2MSetup());auto epoch=x.drain2MPause();x.drain2MArm(epoch);}
 ~Test(){x.drain2MRemove();assert(x.refs==1);if(x.fDrain2MLock)IOSimpleLockFree(x.fDrain2MLock);}
 void enqueue(Packet&m){std::lock_guard<std::recursive_mutex> l(x.com.sc_ic.ic_mgtq.store.mutex);x.com.sc_ic.ic_mgtq.store.packets.push_back(&m);}
 void start(){x.iwx_start(&x.com.sc_ic.ic_if);}
};
void contended(ItlIwx&x,const std::function<void()>&f){std::promise<void> held,done;auto ready=held.get_future();auto finish=done.get_future();std::thread holder([&]{std::lock_guard<std::recursive_mutex> l(x.loop.gate);held.set_value();finish.wait();});ready.wait();f();done.set_value();holder.join();}
int main(){
 {Test t;Packet m;t.enqueue(m);contended(t.x,[&]{t.start();});assert(!m.consumed&&t.x.fDrain2MPending&&t.x.fPath2L.word[6]==1&&t.x.txCalls==0);t.x.loop.deliver();assert(m.consumed&&t.x.txCalls==1&&t.x.fDrain2MStats[7]==1&&t.x.fPath2L.word[11]==1&&t.x.fPath2L.firstFailure.result==uint32_t(kIOReturnCannotLock));assert(t.x.fPath2L.word[17]==1&&t.x.fPath2L.word[18]==1);FILE*f=fopen("drain-fixture.bin","wb");assert(f&&fwrite(t.p.drain.data(),1,t.p.drain.size(),f)==96);fclose(f);}
 puts("PASS actual gate contention -> software event -> recursive serialized drain -> exactly one management TX; refreshed counters and first-failure preservation");
 {Test t;Packet a,b,c;t.enqueue(a);t.enqueue(b);t.enqueue(c);contended(t.x,[&]{for(int i=0;i<100;++i)t.start();});assert(t.x.fDrain2MStats[2]==1&&t.x.fDrain2MStats[3]==99);t.x.loop.deliver();assert(t.x.txCalls==3&&t.x.fDrain2MStats[6]==1);t.x.loop.deliver();assert(t.x.txCalls==3);}
 {Test t;Packet m;t.enqueue(m);contended(t.x,[&]{t.start();});t.start();assert(t.x.txCalls==1);t.x.loop.deliver();assert(t.x.txCalls==1);}
 puts("PASS repeated contention coalesces; intervening successful fast path plus stale event never duplicates dequeue/TX");
 {Test t;Packet a,b;t.enqueue(a);contended(t.x,[&]{t.start();});bool once=false;t.x.duringTx=[&]{if(once)return;once=true;std::thread producer([&]{t.enqueue(b);t.x.drain2MRequest();});producer.join();};t.x.loop.deliver();t.x.loop.deliver();assert(t.x.txCalls==2&&t.x.sent[0]==&a&&t.x.sent[1]==&b);}
 {Test t;Packet a,b;t.enqueue(a);t.x.com.sc_ic.ic_if.if_snd.packets.push_back(&b);contended(t.x,[&]{t.start();});t.x.loop.deliver();assert(t.x.sent.size()==2&&t.x.sent[0]==&a&&t.x.sent[1]==&b&&b.encapsulated);}
 puts("PASS request during callback is not lost; management-before-data ordering and original ownership preserved");
 {Test t;Packet m;t.enqueue(m);t.x.txResult=ENOMEM;contended(t.x,[&]{t.start();});t.x.loop.deliver();assert(m.consumed&&m.node.releases==1&&t.x.fPath2L.word[15]==1);}
 {Test t;Packet m;t.enqueue(m);contended(t.x,[&]{t.start();});t.x.com.qfullmsk=1;t.x.loop.deliver();assert(!m.consumed&&t.x.com.sc_ic.ic_if.if_snd.ifq_oactive==1&&t.x.fDrain2MStats[6]==1);t.x.loop.deliver();assert(t.x.fDrain2MStats[6]==1);}
 {Test t;Packet m;t.enqueue(m);contended(t.x,[&]{t.start();});t.x.gate.forced=kIOReturnNotPermitted;t.x.loop.deliver();assert(!m.consumed&&t.x.fDrain2MStats[8]==1&&t.x.fDrain2MStats[9]==uint32_t(kIOReturnNotPermitted)&&!t.x.fDrain2MPending);}
 puts("PASS original TX error/node release, backpressure and disabled-gate semantics; no self-rescheduling busy loop");
 {Test t;Packet m;t.enqueue(m);contended(t.x,[&]{t.start();});auto e1=t.x.drain2MPause();auto e2=t.x.drain2MPause();t.x.drain2MArm(e1);assert(t.x.fDrain2MPaused&&t.x.fDrain2MStats[23]==1);t.x.drain2MArm(e2);t.x.loop.deliver();assert(!m.consumed&&t.x.txCalls==0);contended(t.x,[&]{t.start();});t.x.loop.deliver();assert(m.consumed&&t.x.txCalls==1);}
 puts("PASS stop cancels pending request; stale event is harmless; older init cannot arm after a newer lifecycle epoch");
 for(bool remove:{false,true}){
  Test t;Packet m;t.enqueue(m);contended(t.x,[&]{t.start();});std::promise<void> entered,finish;auto started=entered.get_future();auto finishwait=finish.get_future();t.x.duringTx=[&]{entered.set_value();finishwait.wait();};auto action=std::async(std::launch::async,[&]{t.x.loop.deliver();});started.wait();auto fence=std::async(std::launch::async,[&]{if(remove)t.x.drain2MRemove();else t.x.drain2MPause();});assert(fence.wait_for(std::chrono::milliseconds(30))==std::future_status::timeout);finish.set_value();action.get();fence.get();assert(m.consumed&&t.x.txCalls==1);t.x.loop.deliver();assert(t.x.txCalls==1);
 }
 puts("PASS active callback finishes BEFORE pause/reset fence and synchronous detach removal return");
 for(int iteration=0;iteration<40;++iteration){Test t;std::atomic<bool> go{false};std::vector<std::thread> threads;for(int n=0;n<4;++n)threads.emplace_back([&]{while(!go.load())std::this_thread::yield();for(int i=0;i<1000;++i)t.x.drain2MRequest();});go=true;t.x.drain2MRemove();for(auto&th:threads)th.join();assert(t.x.refs==1&&t.x.loop.event==nullptr&&t.x.fDrain2MEvent==nullptr);t.x.loop.deliver();}
 puts("PASS 160000 concurrent producer/remove requests: closed-pointer serialization, no source/owner use-after-free, idempotent teardown");
 for(int failure=0;failure<3;++failure){ItlIwx x;failLock=failure==0;failEvent=failure==1;x.loop.addError=failure==2?kIOReturnError:0;assert(!x.drain2MSetup());x.drain2MRemove();assert(x.refs==1);if(x.fDrain2MLock)IOSimpleLockFree(x.fDrain2MLock);failLock=failEvent=false;}
 {Test t;auto*s=t.x.fDrain2MEvent;t.x.loop.removeError=kIOReturnError;t.x.drain2MRemove();assert(t.x.refs==2&&s->refs==2&&t.x.fDrain2MClosed);t.x.drain2MRequest();t.x.loop.deliver();assert(t.x.fDrain2MStats[6]==0);/* Test-only end of retained lifetime, modelling reboot; not production repair. */t.x.loop.removeError=0;t.x.loop.removeEventSource(s);s->release();t.x.release();}
 assert(liveLocks==0&&liveEvents==0);
 puts("PASS setup failures unwind; unexpected removal failure retains closed owner/source rather than freeing callback backing");
 puts("ALL 2M SOURCE-DERIVED OWNERSHIP/SCHEDULING/RACE TESTS PASS (mock SDK/workloop, not kernel/runtime proof)");
}
