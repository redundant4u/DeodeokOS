#!/bin/bash

make clean
make
qemu-system-x86_64 -L . -m 64 -drive file=Disk.img,format=raw -localtime -M pc -curses