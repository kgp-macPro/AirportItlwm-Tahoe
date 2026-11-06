#!/usr/bin/env python3
"""Compile the production reservation queue with locking/mbuf/IOKit mocks."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[2]
src=(root/'itl80211/openbsd/sys/_ifq.cpp').read_text()
body=src[src.index('// ZC7 ADMISSION BEGIN'):src.index('// ZC7 ADMISSION END')]
mock=r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
using UInt32=uint32_t;
std::atomic<int> packets{0},objects{0};
thread_local unsigned locks=0;
struct M {M*next=nullptr; M(){++packets;} ~M(){--packets;}};
using mbuf_t=M*;
mbuf_t mbuf_nextpkt(mbuf_t m){return m->next;}
void mbuf_setnextpkt(mbuf_t m,mbuf_t n){m->next=n;}
void mbuf_freem(mbuf_t m){assert(!locks);delete m;}
using IOSimpleLock=std::mutex;
void IOSimpleLockLock(IOSimpleLock*l){l->lock();++locks;}
void IOSimpleLockUnlock(IOSimpleLock*l){--locks;l->unlock();}
class Obj { std::atomic<unsigned> refs{1}; public:
 Obj(){++objects;} virtual ~Obj(){--objects;}
 void retain(){assert(refs.fetch_add(1)>0);} void release(){if(refs.fetch_sub(1)==1)delete this;}
};
struct IONetworkInterface:Obj {
 std::atomic<unsigned> signals{0},flags{0};
 void signalOutputThread(){assert(!locks);flags.fetch_or(2);++signals;}
 void begin(){flags=0;}
 bool sleepAfterNoResources(){unsigned zero=0;return flags.compare_exchange_strong(zero,1);}
};
struct _ifnet{IONetworkInterface*iface;};
class IOPacketQueue: public Obj {
 std::deque<M*> q; UInt32 cap=0;
protected: IOSimpleLock *_lock=new IOSimpleLock;
public:
 static bool failInit;
 virtual ~IOPacketQueue(){while(!q.empty()){auto m=q.front();q.pop_front();mbuf_freem(m);} delete _lock;}
 bool initWithCapacity(UInt32 n){cap=n;return !failInit;}
 virtual UInt32 getSize()const{return q.size();}
 virtual bool setCapacity(UInt32 n){cap=n;return true;}
 UInt32 getCapacity()const{return cap;}
 bool enqueue(M*m){if(q.size()>=cap)return false;while(m){auto n=m->next;m->next=nullptr;q.push_back(m);m=n;}return true;}
 M*dequeue(){if(q.empty())return nullptr;auto m=q.front();q.pop_front();m->next=nullptr;return m;}
 M*dequeueAll(){M*head=nullptr,*tail=nullptr;while(auto m=dequeue()){if(tail)tail->next=m;else head=m;tail=m;}return head;}
 virtual bool lockEnqueue(M*){assert(false);return false;}
 virtual UInt32 lockEnqueueWithDrop(M*){assert(false);return 0;}
 virtual M*lockDequeue(){assert(false);return nullptr;}
 virtual UInt32 lockFlush(){assert(false);return 0;}
};
bool IOPacketQueue::failInit=false;
struct ifq_reservation{IOPacketQueue*queue=nullptr;uint64_t epoch=0;uint32_t count=0;};
#define OSDeclareDefaultStructors(X)
#define OSDefineMetaClassAndStructors(X,Y)
'''
tests=r'''
void cancel(KgpOutputQueue*q,ifq_reservation&t){q->finish(&t,nullptr);assert(!t.queue);}
void put(KgpOutputQueue*q,ifq_reservation&t){auto m=new M;if(!q->finish(&t,m))mbuf_freem(m);assert(!t.queue);}
int main(){
 auto*i=new IONetworkInterface;_ifnet owner{i};
 IOPacketQueue::failInit=true;assert(!KgpOutputQueue::create(&owner,2));IOPacketQueue::failInit=false;
 // Internal EAPOL before link up; pull cannot dequeue yet.
 {auto*q=KgpOutputQueue::create(&owner,2);ifq_reservation a;
  assert(!q->reserve(&a));assert(q->lockEnqueue(new M));mbuf_freem(q->lockDequeue());q->openPull();
  ifq_reservation b,c;assert(q->reserve(&a)&&q->reserve(&b));assert(!q->reserve(&c));
  auto m=new M;assert(!q->lockEnqueue(m));mbuf_freem(m); // EAPOL cannot steal reservation.
  i->begin();assert(!q->reserve(&c));put(q,a);mbuf_freem(q->lockDequeue());
  assert(!i->sleepAfterNoResources()); // Signal before driver's NoResources return.
  assert(q->reserve(&c));i->begin();ifq_reservation d;assert(!q->reserve(&d));
  assert(i->sleepAfterNoResources());unsigned signals=i->signals;cancel(q,b);assert(i->signals>signals);
  cancel(q,c);q->release();assert(packets==0);}
 // Chain producers are bounded as a unit; packet frees occur outside spinlock.
 {auto*q=KgpOutputQueue::create(&owner,2);q->openPull();ifq_reservation a;assert(q->reserve(&a));
  auto m=new M;m->next=new M;assert(q->lockEnqueueWithDrop(m)==2);cancel(q,a);q->release();}
 // In-flight macOS dequeue crossing flush/reset cancels, never retries freed mbuf.
 {auto*q=KgpOutputQueue::create(&owner,2);q->openPull();ifq_reservation a;assert(q->reserve(&a));
  assert(q->lockEnqueue(new M));assert(q->lockFlush()==1);q->openPull();auto m=new M;
  assert(!q->finish(&a,m));mbuf_freem(m);assert(q->getSize()==0);q->release();}
 // Reservation keeps queue alive after detach; no callback into former owner.
 {auto*q=KgpOutputQueue::create(&owner,1);q->openPull();ifq_reservation a,b;assert(q->reserve(&a));
  assert(!q->reserve(&b));auto before=i->signals.load();q->closePull(true);q->release();cancel(q,a);assert(i->signals==before);}
 // Concurrent producer/consumer, internal producer, flush + rearm.
 {auto*q=KgpOutputQueue::create(&owner,31);q->openPull();std::atomic<unsigned> done{0};
  std::atomic<unsigned> committed{0},cancelled{0},consumed{0},flushed{0};
  auto pull=[&]{for(unsigned n=0;n<50000;++n){ifq_reservation t;
    if(!q->reserve(&t)){std::this_thread::yield();continue;}
    auto m=new M; if(q->finish(&t,m))++committed;else{++cancelled;mbuf_freem(m);}}
    ++done;};
  std::thread p1(pull),p2(pull),internal([&]{for(unsigned n=0;n<30000;++n){auto m=new M;
    if(q->lockEnqueue(m))++committed;else{++cancelled;mbuf_freem(m);}}++done;}),
   consumer([&]{while(done!=3||q->getSize()){if(auto m=q->lockDequeue()){++consumed;mbuf_freem(m);}else std::this_thread::yield();}}),
   reset([&]{for(unsigned n=0;n<1000;++n){flushed+=q->lockFlush();q->openPull();std::this_thread::yield();}});
  p1.join();p2.join();internal.join();reset.join();consumer.join();flushed+=q->lockFlush();
  assert(committed==consumed+flushed);assert(packets==0);q->closePull(true);q->release();
  std::cout<<"conservation accepted="<<committed<<" consumed="<<consumed<<" flushed="<<flushed<<" cancelled="<<cancelled<<"\n";}
 i->release();assert(objects==0&&packets==0);
 std::cout<<"PASS production admission: reservation/consumer, full recovery, EAPOL capacity, both wake orderings, flush epochs, detach, bounded concurrent ownership; no model leaks\n";
}
'''
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
with tempfile.TemporaryDirectory(prefix='zc7-admission-') as t:
 p=Path(t);(p/'test.cpp').write_text(mock+body+tests)
 cmd=[subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip(),'-isysroot',subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip(),'-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',repr(cmd),flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=180)
 print('PASS',san)
