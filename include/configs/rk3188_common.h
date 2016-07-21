/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __CONFIG_RK3188_COMMON_H
#define __CONFIG_RK3188_COMMON_H

#define CONFIG_SYS_CACHELINE_SIZE	32

#include <asm/arch/hardware.h>

#define CONFIG_SYS_NO_FLASH
#define CONFIG_NR_DRAM_BANKS		1
#define CONFIG_ENV_IS_NOWHERE
#define CONFIG_ENV_SIZE			0x2000
#define CONFIG_SYS_MAXARGS		16
#define CONFIG_BAUDRATE			115200
#define CONFIG_SYS_MALLOC_LEN		(32 << 20)
#define CONFIG_SYS_CBSIZE		1024
#define CONFIG_SKIP_LOWLEVEL_INIT
#define CONFIG_SYS_THUMB_BUILD
#define CONFIG_DISPLAY_BOARDINFO

#define CONFIG_SYS_TIMER_RATE		(24 * 1000 * 1000)
#define CONFIG_SYS_TIMER_BASE		0x2000e000 /* TIMER3 */
#define CONFIG_SYS_TIMER_COUNTER	(CONFIG_SYS_TIMER_BASE + 8)
#define CONFIG_SYS_TIMER_COUNTS_DOWN

#define CONFIG_SPL_FRAMEWORK
#define CONFIG_SPL_DRIVERS_MISC_SUPPORT
#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_SERIAL_SUPPORT

#define CONFIG_SYS_NS16550_MEM32
/* #define CONFIG_SPL_BOARD_INIT */

#define CONFIG_SYS_TEXT_BASE		0x60000000
#define CONFIG_SYS_INIT_SP_ADDR		0x60100000
#define CONFIG_SYS_LOAD_ADDR		0x60800800
#define CONFIG_SPL_STACK		0x10087fff
#define CONFIG_SPL_TEXT_BASE		0x10080804

#define CONFIG_ROCKCHIP_MAX_INIT_SIZE	0x8000 - 0x800
#define CONFIG_ROCKCHIP_CHIP_TAG	"RK31"

#define CONFIG_ROCKCHIP_COMMON
#define CONFIG_SPL_ROCKCHIP_COMMON

/* #define CONFIG_SILENT_CONSOLE
#ifndef CONFIG_SPL_BUILD
# define CONFIG_SYS_CONSOLE_IS_IN_ENV
# define CONFIG_CONSOLE_MUX
#endif */

/* MMC/SD IP block */
#define CONFIG_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_SDHCI
#define CONFIG_DWMMC
#define CONFIG_BOUNCE_BUFFER

#define CONFIG_DOS_PARTITION
#define CONFIG_FAT_WRITE
#define CONFIG_PARTITION_UUIDS
#define CONFIG_CMD_PART

#define CONFIG_SPL_PINCTRL_SUPPORT
#define CONFIG_SPL_RAM_SUPPORT
#define CONFIG_SPL_DRIVERS_MISC_SUPPORT

#define CONFIG_SYS_SDRAM_BASE		0x60000000
#define CONFIG_NR_DRAM_BANKS		1
#define SDRAM_BANK_SIZE			(2UL << 30)

#define CONFIG_SPI_FLASH
#define CONFIG_SPI
#define CONFIG_SF_DEFAULT_SPEED 20000000

#ifndef CONFIG_SPL_BUILD
#include <config_distro_defaults.h>

#define ENV_MEM_LAYOUT_SETTINGS \
	"scriptaddr=0x60000000\0" \
	"pxefile_addr_r=0x60100000\0" \
	"fdt_addr_r=0x61f00000\0" \
	"kernel_addr_r=0x62000000\0" \
	"ramdisk_addr_r=0x64000000\0"

/* First try to boot from SD (index 0), then eMMC (index 1 */
#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 0) \
	func(MMC, mmc, 1)

#include <config_distro_bootcmd.h>

/* Linux fails to load the fdt if it's loaded above 512M on rk3188 boards, so
 * limit the fdt reallocation to that */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"fdt_high=0x7fffffff\0" \
	"initrd_high=0x7fffffff\0" \
	ENV_MEM_LAYOUT_SETTINGS \
	ROCKCHIP_DEVICE_SETTINGS \
	BOOTENV
#endif

#endif
