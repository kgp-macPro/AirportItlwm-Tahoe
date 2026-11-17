// KGP experimental coherent BLE backend selection. GPL-2.0.
// Native Ready, initialization and HCI completions are never patched.
#pragma once
#ifndef KGP_BLE_HOST_TEST
#include <Headers/kern_crypto.hpp>
#include <Headers/kern_file.hpp>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <sys/vnode.h>
#endif
#include "KGPBLEQualify.hpp"
static_assert(PAGE_SIZE==4096,"Experimental BLE currently requires x86_64 4-KiB page validation");
namespace KGPBLE {
enum State:uint32_t {NotRequested=0,Waiting,Preparing,Ready,Disabled};
static volatile uint32_t state {};
static IOService *controller {};
static IONotifier *notification {},*interfaceNotification {};
static IOService *candidates[8] {};
static IOWorkLoop *loop {};
static IOTimerEventSource *timer {};
static unsigned attempts {};
static bool accessorsReady {};
static volatile uint32_t appliedMask {};
static Plan plan {}; // immutable after release publication of Ready
static vnode_t qualifiedVnode {}; // retained usecount; no private vnode offsets
static void *(*getBlobs)(vnode_t) {};
static off_t (*getBase)(void *) {};
static const unsigned char *(*getCDHash)(void *) {};
// Labels assist composite-device matching only; Bluetooth-class Intel USB
// devices outside these IDs are accepted too. No HCI capability preflight.
inline bool namedIntelProduct(uint32_t vendor,uint32_t product){
 if(vendor!=0x8087)return false;
 switch(product){case 0x26:case 0x29:case 0x32:case 0x33:case 0x35:case 0x36:case 0x38:return true;default:return false;}
}
inline bool hash(const uint8_t *b,size_t n,uint8_t *out){
 if(n>48*1024*1024)return false;auto *d=Crypto::hash(b,uint32_t(n));if(!d)return false;
 for(unsigned i=0;i<32;++i)out[i]=d[i];Buffer::deleter(d);return true;
}
inline void disable(const char *why){
 uint32_t previous=__atomic_exchange_n(&state,Disabled,__ATOMIC_ACQ_REL);
 if(previous!=Disabled)SYSLOG("kgpble","Intel AX/BE extension OFF: %s; base fixes retained",why);
}
static bool matched(void *,void *,IOService *service,IONotifier *){
 if(!service || (!service->metaCast("IOUSBHostDevice") && !service->metaCast("IOUSBHostInterface")))return true;
 auto number=[](IOService *v,const char *name)->uint32_t{auto *n=OSDynamicCast(OSNumber,v->getProperty(name));return n?n->unsigned32BitValue():0;};
 const bool interfaceBluetooth=number(service,"bInterfaceClass")==0xe0 && number(service,"bInterfaceSubClass")==1 && number(service,"bInterfaceProtocol")==1;
 IOService *device=service;
 // Interfaces can publish their own identical vendor/product properties.
 // Select the physical USB device by class, never by property presence.
 for(unsigned depth=0;device && !device->metaCast("IOUSBHostDevice") && depth<8;++depth)
  device=OSDynamicCast(IOService,device->getParentEntry(gIOServicePlane));
 if(!device || !device->metaCast("IOUSBHostDevice"))return true;
 const auto vendor=number(device,"idVendor"),product=number(device,"idProduct");
 const bool known=namedIntelProduct(vendor,product);
 const bool deviceBluetooth=number(device,"bDeviceClass")==0xe0 && number(device,"bDeviceSubClass")==1 && number(device,"bDeviceProtocol")==1;
 // The generic Intel profile must not classify every Intel USB hub as BT.
 // Unknown products need an actual Bluetooth-class device or interface.
 if(!known && !interfaceBluetooth && !deviceBluetooth)return true;
 // Global process-page edits cannot safely select between simultaneous USB
 // controllers. Retain a bounded candidate set; ambiguous configurations do
 // not gain extras merely because one recognized controller was seen first.
 for(auto &entry:candidates){auto *value=__atomic_load_n(&entry,__ATOMIC_ACQUIRE);if(value==device)return true;
  if(!value){device->retain();IOService *expected=nullptr;
   if(__atomic_compare_exchange_n(&entry,&expected,device,false,__ATOMIC_RELEASE,__ATOMIC_RELAXED))return true;
   device->release();if(expected==device)return true;}}
 disable("Bluetooth USB candidate limit exceeded");return true;
}
// Select the physical Intel USB controller, not firmware/native readiness.
// The stock loader and native HCI stack own firmware/capability initialization.
static bool selectIntelController(){
 IOService *sole=nullptr;
 for(auto &entry:candidates){auto *v=__atomic_load_n(&entry,__ATOMIC_ACQUIRE);if(!v || v->isInactive())continue;if(sole)return false;sole=v;}
 if(!sole)return false;auto *d=__atomic_load_n(&controller,__ATOMIC_ACQUIRE);
 if(d && d!=sole)return false;
 auto *v=OSDynamicCast(OSNumber,sole->getProperty("idVendor"));auto *p=OSDynamicCast(OSNumber,sole->getProperty("idProduct"));
 if(!v || !p || v->unsigned32BitValue()!=0x8087 || !p->unsigned32BitValue())return false;
 if(!d){__atomic_store_n(&controller,sole,__ATOMIC_RELEASE);
  sole->setProperty("KGPBLESelection","Intel USB; no HCI preflight");
  SYSLOG("kgpble","Intel USB selected; no HCI preflight or FirmwareLoaded gate; native initialization unchanged");
 }
 return true;
}
static void initialize(){
 if(!checkKernelArgument("-kgpble") && !checkKernelArgument("-kgpble25332"))return;
 __atomic_store_n(&state,Waiting,__ATOMIC_RELEASE);
 auto *m=IOService::serviceMatching("IOUSBHostDevice");
 if(m){notification=IOService::addMatchingNotification(gIOFirstMatchNotification,m,matched,nullptr);m->release();}
 auto *interfaces=IOService::serviceMatching("IOUSBHostInterface");
 if(interfaces){interfaceNotification=IOService::addMatchingNotification(gIOFirstMatchNotification,interfaces,matched,nullptr);interfaces->release();}
 if(!notification || !interfaceNotification)disable("controller/interface notification unavailable");
}
static void resolve(KernelPatcher &patcher){
 if(__atomic_load_n(&state,__ATOMIC_ACQUIRE)!=Waiting)return;
 getBlobs=reinterpret_cast<decltype(getBlobs)>(patcher.solveSymbol(KernelPatcher::KernelID,"_ubc_get_cs_blobs"));patcher.clearError();
 getBase=reinterpret_cast<decltype(getBase)>(patcher.solveSymbol(KernelPatcher::KernelID,"_csblob_get_base_offset"));patcher.clearError();
 getCDHash=reinterpret_cast<decltype(getCDHash)>(patcher.solveSymbol(KernelPatcher::KernelID,"_csblob_get_cdhash"));patcher.clearError();
 if(!getBlobs || !getBase || !getCDHash)disable("opaque native code-signing accessors unavailable");
 else __atomic_store_n(&accessorsReady,true,__ATOMIC_RELEASE);
}
// Workloop only: no vnode lookup, file I/O, blocking lock or file allocation
// is added to cs_validate_page. A daemon that starts before qualification
// disables the extra component for this boot rather than being patched late.
static void prepare(OSObject *,IOTimerEventSource *source){
 if(__atomic_load_n(&state,__ATOMIC_ACQUIRE)!=Waiting)return;
 if(++attempts>1200){disable("bounded preparation expired");return;}
 if(!__atomic_load_n(&accessorsReady,__ATOMIC_ACQUIRE)){source->setTimeoutMS(50);return;}
 auto ctxt=vfs_context_create(nullptr);if(!ctxt){source->setTimeoutMS(50);return;}
 vnode_t vnode=nullptr;int error=vnode_lookup("/usr/sbin/bluetoothd",0,&vnode,ctxt);
 if(error){vfs_context_rele(ctxt);source->setTimeoutMS(50);return;}
 uint32_t expected=Waiting;
 if(!__atomic_compare_exchange_n(&state,&expected,Preparing,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)){
  vnode_put(vnode);vfs_context_rele(ctxt);return;}
 size_t bytes=FileIO::readFileSize(vnode,ctxt);uint8_t *file=nullptr;Reason result=MalformedImage;uint32_t detail=0xffffffff;
 if(bytes>=32 && bytes<=48*1024*1024){file=Buffer::create<uint8_t>(bytes);
  if(file && !FileIO::readFileData(file,0,bytes,vnode,ctxt))result=qualify(file,bytes,hash,plan,&detail);}
 if(file)Buffer::deleter(file);
 if(result==Good && !vnode_ref(vnode)){
  qualifiedVnode=vnode; // never unloaded: held to prevent vnode reuse/ABA
  expected=Preparing;
  if(__atomic_compare_exchange_n(&state,&expected,Ready,false,__ATOMIC_RELEASE,__ATOMIC_RELAXED))
   SYSLOG("kgpble","native patch locations prepared (%u pages); no HCI preflight; NOT native Ready",plan.pageCount);
  else{qualifiedVnode=nullptr;vnode_rele(vnode);}
 }else {if(detail<contractCount)SYSLOG("kgpble","unmatched native contract: %s",contracts[detail].name);disable(reasonName(result==Good?MalformedImage:result));}
 vnode_put(vnode);vfs_context_rele(ctxt);
}
static void start(IOService *owner){
 if(__atomic_load_n(&state,__ATOMIC_ACQUIRE)!=Waiting)return;
 loop=IOWorkLoop::workLoop();if(!loop){disable("preparation workloop unavailable");return;}
 timer=IOTimerEventSource::timerEventSource(owner,prepare);
 if(!timer || loop->addEventSource(timer)!=kIOReturnSuccess){disable("preparation event source unavailable");return;}
 timer->setTimeoutMS(1);
}
static bool approvePage(vnode_t vnode,uint64_t offset,const void *data,const int *validated,const int *tainted,const int *nx){
 uint32_t current=__atomic_load_n(&state,__ATOMIC_ACQUIRE);
 if(current==Waiting || current==Preparing){disable("bluetoothd validation preceded complete qualification; no late/partial activation");return false;}
 if(current!=Ready)return false;
 const auto *page=pageInfo(plan,offset);if(!page)return false;
 if(vnode!=qualifiedVnode || !validated || !*validated || !tainted || *tainted || !nx || *nx || !data || !selectIntelController()){
  disable("vnode/controller/native validation changed");return false;}
 // Borrowed blob remains inside native vnode/pager validation lifetime. Only
 // opaque read-only accessors, no ubc_cs_getcdhash/vnode locks/private layout.
 void *blob=getBlobs(vnode);const auto *cd=blob?getCDHash(blob):nullptr;
 if(!blob || getBase(blob)!=off_t(plan.sliceOffset) || !cd || !equal(cd,plan.cdhash,20)){
  disable("qualified signed slice binding changed");return false;}
 uint8_t digest[32];if(!hash(static_cast<const uint8_t *>(data),4096,digest) || !equal(digest,page->sha256,32)){
  disable("qualified original page content changed");return false;}
 return true;
}
static void patchPage(uint64_t offset,const void *data,bool approved){
 if(approved && __atomic_load_n(&state,__ATOMIC_ACQUIRE)!=Ready)return;
 uint32_t changed=apply(plan,static_cast<uint8_t *>(const_cast<void *>(data)),4096,offset,approved);
 if(!changed){if(approved)disable("edit preconditions changed after base patch processing");return;}
 uint32_t previous=OSBitOrAtomic(changed,&appliedMask);
 if((previous&changed)!=changed)SYSLOG("kgpble","Intel AX/BE edits page=%llx bits=%x accumulated=%x; native completion still required",offset,changed,previous|changed);
}
}
