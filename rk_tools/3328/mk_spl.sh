#!/bin/sh

set -e
#tools/mkimage -n rk3399 -T rksd -d spl/u-boot-spl.bin idbspl.img
#cp rk_tools/3399/bl31* .
#tools/mkimage -f fit_spl_atf.its -E bl3.itb
echo "IMG ready!"
echo "Write idbspl.img to 0x40"
echo "Write bl3.itb to 0x200"
