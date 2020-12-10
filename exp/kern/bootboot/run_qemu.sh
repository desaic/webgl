#!/usr/bin/sh
cp ../kern.elf ./boot
mkbootimg example.json kern.img
cmd="qemu-system-x86_64 -drive file=kern.img,format=raw -serial stdio"
echo $cmd
$cmd
