/* DirectHW - Kernel extension to pass through IO commands to user space
 *
 * Copyright © 2008-2010 coresystems GmbH <info@coresystems.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <stdint.h>

#include "DirectHW.hpp"
#include "DirectHW_arch.hpp"

#define DEBUG_KEXT

#define super IOService

OSDefineMetaClassAndStructors(DirectHWService, IOService)

bool DirectHWService::start(IOService * provider)
{
        //IOLog("DirectHW: Driver v%s (compiled on %s) loaded. "
		//"Visit http://www.coresystems.de/ for more information.\n",
			//DIRECTHW_VERSION, __DATE__);

	if (super::start(provider)) {
		registerService();
		return true;
	}

	return false;
}

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(DirectHWUserClient, IOUserClient)

const IOExternalMethod DirectHWUserClient::fMethods[kNumberOfMethods] = {
	{0, (IOMethod) & DirectHWUserClient::ReadIO, kIOUCStructIStructO, sizeof(iomem_t), sizeof(iomem_t)},
	{0, (IOMethod) & DirectHWUserClient::WriteIO, kIOUCStructIStructO, sizeof(iomem_t), sizeof(iomem_t)},
	{0, (IOMethod) & DirectHWUserClient::PrepareMap, kIOUCStructIStructO, sizeof(map_t), sizeof(map_t)},
	{0, (IOMethod) & DirectHWUserClient::ReadMSR, kIOUCStructIStructO, sizeof(msrcmd_t), sizeof(msrcmd_t)},
	{0, (IOMethod) & DirectHWUserClient::WriteMSR, kIOUCStructIStructO, sizeof(msrcmd_t), sizeof(msrcmd_t)},
	{0, (IOMethod) & DirectHWUserClient::ReadCpuId, kIOUCStructIStructO, sizeof(cpuid_t), sizeof(cpuid_t)},
	{0, (IOMethod) & DirectHWUserClient::ReadMem, kIOUCStructIStructO, sizeof(readmem_t), sizeof(readmem_t)},
};

bool DirectHWUserClient::initWithTask(task_t task, void *securityID, UInt32 type)
{
	bool ret;

	ret = super::initWithTask(task, securityID, type);

#ifdef DEBUG_KEXT
	IOLog("DirectHW: initWithTask(%p, %p, %08x)\n", (void *)task, securityID, (unsigned int)type);
#endif
        if (!ret) {
		IOLog("DirectHW: initWithTask failed.\n");
		return false;
	}

	fTask = task;

	return true;
}

IOExternalMethod *DirectHWUserClient::getTargetAndMethodForIndex(IOService ** target, UInt32 index)
{
	if (index < (UInt32) kNumberOfMethods) {
		if (fMethods[index].object == (IOService *) 0)
			*target = this;

		return (IOExternalMethod *) & fMethods[index];
	} else {
		*target = NULL;
		return NULL;
	}
}

bool DirectHWUserClient::start(IOService * provider)
{
	bool success;

#ifdef DEBUG_KEXT
	IOLog("DirectHW: Starting DirectHWUserClient\n");
#endif

	fProvider = OSDynamicCast(DirectHWService, provider);
	success = (fProvider != NULL);
/*
	if (kIOReturnSuccess != clientHasPrivilege(current_task(),kIOClientPrivilegeAdministrator)) {
		IOLog("DirectHW: Need to be administrator.\n");
		success = false;
	}
*/
	if (success) {
		success = super::start(provider);
#ifdef DEBUG_KEXT
		IOLog("DirectHW: Client successfully started.\n");
#endif
	} else {
		IOLog("DirectHW: Could not start client.\n");
	}

    /* Dump a few control/system registers as a debug aid (arch specific). */
    dhw::dump_control_regs();
    return success;
}

void DirectHWUserClient::stop(IOService *provider)
{
#ifdef DEBUG_KEXT
	IOLog("DirectHW: Stopping client.\n");
#endif
	super::stop(provider);
}

IOReturn DirectHWUserClient::clientClose(void)
{
	bool success = terminate();
	if (!success) {
		IOLog("DirectHW: Client NOT successfully closed.\n");
	} else {
#ifdef DEBUG_KEXT
		IOLog("DirectHW: Client successfully closed.\n");
#endif
	}

	return kIOReturnSuccess;
}

IOReturn
DirectHWUserClient::ReadIO(iomem_t * inStruct, iomem_t * outStruct,
				IOByteCount inStructSize,
				IOByteCount * outStructSize)
{

	if (fProvider == NULL || isInactive()) {
		return kIOReturnNotAttached;
	}

	/*
	 * x86: `offset` is an I/O port. arm64: there is no port I/O space, so
	 * `offset` is interpreted as a physical MMIO address (see DirectHW_arch.hpp).
	 */
	uint32_t data = 0;
	if (!dhw::port_read(inStruct->offset, inStruct->width, &data)) {
		IOLog("DirectHW: Invalid read attempt %d bytes at IO address %llx\n",
		      (int)inStruct->width, (unsigned long long)inStruct->offset);
		return kIOReturnBadArgument;
	}
	outStruct->data = data;

#ifdef DEBUG_KEXT
	IOLog("DirectHW: Read %d bytes at IO address %x (result=%x)\n",
		      inStruct->width, inStruct->offset, outStruct->data);
#endif


	*outStructSize = sizeof(iomem_t);

	return kIOReturnSuccess;
}


IOReturn
DirectHWUserClient::WriteIO(iomem_t * inStruct, iomem_t * outStruct,
				 IOByteCount inStructSize,
				 IOByteCount * outStructSize)
{
	if (fProvider == NULL || isInactive()) {
		return kIOReturnNotAttached;
	} 
	
#ifdef DEBUG_KEXT
	IOLog("DirectHW: Write %d bytes at IO address %x (value=%x)\n",
		      inStruct->width, inStruct->offset, inStruct->data);
#endif
	
	/* x86: I/O port write. arm64: physical MMIO write (see DirectHW_arch.hpp). */
	if (!dhw::port_write(inStruct->offset, inStruct->width, inStruct->data)) {
		IOLog("DirectHW: Invalid write attempt %d bytes at IO address %llx\n",
		      (int)inStruct->width, (unsigned long long)inStruct->offset);
		return kIOReturnBadArgument;
	}

	*outStructSize = sizeof(iomem_t);

	return kIOReturnSuccess;
}


IOReturn
DirectHWUserClient::PrepareMap(map_t * inStruct, map_t * outStruct,
				IOByteCount inStructSize,
				IOByteCount * outStructSize)
{
	if (fProvider == NULL || isInactive()) {
		return kIOReturnNotAttached;
	} 

	if(LastMapAddr || LastMapSize)
		return kIOReturnNotOpen;

	LastMapAddr = inStruct->addr;
	LastMapSize = inStruct->size;

#ifdef DEBUG_KEXT
	IOLog("DirectHW: PrepareMap 0x%08llx[0x%llx]\n",
		(unsigned long long)LastMapAddr, (unsigned long long)LastMapSize);
#endif

	*outStructSize = sizeof(map_t);

	return kIOReturnSuccess;
}

void
DirectHWUserClient::CPUIDHelperFunction(void *data)
{
	CPUIDHelper * cpuData = (CPUIDHelper *)data;
    cpuData->out->core = (uint32_t)-1;
    if ((int)cpuData->in->core != cpu_number())
        return;
    /* x86: CPUID(eax,ecx). arm64: MIDR/ID_AA64* identification. */
    dhw::cpu_identify(cpuData->in->eax, cpuData->in->ecx, cpuData->out->output);
    cpuData->out->eax = cpuData->in->eax;
    cpuData->out->ecx = cpuData->in->ecx;
    cpuData->out->core = cpuData->in->core;
}

void
DirectHWUserClient::ReadMemHelperFunction(void *data)
{
	ReadMemHelper * memData = (ReadMemHelper *)data;
    memData->out->core = (uint32_t)-1;
    if ((int)memData->in->core != cpu_number())
        return;
    /*
     * Read a 32-bit word from the requested physical address. The original
     * x86 code read an uninitialised stack variable; use the IOKit mapped
     * physical accessor, which works on both architectures.
     */
    memData->out->data = dhw::phys_read32(memData->in->addr);
    memData->out->core = memData->in->core;
}

void
DirectHWUserClient::MSRHelperFunction(void *data)
{
	MSRHelper * MSRData = (MSRHelper *)data;
	msrcmd_t * inStruct = MSRData->in;
	msrcmd_t * outStruct = MSRData->out;

	outStruct->core = (UInt32)-1;
	outStruct->lo = INVALID_MSR_LO;
	outStruct->hi = INVALID_MSR_HI;

#if defined(__x86_64__) || defined(__i386__)
	/*
	 * x86: avoid issuing RDMSR/WRMSR from a Hyper-Threading sibling by
	 * deriving the SMT mask from CPUID. AArch64 has no such notion, so this
	 * whole block is x86 only.
	 */
	uint32_t cpuiddata[4];

	dhw::cpu_identify(1, 0, cpuiddata);
	//bool have_ht = ((cpuiddata[3] & (1 << 28)) != 0);
	uint32_t core_id = cpuiddata[1] >> 24;

	dhw::cpu_identify(11, 0, cpuiddata);
	uint32_t smt_mask = ~((-1) << (cpuiddata[0] &0x1f));

	if ((core_id & smt_mask) != core_id)
		return; // It's a HT thread
#endif

	if ((int)inStruct->core != cpu_number())
		return;

	IOLog("DirectHW: ReadMSRHelper core %u cpu %d index 0x%x\n",
		(unsigned int)inStruct->core, cpu_number(), (unsigned int)inStruct->index);

	/*
	 * x86: RDMSR/WRMSR of inStruct->index.
	 * arm64: inStruct->index is a DHW_ARM_* system-register selector.
	 * A failed access leaves the INVALID_MSR sentinel in the output.
	 */
	if (MSRData->Read) {
		uint32_t lo, hi;
		if (dhw::sysreg_read(inStruct->index, &lo, &hi)) {
			outStruct->lo = lo;
			outStruct->hi = hi;
		}
	} else {
		if (!dhw::sysreg_write(inStruct->index, inStruct->lo, inStruct->hi))
			return; // read-only / unknown selector: leave core = -1
	}

	outStruct->index = inStruct->index;
	outStruct->core = inStruct->core;
}

IOReturn
DirectHWUserClient::ReadCpuId(cpuid_t * inStruct, cpuid_t * outStruct,
				IOByteCount inStructSize,
				IOByteCount * outStructSize)
{
	if (fProvider == NULL || isInactive()) {
		return kIOReturnNotAttached;
	}

	CPUIDHelper cpuidData = { inStruct, outStruct};
	dhw::dispatch_on_core((int)inStruct->core,
			(void (*)(void *))CPUIDHelperFunction, (void *)&cpuidData);

	*outStructSize = sizeof(cpuid_t);

	if (outStruct->core != inStruct->core)
		return kIOReturnIOError;

	return kIOReturnSuccess;
}

IOReturn
DirectHWUserClient::ReadMem(readmem_t * inStruct, readmem_t * outStruct,
				IOByteCount inStructSize,
				IOByteCount * outStructSize)
{
	if (fProvider == NULL || isInactive()) {
		return kIOReturnNotAttached;
	}

	/*
	 * Run the read pinned to the requested core via dispatch_on_core(); the
	 * old code required the calling thread to already be on that core, which
	 * it almost never is.
	 */
	ReadMemHelper memData = { inStruct, outStruct};
	dhw::dispatch_on_core((int)inStruct->core,
			(void (*)(void *))ReadMemHelperFunction, (void *)&memData);

	*outStructSize = sizeof(readmem_t);

	if (outStruct->core != inStruct->core)
		return kIOReturnIOError;
	return kIOReturnSuccess;
}

IOReturn
DirectHWUserClient::ReadMSR(msrcmd_t * inStruct, msrcmd_t * outStruct,
				IOByteCount inStructSize,
				IOByteCount * outStructSize)
{
	if (fProvider == NULL || isInactive()) {
		return kIOReturnNotAttached;
	} 

	MSRHelper MSRData = { inStruct, outStruct, true };
	dhw::dispatch_on_core((int)inStruct->core,
			(void (*)(void *))MSRHelperFunction, (void *)&MSRData);

	*outStructSize = sizeof(msrcmd_t);

	if (outStruct->core != inStruct->core)
		return kIOReturnIOError;

	IOLog("DirectHW: ReadMSR(0x%08x) = 0x%08x:0x%08x\n",
			(unsigned int)inStruct->index,
			(unsigned int)outStruct->hi,
			(unsigned int)outStruct->lo);

	return kIOReturnSuccess;
}

IOReturn
DirectHWUserClient::WriteMSR(msrcmd_t * inStruct, msrcmd_t * outStruct,
				IOByteCount inStructSize,
				IOByteCount * outStructSize)
{
	if (fProvider == NULL || isInactive()) {
		return kIOReturnNotAttached;
	} 

	IOLog("DirectHW: WriteMSR(0x%08x) = 0x%08x:0x%08x\n",
			(unsigned int)inStruct->index,
			(unsigned int)inStruct->hi,
			(unsigned int)inStruct->lo);

	MSRHelper MSRData = { inStruct, outStruct, false };
	dhw::dispatch_on_core((int)inStruct->core,
			(void (*)(void *))MSRHelperFunction, (void *)&MSRData);

	*outStructSize = sizeof(msrcmd_t);

	if (outStruct->core != inStruct->core)
		return kIOReturnIOError;

	return kIOReturnSuccess;
}

IOReturn DirectHWUserClient::clientMemoryForType(UInt32 type, UInt32 *flags, IOMemoryDescriptor **memory)
{
	IOMemoryDescriptor *newmemory;

#ifdef DEBUG_KEXT
	IOLog("DirectHW: clientMemoryForType(%x, %p, %p)\n", type, flags, memory);
#endif
	if (type != 0) {
		IOLog("DirectHW: Unknown mapping type %x.\n", (unsigned int)type);
		return kIOReturnUnsupported;
	}

	if ((LastMapAddr == 0) && (LastMapSize == 0)) {
		IOLog("DirectHW: No PrepareMap called.\n");
		return kIOReturnNotAttached;
	}

#ifdef DEBUG_KEXT
	IOLog("DirectHW: Mapping physical 0x%08llx[0x%llx]\n",
			(unsigned long long)LastMapAddr, (unsigned long long)LastMapSize);
#endif

	newmemory = IOMemoryDescriptor::withPhysicalAddress(LastMapAddr, LastMapSize, kIODirectionIn);
	
	/* Reset mapping to zero */
	LastMapAddr = 0;
	LastMapSize = 0;

	if (newmemory == 0) {
		IOLog("DirectHW: Could not map memory!\n");
		return kIOReturnNotOpen;
	}

	newmemory->retain();
	*memory = newmemory;

#ifdef DEBUG_KEXT
	IOLog("DirectHW: Mapping succeeded.\n");
#endif

	return kIOReturnSuccess;
}

