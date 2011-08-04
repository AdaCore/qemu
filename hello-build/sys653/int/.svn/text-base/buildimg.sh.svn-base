#!/bin/sh

dd if=/dev/zero of=pad1M.bin bs=1024 count=1024
cat coreOS.bin pos.bin configRecord.bin hello.bin pad1M.bin > img0.bin
dd if=img0.bin of=img.bin bs=1024 count=3072

dd if=/dev/zero of=pad256.bin bs=256 count=1
cat pad256.bin sms_romPayload.bin pad1M.bin > boot0.bin
dd if=boot0.bin of=boot.bin bs=1024 count=1024

cat img.bin boot.bin > flash.bin
