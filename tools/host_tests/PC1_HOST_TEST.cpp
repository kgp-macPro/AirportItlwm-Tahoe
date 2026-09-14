#include <cassert>
#include <thread>
#include <mutex>
#include <future>
#include <atomic>
#include <functional>
#include <algorithm>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cstdio>
using IOReturn=int;
#include <cerrno>
constexpr int kIOReturnBusy=0x105;
constexpr unsigned IWX_CMD_QUEUE_SIZE_GEN3=128;
struct iwx_cmd_header_wide { uint64_t bytes; }; using UInt64=uint64_t; using UInt32=uint32_t;
constexpr int kIOReturnSuccess=0,kIOReturnNoMemory=0x101,kIOReturnBadArgument=0x102,kIOReturnError=0x103,kIOReturnNoResources=0x104;
constexpr int kIODirectionIn=1,kIODirectionOut=2,kIODirectionInOut=3,kIOMemoryPhysicallyContiguous=16,kIOMapInhibitCache=32;
constexpr size_t IWX_RBUF_SIZE=4096; constexpr unsigned IWX_RX_MQ_RING_COUNT=512;
#define DMA_BIT_MASK(x) UINT64_MAX
void *kernel_task=nullptr;
int failure=0,liveMD=0,liveCmd=0,genCalls=0,syncCalls=0;size_t requestSize=0;
struct IOBufferMemoryDescriptor {
 std::vector<uint8_t> bytes; int refs=1;bool prepared=false;int commandRefs=0;
 explicit IOBufferMemoryDescriptor(size_t size):bytes(size,0xa5){++liveMD;}
 static IOBufferMemoryDescriptor* inTaskWithPhysicalMask(void*,int options,size_t size,uint64_t mask){
  assert(options==(kIODirectionInOut|kIOMemoryPhysicallyContiguous|kIOMapInhibitCache));assert(mask==UINT64_MAX);requestSize=size;
  return failure==1?nullptr:new IOBufferMemoryDescriptor(size);
 }
 IOReturn prepare(){if(failure==2)return 0x202;assert(!prepared);prepared=true;return 0;}
 void complete(){assert(prepared&&commandRefs==0);prepared=false;}
 void release(){assert(refs>0);if(--refs==0){assert(!prepared&&commandRefs==0);--liveMD;delete this;}}
 void* getBytesNoCopy(){return failure==11?nullptr:bytes.data();}
};
struct IODMACommand {
 enum MappingOptions{kMapped};struct Segment64{uint64_t fIOVMAddr,fLength;};
 IOBufferMemoryDescriptor* md=nullptr;bool active=false;
 static IODMACommand* withSpecification(int output,int bits,int maxseg,MappingOptions mode,int maxtransfer,unsigned alignment){
  assert(output==64&&bits==64&&maxseg==0&&mode==kMapped&&maxtransfer==0&&alignment==4096);
  if(failure==3)return nullptr;++liveCmd;return new IODMACommand;
 }
 IOReturn setMemoryDescriptor(IOBufferMemoryDescriptor* m){assert(m->prepared);md=m;++m->refs;++m->commandRefs;if(failure==4)return 0x204;active=true;return 0;}
 IOReturn gen64IOVMSegments(UInt64* off,Segment64* seg,UInt32* count){assert(active);++genCalls;if(failure==5)return 0x205;
  *off=requestSize-(failure==7?1:0);*count=failure==6?2:1;seg->fLength=requestSize-(failure==8?1:0);seg->fIOVMAddr=0x55550000ULL+(failure==9?1:0);return 0;
 }
 IOReturn synchronize(int direction){assert(active);++syncCalls;assert(direction==kIODirectionIn||direction==kIODirectionOut);return failure==10?0x207:(failure==12?0x209:0);}
 void clearMemoryDescriptor(){if(md){active=false;--md->commandRefs;md->release();md=nullptr;}}
 void release(){assert(!md&&!active);--liveCmd;delete this;}
};
constexpr int kIODMACommandOutputHost64=64;
struct iwx_dma_info {IOBufferMemoryDescriptor* buffer=nullptr;uint64_t paddr=0;void*vaddr=nullptr;size_t size=0;IOBufferMemoryDescriptor*bmd=nullptr;IODMACommand*cmd=nullptr;};

constexpr size_t PAGE_SIZE=4096;
constexpr unsigned IEEE80211_FC0_TYPE_DATA=8,IEEE80211_FC0_TYPE_MGT=0;
#define MIN(a,b) std::min((a),(b))
#define XYLog(...) ((void)0)
struct IOPhysicalSegment {uint64_t location,length;};
struct Packet {std::vector<std::vector<uint8_t>> chain; bool freed=false;};using mbuf_t=Packet*;
std::function<void()> duringCopy; int copyError=0;
size_t mbuf_pkthdr_len(mbuf_t p){size_t n=0;for(auto &x:p->chain)n+=x.size();return n;}
int mbuf_copydata(mbuf_t p,size_t off,size_t len,void* dst){assert(off==0&&!p->freed);if(duringCopy){auto f=std::move(duringCopy);duringCopy={};f();}if(copyError)return copyError;size_t n=0;for(auto&x:p->chain){memcpy((uint8_t*)dst+n,x.data(),x.size());n+=x.size();}assert(n==len);return 0;}
struct iwx_node {};
struct iwx_tx_data {void*map=nullptr;uint64_t cmd_paddr=0;mbuf_t m=nullptr;iwx_node*in=nullptr;int flags=0;uint8_t type=0;uint16_t tx2I_token=0;};

constexpr int IWX_DQA_CMD_QUEUE=0,IWX_DEVICE_FAMILY_AX210=210,IWX_INVALID_QUEUE=-1;
struct iwx_tx_ring {iwx_dma_info desc_dma,cmd_dma,bc_tbl;void*desc=nullptr;iwx_tx_data data[128];int ring_count=128,qid=0,queued=1,cur=14,tail=0,hi_mark=0,low_mark=0;};
struct iwx_softc {struct {bool uc_ok=true;} sc_uc; int sc_generation=7,sc_device_family=IWX_DEVICE_FAMILY_AX210;unsigned qfullmsk=0;void*sc_dmat=nullptr;iwx_tx_ring txq[2];};
int mbufFrees=0;
void mbuf_freem(mbuf_t m){assert(m&&!m->freed);m->freed=true;++mbufFrees;}
void bus_dmamap_destroy(void*,void*){}

struct Provider {std::vector<uint8_t> wire,flow,command,pc1;bool setProperty(const char*n,const void*p,size_t l){if(!strcmp(n,"KGP_PC1"))pc1.assign((const uint8_t*)p,(const uint8_t*)p+l);if(!strcmp(n,"KGP_TX_2I"))wire.assign((const uint8_t*)p,(const uint8_t*)p+l);if(!strcmp(n,"KGP_FLOW_2J"))flow.assign((const uint8_t*)p,(const uint8_t*)p+l);if(!strcmp(n,"KGP_CMD_2K"))command.assign((const uint8_t*)p,(const uint8_t*)p+l);return true;}};
struct ItlIwx; using OSObject=ItlIwx;
constexpr int kIOReturnAborted=0x301,kIOReturnNotReady=0x302;
struct ItlIwx {
    enum { kTx2ICount = 1024, kTx2ISize = 8192 };
    enum { Tx2IEmpty = 0, Tx2IFree, Tx2ICopying, Tx2IInflight, Tx2IQuarantined };
    struct Tx2IBacking {
        struct iwx_dma_info dma;
        struct iwx_tx_data *owner;
        uint32_t state;
        uint32_t length;
    } fTx2I[kTx2ICount] = {};
    uint32_t fTx2IStats[32] = {};
    uint64_t fTx2IFirst[4] = {};
    uint32_t fTx2INext = 0;
    uint32_t fTx2IBlocked = 0;
    uint32_t fTx2IReady = 0;
    bool tx2IAllocate();
    void tx2IError(unsigned, unsigned, uint32_t);
    void tx2IReport(unsigned);
    int tx2IPrepare(struct iwx_tx_data *, mbuf_t, int, int, IOPhysicalSegment *);
    void tx2IPublished(struct iwx_tx_data *);
    void tx2IRetire(struct iwx_tx_data *, bool);
    // One atomic word serializes admission with nested stop windows. Low 16
    // bits count active iwx_tx calls; bits 16..30 count stop scopes; bit 31
    // remains closed for an unresolved ownership boundary. No waiting/lock.
    enum : uint32_t { kTx2JStop = 0x10000U, kTx2JClosed = 0x80000000U };
    uint32_t fTx2JAdmission = 0;
    bool tx2JEnter();
    void tx2JLeave();
    bool tx2IStop(unsigned);
    void tx2JStopEnd(bool, unsigned);
    struct Tx2JScope {
        ItlIwx *owner;
        ~Tx2JScope() { if (owner) owner->tx2JLeave(); }
    };
    struct Flow2JRecord {
        uint64_t ns;
        uint32_t event, code, a, b, c, status;
        uint64_t address, extra;
    };
    struct Flow2JData {
        uint32_t schema, capacity, count, dropped, snapshots, failures, overflow, firstValid;
        Flow2JRecord firstFailure;
        Flow2JRecord records[128];
    } fFlow2J = {};
    uint32_t fFlow2JBusy = 0, fFlow2JDropped = 0;
    static_assert(sizeof(Flow2JRecord) == 48 && sizeof(Flow2JData) == 6224, "2J wire layout");
    void flow2J(unsigned, unsigned, uint32_t = 0, uint32_t = 0, uint32_t = 0,
                uint32_t = 0, uint64_t = 0, uint64_t = 0);
    void flow2JPublish();

    void tx2IFree();

    enum { kCmd2KCount = IWX_CMD_QUEUE_SIZE_GEN3, kCmd2KSize = 4096 };
    enum { Cmd2KFree = 0, Cmd2KCopying, Cmd2KInflight, Cmd2KCompleting, Cmd2KDone, Cmd2KQuarantined };
    struct Cmd2KBacking {
        iwx_dma_info dma;
        uint32_t state, serial, code, generation, callerDone, length;
        mbuf_t packet;
        uint64_t observedAddress;
    } fCmd2K[kCmd2KCount] = {};
    uint32_t fCmd2KFrozen = 0, fCmd2KResetDepth = 0, fCmd2KEpoch = 0;
    uint32_t fCmd2KStats[32] = {};
    uint64_t fCmd2KFirst[8] = {}, fCmd2KLast[8] = {};
    uint32_t fCmd2KDiagBusy = 0;
    bool cmd2KAvailable(unsigned);
    bool cmd2KClaim(unsigned, uint32_t, uint32_t, uint32_t, uint32_t *);
    int cmd2KPrepare(unsigned, uint32_t, uint32_t, mbuf_t, size_t, iwx_tx_data *, bool *, uint64_t *, void **);
    void cmd2KFinish(unsigned, uint32_t, bool, int);
    int cmd2KDone(unsigned, int, iwx_tx_data *);
    void cmd2KDoneEnd(unsigned);
    void cmd2KResetBegin();
    void cmd2KResetEnd();
    void cmd2KQuarantine(unsigned);
    void cmd2KError(unsigned, unsigned, uint32_t);
    void cmd2KObserve(unsigned, unsigned, uint32_t = 0);
    void cmd2KReport(unsigned);
    void cmd2KFree();

    void iwx_reset_tx_ring(struct iwx_softc *, struct iwx_tx_ring *);
    void iwx_free_tx_ring(struct iwx_softc *, struct iwx_tx_ring *);
    struct {Provider*pa_tag;} pci={nullptr};
    iwx_softc com;
    // PC1 control is serialized by the existing main workloop, not a new worker.
    struct Pc1Advance { iwx_tx_ring *ring; int index; unsigned generation; };
    uint32_t fPc1ResetEpoch=0, fPc1TerminalEpoch=0, fPc1ExpectedGeneration=0;
    uint32_t fPc1LastMaster=0, fPc1Fault=0, fPc1Recoveries=0, fPc1QuarantineCount=0;
    uint32_t fPc1Suspended=0, fPc1DroppedCompletion=0, fPc1StaleFlush=0;
    uint32_t fPc1LastRefusal=0, fPc1PublishFailures=0, fPc1Snapshots=0;
    unsigned advances=0;
    std::recursive_mutex gate;
    void iwx_ampdu_txq_advance(iwx_softc *,iwx_tx_ring *,int,bool){++advances;}
    void pc1Report();
    static IOReturn pc1StopAction(OSObject *,void *,void *,void *,void *);
    static IOReturn pc1RingAction(OSObject *,void *,void *,void *,void *);
    static IOReturn pc1AdvanceAction(OSObject *,void *,void *,void *,void *);
    static IOReturn pc1StopEndAction(OSObject *,void *,void *,void *,void *);
    static IOReturn pc1RecoverAction(OSObject *,void *,void *,void *,void *);

    uint64_t time2GNow(){static uint64_t n=0;return ++n;}
    void iwx_dma_contig_free(iwx_dma_info*);
};
static IOReturn allocatePacketBacking2H(struct iwx_dma_info *dma, size_t size,
                                        unsigned alignment, unsigned *stage)
{
    *stage = 1;
    IOBufferMemoryDescriptor *bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous | kIOMapInhibitCache,
        size, DMA_BIT_MASK(64));
    if (!bmd) return kIOReturnNoMemory;
    *stage = 2;
    IOReturn result = bmd->prepare();
    if (result != kIOReturnSuccess) { bmd->release(); return result; }
    *stage = 3;
    IODMACommand *cmd = IODMACommand::withSpecification(kIODMACommandOutputHost64,
        64, 0, IODMACommand::kMapped, 0, alignment);
    if (!cmd) { bmd->complete(); bmd->release(); return kIOReturnNoMemory; }
    *stage = 4;
    result = cmd->setMemoryDescriptor(bmd);
    if (result != kIOReturnSuccess) {
        cmd->clearMemoryDescriptor(); cmd->release(); bmd->complete(); bmd->release(); return result;
    }
    *stage = 5;
    UInt64 offset = 0;
    UInt32 count = 1;
    IODMACommand::Segment64 seg = {};
    result = cmd->gen64IOVMSegments(&offset, &seg, &count);
    if (result == kIOReturnSuccess && (count != 1 || offset != size || seg.fLength != size ||
        !alignment || (seg.fIOVMAddr & (alignment - 1)) || !bmd->getBytesNoCopy())) {
        *stage = 6;
        result = kIOReturnBadArgument;
    }
    if (result != kIOReturnSuccess) {
        cmd->clearMemoryDescriptor(); cmd->release(); bmd->complete(); bmd->release(); return result;
    }
    void *bytes = bmd->getBytesNoCopy();
    memset(bytes, 0, size);
    *stage = 7;
    result = cmd->synchronize(kIODirectionOut);
    if (result != kIOReturnSuccess) {
        cmd->clearMemoryDescriptor(); cmd->release(); bmd->complete(); bmd->release(); return result;
    }
    dma->paddr = seg.fIOVMAddr;
    dma->vaddr = bytes;
    dma->size = size;
    dma->buffer = bmd;
    dma->cmd = cmd;
    *stage = 0;
    return kIOReturnSuccess;
}
void ItlIwx::tx2IReport(unsigned reason)
{
    // Bounded snapshots only: allocation, first error/quarantine, and selected
    // publication/completion milestones. No provider ownership is added.
    uint32_t wire[32] = {};
    const uint32_t first = __atomic_load_n(&fTx2IStats[16], __ATOMIC_ACQUIRE);
    for (unsigned i = 0; i < 32; ++i)
        wire[i] = __atomic_load_n(&fTx2IStats[i], __ATOMIC_RELAXED);
    wire[0] = 1; wire[1] = reason;
    wire[14] = __atomic_load_n(&fTx2IBlocked, __ATOMIC_ACQUIRE);
    wire[16] = first;
    if (first != 2) wire[17] = wire[18] = wire[19] = 0;
    const bool ok = pci.pa_tag && pci.pa_tag->setProperty("KGP_TX_2I", wire, sizeof(wire));
    if (!ok) __atomic_fetch_add(&fTx2IStats[15], 1U, __ATOMIC_RELAXED);
    uint64_t sample[4] = {};
    if (__atomic_load_n(&fTx2IStats[20], __ATOMIC_ACQUIRE) == 2) {
        for (unsigned i = 0; i < 4; ++i)
            sample[i] = __atomic_load_n(&fTx2IFirst[i], __ATOMIC_RELAXED);
        if (pci.pa_tag) pci.pa_tag->setProperty("KGP_TX_FIRST_2I", sample, sizeof(sample));
    }
    XYLog("KGP_TX_2I reason=%u allocated=%u submitted=%u completed=%u quarantined=%u blocked=%u first=%u:%u:0x%08x property=%u\n",
          reason, wire[2], wire[5], wire[6], wire[7], wire[14],
          wire[17], wire[18], wire[19], unsigned(ok));
}
void ItlIwx::tx2IError(unsigned token, unsigned stage, uint32_t result)
{
    uint32_t expected = 0;
    if (__atomic_compare_exchange_n(&fTx2IStats[16], &expected, 1U, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
        __atomic_store_n(&fTx2IStats[17], token, __ATOMIC_RELAXED);
        __atomic_store_n(&fTx2IStats[18], stage, __ATOMIC_RELAXED);
        __atomic_store_n(&fTx2IStats[19], result, __ATOMIC_RELAXED);
        __atomic_store_n(&fTx2IStats[16], 2U, __ATOMIC_RELEASE);
        tx2IReport(3);
    }
}
bool ItlIwx::tx2IAllocate()
{
    fTx2IStats[0] = 1;
    for (unsigned i = 0; i < kTx2ICount; ++i) {
        unsigned stage = 0;
        IOReturn result = allocatePacketBacking2H(&fTx2I[i].dma, kTx2ISize, PAGE_SIZE, &stage);
        if (result != kIOReturnSuccess) {
            tx2IError(i + 1, stage, uint32_t(result)); return false;
        }
        __atomic_store_n(&fTx2I[i].state, uint32_t(Tx2IFree), __ATOMIC_RELEASE);
        ++fTx2IStats[2];
    }
    __atomic_store_n(&fTx2IReady, 1U, __ATOMIC_RELEASE);
    tx2IReport(1);
    return true;
}
int ItlIwx::tx2IPrepare(struct iwx_tx_data *data, mbuf_t packet, int qid, int idx,
                       IOPhysicalSegment *segments)
{
    __atomic_fetch_add(&fTx2IStats[3], 1U, __ATOMIC_RELAXED);
    if (__atomic_load_n(&fTx2IBlocked, __ATOMIC_ACQUIRE)) {
        __atomic_fetch_add(&fTx2IStats[8], 1U, __ATOMIC_RELAXED); return 0;
    }
    const size_t length = mbuf_pkthdr_len(packet);
    if (!length || length > kTx2ISize || data->m ||
        __atomic_load_n(&data->tx2I_token, __ATOMIC_ACQUIRE)) {
        __atomic_fetch_add(&fTx2IStats[10], 1U, __ATOMIC_RELAXED);
        tx2IError(0, 10, uint32_t(kIOReturnBadArgument)); return 0;
    }
    unsigned token = 0;
    for (unsigned n = 0; n < kTx2ICount; ++n) {
        const unsigned i = (fTx2INext + n) % kTx2ICount;
        uint32_t expected = Tx2IFree;
        if (__atomic_compare_exchange_n(&fTx2I[i].state, &expected, uint32_t(Tx2ICopying),
                                         false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
            token = i + 1; fTx2INext = (i + 1) % kTx2ICount; break;
        }
    }
    if (!token) {
        __atomic_fetch_add(&fTx2IStats[9], 1U, __ATOMIC_RELAXED);
        tx2IError(0, 11, uint32_t(kIOReturnNoResources)); return 0;
    }
    Tx2IBacking &backing = fTx2I[token - 1];
    __atomic_store_n(&backing.owner, data, __ATOMIC_RELEASE); backing.length = uint32_t(length);
    __atomic_store_n(&data->tx2I_token, uint16_t(token), __ATOMIC_RELEASE);
    // mbuf_copydata copies the complete final chain after encryption and trim;
    // it does not replace/free the mbuf or transfer the caller's node reference.
    const int copied = mbuf_copydata(packet, 0, length, backing.dma.vaddr);
    IOReturn result = copied ? kIOReturnError : backing.dma.cmd->synchronize(kIODirectionOut);
    if (copied || result != kIOReturnSuccess) {
        __atomic_fetch_add(&fTx2IStats[copied ? 11 : 12], 1U, __ATOMIC_RELAXED);
        tx2IError(token, copied ? 12 : 13, copied ? uint32_t(copied) : uint32_t(result));
        // Never published. A concurrent reset may already have quarantined it;
        // only our still-Copying state can be returned to the free pool.
        __atomic_exchange_n(&data->tx2I_token, uint16_t(0), __ATOMIC_ACQ_REL);
        uint32_t expected = Tx2ICopying;
        __atomic_store_n(&backing.owner, static_cast<iwx_tx_data *>(nullptr), __ATOMIC_RELEASE);
        __atomic_compare_exchange_n(&backing.state, &expected, uint32_t(Tx2IFree),
                                     false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
        return 0;
    }
    // Preserve the header's TB limit (4K-4), and never cross a page boundary.
    // All values derive from the single validated 8K IOVM extent, not the cursor.
    unsigned count = 0;
    for (size_t offset = 0; offset < length;) {
        const uint64_t address = backing.dma.paddr + offset;
        const size_t pageLeft = PAGE_SIZE - (address & (PAGE_SIZE - 1));
        const size_t bytes = MIN(length - offset, MIN(size_t(4092), pageLeft));
        segments[count].location = address; segments[count].length = bytes;
        ++count; offset += bytes;
    }
    uint32_t expected = Tx2ICopying;
    if (__atomic_load_n(&fTx2IBlocked, __ATOMIC_ACQUIRE) ||
        !__atomic_compare_exchange_n(&backing.state, &expected, uint32_t(Tx2IInflight),
                                      false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
        tx2IRetire(data, false); return 0;
    }
    __atomic_fetch_add(&fTx2IStats[4], 1U, __ATOMIC_RELAXED);
    uint32_t sampleExpected = 0;
    if (__atomic_compare_exchange_n(&fTx2IStats[20], &sampleExpected, 1U, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
        __atomic_store_n(&fTx2IFirst[0], backing.dma.paddr, __ATOMIC_RELAXED);
        __atomic_store_n(&fTx2IFirst[1], uint64_t(length), __ATOMIC_RELAXED);
        __atomic_store_n(&fTx2IFirst[2], (uint64_t(uint32_t(qid)) << 32) | uint32_t(idx), __ATOMIC_RELAXED);
        __atomic_store_n(&fTx2IFirst[3], (uint64_t(token) << 32) | count, __ATOMIC_RELAXED);
        __atomic_store_n(&fTx2IStats[20], 2U, __ATOMIC_RELEASE);
    }
    return int(count);
}
void ItlIwx::tx2IPublished(struct iwx_tx_data *data)
{
    const uint32_t n = __atomic_add_fetch(&fTx2IStats[5], 1U, __ATOMIC_RELAXED);
    const unsigned kind = data->type == IEEE80211_FC0_TYPE_DATA ? 22 :
        (data->type == IEEE80211_FC0_TYPE_MGT ? 23 : 24);
    const uint32_t typed = __atomic_add_fetch(&fTx2IStats[kind], 1U, __ATOMIC_RELAXED);
    if (n == 1) __atomic_store_n(&fTx2IStats[29], uint32_t(data->type), __ATOMIC_RELAXED);
    if (n == 1 || n == 16 || n == 256 || n == 1024) tx2IReport(2);
    else if (kind == 22 && typed == 1) tx2IReport(7);
}
void ItlIwx::tx2IRetire(struct iwx_tx_data *data, bool normal)
{
    const unsigned token = __atomic_exchange_n(&data->tx2I_token, uint16_t(0), __ATOMIC_ACQ_REL);
    if (!token) return;
    if (token > kTx2ICount || __atomic_load_n(&fTx2I[token - 1].owner, __ATOMIC_ACQUIRE) != data) {
        __atomic_fetch_or(&fTx2JAdmission, kTx2JClosed, __ATOMIC_ACQ_REL);
        __atomic_store_n(&fTx2IBlocked, 1U, __ATOMIC_RELEASE);
        tx2IError(token, 14, uint32_t(kIOReturnBadArgument)); return;
    }
    Tx2IBacking &backing = fTx2I[token - 1];
    uint32_t expected = Tx2IInflight;
    // IRQ TX/BA retirement and packet publication share the main workloop gate.
    // Ungated reset can win the token exchange; it always quarantines, never frees.
    if (normal && __atomic_compare_exchange_n(&backing.state, &expected, uint32_t(Tx2IFree),
                                               false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
        const uint32_t n = __atomic_add_fetch(&fTx2IStats[6], 1U, __ATOMIC_RELAXED);
        const unsigned kind = data->type == IEEE80211_FC0_TYPE_DATA ? 25 :
            (data->type == IEEE80211_FC0_TYPE_MGT ? 26 : 27);
        const uint32_t typed = __atomic_add_fetch(&fTx2IStats[kind], 1U, __ATOMIC_RELAXED);
        if (n == 1 || n == 16 || n == 256 || n == 1024) tx2IReport(4);
        else if (kind == 25 && typed == 1) tx2IReport(8);
        return;
    }
    __atomic_fetch_or(&fTx2JAdmission, kTx2JClosed, __ATOMIC_ACQ_REL);
    __atomic_store_n(&fTx2IBlocked, 1U, __ATOMIC_RELEASE);
    const uint32_t old = __atomic_exchange_n(&backing.state, uint32_t(Tx2IQuarantined), __ATOMIC_ACQ_REL);
    if (old != Tx2IQuarantined) {
        const uint32_t n = __atomic_add_fetch(&fTx2IStats[7], 1U, __ATOMIC_RELAXED);
        if (n == 1) tx2IReport(5);
    }
}
bool ItlIwx::tx2IStop(unsigned origin)
{
    if (!__atomic_load_n(&fTx2IReady, __ATOMIC_ACQUIRE)) return false;
    // Close admission atomically before examining ownership. Nested scopes do
    // not reopen it until the outermost stop has returned. An active publisher
    // may be between its final software check and WRPTR: never infer a fence.
    const uint32_t prior = __atomic_fetch_add(&fTx2JAdmission, kTx2JStop, __ATOMIC_ACQ_REL);
    uint32_t copying = 0, inflight = 0, quarantined = 0;
    for (unsigned i = 0; i < kTx2ICount; ++i) {
        const uint32_t state = __atomic_load_n(&fTx2I[i].state, __ATOMIC_ACQUIRE);
        copying += state == Tx2ICopying;
        inflight += state == Tx2IInflight;
        quarantined += state == Tx2IQuarantined;
    }
    if ((prior & 0xffffU) || copying || inflight || quarantined) {
        __atomic_fetch_or(&fTx2JAdmission, kTx2JClosed, __ATOMIC_ACQ_REL);
        __atomic_store_n(&fTx2IBlocked, 1U, __ATOMIC_RELEASE);
    }
    flow2J(1, origin, copying, inflight, quarantined, prior & 0xffffU,
            __atomic_load_n(&fTx2IStats[5], __ATOMIC_RELAXED),
            __atomic_load_n(&fTx2IStats[6], __ATOMIC_RELAXED));
    return true;
}
void ItlIwx::tx2IFree()
{
    __atomic_fetch_or(&fTx2JAdmission, kTx2JClosed, __ATOMIC_ACQ_REL);
    __atomic_store_n(&fTx2IBlocked, 1U, __ATOMIC_RELEASE);
    for (unsigned i = 0; i < kTx2ICount; ++i) {
        uint32_t expected = Tx2IFree;
        if (__atomic_compare_exchange_n(&fTx2I[i].state, &expected, uint32_t(Tx2IEmpty),
                                         false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
            iwx_dma_contig_free(&fTx2I[i].dma);
            ++fTx2IStats[13];
        } else if (fTx2I[i].dma.cmd) {
            // Intentional bounded retention through HAL destruction. These are
            // standard SDK objects with no callback into this kext. Reboot is the
            // experiment's recovery boundary; never release an unproven mapping.
            ++fTx2IStats[21];
        }
    }
}
void ItlIwx::iwx_dma_contig_free(struct iwx_dma_info *dma)
{
    if (dma == NULL || dma->cmd == NULL)
        return;
    if (dma->vaddr == NULL)
        return;
    dma->cmd->clearMemoryDescriptor();
    dma->cmd->release();
    dma->cmd = NULL;
    dma->buffer->complete();
    dma->buffer->release();
    dma->buffer = NULL;
    dma->vaddr = NULL;
}

bool ItlIwx::tx2JEnter()
{
    if (!__atomic_load_n(&fTx2IReady, __ATOMIC_ACQUIRE) ||
        __atomic_load_n(&fTx2IBlocked, __ATOMIC_ACQUIRE)) return false;
    uint32_t value = __atomic_load_n(&fTx2JAdmission, __ATOMIC_ACQUIRE);
    do {
        if ((value & 0xffff0000U) || (value & 0xffffU) == 0xffffU) return false;
    } while (!__atomic_compare_exchange_n(&fTx2JAdmission, &value, value + 1U,
                                          false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
    return true;
}
void ItlIwx::tx2JLeave()
{
    __atomic_fetch_sub(&fTx2JAdmission, 1U, __ATOMIC_RELEASE);
}
void ItlIwx::tx2JStopEnd(bool entered, unsigned origin)
{
    if (!entered) return;
    __atomic_fetch_sub(&fTx2JAdmission, kTx2JStop, __ATOMIC_ACQ_REL);
    flow2J(2, origin, __atomic_load_n(&fTx2JAdmission, __ATOMIC_ACQUIRE),
            __atomic_load_n(&fTx2IBlocked, __ATOMIC_ACQUIRE), com.sc_generation);
    tx2IReport(6);
    flow2JPublish();
}
void ItlIwx::flow2J(unsigned event, unsigned code, uint32_t a, uint32_t b,
                    uint32_t c, uint32_t status, uint64_t address, uint64_t extra)
{
    // Post-pool RAM-only recording. No payload/SSID/key bytes or new hardware
    // reads. Nonwaiting exclusion: concurrent observations may be dropped.
    if (!__atomic_load_n(&fTx2IReady, __ATOMIC_ACQUIRE)) return;
    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&fFlow2JBusy, &expected, 1U, false,
                                      __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        __atomic_fetch_add(&fFlow2JDropped, 1U, __ATOMIC_RELAXED); return;
    }
    const Flow2JRecord record = {time2GNow(), event, code, a, b, c, status, address, extra};
    unsigned slot = fFlow2J.count;
    if (slot < 128) ++fFlow2J.count;
    else { slot = 127; ++fFlow2J.overflow; }
    fFlow2J.records[slot] = record;
    // Preserve first original operation failure, not stop user-count metadata.
    if (!fFlow2J.firstValid && ((status && (event == 9 || event == 14 || event == 18 || event == 20 || event == 21)) || event == 16)) {
        fFlow2J.firstFailure = record; fFlow2J.firstValid = 1;
    }
    __atomic_store_n(&fFlow2JBusy, 0U, __ATOMIC_RELEASE);
}
void ItlIwx::flow2JPublish()
{
    if (!__atomic_load_n(&fTx2IReady, __ATOMIC_ACQUIRE)) return;
    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&fFlow2JBusy, &expected, 1U, false,
                                      __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        __atomic_fetch_add(&fFlow2JDropped, 1U, __ATOMIC_RELAXED); return;
    }
    if (fFlow2J.snapshots >= 64) {
        __atomic_store_n(&fFlow2JBusy, 0U, __ATOMIC_RELEASE); return;
    }
    fFlow2J.schema = 1; fFlow2J.capacity = 128;
    fFlow2J.dropped = __atomic_load_n(&fFlow2JDropped, __ATOMIC_RELAXED);
    ++fFlow2J.snapshots;
    // OSData copies this member buffer while writer exclusion is held; no large
    // kernel-stack snapshot, allocation before pool setup, or provider retain.
    const bool ok = pci.pa_tag && pci.pa_tag->setProperty("KGP_FLOW_2J", &fFlow2J, sizeof(fFlow2J));
    if (!ok) ++fFlow2J.failures;
    __atomic_store_n(&fFlow2JBusy, 0U, __ATOMIC_RELEASE);
    XYLog("KGP_FLOW_2J published=%u\n", unsigned(ok));
}
bool ItlIwx::cmd2KAvailable(unsigned idx)
{
    return idx < kCmd2KCount && !__atomic_load_n(&fCmd2KFrozen, __ATOMIC_SEQ_CST) &&
        !__atomic_load_n(&fCmd2KResetDepth, __ATOMIC_SEQ_CST) &&
        __atomic_load_n(&fCmd2K[idx].state, __ATOMIC_SEQ_CST) == Cmd2KFree;
}
bool ItlIwx::cmd2KClaim(unsigned idx, uint32_t code, uint32_t generation,
                         uint32_t epoch, uint32_t *serial)
{
    if (!cmd2KAvailable(idx)) return false;
    auto &b = fCmd2K[idx];
    uint32_t expected = Cmd2KFree;
    if (!__atomic_compare_exchange_n(&b.state, &expected, uint32_t(Cmd2KCopying),
                                      false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) return false;
    *serial = __atomic_add_fetch(&b.serial, 1U, __ATOMIC_SEQ_CST);
    __atomic_store_n(&b.code, code, __ATOMIC_SEQ_CST);
    __atomic_store_n(&b.generation, generation, __ATOMIC_SEQ_CST);
    __atomic_store_n(&b.callerDone, 0U, __ATOMIC_SEQ_CST);
    __atomic_store_n(&b.packet, mbuf_t(nullptr), __ATOMIC_SEQ_CST);
    if (epoch != __atomic_load_n(&fCmd2KEpoch, __ATOMIC_SEQ_CST) ||
        __atomic_load_n(&fCmd2KResetDepth, __ATOMIC_SEQ_CST) ||
        __atomic_load_n(&fCmd2KFrozen, __ATOMIC_SEQ_CST)) {
        expected = Cmd2KCopying;
        __atomic_compare_exchange_n(&b.state, &expected, uint32_t(Cmd2KFree),
                                     false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        return false;
    }
    return true;
}
void ItlIwx::cmd2KQuarantine(unsigned idx)
{
    auto &b = fCmd2K[idx];
    uint32_t state = __atomic_load_n(&b.state, __ATOMIC_SEQ_CST);
    while (state == Cmd2KCopying || state == Cmd2KInflight || state == Cmd2KCompleting) {
        if (__atomic_compare_exchange_n(&b.state, &state, uint32_t(Cmd2KQuarantined),
                                         false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            __atomic_fetch_add(&fCmd2KStats[7], 1U, __ATOMIC_RELAXED); break;
        }
    }
    if (__atomic_load_n(&b.state, __ATOMIC_SEQ_CST) == Cmd2KQuarantined)
        __atomic_store_n(&fCmd2KFrozen, 1U, __ATOMIC_SEQ_CST);
}
void ItlIwx::cmd2KError(unsigned idx, unsigned stage, uint32_t status)
{
    uint32_t expected = 0;
    if (__atomic_compare_exchange_n(&fCmd2KStats[12], &expected, 1U, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
        __atomic_store_n(&fCmd2KStats[13], stage, __ATOMIC_RELAXED);
        __atomic_store_n(&fCmd2KStats[14], status, __ATOMIC_RELAXED);
        __atomic_store_n(&fCmd2KStats[15], idx, __ATOMIC_RELAXED);
        __atomic_store_n(&fCmd2KStats[12], 2U, __ATOMIC_RELEASE);
    }
}
int ItlIwx::cmd2KPrepare(unsigned idx, uint32_t serial, uint32_t epoch, mbuf_t packet,
                         size_t length, iwx_tx_data *data, bool *linked,
                         uint64_t *address, void **bytes)
{
    auto &b = fCmd2K[idx];
    if (length > kCmd2KSize || length <= sizeof(iwx_cmd_header_wide) ||
        __atomic_load_n(&b.serial, __ATOMIC_SEQ_CST) != serial) {
        cmd2KError(idx, 8, kIOReturnBadArgument); return EINVAL;
    }
    if (!b.dma.cmd) {
        unsigned stage = 0;
        IOReturn result = allocatePacketBacking2H(&b.dma, kCmd2KSize, PAGE_SIZE, &stage);
        if (result != kIOReturnSuccess) { cmd2KError(idx, stage, uint32_t(result)); return ENOMEM; }
        __atomic_fetch_add(&fCmd2KStats[2], 1U, __ATOMIC_RELAXED);
    }
    __atomic_store_n(&b.observedAddress, uint64_t(b.dma.paddr), __ATOMIC_SEQ_CST);
    __atomic_store_n(&b.length, uint32_t(length), __ATOMIC_SEQ_CST);
    const int copied = mbuf_copydata(packet, 0, length, b.dma.vaddr);
    if (copied) {
        __atomic_fetch_add(&fCmd2KStats[9], 1U, __ATOMIC_RELAXED);
        cmd2KError(idx, 9, uint32_t(copied)); return ENOMEM;
    }
    const IOReturn synced = b.dma.cmd->synchronize(kIODirectionOut);
    if (synced != kIOReturnSuccess) {
        __atomic_fetch_add(&fCmd2KStats[10], 1U, __ATOMIC_RELAXED);
        cmd2KError(idx, 10, uint32_t(synced)); return ENOMEM;
    }
    __atomic_fetch_add(&fCmd2KStats[3], 1U, __ATOMIC_RELAXED);
    // Reset closes admission/increments epoch before inspecting states. A reset
    // either sees this Copying/Inflight state or makes the checks below fail.
    if (__atomic_load_n(&fCmd2KFrozen, __ATOMIC_SEQ_CST) ||
        __atomic_load_n(&fCmd2KResetDepth, __ATOMIC_SEQ_CST) ||
        epoch != __atomic_load_n(&fCmd2KEpoch, __ATOMIC_SEQ_CST) ||
        int(__atomic_load_n(&b.generation, __ATOMIC_SEQ_CST)) != com.sc_generation) return ENXIO;
    __atomic_store_n(&b.packet, packet, __ATOMIC_SEQ_CST);
    mbuf_t empty = nullptr;
    if (!__atomic_compare_exchange_n(&data->m, &empty, packet, false,
                                      __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        cmd2KError(idx, 11, kIOReturnBusy); return EBUSY;
    }
    *linked = true; // reset/CmdDone now share the original mbuf disposition.
    uint32_t expected = Cmd2KCopying;
    if (!__atomic_compare_exchange_n(&b.state, &expected, uint32_t(Cmd2KInflight),
                                      false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) return ENXIO;
    *address = b.dma.paddr; *bytes = b.dma.vaddr;
    cmd2KObserve(1, idx);
    return 0;
}
void ItlIwx::cmd2KFinish(unsigned idx, uint32_t serial, bool published, int result)
{
    auto &b = fCmd2K[idx];
    if (__atomic_load_n(&b.serial, __ATOMIC_SEQ_CST) != serial) return;
    if (published) {
        __atomic_fetch_add(&fCmd2KStats[4], 1U, __ATOMIC_RELAXED);
        if (__atomic_load_n(&b.code, __ATOMIC_SEQ_CST) == 0x010d)
            __atomic_fetch_add(&fCmd2KStats[22], 1U, __ATOMIC_RELAXED);
        if (result == EWOULDBLOCK) __atomic_fetch_add(&fCmd2KStats[24], 1U, __ATOMIC_RELAXED);
        if (result == ENXIO) __atomic_fetch_add(&fCmd2KStats[25], 1U, __ATOMIC_RELAXED);
        // A concurrently accepted CmdDone is positive evidence; timeout alone
        // cannot make a still-Inflight entry Free. Late ack after quarantine is
        // intentionally not used to recover this experimental attachment.
        if (result && __atomic_load_n(&b.state, __ATOMIC_SEQ_CST) == Cmd2KInflight)
            cmd2KQuarantine(idx);
    } else {
        uint32_t expected = Cmd2KCopying;
        __atomic_compare_exchange_n(&b.state, &expected, uint32_t(Cmd2KFree),
                                     false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }
    cmd2KObserve(published ? 2 : 3, idx, uint32_t(result));
    __atomic_store_n(&b.callerDone, 1U, __ATOMIC_SEQ_CST);
    uint32_t expected = Cmd2KDone;
    if (__atomic_compare_exchange_n(&b.state, &expected, uint32_t(Cmd2KFree),
                                     false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
        __atomic_fetch_add(&fCmd2KStats[6], 1U, __ATOMIC_RELAXED);
    cmd2KReport(published ? 2 : 3);
}
int ItlIwx::cmd2KDone(unsigned idx, int code, iwx_tx_data *data)
{
    if (idx >= kCmd2KCount) return -1;
    auto &b = fCmd2K[idx];
    uint32_t state = __atomic_load_n(&b.state, __ATOMIC_SEQ_CST);
    if (state == Cmd2KFree) return 0; // original inline/other command behavior.
    if (state != Cmd2KInflight ||
        uint32_t(code) != __atomic_load_n(&b.code, __ATOMIC_SEQ_CST) ||
        int(__atomic_load_n(&b.generation, __ATOMIC_SEQ_CST)) != com.sc_generation ||
        __atomic_load_n(&data->m, __ATOMIC_SEQ_CST) != __atomic_load_n(&b.packet, __ATOMIC_SEQ_CST) ||
        !__atomic_compare_exchange_n(&b.state, &state, uint32_t(Cmd2KCompleting),
                                      false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        __atomic_fetch_add(&fCmd2KStats[18], 1U, __ATOMIC_RELAXED);
        cmd2KQuarantine(idx); cmd2KObserve(5, idx, uint32_t(code)); cmd2KReport(5);
        return -1;
    }
    __atomic_fetch_add(&fCmd2KStats[5], 1U, __ATOMIC_RELAXED);
    if (uint32_t(code) == 0x010d) __atomic_fetch_add(&fCmd2KStats[23], 1U, __ATOMIC_RELAXED);
    return 1;
}
void ItlIwx::cmd2KDoneEnd(unsigned idx)
{
    auto &b = fCmd2K[idx];
    cmd2KObserve(4, idx);
    uint32_t expected = Cmd2KCompleting;
    if (__atomic_compare_exchange_n(&b.state, &expected, uint32_t(Cmd2KDone),
                                     false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) &&
        __atomic_load_n(&b.callerDone, __ATOMIC_SEQ_CST)) {
        expected = Cmd2KDone;
        if (__atomic_compare_exchange_n(&b.state, &expected, uint32_t(Cmd2KFree),
                                         false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            __atomic_fetch_add(&fCmd2KStats[6], 1U, __ATOMIC_RELAXED);
    }
    cmd2KReport(4);
}
void ItlIwx::cmd2KResetBegin()
{
    __atomic_fetch_add(&fCmd2KResetDepth, 1U, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&fCmd2KEpoch, 1U, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&fCmd2KStats[21], 1U, __ATOMIC_RELAXED);
    for (unsigned i = 0; i < kCmd2KCount; ++i) cmd2KQuarantine(i);
}
void ItlIwx::cmd2KResetEnd()
{
    __atomic_fetch_sub(&fCmd2KResetDepth, 1U, __ATOMIC_SEQ_CST);
}
void ItlIwx::cmd2KFree()
{
    for (unsigned i = 0; i < kCmd2KCount; ++i) {
        if (__atomic_load_n(&fCmd2K[i].state, __ATOMIC_SEQ_CST) == Cmd2KFree) {
            if (fCmd2K[i].dma.cmd) { iwx_dma_contig_free(&fCmd2K[i].dma); ++fCmd2KStats[17]; }
        } else if (fCmd2K[i].dma.cmd) ++fCmd2KStats[16];
    }
}
void ItlIwx::cmd2KObserve(unsigned event, unsigned idx, uint32_t status)
{
    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&fCmd2KDiagBusy, &expected, 1U, false,
                                      __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        __atomic_fetch_add(&fCmd2KStats[27], 1U, __ATOMIC_RELAXED); return;
    }
    auto &b = fCmd2K[idx];
    const uint32_t serial = __atomic_load_n(&b.serial, __ATOMIC_SEQ_CST);
    uint64_t row[8] = {time2GNow(), __atomic_load_n(&b.observedAddress, __ATOMIC_SEQ_CST),
        __atomic_load_n(&b.length, __ATOMIC_SEQ_CST),
        (uint64_t(__atomic_load_n(&b.code, __ATOMIC_SEQ_CST)) << 32) | idx,
        (uint64_t(__atomic_load_n(&b.generation, __ATOMIC_SEQ_CST)) << 32) | serial,
        (uint64_t(event) << 32) | __atomic_load_n(&b.state, __ATOMIC_SEQ_CST), status, 1};
    if (serial != __atomic_load_n(&b.serial, __ATOMIC_SEQ_CST)) row[7] = 0;
    memcpy(fCmd2KLast, row, sizeof(row));
    if (event == 1 && !fCmd2KFirst[7]) memcpy(fCmd2KFirst, row, sizeof(row));
    __atomic_store_n(&fCmd2KDiagBusy, 0U, __ATOMIC_RELEASE);
}
void ItlIwx::cmd2KReport(unsigned reason)
{
    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&fCmd2KDiagBusy, &expected, 1U, false,
                                      __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return;
    if (fCmd2KStats[26] >= 64) { __atomic_store_n(&fCmd2KDiagBusy, 0U, __ATOMIC_RELEASE); return; }
    struct { uint32_t word[32]; uint64_t first[8], last[8]; } wire = {};
    const uint32_t firstError = __atomic_load_n(&fCmd2KStats[12], __ATOMIC_ACQUIRE);
    for (unsigned i = 0; i < 32; ++i) wire.word[i] = __atomic_load_n(&fCmd2KStats[i], __ATOMIC_RELAXED);
    wire.word[0] = 1; wire.word[1] = reason;
    wire.word[11] = __atomic_load_n(&fCmd2KFrozen, __ATOMIC_SEQ_CST);
    wire.word[12] = firstError;
    if (firstError != 2) wire.word[13] = wire.word[14] = wire.word[15] = 0;
    wire.word[26] = ++fCmd2KStats[26];
    memcpy(wire.first, fCmd2KFirst, sizeof(wire.first)); memcpy(wire.last, fCmd2KLast, sizeof(wire.last));
    const bool ok = pci.pa_tag && pci.pa_tag->setProperty("KGP_CMD_2K", &wire, sizeof(wire));
    if (!ok) ++fCmd2KStats[20];
    __atomic_store_n(&fCmd2KDiagBusy, 0U, __ATOMIC_RELEASE);
    XYLog("KGP_CMD_2K reason=%u mapped=%u copied=%u submitted=%u done=%u quarantined=%u frozen=%u\n",
          reason, wire.word[2], wire.word[3], wire.word[4], wire.word[5], wire.word[7], wire.word[11]);
}
void ItlIwx::
iwx_reset_tx_ring(struct iwx_softc *sc, struct iwx_tx_ring *ring)
{
    const bool command2K = sc->sc_device_family >= IWX_DEVICE_FAMILY_AX210 && ring == &sc->txq[IWX_DQA_CMD_QUEUE];
    if (command2K) cmd2KResetBegin();
    int i;
    
    for (i = 0; i < ring->ring_count; i++) {
        struct iwx_tx_data *data = &ring->data[i];
        tx2IRetire(data, false);
        
        if (command2K) {
            mbuf_t retired2K = __atomic_exchange_n(&data->m, mbuf_t(nullptr), __ATOMIC_SEQ_CST);
            if (retired2K) mbuf_freem(retired2K);
        } else if (data->m != NULL) {
            //            bus_dmamap_sync(sc->sc_dmat, data->map, 0,
            //                data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
            //            bus_dmamap_unload(sc->sc_dmat, data->map);
            mbuf_freem(data->m);
            data->m = NULL;
        }
    }

    if (command2K && __atomic_load_n(&fCmd2KFrozen, __ATOMIC_SEQ_CST)) {
        cmd2KResetEnd(); return; // preserve descriptor bytes and software indices.
    }
    if (ring->qid == IWX_INVALID_QUEUE || !ring->desc) {
        if (command2K) cmd2KResetEnd();
        return;
    }
    
    /* Clear byte count table. */
    memset(ring->bc_tbl.vaddr, 0, ring->bc_tbl.size);
    
    /* Clear TX descriptors. */
    memset(ring->desc, 0, ring->desc_dma.size);
    //    bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map, 0,
    //        ring->desc_dma.size, BUS_DMASYNC_PREWRITE);
    sc->qfullmsk &= ~(1 << ring->qid);
    ring->queued = 0;
    ring->cur = 0;
    ring->tail = 0;
    if (command2K) cmd2KResetEnd();
}
void ItlIwx::
iwx_free_tx_ring(struct iwx_softc *sc, struct iwx_tx_ring *ring)
{
    const bool command2K = sc->sc_device_family >= IWX_DEVICE_FAMILY_AX210 && ring == &sc->txq[IWX_DQA_CMD_QUEUE];
    if (command2K) cmd2KResetBegin();
    int i;
    
    if (!(command2K && __atomic_load_n(&fCmd2KFrozen, __ATOMIC_SEQ_CST))) {
    iwx_dma_contig_free(&ring->desc_dma);
    iwx_dma_contig_free(&ring->cmd_dma);
    iwx_dma_contig_free(&ring->bc_tbl);
    } else {
        __atomic_store_n(&fCmd2KStats[28], 1U, __ATOMIC_RELAXED);
    }
    
    for (i = 0; i < ring->ring_count; i++) {
        struct iwx_tx_data *data = &ring->data[i];
        tx2IRetire(data, false);
        
        if (command2K) {
            mbuf_t retired2K = __atomic_exchange_n(&data->m, mbuf_t(nullptr), __ATOMIC_SEQ_CST);
            if (retired2K) mbuf_freem(retired2K);
        } else if (data->m != NULL) {
            //            bus_dmamap_sync(sc->sc_dmat, data->map, 0,
            //                data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
            //            bus_dmamap_unload(sc->sc_dmat, data->map);
            mbuf_freem(data->m);
            data->m = NULL;
        }
        if (data->map != NULL) {
            bus_dmamap_destroy(sc->sc_dmat, data->map);
            data->map = NULL;
        }
    }
    // A publisher can have passed its final check before reset quarantines it.
    // Preserve the command ring coordinates as well as DMA bytes so such a
    // late original WRPTR write cannot acquire an INVALID_QUEUE value.
    if (command2K && __atomic_load_n(&fCmd2KFrozen, __ATOMIC_SEQ_CST)) return;
    ring->qid = IWX_INVALID_QUEUE;
    ring->hi_mark = 0;
    ring->low_mark = 0;
    ring->ring_count = 0;
}
static Packet packet(size_t n){Packet p; for(size_t off=0;off<n;){size_t k=std::min(size_t(137),n-off);p.chain.emplace_back(k);for(size_t i=0;i<k;++i)p.chain.back()[i]=uint8_t((off+i)*31+7);off+=k;}return p;}
static void checkPayload(ItlIwx&x,iwx_tx_data&d,Packet&p,IOPhysicalSegment*seg,int n){
 auto&b=x.fTx2I[d.tx2I_token-1];size_t off=0;
 for(int i=0;i<n;++i){assert(seg[i].location==b.dma.paddr+off);assert(seg[i].length>0&&seg[i].length<=4092);assert((seg[i].location&4095)+seg[i].length<=4096);off+=seg[i].length;}
 assert(off==mbuf_pkthdr_len(&p)&&n<=23);
 size_t at=0;for(auto&v:p.chain)for(auto byte:v){assert(((uint8_t*)b.dma.vaddr)[at++]==byte);}
}
static void rebootOnlyCleanup(ItlIwx&x){for(auto&b:x.fTx2I)if(b.dma.cmd)x.iwx_dma_contig_free(&b.dma);assert(liveMD==0&&liveCmd==0);}
int runExistingTxTests(){
 static_assert(sizeof(iwx_tx_data)==40&&offsetof(iwx_tx_data,tx2I_token)==38,"stock layout");
 // Every allocator failure must preserve its original status and paired lifetime.
 for(int f=1;f<=11;++f){auto x=std::make_unique<ItlIwx>();failure=f;assert(!x->tx2IAllocate());assert(x->fTx2IStats[16]==2);x->tx2IFree();assert(liveMD==0&&liveCmd==0);}
 failure=0;
 auto x=std::make_unique<ItlIwx>();assert(x->tx2IAllocate());assert(x->fTx2IStats[2]==1024&&liveCmd==1024);
 const int maps=genCalls;
 iwx_tx_data d;d.type=IEEE80211_FC0_TYPE_DATA;IOPhysicalSegment segments[23]={};
 // Fragmented final (already encrypted/trimmed) bytes, boundary sizes, repeated
 // normal completion, pool wrap. Packet/node are never freed or reassigned here.
 for(unsigned j=0;j<2050;++j){size_t sizes[]={1,4091,4092,4093,4096,4097,8191,8192};auto p=packet(sizes[j%8]);
  int n=x->tx2IPrepare(&d,&p,2,j%1024,segments);assert(n>0);checkPayload(*x,d,p,segments,n);
  auto token=d.tx2I_token;auto&b=x->fTx2I[token-1];auto saved=std::vector<uint8_t>((uint8_t*)b.dma.vaddr,(uint8_t*)b.dma.vaddr+mbuf_pkthdr_len(&p));
  p.chain[0][0]^=0xff;assert(!memcmp(saved.data(),b.dma.vaddr,saved.size()));
  x->tx2IPublished(&d);x->tx2IRetire(&d,true);assert(!d.tx2I_token&&b.state==ItlIwx::Tx2IFree&&!p.freed);
 }
 assert(genCalls==maps&&x->fTx2IStats[6]==2050);assert(x->fTx2IStats[22]==2050&&x->fTx2IStats[25]==2050);x->tx2IFree();assert(!liveMD&&!liveCmd);
 puts("PASS fragmented final payload, IOVA/TB extent/page limits, immutability, 2050 completions/pool wrap, no remap");
 for(auto size:{size_t(0),size_t(8193)}){auto y=std::make_unique<ItlIwx>();assert(y->tx2IAllocate());auto p=packet(size);assert(!y->tx2IPrepare(&d,&p,2,0,segments));assert(!d.tx2I_token);y->tx2IFree();assert(!liveMD&&!liveCmd);}
 for(int f:{0,12}){auto y=std::make_unique<ItlIwx>();failure=0;assert(y->tx2IAllocate());auto p=packet(1500);copyError=f?0:77;failure=f;assert(!y->tx2IPrepare(&d,&p,2,0,segments));assert(y->fTx2IStats[19]==(f?0x209:77));failure=0;copyError=0;y->tx2IFree();assert(!liveMD&&!liveCmd);}
 puts("PASS bounds, copy errno, sync IOReturn, no-publication cleanup");
 // Pool exhaustion must not overwrite any inflight immutable backing.
 {auto y=std::make_unique<ItlIwx>();assert(y->tx2IAllocate());std::vector<iwx_tx_data> slots(1025);auto p=packet(1500);
  for(int i=0;i<1024;++i)assert(y->tx2IPrepare(&slots[i],&p,2,i,segments)>0);
  assert(!y->tx2IPrepare(&slots[1024],&p,2,0,segments));assert(y->fTx2IStats[9]==1);
  for(int i=0;i<1024;++i)y->tx2IRetire(&slots[i],true);y->tx2IFree();assert(!liveMD&&!liveCmd);}
 puts("PASS pool exhaustion/admission and complete matching retirement");
 // A reset/flush retirement is not a normal TX notification; neither duplicate
 // callback, software mbuf deletion, stop, nor final free may reclaim its map.
 {auto y=std::make_unique<ItlIwx>();assert(y->tx2IAllocate());auto p=packet(1500);assert(y->tx2IPrepare(&d,&p,2,0,segments)>0);auto t=d.tx2I_token;
  y->tx2IRetire(&d,false);assert(y->fTx2I[t-1].state==ItlIwx::Tx2IQuarantined);assert(y->fTx2IBlocked);y->tx2IRetire(&d,true);y->tx2IStop(99);p.freed=true;
  iwx_tx_data d2;auto q=packet(1500);assert(!y->tx2IPrepare(&d2,&q,2,0,segments));y->tx2IFree();assert(liveMD==1&&liveCmd==1);assert(y->fTx2IStats[21]==1);rebootOnlyCleanup(*y);}
 // Reset concurrent with CPU copy: payload was never published; conservatively
 // retain quarantine and ensure it cannot be returned as a usable TX mapping.
 {auto y=std::make_unique<ItlIwx>();assert(y->tx2IAllocate());auto p=packet(1500);duringCopy=[&]{y->tx2IRetire(&d,false);};assert(!y->tx2IPrepare(&d,&p,2,0,segments));y->tx2IFree();assert(liveMD==1&&liveCmd==1);rebootOnlyCleanup(*y);}

 // No ownership: initial stop, post-pool stop and nested stop all leave maps
 // untouched, deny admission only for the stop window, then allow ordinary TX.
 {auto y=std::make_unique<ItlIwx>();assert(!y->tx2IStop(1));assert(y->tx2IAllocate());const int maps=genCalls;
  bool a=y->tx2IStop(0x105),b=y->tx2IStop(0x205);assert(a&&b&&!y->fTx2IBlocked&&!y->tx2JEnter());
  y->tx2JStopEnd(b,0x205);assert(!y->tx2JEnter());y->tx2JStopEnd(a,0x105);assert(y->tx2JEnter());
  auto p=packet(1500);assert(y->tx2IPrepare(&d,&p,2,0,segments)>0);y->tx2IPublished(&d);y->tx2JLeave();y->tx2IRetire(&d,true);
  assert(y->tx2IStop(2));y->tx2JStopEnd(true,2);assert(!y->fTx2IBlocked&&y->tx2JEnter());y->tx2JLeave();
  assert(genCalls==maps);y->tx2IFree();assert(!liveMD&&!liveCmd);}
 // An admitted publisher overlapping stop is unresolved even if its last
 // publication check has already executed. Closing the word wins future entries.
 {auto y=std::make_unique<ItlIwx>();assert(y->tx2IAllocate());assert(y->tx2JEnter());
  assert(y->tx2IStop(2));assert(y->fTx2IBlocked);y->tx2JLeave();y->tx2JStopEnd(true,2);assert(!y->tx2JEnter());
  y->tx2IFree();assert(!liveMD&&!liveCmd);}
 // Published mapping cannot be reclaimed/reused at stop or late completion.
 {auto y=std::make_unique<ItlIwx>();assert(y->tx2IAllocate());assert(y->tx2JEnter());auto p=packet(1500);
  assert(y->tx2IPrepare(&d,&p,2,0,segments)>0);y->tx2IPublished(&d);y->tx2JLeave();auto token=d.tx2I_token;
  assert(y->tx2IStop(2));y->tx2IRetire(&d,false);y->tx2JStopEnd(true,2);assert(y->fTx2IBlocked&&!y->tx2JEnter());
  y->tx2IRetire(&d,true);assert(y->fTx2I[token-1].state==ItlIwx::Tx2IQuarantined);y->tx2IFree();assert(liveCmd==1);rebootOnlyCleanup(*y);}
 // Stop during copy: mapped address is never returned to publication and stays
 // retained; an empty-stop correction must not turn this into an idle case.
 {auto y=std::make_unique<ItlIwx>();assert(y->tx2IAllocate());assert(y->tx2JEnter());auto p=packet(1500);
  duringCopy=[&]{assert(y->tx2IStop(2));y->tx2JStopEnd(true,2);};assert(!y->tx2IPrepare(&d,&p,2,0,segments));
  y->tx2JLeave();y->tx2IFree();assert(liveCmd==1);rebootOnlyCleanup(*y);}
 // Every RAII exit decrements exactly once, including failed packet paths.
 {auto y=std::make_unique<ItlIwx>();assert(y->tx2IAllocate());{assert(y->tx2JEnter());ItlIwx::Tx2JScope guard{y.get()};assert((y->fTx2JAdmission&65535)==1);}
  assert(y->fTx2JAdmission==0);y->tx2IFree();assert(!liveMD&&!liveCmd);}
 // First operation failure survives later generic init errors/history overflow;
 // no evidence write waits for an owner. Publisher snapshots are bounded.
 {auto y=std::make_unique<ItlIwx>();assert(y->tx2IAllocate());Provider provider;y->pci.pa_tag=&provider;
  y->flow2J(1,2,0,0,0,1);assert(!y->fFlow2J.firstValid);
  y->flow2J(14,0x180,2000,0,7,35);assert(y->fFlow2J.firstValid&&y->fFlow2J.firstFailure.code==0x180);
  for(int i=0;i<300;++i)y->flow2J(9,1,0,0,0,6);assert(y->fFlow2J.count==128&&y->fFlow2J.overflow>0&&y->fFlow2J.firstFailure.status==35);
  y->fFlow2JBusy=1;y->flow2J(14,0xff,0,0,0,5);y->flow2JPublish();y->fFlow2JBusy=0;assert(y->fFlow2JDropped==2);
  for(int i=0;i<70;++i)y->flow2JPublish();assert(y->fFlow2J.snapshots==64);FILE *fp=fopen("flow-fixture.bin","wb");assert(fp);assert(fwrite(provider.flow.data(),1,provider.flow.size(),fp)==provider.flow.size());fclose(fp);y->tx2IFree();assert(!liveMD&&!liveCmd);}
 puts("PASS empty and nested stop reopening; normal-completed reuse; active/inflight/copy quarantine; RAII exits; bounded first-failure evidence");
 puts("SOURCE-DERIVED HOST TEST PASS (mock SDK; not kernel or hardware proof)");
 return 0;
}

static void initRing(ItlIwx&x){auto &ring=x.com.txq[0];unsigned stage=0;
 for(auto*d:{&ring.desc_dma,&ring.cmd_dma,&ring.bc_tbl}){assert(!allocatePacketBacking2H(d,4096,4096,&stage));memset(d->vaddr,0xa7,4096);}ring.desc=ring.desc_dma.vaddr;}
static void simulatedReboot(ItlIwx&x){for(auto&b:x.fCmd2K)if(b.dma.cmd)x.iwx_dma_contig_free(&b.dma);for(auto*d:{&x.com.txq[0].desc_dma,&x.com.txq[0].cmd_dma,&x.com.txq[0].bc_tbl})x.iwx_dma_contig_free(d);assert(!liveMD&&!liveCmd);}
static bool claim(ItlIwx&x,unsigned idx,uint32_t&serial){return x.cmd2KClaim(idx,0x10d,x.com.sc_generation,x.fCmd2KEpoch,&serial);}
static int prepare(ItlIwx&x,unsigned idx,uint32_t serial,Packet&p,bool&linked){uint64_t addr=0;void*bytes=nullptr;
 int rc=x.cmd2KPrepare(idx,serial,x.fCmd2KEpoch,&p,mbuf_pkthdr_len(&p),&x.com.txq[0].data[idx],&linked,&addr,&bytes);
 if(!rc){assert(linked&&addr==x.fCmd2K[idx].dma.paddr&&bytes==x.fCmd2K[idx].dma.vaddr);size_t offset=0;for(auto&v:p.chain){assert(!memcmp((uint8_t*)bytes+offset,v.data(),v.size()));offset+=v.size();}assert(offset==mbuf_pkthdr_len(&p));}return rc;}
static void normalDone(ItlIwx&x,unsigned idx){auto&d=x.com.txq[0].data[idx];assert(x.cmd2KDone(idx,0x10d,&d)==1);mbuf_t m=__atomic_exchange_n(&d.m,mbuf_t(nullptr),__ATOMIC_SEQ_CST);if(m)mbuf_freem(m);x.cmd2KDoneEnd(idx);}
int golden_main(){runExistingTxTests();
 failure=0;copyError=0;duringCopy={};
 // Final wide header+payload bytes, successful sync-before-publication, normal
 // completion on either side of caller return. 260 rounds exercise slot reuse.
 {auto x=std::make_unique<ItlIwx>();for(unsigned n=0;n<260;++n){unsigned idx=n%128;uint32_t serial=0;assert(claim(*x,idx,serial));auto p=packet(n%2?1948:4096);bool linked=false;
  assert(!prepare(*x,idx,serial,p,linked));auto before=std::vector<uint8_t>((uint8_t*)x->fCmd2K[idx].dma.vaddr,(uint8_t*)x->fCmd2K[idx].dma.vaddr+mbuf_pkthdr_len(&p));p.chain[0][0]^=255;assert(!memcmp(before.data(),x->fCmd2K[idx].dma.vaddr,before.size()));
  assert(!x->cmd2KAvailable(idx));
  if(n%2){x->cmd2KFinish(idx,serial,true,0);assert(!x->cmd2KAvailable(idx));normalDone(*x,idx);}
  else{normalDone(*x,idx);assert(!x->cmd2KAvailable(idx));x->cmd2KFinish(idx,serial,true,0);}
  assert(x->cmd2KAvailable(idx)&&p.freed);}
  assert(x->fCmd2KStats[2]==128&&x->fCmd2KStats[3]==260&&x->fCmd2KStats[4]==260&&x->fCmd2KStats[5]==260&&x->fCmd2KStats[6]==260);x->cmd2KFree();assert(!liveMD&&!liveCmd);}
 puts("PASS command exact immutable bytes/IOVA; 260 lifetimes; caller-first and CmdDone-first; cached prepared map reuse");
 // Every mapped allocation stage, copy and sync failure retains its original
 // diagnostic status and remains never-published. Failed cursor path is unused.
 for(int f=1;f<=12;++f){auto x=std::make_unique<ItlIwx>();uint32_t serial;assert(claim(*x,13,serial));auto p=packet(1948);bool linked=false;failure=f;assert(prepare(*x,13,serial,p,linked));assert(!linked&&!x->com.txq[0].data[13].m);failure=0;x->cmd2KFinish(13,serial,false,ENOMEM);assert(x->fCmd2KStats[12]==2&&x->cmd2KAvailable(13));x->cmd2KFree();assert(!liveMD&&!liveCmd);}
 {auto x=std::make_unique<ItlIwx>();uint32_t serial;assert(claim(*x,13,serial));auto p=packet(1948);bool linked=false;copyError=77;assert(prepare(*x,13,serial,p,linked));copyError=0;assert(x->fCmd2KStats[14]==77);x->cmd2KFinish(13,serial,false,ENOMEM);x->cmd2KFree();assert(!liveMD&&!liveCmd);}
 puts("PASS mapped allocator stage faults, original copy/sync results, no-publication cleanup");
 // Timeout is not a DMA terminal; freeze all command admission, preserve exact
 // descriptor bytes/IOVA and backing through reset/free; reject late completion.
 {auto x=std::make_unique<ItlIwx>();initRing(*x);uint32_t serial;assert(claim(*x,13,serial));auto p=packet(1948);bool linked=false;assert(!prepare(*x,13,serial,p,linked));
  x->cmd2KFinish(13,serial,true,EWOULDBLOCK);assert(x->fCmd2KFrozen&&!x->cmd2KAvailable(0)&&!x->cmd2KAvailable(13));
  x->iwx_reset_tx_ring(&x->com,&x->com.txq[0]);assert(p.freed);assert(((uint8_t*)x->com.txq[0].desc)[0]==0xa7);assert(x->com.txq[0].cur==14);
  assert(x->cmd2KDone(13,0x10d,&x->com.txq[0].data[13])==-1);x->iwx_free_tx_ring(&x->com,&x->com.txq[0]);x->cmd2KFree();assert(liveCmd==4&&x->fCmd2KStats[16]==1&&x->fCmd2KStats[28]==1);assert(x->com.txq[0].qid==0&&x->com.txq[0].ring_count==128&&x->com.txq[0].cur==14);simulatedReboot(*x);}
 // Reset during CPU copy must retain backing/ring pages and prohibit publication.
 {auto x=std::make_unique<ItlIwx>();initRing(*x);uint32_t serial;assert(claim(*x,13,serial));auto p=packet(1948);bool linked=false;
  duringCopy=[&]{x->iwx_reset_tx_ring(&x->com,&x->com.txq[0]);};assert(prepare(*x,13,serial,p,linked)==ENXIO);assert(!linked&&!p.freed);mbuf_freem(&p);x->cmd2KFinish(13,serial,false,ENXIO);x->iwx_free_tx_ring(&x->com,&x->com.txq[0]);x->cmd2KFree();assert(liveCmd==4);simulatedReboot(*x);}
 // Reset after final arm but before caller's doorbell remains unresolved; no
 // backing or descriptor address can be recycled even if late write occurs.
 {auto x=std::make_unique<ItlIwx>();initRing(*x);uint32_t serial;assert(claim(*x,13,serial));auto p=packet(1948);bool linked=false;assert(!prepare(*x,13,serial,p,linked));
  x->iwx_reset_tx_ring(&x->com,&x->com.txq[0]);assert(p.freed&&x->fCmd2KFrozen);x->cmd2KFinish(13,serial,true,ENXIO);x->iwx_free_tx_ring(&x->com,&x->com.txq[0]);x->cmd2KFree();assert(liveCmd==4);simulatedReboot(*x);}
 puts("PASS timeout, copy/reset, final-publication/reset; descriptor/backing quarantine through teardown; late ack rejected");
 // Positively accepted completion racing timeout may retire backing only after
 // CmdDone finishes; a reset winning the Completing state instead quarantines.
 for(bool reset:{false,true}){auto x=std::make_unique<ItlIwx>();uint32_t serial;assert(claim(*x,13,serial));auto p=packet(1948);bool linked=false;assert(!prepare(*x,13,serial,p,linked));auto&d=x->com.txq[0].data[13];assert(x->cmd2KDone(13,0x10d,&d)==1);
  if(reset)x->cmd2KResetBegin();x->cmd2KFinish(13,serial,true,EWOULDBLOCK);mbuf_freem(__atomic_exchange_n(&d.m,mbuf_t(nullptr),__ATOMIC_SEQ_CST));x->cmd2KDoneEnd(13);
  assert(x->cmd2KAvailable(13)==!reset);x->cmd2KFree();if(reset){assert(liveCmd==1);simulatedReboot(*x);}else assert(!liveCmd);}
 // Wrong ID, wrong generation and wrong mbuf never retire a mapped command.
 for(int mode=0;mode<3;++mode){auto x=std::make_unique<ItlIwx>();uint32_t serial;assert(claim(*x,13,serial));auto p=packet(1948);bool linked=false;assert(!prepare(*x,13,serial,p,linked));auto&d=x->com.txq[0].data[13];if(mode==1)++x->com.sc_generation;if(mode==2)d.m=nullptr;
  assert(x->cmd2KDone(13,mode==0?0x10c:0x10d,&d)==-1);x->cmd2KFinish(13,serial,true,ENXIO);if(d.m)mbuf_freem(d.m);else mbuf_freem(&p);d.m=nullptr;x->cmd2KFree();assert(liveCmd==1);simulatedReboot(*x);}
 // Empty reset opens; a caller holding a pre-reset epoch cannot claim afterward.
 {auto x=std::make_unique<ItlIwx>();uint32_t serial;auto epoch=x->fCmd2KEpoch;x->cmd2KResetBegin();assert(!claim(*x,13,serial));x->cmd2KResetEnd();assert(x->cmd2KAvailable(13));assert(!x->cmd2KClaim(13,0x10d,7,epoch,&serial));assert(x->cmd2KAvailable(13));x->cmd2KFree();assert(!liveCmd);}
 puts("PASS completion/timeout/reset ordering, strict tuple matches, epoch/admission checks");

 // Cached mapping succeeds first, then fail the per-command (not allocator)
 // outward synchronization. Also exercise hard length bounds and wire encoding.
 {Provider provider;auto x=std::make_unique<ItlIwx>();x->pci.pa_tag=&provider;uint32_t serial;
  auto p=packet(1948);bool linked=false;assert(claim(*x,13,serial));assert(!prepare(*x,13,serial,p,linked));
  normalDone(*x,13);x->cmd2KFinish(13,serial,true,0);
  auto q=packet(1948);linked=false;assert(claim(*x,13,serial));failure=10;
  assert(prepare(*x,13,serial,q,linked)==ENOMEM&&!linked);failure=0;
  assert(x->fCmd2KStats[13]==10&&x->fCmd2KStats[14]==0x207&&x->fCmd2KStats[10]==1);
  x->cmd2KFinish(13,serial,false,ENOMEM);mbuf_freem(&q);
  assert(provider.command.size()==256);FILE*fp=fopen("command-fixture.bin","wb");assert(fp);assert(fwrite(provider.command.data(),1,256,fp)==256);fclose(fp);
  for(int n=0;n<80;++n)x->cmd2KReport(3);assert(x->fCmd2KStats[26]==64);
  x->cmd2KError(99,99,99);assert(x->fCmd2KStats[13]==10&&x->fCmd2KStats[14]==0x207);
  x->cmd2KFree();assert(!liveMD&&!liveCmd);}
 {auto x=std::make_unique<ItlIwx>();uint32_t serial;assert(claim(*x,13,serial));auto p=packet(4097);bool linked=false;
  assert(prepare(*x,13,serial,p,linked)==EINVAL&&!linked&&!liveMD&&!liveCmd);x->cmd2KFinish(13,serial,false,EINVAL);mbuf_freem(&p);x->cmd2KFree();}
 puts("PASS per-command sync failure, oversize refusal, first-error preservation and bounded source-generated wire fixture");
 puts("ALL SOURCE-DERIVED COMMAND + EXISTING TX TESTS PASS (mock IOKit; not runtime proof)");
 return 0;
}


std::function<void()> recoveryCasHook;
template<typename T> bool casPC1(T *p,T *e,T d,bool w,int a,int b){if(recoveryCasHook){auto h=std::move(recoveryCasHook);recoveryCasHook={};h();}return __atomic_compare_exchange_n(p,e,d,w,a,b);}

void ItlIwx::pc1Report()
{
    if (fPc1Snapshots >= 32 || !pci.pa_tag) return;
    uint32_t v[20] = {1, ++fPc1Snapshots, fPc1ResetEpoch, fPc1TerminalEpoch,
        fPc1ExpectedGeneration, unsigned(com.sc_generation), fPc1LastMaster,
        __atomic_load_n(&fPc1Fault,__ATOMIC_ACQUIRE), fPc1Recoveries, fPc1QuarantineCount,
        __atomic_load_n(&fPc1Suspended,__ATOMIC_ACQUIRE),
        __atomic_load_n(&fTx2IBlocked,__ATOMIC_ACQUIRE),
        __atomic_load_n(&fTx2JAdmission,__ATOMIC_ACQUIRE),
        fPc1DroppedCompletion, fPc1StaleFlush, fPc1LastRefusal, fPc1PublishFailures};
    for (unsigned i=0;i<kTx2ICount;++i) {
        const unsigned s=__atomic_load_n(&fTx2I[i].state,__ATOMIC_ACQUIRE);
        if (s==Tx2IQuarantined) ++v[17];
        if (s==Tx2IInflight || s==Tx2ICopying) ++v[18];
    }
    v[19]=32;
    if (!pci.pa_tag->setProperty("KGP_PC1",v,sizeof(v))) ++fPc1PublishFailures;
    XYLog("KGP_PC1 reset=%u terminal=%u generation=%u master=%u recoveries=%u retained=%u blocked=%u fault=%u refusal=%u\n",
          v[2],v[3],v[5],v[6],v[8],v[17],v[11],v[7],v[15]);
}

#define __atomic_compare_exchange_n casPC1

IOReturn ItlIwx::pc1RecoverAction(OSObject *owner,void *arg,void *,void *,void *)
{
    auto that=static_cast<ItlIwx *>(owner);const unsigned generation=unsigned(uintptr_t(arg));
    if (generation!=unsigned(that->com.sc_generation)) return kIOReturnAborted;
    if (!__atomic_load_n(&that->fPc1Suspended,__ATOMIC_ACQUIRE)) return kIOReturnSuccess;
    unsigned refusal=0,quarantines=0;
    if (__atomic_load_n(&that->fPc1Fault,__ATOMIC_ACQUIRE) || !that->fPc1TerminalEpoch || that->fPc1TerminalEpoch!=that->fPc1ResetEpoch) refusal=1;
    if (generation!=unsigned(that->com.sc_generation) || generation!=that->fPc1ExpectedGeneration) refusal=2;
    if (!that->com.sc_uc.uc_ok || __atomic_load_n(&that->fCmd2KFrozen,__ATOMIC_SEQ_CST)) refusal=3;
    if (__atomic_load_n(&that->fTx2IStats[16],__ATOMIC_ACQUIRE)) refusal=4;
    for (unsigned i=0;i<kTx2ICount;++i) {
        const unsigned s=__atomic_load_n(&that->fTx2I[i].state,__ATOMIC_ACQUIRE);
        if (s==Tx2IQuarantined) ++quarantines;
        else if(s!=Tx2IFree) refusal=5;
    }
    if (quarantines!=that->fPc1QuarantineCount) refusal=6;
    uint32_t admission=__atomic_load_n(&that->fTx2JAdmission,__ATOMIC_ACQUIRE);
    if (admission!=0 && admission!=kTx2JClosed) refusal=7;
    if (!refusal) {
        // Admission stays closed while blocked is lowered. A concurrent stop
        // changes stop depth; the CAS then fails and blocked is restored.
        __atomic_store_n(&that->fTx2IBlocked,0U,__ATOMIC_RELEASE);
        if (!__atomic_compare_exchange_n(&that->fTx2JAdmission,&admission,0U,false,
                                         __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) refusal=8;
    }
    if (refusal) {
        __atomic_store_n(&that->fTx2IBlocked,1U,__ATOMIC_RELEASE);
        that->fPc1LastRefusal=refusal;that->pc1Report();return kIOReturnNotReady;
    }
    __atomic_store_n(&that->fPc1Suspended,0U,__ATOMIC_RELEASE);
    ++that->fPc1Recoveries;that->fPc1LastRefusal=0;that->pc1Report();return kIOReturnSuccess;
}

#undef __atomic_compare_exchange_n

IOReturn ItlIwx::pc1StopEndAction(OSObject *owner,void *,void *,void *,void *)
{
    auto that=static_cast<ItlIwx *>(owner);
    // A full stop adds one software generation after the device stop.
    if (that->fPc1TerminalEpoch==that->fPc1ResetEpoch && that->fPc1TerminalEpoch &&
        unsigned(that->com.sc_generation)==that->fPc1ExpectedGeneration)
        ++that->fPc1ExpectedGeneration;
    that->pc1Report();return kIOReturnSuccess;
}

IOReturn ItlIwx::pc1AdvanceAction(OSObject *owner,void *arg,void *,void *,void *)
{
    auto that=static_cast<ItlIwx *>(owner);
    auto c=static_cast<Pc1Advance *>(arg);
    if (c->generation!=unsigned(that->com.sc_generation) ||
        __atomic_load_n(&that->fPc1Suspended,__ATOMIC_ACQUIRE)) {
        ++that->fPc1StaleFlush;return kIOReturnAborted;
    }
    that->iwx_ampdu_txq_advance(&that->com,c->ring,c->index,false);
    return kIOReturnSuccess;
}

void ready(ItlIwx &x) {
 assert(x.tx2IAllocate());x.com.sc_generation=1;x.fPc1ResetEpoch=1;x.fPc1TerminalEpoch=1;
 x.fPc1ExpectedGeneration=3;x.fPc1Suspended=1;x.fPc1LastMaster=1;
}
void retainOne(ItlIwx &x,iwx_tx_data &d,Packet &p) {
 IOPhysicalSegment seg[8];d.type=IEEE80211_FC0_TYPE_MGT;
 assert(x.tx2JEnter());assert(x.tx2IPrepare(&d,&p,1,15,seg)>0);x.tx2IPublished(&d);x.tx2JLeave();
 auto stop=x.tx2IStop(0x105);x.tx2IRetire(&d,false);x.tx2JStopEnd(stop,0x105);
 x.fPc1QuarantineCount=1;x.com.sc_generation=3;
}
IOReturn recover(ItlIwx &x) {std::lock_guard<std::recursive_mutex> l(x.gate);return ItlIwx::pc1RecoverAction(&x,(void*)uintptr_t(x.com.sc_generation),nullptr,nullptr,nullptr);}
void cleanup(ItlIwx &x) {x.tx2IFree();rebootOnlyCleanup(x);assert(!liveCmd&&!liveMD);}
int main(){
 golden_main();
 {auto p=packet(30);iwx_tx_data d;auto x=std::make_unique<ItlIwx>();ready(*x);retainOne(*x,d,p);
  auto old=x->fTx2I[0].dma.paddr;assert(recover(*x)==0&&!x->fTx2IBlocked&&!x->fTx2JAdmission);
  assert(x->fTx2I[0].state==ItlIwx::Tx2IQuarantined&&x->fTx2I[0].dma.paddr==old);
  IOPhysicalSegment seg[8];auto q=packet(30);assert(x->tx2JEnter());assert(x->tx2IPrepare(&d,&q,1,15,seg)>0);
  assert(d.tx2I_token!=1);x->tx2IPublished(&d);x->tx2JLeave();x->tx2IRetire(&d,true);
  assert(x->fTx2IStats[6]==1&&x->fTx2IStats[7]==1);
  // A later cycle: retain a second unresolved payload and recover independently.
  assert(x->tx2JEnter());assert(x->tx2IPrepare(&d,&q,1,16,seg)>0);x->tx2IPublished(&d);x->tx2JLeave();
  auto stop=x->tx2IStop(0x105);x->tx2IRetire(&d,false);x->tx2JStopEnd(stop,0x105);
  x->fPc1ResetEpoch=x->fPc1TerminalEpoch=2;x->fPc1Suspended=1;x->fPc1ExpectedGeneration=x->com.sc_generation=5;x->fPc1QuarantineCount=2;
  assert(recover(*x)==0&&x->fPc1Recoveries==2&&x->fTx2IStats[7]==2);cleanup(*x);
 }
 puts("PASS exact recovery and golden TX helpers: two cycles, retained old mappings, distinct new token, normal new completion");
 for(unsigned failure=1;failure<=11;++failure){
  auto x=std::make_unique<ItlIwx>();ready(*x);auto p=packet(30);iwx_tx_data d;retainOne(*x,d,p);
  switch(failure){case 1:x->fPc1Fault=2;break;case 2:x->fPc1TerminalEpoch=0;break;
   case 3:x->fPc1TerminalEpoch=2;break;case 4:x->fPc1ExpectedGeneration=4;break;
   case 5:x->com.sc_uc.uc_ok=false;break;case 6:x->fCmd2KFrozen=1;break;
   case 7:x->fTx2IStats[16]=2;break;case 8:x->fTx2I[1].state=ItlIwx::Tx2ICopying;break;
   case 9:x->fPc1QuarantineCount=0;break;case 10:x->fTx2JAdmission|=ItlIwx::kTx2JStop;break;case 11:x->fTx2JAdmission|=1;break;}
  assert(recover(*x)!=0&&x->fTx2IBlocked&&x->fPc1Suspended&&x->fPc1Recoveries==0);
  x->fTx2I[1].state=ItlIwx::Tx2IFree;cleanup(*x);
 }
 {auto x=std::make_unique<ItlIwx>();ready(*x);auto p=packet(30);iwx_tx_data d;retainOne(*x,d,p);
  bool stopped=false;recoveryCasHook=[&]{assert(!x->tx2JEnter());stopped=x->tx2IStop(0x105);};
  assert(recover(*x)!=0&&x->fPc1LastRefusal==8&&x->fTx2IBlocked&&x->fPc1Suspended);
  x->tx2JStopEnd(stopped,0x105);assert(!x->tx2JEnter());cleanup(*x);
 }
 puts("PASS concurrent stop at recovery CAS: no publisher admitted, failed CAS restores blocked");
 puts("PASS fail-closed: failed/missing terminal, reset epoch, generation, ALIVE, command freeze, mapping error, Copying, quarantine change, stop nesting, publisher");
 {auto x=std::make_unique<ItlIwx>();ready(*x);x->com.sc_generation=2;x->fPc1ExpectedGeneration=2;
  ItlIwx::pc1StopEndAction(x.get(),nullptr,nullptr,nullptr,nullptr);assert(x->fPc1ExpectedGeneration==3);
  ItlIwx::pc1StopEndAction(x.get(),nullptr,nullptr,nullptr,nullptr);assert(x->fPc1ExpectedGeneration==3);
  x->fPc1Suspended=0;x->com.sc_generation=3;
  assert(ItlIwx::pc1RecoverAction(x.get(),(void*)uintptr_t(1),nullptr,nullptr,nullptr)!=0);
  ItlIwx::Pc1Advance a{&x->com.txq[0],16,1};assert(ItlIwx::pc1AdvanceAction(x.get(),&a,nullptr,nullptr,nullptr)!=0&&x->advances==0);
  a.generation=3;assert(ItlIwx::pc1AdvanceAction(x.get(),&a,nullptr,nullptr,nullptr)==0&&x->advances==1);
  x->fPc1Suspended=1;assert(ItlIwx::pc1AdvanceAction(x.get(),&a,nullptr,nullptr,nullptr)!=0&&x->advances==1);cleanup(*x);}
 puts("PASS full-stop generation ticket, stale initialization, delayed pre-reset flush, reset-interval refusal");
 // Recursive mutex models SDK runAction. An old callback owning the gate must
 // finish before reset can clear RX producer state or permit identity reuse.
 for(unsigned n=0;n<100;++n){
  std::recursive_mutex gate;std::promise<void> entered,release;auto released=release.get_future();std::atomic<bool> reset{false};int identity=1;
  std::thread callback([&]{std::lock_guard<std::recursive_mutex> l(gate);entered.set_value();released.wait();assert(identity==1);});
  entered.get_future().wait();std::thread stop([&]{std::lock_guard<std::recursive_mutex> l(gate);identity=2;reset=true;});
  assert(!reset);release.set_value();callback.join();stop.join();assert(reset&&identity==2);
 }
 puts("PASS 100 contended callback/reset fences; old handler cannot cross identity reuse (SDK-contract model)");
 {auto x=std::make_unique<ItlIwx>();ready(*x);Provider p;x->pci.pa_tag=&p;
  for(int i=0;i<100;++i)x->pc1Report();assert(x->fPc1Snapshots==32&&p.pc1.size()==80);cleanup(*x);}
 puts("PASS bounded 80-byte snapshot / 32 publications");
 puts("ALL PC1 TESTS PASS. Hardware terminal remains a driver-contract premise, not emulated hardware proof.");
}
