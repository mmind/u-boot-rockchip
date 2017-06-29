#!/bin/sh -e

#CROSS_COMPILE=aarch64-linux-gnu- CFLAGS='-gdwarf-3' make evb-rk3399_defconfig all -j8
#~/src/gerrit-rockchip/u-boot/tools/loaderimage --pack --uboot ./u-boot.bin rk3399_uboot.img
#echo "IMG ready!"

rk_tools/loaderimage --pack --uboot ./u-boot-dtb.bin uboot.img

dd if=rk_tools/3328/rk3328_ddr_786MHz_v1.06.bin of=DDRTEMP bs=4 skip=1
tools/mkimage -n rk3328 -T rksd -d DDRTEMP idbloader.img
cat rk_tools/3328/rk3328_miniloader_v2.43.bin >> idbloader.img
#cp idbloader.img ${OUT}/u-boot/	
#cp rk_tools/3328/rk3328_loader_ddr786_v1.06.243.bin ${OUT}/u-boot/

cat >trust.ini <<EOF
[VERSION]
MAJOR=1
MINOR=2
[BL30_OPTION]
SEC=0
[BL31_OPTION]
SEC=1
PATH=rk_tools/3328/rk3328_bl31_v1.34.bin
ADDR=0x10000
[BL32_OPTION]
SEC=0
[BL33_OPTION]
SEC=0
[OUTPUT]
PATH=trust.img
EOF

rk_tools/trust_merger trust.ini
