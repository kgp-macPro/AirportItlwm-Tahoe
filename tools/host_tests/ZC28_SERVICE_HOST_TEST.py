#!/usr/bin/env python3
"""Extract actual ZC28 queue, unchanged frontend, both start routes and completion.

The IOKit/mbuf/ring effects are host mocks. This checks software ownership,
capacity signalling and concurrency; it is neither a DMA nor throughput test.
"""
from pathlib import Path
import ast, hashlib, os, subprocess, sys, tempfile
root = Path(os.environ.get('ZC28_SOURCE_ROOT', Path(__file__).resolve().parents[2]))
old = ast.parse((root/'tools/host_tests/ZC7_ADMISSION_HOST_TEST.py').read_text())
mock = next(ast.literal_eval(n.value) for n in old.body if isinstance(n, ast.Assign) and any(isinstance(x, ast.Name) and x.id == 'mock' for x in n.targets))
mock = mock.replace('#include <vector>', '#include <vector>\n#include <functional>\n#include <algorithm>\n#include <cstring>')
mock = mock.replace('struct M {M*next=nullptr;', 'struct M {M*next=nullptr; int flags=1,type=1,id=0,len=64; bool pullupFail=false,encapFail=false; void*node=nullptr; char data[64]={};')
mock = mock.replace('struct IONetworkInterface:Obj {', '''
using IOReturn=int; using IOOptionBits=unsigned; using OSObject=Obj;
constexpr int kIOReturnSuccess=0,kIOReturnNoResources=1,kIOReturnNoFrames=2,kIOReturnOutputSuccess=0,kIOReturnOutputDropped=3,kIOReturnError=-1;
constexpr int MBUF_TYPE_FREE=0,MBUF_PKTHDR=1,IEEE80211_S_RUN=4;
int mbuf_type(M*m){return m->type;} int mbuf_flags(M*m){return m->flags;}
struct IONetworkInterface:Obj {
 std::function<void()> onSignal;
 std::deque<M*> input; unsigned pulls=0;std::function<void()> before;int error=0;bool overReturn=false;
 IOReturn dequeueOutputPackets(uint32_t count,M**head){
  ++pulls;*head=nullptr;if(before){auto f=before;before=nullptr;f();}
  if(error)return error;if(input.empty())return kIOReturnNoFrames;
  if(overReturn)++count;M*tail=nullptr;
  while(count--&&!input.empty()){auto m=input.front();input.pop_front();m->next=nullptr;
   if(tail)tail->next=m;else *head=m;tail=m;}return kIOReturnSuccess;
 }
''')
mock = mock.replace('++signals;}', '++signals;if(onSignal)onSignal();}')
mock = mock.replace('struct _ifnet{IONetworkInterface*iface;};', '''
class IOPacketQueue;
struct _ifqueue {unsigned ifq_oactive=0;IOPacketQueue*queue=nullptr;};
struct ifq_service {IOPacketQueue*queue=nullptr;};
struct Stats {unsigned outputErrors=0,outputPackets=0;};
struct _ifnet {IONetworkInterface*iface=nullptr;_ifqueue if_snd;Stats*netStat=nullptr;void(*if_start)(_ifnet*)=nullptr;void*if_softc=nullptr;unsigned if_flags=3,if_timer=0;};
''')
qtext = (root/'itl80211/openbsd/sys/_ifq.cpp').read_text()
assert 'serviceDepth' in qtext and 'ifq_service_snapshot' in qtext, 'ZC28 source not yet integrated'
qbody = qtext[qtext.index('// ZC7 ADMISSION BEGIN'):]
fronttext = (root/'AirportItlwm/AirportItlwm.cpp').read_text()
start = fronttext[fronttext.index('IOReturn AirportItlwm::outputStart('):fronttext.index('\n#endif', fronttext.index('IOReturn AirportItlwm::outputStart('))]
batch = fronttext[fronttext.index('UInt32 AirportItlwm::outputReservedBatch('):fronttext.index('// ZC8 BATCH END')]
# This narrow port must not import ZC12's hardware-oactive admission bypass.
assert 'ifq_is_oactive(&ifp->if_snd)' in start
assert 'ifq_service_handoff' not in qtext + (root/'itl80211/openbsd/sys/_ifq.h').read_text() + start
iwxtext = (root/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
def extract_function(text,signature):
 a = text.index(signature)
 b = text.index('\n}\n', a) + 3
 return text[a:b]
def function(signature):return extract_function(iwxtext,signature)
clear = function('void ItlIwx::\niwx_clear_oactive(')
route = function('IOReturn ItlIwx::\n_iwx_start_task(')
conventional = function('IOReturn ItlIwx::\ndexterOriginal__iwx_start_task(')
assert route.index('ifq_begin_service') < route.index('dexterOriginal__iwx_start_task'), 'Scope must cover both positive DMA routes'
assert 'signalOutputThread' in clear
baseline = '48e9a31e68c328b5f19e5bdc81901e900d350738'
def previous(path):return subprocess.check_output(['git','-C',str(root),'show',baseline+':'+path],text=True)
oldiwx=previous('itlwm/hal_iwx/ItlIwx.cpp')
assert clear==extract_function(oldiwx,'void ItlIwx::\niwx_clear_oactive('), 'Positive completion wakes must remain exact baseline'
assert conventional==extract_function(oldiwx,'IOReturn ItlIwx::\ndexterOriginal__iwx_start_task('), 'Conventional service body must remain baseline'
oldroute=extract_function(oldiwx,'IOReturn ItlIwx::\n_iwx_start_task(')
assert route[route.index('    // ZC8: acknowledge'):]==oldroute[oldroute.index('    // ZC8: acknowledge'):], 'Only RAII prefix may change mapped service'
assert fronttext==previous('AirportItlwm/AirportItlwm.cpp'), 'No frontend bypass or admission change is part of narrow port'
oldq=previous('itl80211/openbsd/sys/_ifq.cpp')
for signature in ['    bool reserve(', '    bool finish(', '    void openPull(', '    void closePull(', '    bool lockEnqueue(', '    UInt32 lockEnqueueWithDrop(', '    mbuf_t lockDequeue(', '    UInt32 lockFlush(', '    UInt32 getSize(', '    void resize(']:
 def method(text):
  a=text.index(signature);return text[a:text.index('\n    }',a)+6]
 assert method(qtext)==method(oldq), 'Existing admission/ownership method changed: '+signature
print('PRESERVED_BASELINE',baseline,'exact frontend, both service bodies, completion, admission/epochs/ownership methods',flush=True)
front = r'''
#define IFF_RUNNING 1
#define IFF_UP 2
#define IWX_FLAG_TXFLUSH 1
#define IEEE80211_F_TX_MGMT_ONLY 2
#define EDCA_AC_BE 0
struct ether_header {char bytes[14];};
struct ieee80211_node {};
static ieee80211_node mockNode;
struct ieee80211com {struct {_ifnet ac_if;}ic_ac;int ic_state=IEEE80211_S_RUN;unsigned ic_xflags=0;std::deque<M*>ic_mgtq;};
#define ic_if ic_ac.ac_if
struct iwx_softc {ieee80211com sc_ic;unsigned qfullmsk=0,sc_flags=0,sc_tx_timer=0;};
struct iwx_tx_ring {int queued=0,low_mark=192,qid=2;};
struct Start2LContext {bool entered=false;uint32_t reason=0;};
struct ScopeLoop {bool gated=true;bool inGate(){return gated;}};
struct ItlIwx {
 iwx_softc com;
 ScopeLoop loop;bool mapped=true;
 unsigned submissions=0,releaseNodes=0;int txError=0;
 std::function<void()> beforeTx;
 ScopeLoop*getMainWorkLoop(){return &loop;}
 bool dma5Mapped(){return mapped;}
 void path2L(unsigned,uint32_t=0,uint32_t=0,uint8_t=0){}
 int iwx_tx(iwx_softc*,M*m,ieee80211_node*,int){++submissions;if(beforeTx){auto f=beforeTx;beforeTx=nullptr;f();}mbuf_freem(m);return txError;}
 static IOReturn _iwx_start_task(OSObject*,void*,void*,void*,void*);
 static IOReturn dexterOriginal__iwx_start_task(OSObject*,void*,void*,void*,void*);
 void iwx_clear_oactive(iwx_softc*,iwx_tx_ring*);
};
// com is the first mock member; no retained device/DMA pointer is modeled.
#define container_of(P,T,M) reinterpret_cast<T*>(P)
M*mq_dequeue(std::deque<M*>*q){if(q->empty())return nullptr;auto m=q->front();q->pop_front();return m;}
int mbuf_len(M*m){return m->len;}
void*mbuf_data(M*m){return m->data;}
void*mbuf_pkthdr_rcvif(M*m){return m->node;}
int mbuf_pullup(M**m,unsigned n){if((*m)->pullupFail){mbuf_freem(*m);*m=nullptr;return 1;}(*m)->len=n;return 0;}
M*ieee80211_encap(_ifnet*,M*m,ieee80211_node**ni){if(m->encapFail){mbuf_freem(m);return nullptr;}*ni=&mockNode;return m;}
void ieee80211_release_node(ieee80211com*,ieee80211_node*){}
struct Hal {ieee80211com ic;ieee80211com*external=nullptr;ieee80211com*get80211Controller(){return external?external:&ic;}};
struct AirportItlwm {Hal*fHalService;void freePacket(M*m){mbuf_freem(m);}IOReturn outputStart(IONetworkInterface*,IOOptionBits);UInt32 outputReservedBatch(mbuf_t,ifq_reservation*);};
'''
tests=(Path(__file__).with_name('ZC28_SERVICE_TEST.cpp')).read_text()
san = sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
identity={name:hashlib.sha256(text.encode()).hexdigest() for name,text in [('queue',qtext),('frontend',fronttext),('start',route),('conventional',conventional),('completion',clear)]}
print('ACTUAL_SOURCE',root,identity,flush=True)
with tempfile.TemporaryDirectory(prefix='zc28-service-') as td:
 p=Path(td);(p/'test.cpp').write_text(mock+qbody+front+clear+conventional+route+start+batch+tests)
 cmd=['/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++','-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk','-D__PRIVATE_SPI__','-DAIRPORT','-DNBPFILTER=0','-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
 print('COMMAND',repr(cmd),flush=True)
 try:subprocess.run(cmd,check=True)
 except subprocess.CalledProcessError:
  failure=Path(__file__).with_name('LAST_FAILED_GENERATED.cpp');failure.write_text((p/'test.cpp').read_text());raise
 subprocess.run([str(p/'test')],check=True,timeout=180)
 print('PASS',san,flush=True)
