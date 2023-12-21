
obj-m := stplr.o

ifeq ($(KERNELRELEASE),)

KERNELRELEASE := `uname -r`
KDIR := /lib/modules/$(KERNELRELEASE)/build

.PHONY: all install clean distclean
.PHONY: stplr.ko

all: stplr.ko

stplr.ko:
	@echo "Building $@ driver..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

clean:
	rm -f *~
	rm -f Module.symvers Module.markers modules.order
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif # !KERNELRELEASE
