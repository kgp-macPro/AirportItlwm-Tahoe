//
//  _ifq.cpp
//  itlwm
//
//  Created by zxystd on 2022/8/13.
//  Copyright © 2022 钟先耀. All rights reserved.
//

#include <sys/_ifq.h>
#include <sys/_if_ether.h>

// ZC7 ADMISSION BEGIN: KGP-native bounded pull reservation, no DMA ownership.
// All queue membership and reservation decisions use IOPacketQueue's same lock.
// No stack call, packet destruction or gate acquisition occurs under this lock.
class KgpOutputQueue : public IOPacketQueue {
    OSDeclareDefaultStructors(KgpOutputQueue)
    _ifnet *owner = nullptr;
    uint32_t reserved = 0;
    uint64_t epoch = 1;
    bool pullOpen = false, waiting = false;

    bool available() const {
        return uint64_t(IOPacketQueue::getSize()) + reserved < IOPacketQueue::getCapacity();
    }
    IONetworkInterface *takeWake() {
        if (!waiting || !pullOpen || !available()) return nullptr;
        waiting = false;
        auto *iface = owner ? owner->iface : nullptr;
        if (iface) iface->retain(); // Survives a concurrent interface detach.
        return iface;
    }
    static void wake(IONetworkInterface *iface) {
        if (iface) { iface->signalOutputThread(); iface->release(); }
    }
    static UInt32 dispose(mbuf_t m) {
        UInt32 n = 0;
        while (m) { auto next = mbuf_nextpkt(m); mbuf_setnextpkt(m, nullptr);
            mbuf_freem(m); m = next; ++n; }
        return n;
    }
public:
    static KgpOutputQueue *create(_ifnet *ifp, UInt32 capacity) {
        auto *q = new KgpOutputQueue;
        if (q && !q->initWithCapacity(capacity)) { q->release(); q = nullptr; }
        if (q) q->owner = ifp;
        return q;
    }
    bool reserve(ifq_reservation *ticket, uint32_t limit = 1) {
        IOSimpleLockLock(_lock);
        const bool ok = limit && pullOpen && available();
        if (ok) {
            const uint32_t freeSlots = IOPacketQueue::getCapacity() -
                                       IOPacketQueue::getSize() - reserved;
            ticket->count = limit < freeSlots ? limit : freeSlots;
            reserved += ticket->count;
            retain(); ticket->queue = this; ticket->epoch = epoch;
        } else if (pullOpen) waiting = true;
        IOSimpleLockUnlock(_lock);
        return ok; // On false, macOS still owns its head packet.
    }
    bool finish(ifq_reservation *ticket, mbuf_t m) {
        // The producer owns this chain exclusively until enqueue succeeds.
        // Reject an oversized chain rather than exceed reserved capacity.
        uint32_t count = 0;
        for (auto p = m; p && count <= ticket->count; p = mbuf_nextpkt(p)) ++count;
        IOSimpleLockLock(_lock);
        // Exactly one finish per successful reserve; ticket is private to caller.
        bool accepted = false;
        reserved -= ticket->count;
        if (m && count <= ticket->count && pullOpen && ticket->epoch == epoch)
            accepted = IOPacketQueue::enqueue(m);
        auto *iface = takeWake();
        ticket->queue = nullptr; ticket->count = 0;
        IOSimpleLockUnlock(_lock);
        wake(iface); release();
        return accepted; // On false, caller still owns m. Never free it here.
    }
    void openPull() {
        IOSimpleLockLock(_lock); pullOpen = true; auto *iface = takeWake();
        IOSimpleLockUnlock(_lock); wake(iface);
    }
    void closePull(bool detach = false) {
        IOSimpleLockLock(_lock);
        pullOpen = false; waiting = false; ++epoch;
        if (detach) owner = nullptr;
        IOSimpleLockUnlock(_lock);
    }
    bool lockEnqueue(mbuf_t m) override {
        // Internal EAPOL producers participate in reservation capacity, but are
        // not gated by pullOpen: authentication must work before link-up.
        UInt32 count = 0;
        for (auto p = m; p; p = mbuf_nextpkt(p)) ++count;
        IOSimpleLockLock(_lock);
        bool ok = owner && uint64_t(IOPacketQueue::getSize()) + reserved + count <=
                  IOPacketQueue::getCapacity() && IOPacketQueue::enqueue(m);
        IOSimpleLockUnlock(_lock);
        return ok;
    }
    UInt32 lockEnqueueWithDrop(mbuf_t m) override {
        return lockEnqueue(m) ? 0 : dispose(m);
    }
    mbuf_t lockDequeue() override {
        IOSimpleLockLock(_lock);
        auto m = IOPacketQueue::dequeue();
        auto *iface = takeWake();
        IOSimpleLockUnlock(_lock);
        wake(iface);
        return m;
    }
    UInt32 lockFlush() override {
        IOSimpleLockLock(_lock);
        // Flush is a stop/reset boundary. Outstanding reservations stay counted
        // until their caller cancels/commits, and their old epoch cannot enqueue.
        pullOpen = false; waiting = false; ++epoch;
        auto m = IOPacketQueue::dequeueAll();
        IOSimpleLockUnlock(_lock);
        return dispose(m);
    }
    UInt32 getSize() const override {
        IOSimpleLockLock(_lock); auto n = IOPacketQueue::getSize();
        IOSimpleLockUnlock(_lock); return n;
    }
    void resize(UInt32 capacity) {
        IOSimpleLockLock(_lock);
        if (uint64_t(IOPacketQueue::getSize()) + reserved <= capacity)
            IOPacketQueue::setCapacity(capacity);
        IOSimpleLockUnlock(_lock);
    }
};
OSDefineMetaClassAndStructors(KgpOutputQueue, IOPacketQueue)
// ZC7 ADMISSION END

bool ifq_reserve(struct _ifqueue *q, struct ifq_reservation *ticket)
{
    return q->queue && static_cast<KgpOutputQueue *>(q->queue)->reserve(ticket);
}

uint32_t ifq_reserve_batch(struct _ifqueue *q, struct ifq_reservation *ticket, uint32_t limit)
{
    return q->queue && static_cast<KgpOutputQueue *>(q->queue)->reserve(ticket, limit)
           ? ticket->count : 0;
}

bool ifq_commit(struct ifq_reservation *ticket, mbuf_t m)
{
    return ticket->queue && static_cast<KgpOutputQueue *>(ticket->queue)->finish(ticket, m);
}

void ifq_cancel(struct ifq_reservation *ticket)
{
    if (ticket->queue) static_cast<KgpOutputQueue *>(ticket->queue)->finish(ticket, nullptr);
}

void ifq_start_pull(struct _ifqueue *q)
{
    if (q->queue) static_cast<KgpOutputQueue *>(q->queue)->openPull();
}

void ifq_stop_pull(struct _ifqueue *q)
{
    if (q->queue) static_cast<KgpOutputQueue *>(q->queue)->closePull();
}

void ifq_init(struct _ifqueue *ifq, struct _ifnet *ifp, unsigned int maxLen)
{
    if (!ifq->queue)
        ifq->queue = KgpOutputQueue::create(ifp, maxLen);
    else
        static_cast<KgpOutputQueue *>(ifq->queue)->resize(maxLen);
    ifq->ifq_oactive = 0;
}

void ifq_destroy(struct _ifqueue *ifq)
{
    if (ifq->queue) {
        static_cast<KgpOutputQueue *>(ifq->queue)->closePull(true);
        ifq->queue->release();
        ifq->queue = nullptr;
    }
}

void ifq_flush(struct _ifqueue *ifq)
{
    if (ifq->queue)
        ifq->queue->lockFlush();
}

bool ifq_empty(struct _ifqueue *ifq)
{
    return ifq->queue->getSize() == 0;
}

uint32_t ifq_len(struct _ifqueue *ifq)
{
    return ifq->queue->getSize();
}

void ifq_set_maxlen(struct _ifqueue *ifq, uint32_t maxLen)
{
    static_cast<KgpOutputQueue *>(ifq->queue)->resize(maxLen);
}

void ifq_set_oactive(struct _ifqueue *ifq)
{
    ifq->ifq_oactive = 1;
}

unsigned int ifq_is_oactive(struct _ifqueue *ifq)
{
    return ifq->ifq_oactive;
}

void ifq_clr_oactive(struct _ifqueue *ifq)
{
    ifq->ifq_oactive = 0;
}

mbuf_t ifq_dequeue(struct _ifqueue *ifq)
{
    return ifq->queue->lockDequeue();
}

int ifq_enqueue(struct _ifqueue *ifq, mbuf_t m)
{
    ifq->queue->lockEnqueueWithDrop(m);
    return 0;
}
