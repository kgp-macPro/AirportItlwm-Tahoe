#!/usr/bin/env python3
"""Compile literal ZC28 rare observer helpers with mocked provider/gate/queue.

Usage: ZC28_SOURCE_ROOT=/absolute/worktree python3 this.py [address,undefined|thread]
The queue implementation itself is tested separately. No driver or traffic runs.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = Path(os.environ.get('ZC28_SOURCE_ROOT', Path(__file__).resolve().parents[2]))
SOURCE = ROOT / 'itlwm/hal_iwx/ItlIwx.cpp'
HEADER = ROOT / 'itlwm/hal_iwx/ItlIwx.hpp'
REG = ROOT / 'itlwm/hal_iwx/if_iwxreg.h'
source = SOURCE.read_text()
reg = REG.read_text()
masked = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',
                lambda match: ' ' * len(match[0]), source, flags=re.S)


def extract(name):
    match = re.search(r'(?:static\s+)?(?:void|unsigned|uint64_t|IOReturn|int)\s+'
                      r'(?:ItlIwx::\s*)?' + re.escape(name) + r'\s*\(', masked)
    assert match, name
    brace = masked.index('{', match.end())
    end, depth = brace + 1, 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return source[match.start():end]


def structure(name):
    match = re.search(r'struct ' + re.escape(name) + r'\s*\{.*?\}\s*__packed;', reg, re.S)
    assert match, name
    return match[0]


NAMES = ['zc28Note', 'zc28Aggregate', 'zc28Capture', 'zc28Publish']
functions = '\n'.join(extract(name) for name in NAMES)
assert 'fZc28Rare[8]' in HEADER.read_text()
assert 'fZc28Wire[32]' in HEADER.read_text()
for name in NAMES:
    assert not re.search(r'\b(?:time2GNow|clock_get_uptime|absolutetime_to_nanoseconds|'
                         r'malloc|IOMalloc|new|IOSleep|msleep|printf|XYLog|IOLog)\s*\(', extract(name)), name
    assert name == 'zc28Publish' or 'setProperty' not in extract(name)
for name in ['iwx_tx', 'dexterOriginal_iwx_tx', 'iwx_txd_done',
             'iwx_rx_tx_ba_notif', 'iwx_notif_intr', 'dexterOriginal_iwx_notif_intr']:
    assert not re.search(r'\bzc(?:27|28)(?:Note|Status|Aggregate|BaStatus|RxStart|RxEnd)\s*\(', extract(name)), name
assert 'that->zc28Capture();' in extract('zc19Snapshot')
assert 'zc28Publish();' in extract('zc19Publish')
assert 'zc28Aggregate(pkt)' in extract('iwx_rx_tx_cmd')
assert 'zc28Aggregate(pkt)' in extract('dexterOriginal_iwx_rx_tx_cmd')

mock = r'''
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#define __packed __attribute__((packed))
using __le16=uint16_t;using __le32=uint32_t;using u8=uint8_t;
static constexpr uint32_t IWX_FH_RSCSR_FRAME_SIZE_MSK=0x3fff;
#ifndef le32toh
#define le32toh(x) (x)
#endif
struct Loop { static thread_local bool held; bool inGate(){return held;} };
thread_local bool Loop::held=false;
struct Queue {
 std::mutex lock;uint64_t words[8]={};unsigned snapshots=0;
};
struct _ifqueue {Queue *queue=nullptr;};
static bool ifq_service_snapshot(_ifqueue *q,uint64_t *out){
 assert(Loop::held);
 if(!q->queue)return false;
 std::lock_guard<std::mutex> g(q->queue->lock);++q->queue->snapshots;
 std::memcpy(out,q->queue->words,sizeof(q->queue->words));return true;
}
struct Provider {
 unsigned calls=0;bool fail=false;uint64_t saved[32]={};
 bool setProperty(const char *name,const void *data,size_t n){
  assert(!Loop::held);assert(!std::strcmp(name,"KGP_ZC28_SERVICE"));
  assert(n==sizeof(saved));++calls;if(fail)return false;
  std::memcpy(saved,data,n);return true;
 }
};
struct iwx_rx_packet;
struct ItlIwx {
 uint64_t fZc28Rare[8]={},fZc28Wire[32]={},fZc28PubFail=0,fZc19Health[32]={};
 struct { struct {struct {_ifqueue if_snd;}ic_if;}sc_ic;}com;
 struct {Provider *pa_tag=nullptr;}pci;
 Loop *loop=nullptr;Loop *getMainWorkLoop(){return loop;}
 void zc28Note(unsigned,uint64_t=0,uint64_t=0,uint64_t=0);
 void zc28Aggregate(const iwx_rx_packet *);void zc28Capture();void zc28Publish();
};
'''
structures = '\n'.join(structure(name) for name in [
    'iwx_cmd_header', 'iwx_rx_packet', 'iwx_agg_tx_status', 'iwx_tx_resp'])
length_match = re.search(r'static uint32_t\s+iwx_rx_packet_len\s*\([^)]*\)\s*\{[^}]*\}', reg)
assert length_match
length_helper = length_match[0]
tests = r'''
static unsigned checks=0;
static void check(bool b){++checks;assert(b);}
static uint64_t load(const uint64_t *p){return __atomic_load_n(p,__ATOMIC_RELAXED);}
struct Packet {
 std::unique_ptr<uint8_t[]> data;size_t bytes;
 explicit Packet(size_t n):data(new uint8_t[sizeof(iwx_rx_packet)+n]()),bytes(n){
  pkt()->len_n_flags=uint32_t(sizeof(pkt()->hdr)+n);
 }
 iwx_rx_packet *pkt(){return reinterpret_cast<iwx_rx_packet *>(data.get());}
};
static void rare_events(){
 ItlIwx d;
 for(unsigned e:{0U,1U,2U,4U,6U,9U,999U})d.zc28Note(e,~0ULL,~0ULL,~0ULL);
 for(auto n:d.fZc28Rare)check(n==0);
 d.zc28Note(3,0x123456,0x1234);
 check(d.fZc28Rare[0]==1&&d.fZc28Rare[1]==0x343456);
 d.zc28Note(3,0,0);check(d.fZc28Rare[0]==2&&d.fZc28Rare[1]==0);
 d.zc28Note(5,0xabcdef1234567890ULL,0x1234567800000007ULL);
 check(d.fZc28Rare[4]==1&&d.fZc28Rare[5]==0x0000000734567890ULL);
 d.zc28Note(7,12,0);d.zc28Note(8,12,511);
 check(d.fZc28Rare[6]==1&&d.fZc28Rare[7]==1);
 // Atomic rare records are attachment cumulative, independent of health epoch.
 d.fZc19Health[6]=2;d.fZc19Health[11]=3;d.fZc19Health[12]=4;
 d.zc28Note(5,0,0);check(d.fZc28Rare[4]==2&&d.fZc28Rare[5]==0);
}
static void aggregate_bounds(){
 static_assert(offsetof(iwx_tx_resp,status)==40,"actual FW6/7 status offset");
 for(unsigned n=0;n<sizeof(iwx_cmd_header);++n){
  ItlIwx d;Packet p(0);p.pkt()->len_n_flags=n;
  d.zc28Aggregate(p.pkt());check(d.fZc28Rare[2]==1&&d.fZc28Rare[3]==0);
 }
 // Exact prefix allocation makes a newly introduced data read below the
 // guarded threshold fail under ASan. The observer does not parse frame arrays.
 for(size_t bytes=0;bytes<=offsetof(iwx_tx_resp,status)+8;++bytes){
  for(unsigned count=0;count<256;++count){
   ItlIwx d;Packet p(bytes);if(bytes)p.pkt()->data[0]=uint8_t(count);
   d.zc28Aggregate(p.pkt());check(d.fZc28Rare[2]==1);
   check(d.fZc28Rare[3]==(bytes>=offsetof(iwx_tx_resp,status)?count:0));
  }
 }
 ItlIwx d;Packet good(40);good.pkt()->data[0]=255;d.zc28Aggregate(good.pkt());
 Packet short_packet(0);short_packet.pkt()->len_n_flags=0;d.zc28Aggregate(short_packet.pkt());
 check(d.fZc28Rare[2]==2&&d.fZc28Rare[3]==255);
 // Flags outside masked length do not affect the observer's read extent.
 good.pkt()->len_n_flags|=0xffffc000U;good.pkt()->data[0]=2;d.zc28Aggregate(good.pkt());
 check(d.fZc28Rare[2]==3&&d.fZc28Rare[3]==2);
}
static void capture_and_export(){
 ItlIwx d;Loop l;Provider p;Queue q;d.loop=&l;d.pci.pa_tag=&p;
 for(unsigned i=0;i<8;++i)q.words[i]=i==7?0:i+1;
 d.zc28Note(3,0x81,8);
 for(unsigned mode:{0U,1U,2U}){
  d.fZc19Health[3]=100+mode;d.fZc19Health[4]=9000+mode;
  d.fZc19Health[5]=mode;d.fZc19Health[6]=10;d.fZc19Health[11]=11;d.fZc19Health[12]=12;
  Loop::held=true;d.zc28Capture();check(d.fZc28Wire[10]==0);
  for(unsigned i=12;i<24;++i)check(d.fZc28Wire[i]==0);
  d.com.sc_ic.ic_if.if_snd.queue=&q;d.zc28Capture();
  check(d.fZc28Wire[10]==1);for(unsigned i=0;i<8;++i)check(d.fZc28Wire[16+i]==q.words[i]);
  d.zc28Publish();check(p.calls==mode);
  Loop::held=false;d.zc28Publish();check(p.calls==mode+1);
  check(p.saved[0]==28&&p.saved[1]==1&&p.saved[2]==256);
  check(p.saved[3]==100+mode&&p.saved[4]==9000+mode&&p.saved[5]==mode);
  check(p.saved[6]==10&&p.saved[7]==11&&p.saved[8]==12&&p.saved[9]==1);
  check(p.saved[24]==1&&p.saved[25]==0x80081);
  for(unsigned i=12;i<16;++i)check(p.saved[i]==0);
  d.com.sc_ic.ic_if.if_snd.queue=nullptr;
 }
 // Failed export updates only watchdog-owned failure count, visible next snapshot.
 p.fail=true;d.zc28Publish();check(d.fZc28PubFail==1);
 Loop::held=true;d.zc28Capture();Loop::held=false;
 check(d.fZc28Wire[11]==1);p.fail=false;d.zc28Publish();check(p.saved[11]==1);
 const auto prior=p.calls;
 d.pci.pa_tag=nullptr;d.zc28Publish();check(p.calls==prior);
 d.pci.pa_tag=&p;d.loop=nullptr;d.zc28Publish();check(p.calls==prior);
 // Queue detach invalidates only the queue snapshot; rare attachment history persists.
 d.loop=&l;Loop::held=true;d.zc28Capture();Loop::held=false;
 check(d.fZc28Wire[10]==0&&d.fZc28Wire[24]==1);
 for(unsigned i=16;i<24;++i)check(d.fZc28Wire[i]==0);
}
static void concurrency(){
 ItlIwx d;Loop l;Provider p;Queue q;d.loop=&l;d.pci.pa_tag=&p;
 d.com.sc_ic.ic_if.if_snd.queue=&q;std::atomic<unsigned> active{4};
 constexpr unsigned count=4000;std::vector<std::thread> workers;
 for(unsigned id=0;id<4;++id)workers.emplace_back([&,id]{
  Packet a(40);a.pkt()->data[0]=uint8_t(2+id);
  for(unsigned n=0;n<count;++n){
   d.zc28Note(3,id,8);d.zc28Note(5,id,7);d.zc28Note(7);d.zc28Note(8);
   d.zc28Aggregate(a.pkt());
   std::lock_guard<std::mutex> g(q.lock);++q.words[0];++q.words[1];
  }
  --active;
 });
 while(active.load()){
  // Only this watchdog thread owns health, wire, publication and failure fields.
  Loop::held=true;++d.fZc19Health[3];d.zc28Capture();Loop::held=false;d.zc28Publish();
  check(p.saved[16]==p.saved[17]); // queue snapshot lock, not all wire counters atomic
  check(p.saved[24]<=count*4&&p.saved[28]<=count*4);
 }
 for(auto &t:workers)t.join();
 for(unsigned i:{0U,2U,4U,6U,7U})check(load(&d.fZc28Rare[i])==count*4);
 Loop::held=true;d.zc28Capture();Loop::held=false;
 check(d.fZc28Wire[16]==count*4&&d.fZc28Wire[17]==count*4);
 check(d.fZc28Wire[24]==count*4&&d.fZc28Wire[28]==count*4);
}
int main(){
 rare_events();aggregate_bounds();capture_and_export();concurrency();
 std::cout<<"PASS: "<<checks<<" literal-helper checks; atomic rare concurrency, prefix bounds, exact 256-byte schema, queue snapshot, attachment retention and off-gate publication\n";
 std::cout<<"LIMIT: queue is mocked; no DMA/hardware, physical overhead, global cross-counter atomicity or throughput proof\n";
}
'''

sanitizer = sys.argv[1] if len(sys.argv) > 1 else 'address,undefined'
assert sanitizer in ('address,undefined', 'thread')
model = mock + structures + length_helper + functions + tests
with tempfile.TemporaryDirectory(prefix='zc28-rare-observer-') as temp:
    path = Path(temp)
    (path / 'model.cpp').write_text(model)
    cmd = ['xcrun', 'clang++', '-std=c++17', '-g', '-O1', '-pthread',
           '-fno-omit-frame-pointer', '-fsanitize=' + sanitizer,
           str(path / 'model.cpp'), '-o', str(path / 'model')]
    subprocess.run(cmd, check=True)
    subprocess.run([str(path / 'model')], check=True)
print(json.dumps({'source': str(SOURCE), 'source_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                  'helpers_sha256': hashlib.sha256(functions.encode()).hexdigest(),
                  'sanitizer': sanitizer, 'actual_helpers': NAMES, 'source_checks': 'PASS'}))
