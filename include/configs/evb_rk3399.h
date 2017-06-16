/*
 * (C) Copyright 2016 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __EVB_RK3399_H
#define __EVB_RK3399_H

#include <configs/rk3399_common.h>

#define CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV 1
/*
 * SPL @ 32k for ~36k
 * ENV @ 96k
 * u-boot @ 128K
 */
#define CONFIG_ENV_OFFSET (96 * 1024)

#define SDRAM_BANK_SIZE			(2UL << 30)

#ifndef CONFIG_SPL_BUILD

#undef CONFIG_BOOTARGS
#undef CONFIG_EXTRA_ENV_SETTINGS


/*	"ethaddr=CA:FF:EE:00:00:02\0" \ */

#define CONFIG_EXTRA_ENV_SETTINGS \
	"scriptaddr=0x00500000\0" \
	"pxefile_addr_r=0x00600000\0" \
	"fdt_addr_r=0x01f00000\0" \
	"kernel_addr_r=0x02000000\0" \
	"ramdisk_addr_r=0x04000000\0" \
	"bootargs=earlycon=uart8250,mmio32,0xff1a0000 console=tty1 console=ttyS2,1500000n8 init=/sbin/init ip=dhcp nfsroot=192.168.140.1:/home/devel/nfs/rootfs-firefly-rk3399 root=/dev/nfs rw\0" \
	BOOTENV
#endif

/* USB */
#define CONFIG_USB_HOST_ETHER	/* Enable USB Ethernet adapters */

#define CONFIG_USB_ETHER_ASIX

#define CONFIG_BOOTP_SUBNETMASK
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME
#define CONFIG_BOOTP_BOOTPATH

#define CONFIG_SERVERIP		192.168.140.1
#define CONFIG_BOOTFILE		"netboot/firefly-rk3399.vmlinuz"

/* 	"regulator dev host-pwr; " \
	"regulator enable; " \
*/
#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND \
	"usb start; " \
	"dhcp 0x20000000; " \
	"bootm 0x20000000; "

#endif
