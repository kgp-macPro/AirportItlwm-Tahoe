#!/usr/bin/env python3
"""Decode saved `ioreg -a -r -c AirportItlwm` snapshots. Read-only; no tests or changes.
Usage: python3 ZC6_PERF1_DECODE.py before.plist [after.plist]
Counter deltas are asynchronous lifetime aggregates, NOT flow-specific loss ledgers.
"""
import sys,plistlib,struct,json
from pathlib import Path
NAMES = ['OutputStartCalls', 'OutputStartPreOactive', 'MacDequeueCalls', 'MacDequeued', 'MacDequeuedBytes', 'MacNoFrames', 'MacDequeueOther', 'OutputStartDropped', 'OutputStartPostOactive', 'OutputStartDrained', 'OutputPacketCalls', 'OutputPacketBytes', 'OutputNotRun', 'OutputNull', 'OutputNoHeader', 'OutputFree', 'SoftwareAccepted', 'SoftwareRefused', 'OutputSuccess', 'OutputDropped', 'IfStartCalls', 'GateCalls', 'GateCannotLock', 'GateOtherError', 'StartEntered', 'StartNotRunning', 'StartOactive', 'StartQfull', 'StartFlush', 'StartMgmtOnly', 'StartQueueEmpty', 'MgmtDequeued', 'DataDequeued', 'DataDequeuedBytes', 'PullupDrop', 'EncapDrop', 'TxAttempts', 'TxErrors', 'OutputPacketsIncrement', 'RingAbsentDrop', 'RingOwnershipReject', 'AdmissionReject', 'GenerationReject', 'CryptoDrop', 'MappingDrop', 'DescriptorsSubmitted', 'Submitted80211Bytes', 'HighWaterEvents', 'FullTransitions', 'OactiveSet', 'OactiveClear', 'LowWaterCalls', 'OutputSignals', 'RestartCalls', 'RetiredObjects', 'TxCmdNotifications', 'BaNotifications', 'BaTxed', 'BaAcked', 'BaRetries', 'SingleFailStatus', 'AggregateStatusFrames', 'DrainRequests', 'DrainRuns', 'DrainEntered', 'DrainMissed', 'RxMpduNotifications', 'RxInputFrames', 'RxCrcOrOverrunDrop', 'RxLengthDrop', 'RxDecryptDrop', 'RingResets', 'FlushCalls', 'InterruptActions', 'PublishCalls', 'PublishFailures', 'LinkFlushCalls']
QUEUE_FIELDS = ['submitted','retired_objects','high_water_events','peak_queued','last_queued','last_capacity']
def walk(o):
    if isinstance(o,dict):
        if isinstance(o.get('KGP_ZC6_PERF1'),bytes):yield o['KGP_ZC6_PERF1']
        for v in o.values():yield from walk(v)
    elif isinstance(o,list):
        for v in o:yield from walk(v)
def read(path):
    matches=list(walk(plistlib.loads(Path(path).read_bytes())))
    if len(matches)!=1:raise SystemExit('Expected exactly one PERF1 instance; found '+str(len(matches)))
    data=matches[0]
    if len(data)%8:raise SystemExit('Truncated counter data')
    w=struct.unpack('<'+'Q'*(len(data)//8),data)
    if w[:3]!=(6,1,303) or len(w)!=303:raise SystemExit('Unsupported schema/size')
    return {'source':path,'schema':w[1],'sequence':w[3],'snapshot_ns':w[4],'mapped':bool(w[5]),'pc1_fault':w[6],'tx_blocked':w[7],
        'counters':dict(zip(NAMES,w[8:85])),
        'queues':{str(q):dict(zip(QUEUE_FIELDS,w[85+q*6:91+q*6])) for q in range(31)},
        'existing_mapped_tx_words':list(w[271:303])}
a=read(sys.argv[1]);result={'before':a}
if len(sys.argv)>2:
    b=read(sys.argv[2]);result['after']=b
    if b['sequence']<=a['sequence'] or b['snapshot_ns']<=a['snapshot_ns'] or b['mapped']!=a['mapped']:
        raise SystemExit('Stale/restarted/mode-changing snapshot pair: do not subtract')
    if any(b['counters'][n]<a['counters'][n] for n in NAMES):raise SystemExit('Counter reset: do not subtract')
    result['counter_deltas']={n:b['counters'][n]-a['counters'][n] for n in NAMES}
    result['queue_event_deltas']={q:{n:b['queues'][q][n]-a['queues'][q][n] for n in QUEUE_FIELDS[:3]} for q in a['queues']}
    result['limitations']='Check PC1/epoch, RingResets/FlushCalls/LinkFlushCalls, instance identity and stable drained endpoints. NoFrames includes stack throttling. Retirements include software flush. Age words are sampled mapped-only uptime ticks. No per-flow or pre-dequeue drop attribution.'
print(json.dumps(result,indent=2))
