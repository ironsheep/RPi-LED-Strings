#
# REF: https://qnaplus.com/how-to-compile-linux-kernel-module/
#

LKM_NAME=LEDfifo

obj-m+=$(LKM_NAME).o

SRCS = LEDfifoLKM.c

OBJS = $(SRCS:.c=.o)

$(LKM_NAME)-y = $(OBJS)
 
all: 
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean: 
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
