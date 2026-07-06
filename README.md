## DirectHW by coresystems GmbH

DirectHW is a software compatibility layer for Mac OS X. It provides a kernel driver and framework that emulates the most commonly used hardware access functions on x86 machines, such as:

* iopl
* inb, inw, inl, outb, outw, outl
* rdmsr, wrmsr
* mmap() of physical address space

This package was originally intended to get the coreboot® utilities running on Mac OS X. But you're encouraged to use it for many more opportunities.

Visit http://www.coresystems.de/ for more information.

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
