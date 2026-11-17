// Kext module metadata for the separate experimental BlueToolFixup build.
// GPL-2.0. Lilu's original lifecycle functions are retained.
#include <mach/mach_types.h>
#include <libkern/libkern.h>
extern "C" {
extern kern_return_t _start(kmod_info_t *,void *);
extern kern_return_t _stop(kmod_info_t *,void *);
extern kern_return_t BlueToolFixup_kern_start(kmod_info_t *,void *);
extern kern_return_t BlueToolFixup_kern_stop(kmod_info_t *,void *);
KMOD_EXPLICIT_DECL(as.acidanthera.BlueToolFixup,"2.7.4",_start,_stop)
__attribute__((visibility("hidden"))) kmod_start_func_t *_realmain=BlueToolFixup_kern_start;
__attribute__((visibility("hidden"))) kmod_stop_func_t *_antimain=BlueToolFixup_kern_stop;
__attribute__((visibility("hidden"))) int _kext_apple_cc=__APPLE_CC__;
}
