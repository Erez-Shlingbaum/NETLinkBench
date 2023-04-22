KDIR := /lib/modules/$(shell uname -r)/build

obj-m += nlbench.o

default:
	$(MAKE) -C $(KDIR) M=$(shell pwd) modules

userspace:
	${CC} -g -c lib.c -o lib.o
	${CC} -g -c send.c -o send.o
	${CC} -g -c recv.c -o recv.o
	${CC} -g -c nlping.c -o nlping.o
	${CC} send.o lib.o -o nlbenchsend
	${CC} recv.o lib.o -o nlbenchrecv
	${CC} nlping.o lib.o -o nlping

clean:
	rm -rf *.o *.ko *.mod.* *.cmd .module* modules* Module* .*.cmd .tmp*
	rm -f netlinkbench nlbenchsend nlbenchrecv nlping
