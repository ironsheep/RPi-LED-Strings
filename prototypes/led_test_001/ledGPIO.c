/* 
   led_test_001 - testbed for GPIO and WS2812B String

   Copyright (C) 2019 Stephen M Moraco

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  

*/

#include <stdio.h> 
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>	// sleep() and others

#include "ledGPIO.h"
#include "piSystem.h"

// Access from ARM Running Linux

//#define BCM2708_PERI_BASE        0x20000000
	// RPi 4
#define BCM2708_PERI_BASE        0xFE000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */


#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
void *gpio_map;

// I/O access
volatile unsigned *gpio;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define SET_GPIO(g) (*(gpio+7) = 1<<(g))
#define CLR_GPIO(g) (*(gpio+10) = 1<<(g))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock



void initGPIO(void)
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }

   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );

   close(mem_fd); //No need to keep mem_fd open after mmap

   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }

   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;

   // setup GPIO.0 as output
   INP_GPIO(LSP_TOP);
   OUT_GPIO(LSP_TOP);
   
   INP_GPIO(LSP_MIDDLE);
   OUT_GPIO(LSP_MIDDLE);
   
   INP_GPIO(LSP_BOTTOM);
   OUT_GPIO(LSP_BOTTOM);

   printf("- GPIO is setup\n");

   showSysInfo();
}

void restoreGPIO(void)
{
   INP_GPIO(LSP_TOP);
   INP_GPIO(LSP_MIDDLE);
   INP_GPIO(LSP_BOTTOM);
   printf("- GPIO is reset\n");
}

void blinkLED(void)
{
	// TEST code! LED Resistor wired to GPIO (pin 17)
 	int uSec50milliSec = 50 * 1000;
	int loopMax = 100;
	for(int x=0; x<loopMax; x++) {
		printf("- blink %d of %d\n", x+1, loopMax);
		SET_GPIO(LSP_TOP);
		usleep(uSec50milliSec);
		CLR_GPIO(LSP_TOP);
		usleep(uSec50milliSec);
	}
}
#define USING_WS2812B

// timing for WS2815  1360nSec period | 735.294KHz
#ifdef USING_WS2815

#define BASE_PERIOD_IN_NSEC 51
#define T0H_MULTIPLE 6
#define T1H_MULTIPLE 21
#define T0_PERIOD_MULTIPLE 27
#define TRESET_IN_USEC 280

#endif

// timing for WS2812B  1250nSec period | 800KHz
#ifdef USING_WS2812B

#define BASE_PERIOD_IN_NSEC 50
#define T0H_MULTIPLE 8
#define T1H_MULTIPLE 16
#define T01_PERIOD_MULTIPLE 25
#define TRESET_IN_USEC 50

#endif


#define T0H_IN_NSEC (T0H_MULTIPLE * BASE_PERIOD_IN_NSEC) 
#define T0L_IN_NSEC ((T01_PERIOD_MULTIPLE - T0H_MULTIPLE) * BASE_PERIOD_IN_NSEC)
#define T1H_IN_NSEC (T1H_MULTIPLE * BASE_PERIOD_IN_NSEC) 
#define T1L_IN_NSEC ((T01_PERIOD_MULTIPLE - T1H_MULTIPLE) * BASE_PERIOD_IN_NSEC)
#define TRESET_IN_NSEC (TRESET_IN_USEC * 1000) 

void xmitOne(eLedStringPins gpioPin)
{
	struct timespec nIdleTime;
	struct timespec nTimeRemaining;
	int errno;
	
	nIdleTime.tv_nsec = T1H_IN_NSEC;
	SET_GPIO(gpioPin);
	errno = nonosleep(&nIdleTime, &nTimeRemaining);
	if(errno != 0 || nTimeRemaining.tv_nsec != 0) {
		printf("- T1H short! interrupted or error");
	}
	nIdleTime.tv_nsec = T1L_IN_NSEC;
	CLR_GPIO(gpioPin);
	errno = nonosleep(&nIdleTime, &nTimeRemaining);
	if(errno != 0 || nTimeRemaining.tv_nsec != 0) {
		printf("- T1L short! interrupted or error");
	}
}

void xmitZero(eLedStringPins gpioPin)
{
	struct timespec nIdleTime;
	struct timespec nTimeRemaining;
	int errno;
	
	nIdleTime.tv_nsec = T0H_IN_NSEC;
	SET_GPIO(gpioPin);
	errno = nonosleep(&nIdleTime, &nTimeRemaining);
	if(errno != 0 || nTimeRemaining.tv_nsec != 0) {
		printf("- T1H short! interrupted or error");
	}
	nIdleTime.tv_nsec = T0L_IN_NSEC;
	CLR_GPIO(gpioPin);
	errno = nonosleep(&nIdleTime, &nTimeRemaining);
	if(errno != 0 || nTimeRemaining.tv_nsec != 0) {
		printf("- T1L short! interrupted or error");
	}
}

void xmitReset(eLedStringPins gpioPin)
{
	struct timespec nIdleTime;
	struct timespec nTimeRemaining;
	int errno;
	
	nIdleTime.tv_nsec = TRESET_IN_NSEC;
	CLR_GPIO(gpioPin);
	errno = nonosleep(&nIdleTime, &nTimeRemaining);
	if(errno != 0 || nTimeRemaining.tv_nsec != 0) {
		printf("- T1L short! interrupted or error");
	}
}

void testBit0Send(void)
{
	printf("- TEST 0's START");
	for(int ctr=0; ctr<1000; ctr++) {
		xmitZero(LSP_TOP);
	}
	printf("- TEST 0's END");
}

void testBit1Send(void);
{
	printf("- TEST 1's START");
	for(int ctr=0; ctr<1000; ctr++) {
		xmitOne(LSP_TOP);
	}
	printf("- TEST 1's START");
}

void testResetSend(void);
{
	printf("- TEST RESET's START");
	SET_GPIO(LSP_TOP);
	for(int ctr=0; ctr<1000; ctr++) {
		xmitReset(LSP_TOP);
		SET_GPIO(LSP_TOP);
	}
	printf("- TEST RESET's START");
}


