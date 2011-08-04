#!/bin/sh

prjCreate -type bootapp -p $PWD/bootrom653 -bsp qemu

cd bootrom653

#MODE=romResident
MODE=romCompressed

prj projBuildSet -prj . PPC604gnu_$MODE

prj domComponentRemove -prj . INCLUDE_FLASH
prj domComponentRemove -prj . INCLUDE_MV_END
prj domComponentRemove -prj . INCLUDE_BYTENVRAM

#prj domComponentAdd -prj . INCLUDE_TFTP_CLIENT
#prj domComponentAdd -prj . INCLUDE_MAL_TFTP
#prj domComponentAdd -prj . INCLUDE_MAL_FTP
#prj domComponentAdd -prj . INCLUDE_MAL_HTTP

#prj domComponentAdd -prj . INCLUDE_ATA
#prj domParameterValueSet -prj . DEFAULT_BOOT_LINE '"ata@0,0(0,0)host:/hda/boot.txt h=10.0.2.2 e=10.0.2.1"'

# For FTP
prj domParameterValueSet -prj . DEFAULT_BOOT_LINE '"ene(0,0)host:boot.txt e=10.0.2.1:ffffff00 h=10.0.2.2 u=vxworks pw=secret"'

# INCLUDE_CBIO_MAIN
# INCLUDE_CBIO_DCACHE
# INCLUDE_CBIO_DPART
# INCLUDE_DOSFS2
# INCLUDE_KERNEL_FULL
# INCLUDE_DOSFS2_DIR_VFAT

make ADD_NEEDED
make



dd if=/dev/zero bs=256 count=1 of=pad256.bin
dd if=/dev/zero bs=1024 count=1024 of=pad1M.bin
objcopyppc -O binary PPC604gnu_$MODE/bootApp_$MODE bootapp.kern
cat pad256.bin bootapp.kern pad1M.bin > bootapp.tmp
dd if=bootapp.tmp bs=1024 count=1024 of=bootapp.bin
rm bootapp.tmp


# ~/src/qemu.hg/ppc-softmmu/qemu-system-ppc -M prep -cpu 750gx -L ../bootrom653 -bios bootapp.bin -nographic -m 256 nul
