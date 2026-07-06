/* DirectHW_kmod.c - kernel module descriptor for the DirectHW kext
 *
 * A kext must export a `kmod_info` symbol describing the module; Xcode
 * normally generates this, but a plain clang/-kext build has to provide it.
 * The actual _start/_stop entry points come from Apple's libkmod.a (linked
 * with -lkmod); on modern macOS the kernel performs C++ runtime init itself,
 * so libkmodc++.a is an empty stub and is not needed.
 *
 * This mirrors the file Xcode's "Generate KEXT Info" phase emits.
 */

#include <mach/mach_types.h>

/* Populated from the build (kext/Makefile). Fallbacks keep the file
 * self-contained if compiled standalone. */
#ifndef BUNDLEID_S
#define BUNDLEID_S    "com.beako.utils.DirectHW"
#endif
#ifndef KEXTVERSION_S
#define KEXTVERSION_S "0.0"
#endif

/* Provided by libkmod.a; invoked by the kernel on load/unload. */
extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);

extern void IOLog(const char *fmt, ...);

/*
 * The IOResources personality is not matched on a live-loaded kext, so we
 * publish/terminate DirectHWService explicitly from the module start/stop
 * (implemented in DirectHW.cpp).
 */
extern kern_return_t DirectHW_publishService(void);
extern void          DirectHW_terminateService(void);

static kern_return_t dhw_realmain(kmod_info_t *ki, void *data)
{
	kern_return_t r;
	(void)ki; (void)data;
	r = DirectHW_publishService();
	IOLog("DirectHW: publishService returned 0x%x\n", r);
	return KERN_SUCCESS;	/* stay loaded regardless, for diagnosability */
}

static kern_return_t dhw_antimain(kmod_info_t *ki, void *data)
{
	(void)ki; (void)data;
	DirectHW_terminateService();
	return KERN_SUCCESS;
}

__attribute__((visibility("default")))
kmod_info_t kmod_info = {
	.next            = NULL,
	.info_version    = KMOD_INFO_VERSION,
	.id              = (uint32_t)-1,	/* assigned by the kernel */
	.name            = BUNDLEID_S,
	.version         = KEXTVERSION_S,
	.reference_count = -1,
	.reference_list  = NULL,
	.address         = 0,
	.size            = 0,
	.hdr_size        = 0,
	.start           = _start,
	.stop            = _stop,
};

/* IOKit driver kexts have no explicit main(); the OSMetaClass machinery does
 * the work. libkmod's _start/_stop call through these null hooks. */
__private_extern__ kmod_start_func_t *_realmain = dhw_realmain;
__private_extern__ kmod_stop_func_t  *_antimain = dhw_antimain;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
