/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define ROCKCHIP_DEVICE_SETTINGS \
		"stdin=serial,cros-ec-keyb\0" \
		"stdout=serial,vidconsole\0" \
		"stderr=serial,vidconsole\0"

#include <configs/rk3288_common.h>

#define CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV 0
/* SPL @ 32k for ~36k
 * ENV @ 96k
 * u-boot @ 128K
 */
#define CONFIG_ENV_OFFSET (96 * 1024)

#define CONFIG_SYS_WHITE_ON_BLACK

#ifndef CONFIG_SPL_BUILD

#undef CONFIG_BOOTARGS
#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS \
	"fdt_high=0x0fffffff\0" \
	"initrd_high=0x0fffffff\0" \
	"loadaddr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"bootargs=earlyprintk console=ttyS2,115200n8 init=/sbin/init ip=dhcp nfsroot=192.168.140.1:/home/devel/nfs/rootfs-firefly root=/dev/nfs rw noinitrd\0" \

#endif

/* USB */
#define CONFIG_USB_DWC2
#define CONFIG_USB_HOST_ETHER	/* Enable USB Ethernet adapters */

#define CONFIG_USB_ETHER_ASIX

#define CONFIG_BOOTP_SUBNETMASK
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME
#define CONFIG_BOOTP_BOOTPATH

#define CONFIG_SERVERIP		192.168.140.1
#define CONFIG_BOOTFILE		"hstuebner/firefly.vmlinuz"

/* 	"regulator dev host-pwr; " \
	"regulator enable; " \
	"usb start; " \
*/
#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND \
	"dhcp 0x4000000; " \
	"bootm 0x4000000; "

#endif
