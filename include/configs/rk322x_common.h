/*
 * (C) Copyright 2017 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */
#ifndef __CONFIG_RK322X_COMMON_H
#define __CONFIG_RK322X_COMMON_H

#include <asm/arch/hardware.h>
#include "rockchip-common.h"

#define CONFIG_SKIP_LOWLEVEL_INIT
#define CONFIG_ENV_SIZE			0x2000
#define CONFIG_SYS_MAXARGS		16
#define CONFIG_SYS_MALLOC_LEN		(32 << 20)
#define CONFIG_SYS_CBSIZE		1024
#define CONFIG_SYS_BOOTM_LEN	(64 << 20)	/*  64M */

#define CONFIG_SYS_TIMER_RATE		(24 * 1000 * 1000)
#define CONFIG_SYS_TIMER_BASE		0x110c00a0 /* TIMER5 */
#define CONFIG_SYS_TIMER_COUNTER	(CONFIG_SYS_TIMER_BASE + 8)

#define CONFIG_SPL_FRAMEWORK
#define CONFIG_SYS_NS16550_MEM32
#define CONFIG_SYS_TEXT_BASE		0x60000000
#define CONFIG_SYS_INIT_SP_ADDR		0x60100000
#define CONFIG_SYS_LOAD_ADDR		0x60800800
#define CONFIG_SPL_STACK		0x10088000
#define CONFIG_SPL_TEXT_BASE		0x10081004

#define CONFIG_ROCKCHIP_MAX_INIT_SIZE	(28 << 10)
#define CONFIG_ROCKCHIP_CHIP_TAG	"RK32"

/* MMC/SD IP block */
#define CONFIG_BOUNCE_BUFFER

#define CONFIG_SYS_SDRAM_BASE		0x60000000
#define CONFIG_NR_DRAM_BANKS		2
#define SDRAM_BANK_SIZE			(512UL << 20UL)
#define SDRAM_MAX_SIZE			0x80000000

#ifndef CONFIG_SPL_BUILD
/* usb otg */
#define CONFIG_USB_GADGET
#define CONFIG_USB_GADGET_DUALSPEED
#define CONFIG_USB_GADGET_DWC2_OTG
#define CONFIG_USB_GADGET_VBUS_DRAW	0

/* fastboot  */
#define CONFIG_CMD_FASTBOOT
#define CONFIG_USB_FUNCTION_FASTBOOT
#define CONFIG_FASTBOOT_FLASH
#define CONFIG_FASTBOOT_FLASH_MMC_DEV	0
#define CONFIG_FASTBOOT_BUF_ADDR	CONFIG_SYS_LOAD_ADDR
#define CONFIG_FASTBOOT_BUF_SIZE	0x08000000

/* usb mass storage */
#define CONFIG_USB_FUNCTION_MASS_STORAGE
#define CONFIG_CMD_USB_MASS_STORAGE

#define CONFIG_USB_GADGET_DOWNLOAD
#define CONFIG_G_DNL_MANUFACTURER	"Rockchip"
#define CONFIG_G_DNL_VENDOR_NUM		0x2207
#define CONFIG_G_DNL_PRODUCT_NUM	0x320a

/* usb host */
#ifdef CONFIG_CMD_USB
#define CONFIG_USB_DWC2
#define CONFIG_USB_HOST_ETHER
#define CONFIG_USB_ETHER_SMSC95XX
#define CONFIG_USB_ETHER_ASIX
#endif
#define ENV_MEM_LAYOUT_SETTINGS \
	"scriptaddr=0x60000000\0" \
	"pxefile_addr_r=0x60100000\0" \
	"fdt_addr_r=0x61f00000\0" \
	"kernel_addr_r=0x62000000\0" \
	"ramdisk_addr_r=0x64000000\0"

#include <config_distro_bootcmd.h>

/* Linux fails to load the fdt if it's loaded above 512M on a evb-rk3036 board,
 * so limit the fdt reallocation to that */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"fdt_high=0x7fffffff\0" \
	"partitions=" PARTS_DEFAULT \
	ENV_MEM_LAYOUT_SETTINGS \
	BOOTENV
#endif

#define CONFIG_PREBOOT

#endif
