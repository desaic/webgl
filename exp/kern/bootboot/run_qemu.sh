#!/usr/bin/sh
qemu-system-x86_64 -drive file=kern.img,format=raw -serial stdio
