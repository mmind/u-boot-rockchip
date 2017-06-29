#!/bin/sh -e

# You can use DB, WL to replace these command set
# Flash U-Boot to offset 0x4000, atf to offset 0x6000
#~/src/upgrade_tool/upgrade_tool UL ~/src/upgrade_tool/rk3399miniloader_V040601_emmc100M_DDR200.bin
#~/src/upgrade_tool/upgrade_tool DI uboot rk3399_uboot.img ~/src/upgrade_tool/parameter.txt
#~/src/upgrade_tool/upgrade_tool RD

LOADER1_SIZE=8000
RESERVED1_SIZE=128
RESERVED2_SIZE=8192
LOADER2_SIZE=8192
ATF_SIZE=8192
BOOT_SIZE=229376

SYSTEM_START=0
LOADER1_START=64
RESERVED1_START=$(expr ${LOADER1_START} + ${LOADER1_SIZE})
RESERVED2_START=$(expr ${RESERVED1_START} + ${RESERVED1_SIZE})
LOADER2_START=$(expr ${RESERVED2_START} + ${RESERVED2_SIZE})
ATF_START=$(expr ${LOADER2_START} + ${LOADER2_SIZE})
BOOT_START=$(expr ${ATF_START} + ${ATF_SIZE})
ROOTFS_START=$(expr ${BOOT_START} + ${BOOT_SIZE})

../../03_rockchip/rkdeveloptool/rkdeveloptool db ../../03_rockchip/rkbin/rk33/rk3328_loader_ddr786_v1.06.243.bin
sleep 2
../../03_rockchip/rkdeveloptool/rkdeveloptool wl $LOADER1_START idbloader.img
../../03_rockchip/rkdeveloptool/rkdeveloptool wl $LOADER2_START uboot.img
../../03_rockchip/rkdeveloptool/rkdeveloptool wl $ATF_START trust.img
../../03_rockchip/rkdeveloptool/rkdeveloptool rd


