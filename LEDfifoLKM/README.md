# RPi-LED-Strings/LEDfifoLKM
Linux Kernel Loadable Module: a 256x3x24b FIFO for driving LED Matrix GPIO Pins

- ioctl to configure bit rate/timing
- write to hand off single screen FIFO content for display on LED Matrix Screen
- writev to hand off DMA-like FIFO content (multiple frames) for display on LED Matrix Screen
- ioctl to configure looping/replay of multi-frame screen-set
- /proc/drivers - cat 'em to see current config values


---

