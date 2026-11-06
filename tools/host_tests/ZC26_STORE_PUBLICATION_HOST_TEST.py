#!/usr/bin/env python3
"""Actual-source TX store publication contract + compiler witness + host lifetime model.
The abstract visibility model is explicitly a simulation of the stated full-fence
store-order contract. Sanitizers do NOT see DMA or prove hardware visibility.
"""
from pathlib import Path
import os,re,subprocess,tempfile,sys,itertools
R=Path(os.environ.get('ZC26_SOURCE_ROOT',Path(__file__).resolve().parents[2]));s=(R/'itlwm/hal_iwx/ItlIwx.cpp').read_text()
def mask(t):return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),t,flags=re.S)
def extract(n):
    m=mask(s);q=re.search(r'(?:static\s+inline\s+|static\s+)?(?:void|int|bool|IOReturn)\s+(?:ItlIwx::\s*)?'+re.escape(n)+r'\s*\(',m);assert q,n
    a=m.index('{',q.end());b=a+1;d=1
    while d:d+=(m[b]=='{')-(m[b]=='}');b+=1
    return s[q.start():b]
barrier=extract('iwx_dma_publish_barrier')
queue_barrier=extract('iwx_dma_queue_publish_barrier')
assert '__ATOMIC_SEQ_CST' in queue_barrier and 'sfence' not in queue_barrier
assert 'sfence' not in mask(barrier).lower() and '__ATOMIC_SEQ_CST' in barrier
assert '__ATOMIC_SEQ_CST' in barrier # all architectures retain the full compiler/CPU fence
assert len(re.findall(r'\biwx_dma_publish_barrier\s*\(\s*\)\s*;',mask(s)))==2
assert len(re.findall(r'\biwx_dma_queue_publish_barrier\s*\(\s*\)\s*;',mask(s)))==2
proof=[]
for name in ['iwx_enable_txq','iwx_tvqm_enable_txq','iwx_tx','dexterOriginal_iwx_tx']:
    f=extract(name);m=mask(f)
    selected='iwx_dma_publish_barrier' if name in ['iwx_tx','dexterOriginal_iwx_tx'] else 'iwx_dma_queue_publish_barrier'
    fs=list(re.finditer(r'\b'+selected+r'\s*\(\s*\)\s*;',m));assert len(fs)==1,name
    fence=fs[0].start()
    if name in ['iwx_tx','dexterOriginal_iwx_tx']:
        prep=m.index('iwx_tx_publish_control(');bc=m.index('iwx_tx_update_byte_tbl(',prep)
        kick=re.search(r'IWX_WRITE\s*\(sc,\s*IWX_HBUS_TARG_WRPTR',m).start()
        assert prep<bc<fence<kick,(name,prep,bc,fence,kick)
        assert m.index('data->m = m;',prep)<bc
        proof.append((name,'private construction/validation -> ownership -> BC store -> store fence -> WRPTR'))
    else:
        config=m.index('hcmd.data[0] = &cmd;');send=m.index('iwx_send_cmd(sc, &hcmd)',fence)
        assert config<fence<send
        if name=='iwx_tvqm_enable_txq':assert m.index('iwx_tx_control_alloc(')<config
        proof.append((name,'initialized queue/control config -> retained full SEQ_CST fence -> existing firmware command'))
# Exact unchanged completion/reset code is checked elsewhere; this model never
# treats a publication fence as positive completion or a resource-reuse proof.
orders=list(itertools.permutations('PCTBD')) # payload/command/TFD/bytecount/doorbell
with_fence=[x for x in orders if x[-1]=='D']
unsafe_without=[x for x in orders if x[-1]!='D']
assert len(with_fence)==24 and len(unsafe_without)==96
assert all(all(x.index(k)<x.index('D') for k in 'PCTB') for x in with_fence)
print('SIMULATION abstract store-visibility litmus: 24 safe full-fence-allowed orders, 96 premature-doorbell orders excluded; not measured DMA.',flush=True)
clang=subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip()
sdk=subprocess.check_output(['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()
san=sys.argv[1] if len(sys.argv)>1 else 'address,undefined'
mock=r'''
#include <atomic>
#include <array>
#include <thread>
#include <cassert>
#include <cstdint>
#include <iostream>
'''
model=r'''
struct Slot {uint64_t payload=0,command=0,tfd=0,bc=0;std::atomic<unsigned> state{0};};
int main(){std::array<Slot,64>slots{};constexpr unsigned N=4096;
 std::thread producer([&]{for(unsigned i=1;i<=N;i++){auto&s=slots[i%slots.size()];while(s.state.load(std::memory_order_acquire)!=0)std::this_thread::yield();s.payload=i;s.command=i+1;s.tfd=i+2;s.bc=i+3;iwx_dma_publish_barrier();s.state.store(1,std::memory_order_release);}});
 std::thread completion([&]{for(unsigned i=1;i<=N;i++){auto&s=slots[i%slots.size()];while(s.state.load(std::memory_order_acquire)!=1)std::this_thread::yield();assert(s.payload==i&&s.command==i+1&&s.tfd==i+2&&s.bc==i+3);s.state.store(0,std::memory_order_release);}});
 producer.join();completion.join();for(auto&s:slots)assert(s.state.load()==0);
 std::cout<<"PASS 4096 host publication/completion handshakes with 64-slot wrap. Release/acquire supplies CPU synchronization; The fence is not represented as a TSan DMA fence.\n";
}
'''
witness=r'''
extern "C" __attribute__((noinline)) void zc26_store_witness(unsigned long long*data,volatile unsigned*doorbell,unsigned long long v,unsigned q){data[0]=v;data[1]=v+1;iwx_dma_publish_barrier();*doorbell=q;}
'''
with tempfile.TemporaryDirectory(prefix='zc26-store-') as td:
    p=Path(td);(p/'witness.cpp').write_text(barrier+'\n'+witness)
    base=[clang,'-std=c++17','-O2','-ffreestanding','-mno-sse','-mno-sse2','-mno-mmx','-S',str(p/'witness.cpp')]
    cmd=base+['-o',str(p/'witness.s')];print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True)
    asm=(p/'witness.s').read_text();fn=asm[asm.index('_zc26_store_witness:'):];fn=fn[:fn.index('.cfi_endproc')] if '.cfi_endproc' in fn else fn
    assert 'sfence' not in fn
    fences=list(re.finditer(r'\b(?:mfence|lock\w*)\b',fn));assert len(fences)==1
    before,after=fn[:fences[0].start()],fn[fences[0].end():]
    assert len(re.findall(r'\bmov[qwlb]\b',before))>=2 and re.search(r'\bmov[qwlb]\b',after)
    print('PASS actual-helper x86_64 compiler witness: control stores -> full fence -> volatile doorbell; no optional SFENCE weakening.',flush=True)
    cmd=base+['-U__x86_64__','-o',str(p/'fallback.s')];subprocess.run(cmd,check=True)
    fallback=(p/'fallback.s').read_text();assert re.search(r'\b(?:mfence|lock\w*)\b',fallback) and 'sfence' not in fallback
    print('PASS conservative non-x86 preprocessor fallback compiled with full fence (host target); not a qualification of other architecture.',flush=True)
    (p/'queue.cpp').write_text(queue_barrier+'\n'+witness.replace('iwx_dma_publish_barrier()', 'iwx_dma_queue_publish_barrier()'))
    cmd=[clang,'-std=c++17','-O2','-ffreestanding','-mno-sse','-mno-sse2','-mno-mmx','-S',str(p/'queue.cpp'),'-o',str(p/'queue.s')];subprocess.run(cmd,check=True)
    qasm=(p/'queue.s').read_text();assert re.search(r'\b(?:mfence|lock\w*)\b',qasm) and 'sfence' not in qasm
    print('PASS actual cold queue-enable helper retains full SEQ_CST instruction; not counted as a per-packet fence.',flush=True)
    (p/'test.cpp').write_text(mock+'\n'+barrier+'\n'+model)
    cmd=[clang,'-isysroot',sdk,'-std=c++17','-O1','-g','-pthread','-fsanitize='+san,str(p/'test.cpp'),'-o',str(p/'test')]
    print('SOURCE',R,flush=True);print('COMMAND',cmd,flush=True);subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True,timeout=60)
for name,contract in proof:print('PASS actual source callsite',name,contract)
print('PASS actual source/barrier and compiler validation; physical cost, PAT and TCP improvement UNPROVEN.')
