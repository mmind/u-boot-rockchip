#!/bin/sh

tools/mkimage -n rk3036 -T rksd -d spl/u-boot-spl.bin uboot-rk3036.img
cat u-boot-dtb.bin >>uboot-rk3036.img

DDRINIT=RK3036_DDR3_400M_V1.06.bin
USBPLUG=rk303xusbplug.bin
LOADER=uboot-rk3036.img

echo rc4 ddrinit $DDRINIT
cat $DDRINIT | openssl rc4 -K 7c4e0304550509072d2c7b38170d1711 | rkflashtool l
sleep 1
echo rc4:usbplug $USBPLUG
cat $USBPLUG | openssl rc4 -K 7c4e0304550509072d2c7b38170d1711 | rkflashtool L
sleep 3
LOADER_SIZE=$(wc -c $LOADER  | awk '{print int(($1+511)/512)}')
echo loader: $LOADER, size: $LOADER_SIZE
rkflashtool w 64 $LOADER_SIZE < $LOADER
