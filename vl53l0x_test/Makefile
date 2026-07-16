CFLAGS=-I$(ROOTFS_INC) -I./
CC=/opt/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.6/bin/loongarch64-linux-gnu-gcc
all: vl53l0x_test vl53l0x_reg vl53l0x_parameter
vl53l0x_test: vl53l0x_test.o
	$(CC) -o vl53l0x_test vl53l0x_test.o $(CFLAGS)
vl53l0x_reg: vl53l0x_reg.o
	$(CC) -o vl53l0x_reg vl53l0x_reg.o $(CFLAGS)
vl53l0x_parameter: vl53l0x_parameter.o
	$(CC) -o vl53l0x_parameter vl53l0x_parameter.o $(CFLAGS)

.PHONY: clean

clean:
	rm -f ./*.o *~ core vl53l0x_test vl53l0x_reg vl53l0x_parameter

