# RPi-LED-Strings/LEDfifoLKM
Linux Kernel Loadable Module: a 256x3x24b FIFO for driving LED Matrix GPIO Pins

- ioctl(2) to configure bit rate/timing
- write(2) to hand off single screen FIFO content for display on LED Matrix Screen
- writev(2) to hand off DMA-like FIFO content (multiple frames) for display on LED Matrix Screen
- ioctl(2) to configure looping/replay of multi-frame screen-set
- /proc filesystem:  cat  /proc/driver/ledfifo/config  to see current config values

---

