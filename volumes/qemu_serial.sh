#!/bin/bash

qemu-system-x86_64 -L . -m 64 -fda ./Disk.img -hda ./HDD.img -boot a -localtime -M pc -curses -serial tcp::3030,server,nowait