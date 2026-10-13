// KGP ZC6-PERF1. Passive counters only; never consulted for packet decisions.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef KGP_PERF1_HPP
#define KGP_PERF1_HPP
#include <stdint.h>
struct KgpPerf1 {
    enum Counter {
        OutputStartCalls, // 0
        OutputStartPreOactive, // 1
        MacDequeueCalls, // 2
        MacDequeued, // 3
        MacDequeuedBytes, // 4
        MacNoFrames, // 5
        MacDequeueOther, // 6
        OutputStartDropped, // 7
        OutputStartPostOactive, // 8
        OutputStartDrained, // 9
        OutputPacketCalls, // 10
        OutputPacketBytes, // 11
        OutputNotRun, // 12
        OutputNull, // 13
        OutputNoHeader, // 14
        OutputFree, // 15
        SoftwareAccepted, // 16
        SoftwareRefused, // 17
        OutputSuccess, // 18
        OutputDropped, // 19
        IfStartCalls, // 20
        GateCalls, // 21
        GateCannotLock, // 22
        GateOtherError, // 23
        StartEntered, // 24
        StartNotRunning, // 25
        StartOactive, // 26
        StartQfull, // 27
        StartFlush, // 28
        StartMgmtOnly, // 29
        StartQueueEmpty, // 30
        MgmtDequeued, // 31
        DataDequeued, // 32
        DataDequeuedBytes, // 33
        PullupDrop, // 34
        EncapDrop, // 35
        TxAttempts, // 36
        TxErrors, // 37
        OutputPacketsIncrement, // 38
        RingAbsentDrop, // 39
        RingOwnershipReject, // 40
        AdmissionReject, // 41
        GenerationReject, // 42
        CryptoDrop, // 43
        MappingDrop, // 44
        DescriptorsSubmitted, // 45
        Submitted80211Bytes, // 46
        HighWaterEvents, // 47
        FullTransitions, // 48
        OactiveSet, // 49
        OactiveClear, // 50
        LowWaterCalls, // 51
        OutputSignals, // 52
        RestartCalls, // 53
        RetiredObjects, // 54
        TxCmdNotifications, // 55
        BaNotifications, // 56
        BaTxed, // 57
        BaAcked, // 58
        BaRetries, // 59
        SingleFailStatus, // 60
        AggregateStatusFrames, // 61
        DrainRequests, // 62
        DrainRuns, // 63
        DrainEntered, // 64
        DrainMissed, // 65
        RxMpduNotifications, // 66
        RxInputFrames, // 67
        RxCrcOrOverrunDrop, // 68
        RxLengthDrop, // 69
        RxDecryptDrop, // 70
        RingResets, // 71
        FlushCalls, // 72
        InterruptActions, // 73
        PublishCalls, // 74
        PublishFailures, // 75
        LinkFlushCalls, // 76
        Count
    };
    enum { Queues = 31, QueueWords = 6 };
    // Queue words: submitted, retired, high-water events, peak queued,
    // last observed queued, last observed ring capacity. NOT a wire packet ledger.
    alignas(8) uint64_t counters[Count] = {};
    alignas(8) uint64_t queues[Queues][QueueWords] = {};
    void add(Counter c, uint64_t n = 1) {
        __atomic_fetch_add(&counters[c], n, __ATOMIC_RELAXED);
    }
    uint64_t read(Counter c) const { return __atomic_load_n(&counters[c], __ATOMIC_RELAXED); }
    void queue(unsigned q, unsigned word, uint64_t n = 1) {
        if (q < Queues && word < QueueWords) __atomic_fetch_add(&queues[q][word], n, __ATOMIC_RELAXED);
    }
    // Ring writers already run under the existing gate. Observers only read.
    void occupancy(unsigned q, unsigned size, unsigned used) {
        if (q >= Queues) return;
        __atomic_store_n(&queues[q][4], uint64_t(used), __ATOMIC_RELAXED);
        __atomic_store_n(&queues[q][5], uint64_t(size), __ATOMIC_RELAXED);
        if (used > __atomic_load_n(&queues[q][3], __ATOMIC_RELAXED))
            __atomic_store_n(&queues[q][3], uint64_t(used), __ATOMIC_RELAXED);
    }
    void snapshot(uint64_t *out) const {
        for (unsigned i=0; i<Count; ++i) out[i]=__atomic_load_n(&counters[i], __ATOMIC_RELAXED);
        for (unsigned q=0; q<Queues; ++q)
            for (unsigned i=0; i<QueueWords; ++i)
                out[Count+q*QueueWords+i]=__atomic_load_n(&queues[q][i], __ATOMIC_RELAXED);
    }
};
static_assert(__atomic_always_lock_free(sizeof(uint64_t), nullptr), "PERF1 requires lock-free 64-bit counters");
#endif
