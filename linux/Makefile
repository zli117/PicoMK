ccflags-y := -D IBP_KERNEL_MODULE=1 -std=gnu11 -Wno-declaration-after-statement

obj-m += spi_picomk.o
spi_picomk-objs+= spi_module.o ../ibp_lib.o keyboard.o mouse.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install

device_tree:
	dtc -O dtb -o spi1-picomk.dtbo spi1-picomk-overlay.dts
