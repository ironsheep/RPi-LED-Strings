#
# REF: https://qnaplus.com/how-to-compile-linux-kernel-module/
#

LKM_NAME=LEDfifo

obj-m+=$(LKM_NAME).o

SRCS = LEDfifoLKM.c
TEST_SRCS = testApp.c ioctlSampleApp.c

OBJS = $(SRCS:.c=.o)
TEST_OBJS = $(TEST_SRCS:.c=.o)

$(LKM_NAME)-y = $(OBJS)

testApp: testApp.o

ioctlSampleApp: ioctlSampleApp.o
 
all: testApp ioctlSampleApp
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

cleanTest:
	rm -f testApp ioctlSampleApp

clean:  cleanTest
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	
