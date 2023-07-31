ccflags-y := -D IBP_KERNEL_MODULE=1 -std=gnu11 -Wno-declaration-after-statement

obj-m += picomk_spi.o
picomk_spi-objs+= spi_module.o ../ibp_lib.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean