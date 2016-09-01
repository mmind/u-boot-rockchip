/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define ROCKCHIP_DEVICE_SETTINGS \
		"stdin=serial\0" \
		"stdout=serial\0" \
		"stderr=serial\0"

#include <configs/rk3188_common.h>

#define CONFIG_SYS_DCACHE_OFF

#define CONFIG_ENV_IS_NOWHERE
#define CONFIG_SPL_MMC_SUPPORT

#ifndef CONFIG_SPL_BUILD

#undef CONFIG_BOOTARGS
#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS \
	"fdt_high=0x7fffffff\0" \
	"initrd_high=0x7fffffff\0" \
	"loadaddr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"bootargs=earlyprintk console=ttyS2,115200n8 init=/sbin/init ip=dhcp nfsroot=192.168.140.1:/home/devel/nfs/rootfs-rock root=/dev/nfs rw noinitrd\0" \

#endif

/* USB */
#define CONFIG_USB_DWC2
#define CONFIG_USB_HOST_ETHER	/* Enable USB Ethernet adapters */

#define CONFIG_USB_ETHER_ASIX

#define CONFIG_BOOTP_SUBNETMASK
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME
#define CONFIG_BOOTP_BOOTPATH

/* Enable atags */
#define CONFIG_SYS_BOOTPARAMS_LEN	(64*1024)
#define CONFIG_INITRD_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_CMDLINE_TAG


/*#define CONFIG_IPADDR		10.0.0.2  (replace with your value)
*/
#define CONFIG_SERVERIP		192.168.140.1
#define CONFIG_BOOTFILE		"hstuebner/rock.vmlinuz"

#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND \
	"regulator dev host-pwr; " \
	"regulator enable; " \
	"usb start; " \
	"dhcp 0x62000000; " \
	"bootm 0x62000000; " \


#endif
