## DirectHW by coresystems GmbH

DirectHW is a software compatibility layer for macOS. It provides a kernel driver and framework that emulates the most commonly used hardware access functions, such as:

* iopl
* inb, inw, inl, outb, outw, outl
* rdmsr, wrmsr
* mmap() of physical address space

This package was originally intended to get the coreboot® utilities running on Mac OS X. But you're encouraged to use it for many more opportunities.

Visit http://www.coresystems.de/ for more information.

### Architecture support

DirectHW builds for both Intel (`x86_64`) and Apple Silicon (`arm64e`).
Because the hardware access model differs fundamentally between the two, some
primitives change meaning on Apple Silicon:

| API | x86_64 | arm64e |
|-----|--------|--------|
| `inb`/`outb` … | I/O port instructions (`IN`/`OUT`) | **physical MMIO** access — there is no I/O port space on ARM, so the address is treated as a physical address |
| `rdmsr`/`wrmsr` | `RDMSR`/`WRMSR` of any MSR number | a **curated set** of AArch64 system registers, selected with the `DHW_ARM_*` constants in `DirectHW.h` (`MRS`/`MSR`) |
| `rdcpuid` | `CPUID` | `MIDR_EL1` / `MPIDR_EL1` identification |
| `map_physical`/`readmem32` | physical mapping | physical mapping (subject to PPL/SPTM restrictions) |

Note that on Apple Silicon many EL1 registers and physical ranges are protected
by the platform, and loading a third-party kext requires reduced security plus
explicit user approval.

### Building/Installing


Partially disable SIP and enable third-party kexts if not already set.

Boot to Recovery mode. Then in the Recovery Mode Terminal:

	csrutil enable --without kext --without debug --without nvram
	reboot

In Terminal:

	sudo nvram boot-args="-arm64e_preview_abi kext-dev-mode=1 debug=0x144"
	sudo reboot

Then simply:

	git clone https://github.com/sigma-1/directhw.git
	cd directhw
	make && sudo make install

Select a target architecture explicitly with `ARCH`:

	make ARCH=arm64e      # Apple Silicon
	make ARCH=x86_64      # Intel
	make ARCH=universal   # fat (arm64e + x86_64)
