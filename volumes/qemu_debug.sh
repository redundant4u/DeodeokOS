#!/bin/bash

qemu-system-x86_64 -L . -m 64 -fda ./Disk.img -hda ./HDD.img -localtime -M pc -curses -gdb tcp::1234 -S
#qemu-system-x86_64 -L . -m 64 -drive file=Disk.img,format=raw -localtime -M pc -curses
