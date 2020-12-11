#
# mykernel/Makefile
#
# Copyright (C) 2017 - 2020 bzt (bztsrc@gitlab)
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation
# files (the "Software"), to deal in the Software without
# restriction, including without limitation the rights to use, copy,
# modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
# This file is part of the BOOTBOOT Protocol package.
# @brief An example Makefile for sample kernel
#
#

CFLAGS = -g -Wall -fpic -ffreestanding -fno-stack-protector -nostdinc -nostdlib -I./bootboot/dist/

all: kern.elf

SRCS=$(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))

#ASMS=$(wildcard *.S)
#AOBJS := $(patsubst %.S,%.o,$(ASMS))

CC=x86_64-elf-gcc
LD=x86_64-elf-ld
STRIP=x86_64-elf-strip
OBJDUMP=x86_64-elf-objdump
OBJCOPY=x86_64-elf-objcopy

kern.elf: $(OBJS) font.o
	$(CC) -nostdlib -nostartfiles -T link.ld $^ -o kern.elf -lgcc
	$(OBJCOPY) --only-keep-debug kern.elf kern.sym
	$(STRIP) -s -K mmio -K fb -K bootboot -K environment kern.elf

font.o: font.psf
	$(LD) -r -b binary -o font.o font.psf

%.o: %.c
	$(CC) $(CFLAGS) -mno-red-zone -c $< -o $@

#%.o: %.S
#	$(CC) $(CFLAGS) -mno-red-zone -c $< -o $@
clean:
	rm -f *.o *.elf *.txt bootboot/*.img
	rm bootboot/boot/*.elf
