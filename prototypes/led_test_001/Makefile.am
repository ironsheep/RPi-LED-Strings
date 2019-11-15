bin_PROGRAMS=led_test_001
led_test_001_SOURCES=led_test_001.c nSecDelayASM.s xmalloc.c system.h aclocal.m4 led_test_001.1 ledGPIO.c frameBuffer.c ledScreen.c  imageLoader.c commandProcessor.c debug.c 

#  uncomment the following if led_test_001 requires the math library
#led_test_001_LDADD=-lm
#led_test_001_LDADD=-lpthread -lbcm_host -lrt
#led_test_001_LDADD=-lpthread -lrt

EXTRA_DIST=led_test_001.lsm.in led_test_001.spec.in led_test_001.texinfo

#  if you write a self-test script named `chk', uncomment the
#  following and add `chk' to the EXTRA_DIST list
#TESTS=chk

#  build and install the .info pages
info_TEXINFOS = led_test_001.texinfo
led_test_001_TEXINFOS = gpl.texinfo

#  install the man pages
man_MANS=led_test_001.1
