/*
 * Configuation settings for the rk33xx chip platform.
 *
 * (C) Copyright 2008-2016 Fuzhou Rockchip Electronics Co., Ltd
 * Peter, Software Engineering, <superpeter.cai@gmail.com>.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __RK33PLAT_CONFIG_H
#define __RK33PLAT_CONFIG_H

#include <asm/arch/io.h>

/* rk gic400 is GICV2 */
#define CONFIG_GICV2
#define GICD_BASE			RKIO_GICD_PHYS
#define GICC_BASE			RKIO_GICC_PHYS


/* Generic Timer Definitions */
#define COUNTER_FREQUENCY		CONFIG_SYS_CLK_FREQ_CRYSTAL


/*
 * uboot ram config.
 */
#include <linux/sizes.h>
#define CONFIG_RAM_PHY_START		0x00000000
#define CONFIG_RAM_PHY_SIZE		SZ_128M
#define CONFIG_RAM_PHY_END		(CONFIG_RAM_PHY_START + CONFIG_RAM_PHY_SIZE)


/* reserve iomap memory. */
#define CONFIG_MAX_MEM_ADDR		RKIO_IOMEMORYMAP_START


/*
 * 		define uboot loader addr.
 * notice: CONFIG_SYS_TEXT_BASE must be an immediate,
 * so if CONFIG_RAM_PHY_START is changed, also update CONFIG_SYS_TEXT_BASE define.
 *
 * Resersed 2M space(0 - 2M) for Runtime ARM Firmware bin, such as bl30/bl31/bl32 and so on.
 *
 */
#ifdef CONFIG_SECOND_LEVEL_BOOTLOADER
       #define CONFIG_SYS_TEXT_BASE    0x00200000 /* Resersed 2M space Runtime Firmware bin. */
#else
       #define CONFIG_SYS_TEXT_BASE    0x00000000
#endif


/* kernel load to the running address */
#define CONFIG_KERNEL_RUNNING_ADDR	(CONFIG_SYS_TEXT_BASE + SZ_512K)


/*
 * rk plat default configs.
 */
#include <configs/rk_default_config.h>

/* el3 switch to el1 disable */
#ifndef CONFIG_SECOND_LEVEL_BOOTLOADER
#define CONFIG_SWITCH_EL3_TO_EL1
#endif

/* do_nonsec_virt_switch when enter kernel */
#define CONFIG_ARMV8_SWITCH_TO_EL1

/* icache enable when start to kernel */
#define CONFIG_ICACHE_ENABLE_FOR_KERNEL


/* undef some module for rk chip */
#if defined(CONFIG_RKCHIP_RK3368)
	#define CONFIG_RK_MCU
	#define CONFIG_SECUREBOOT_CRYPTO
	#define CONFIG_SECUREBOOT_SHA256
	#undef CONFIG_RK_TRUSTOS

	#undef CONFIG_RK_UMS_BOOT_EN
	#undef CONFIG_RK_PL330_DMAC
#endif

/* fpga board configure */
#ifdef CONFIG_FPGA_BOARD
	#define CONFIG_BOARD_DEMO
	#define CONFIG_RK_IO_TOOL

	#define CONFIG_SKIP_RELOCATE_UBOOT
	#define CONFIG_SYS_ICACHE_OFF
	#define CONFIG_SYS_DCACHE_OFF
	#define CONFIG_SYS_L2CACHE_OFF
	#define CONFIG_RKTIMER_INCREMENTER

	#undef CONFIG_RK_MCU
	#undef CONFIG_SECUREBOOT_CRYPTO
	#undef CONFIG_MERGER_MINILOADER
	#undef CONFIG_MERGER_TRUSTIMAGE
	#undef CONFIG_RK_SDCARD_BOOT_EN
	#undef CONFIG_RK_FLASH_BOOT_EN
	#undef CONFIG_RK_UMS_BOOT_EN
	#undef CONFIG_LCD
	#undef CONFIG_RK_POWER
	#undef CONFIG_PM_SUBSYSTEM
	#undef CONFIG_RK_CLOCK
	#undef CONFIG_RK_IOMUX
	#undef CONFIG_RK_I2C
	#undef CONFIG_RK_KEY
#endif

/* if uboot as first level loader, no start mcu. */
#ifndef CONFIG_SECOND_LEVEL_BOOTLOADER
	#undef CONFIG_RK_MCU
#endif

/* ARMv8 RSA key in ram, MiniLoader copy RSA KEY to fixed address */
#if defined(CONFIG_SECOND_LEVEL_BOOTLOADER) && defined(CONFIG_SECUREBOOT_CRYPTO)
#define CONFIG_SECURE_RSA_KEY_IN_RAM
#define CONFIG_SECURE_RSA_KEY_ADDR	(CONFIG_RKNAND_API_ADDR + SZ_2K)
#endif /* CONFIG_SECUREBOOT_CRYPTO */


/* mod it to enable console commands.	*/
#define CONFIG_BOOTDELAY		2

/* efuse version */
#ifdef CONFIG_RK_EFUSE
	#define CONFIG_RKEFUSE_V2
#endif

/* mmc using dma */
#define CONFIG_RK_MMC_DMA
#define CONFIG_RK_MMC_IDMAC	/* internal dmac */
#undef CONFIG_RK_MMC_DDR_MODE	/* mmc using ddr mode */

/* more config for rockusb */
#ifdef CONFIG_CMD_ROCKUSB

/* support rockusb timeout check */
#define CONFIG_ROCKUSB_TIMEOUT_CHECK	1

/* rockusb VID/PID should the same as maskrom */
#define CONFIG_USBD_VENDORID			0x2207
#if defined(CONFIG_RKCHIP_RK3368)
	#define CONFIG_USBD_PRODUCTID_ROCKUSB	0x330A
#else
	#error "PLS config rk chip for rockusb PID!"
#endif

#endif /* CONFIG_CMD_ROCKUSB */


/* more config for fastboot */
#ifdef CONFIG_CMD_FASTBOOT

#define CONFIG_USBD_PRODUCTID_FASTBOOT	0x0006
#define CONFIG_USBD_MANUFACTURER	"Rockchip"
#define CONFIG_USBD_PRODUCT_NAME	"rk30xx"

#define FASTBOOT_PRODUCT_NAME		"fastboot" /* Fastboot product name */

#define CONFIG_FASTBOOT_LOG
#define CONFIG_FASTBOOT_LOG_SIZE	(SZ_2M)

#endif /* CONFIG_CMD_FASTBOOT */


#ifdef CONFIG_RK_UMS_BOOT_EN
/*
 * USB Host support, default no using
 * Please first select USB host controller if you want to use UMS Boot
 * Up to one USB host controller could be selected to enable for booting
 * from USB Mass Storage device.
 *
 * PLS define a host controler from:
 *	RKUSB_UMS_BOOT_FROM_DWC2_OTG
 *	RKUSB_UMS_BOOT_FROM_DWC2_HOST
 *	RKUSB_UMS_BOOT_FROM_EHCI_HOST1
 *
 * First define the host controller here
 */
#undef RKUSB_UMS_BOOT_FROM_DWC2_OTG
#undef RKUSB_UMS_BOOT_FROM_DWC2_HOST
#undef RKUSB_UMS_BOOT_FROM_EHCI_HOST1


/* Check UMS Boot Host define */
#define RKUSB_UMS_BOOT_CNT (defined(RKUSB_UMS_BOOT_FROM_DWC2_OTG) + \
			    defined(RKUSB_UMS_BOOT_FROM_DWC2_HOST) + \
			    defined(RKUSB_UMS_BOOT_FROM_EHCI_HOST1))

#if (RKUSB_UMS_BOOT_CNT == 0)
	#error "PLS Select a USB host controller!"
#elif (RKUSB_UMS_BOOT_CNT > 1)
	#error "Only one USB host controller can be selected!"
#endif


/*
 * USB Host support, default no using
 * please first check plat if you want to using usb host
 */
#if defined(RKUSB_UMS_BOOT_FROM_EHCI_HOST1)
	/* ehci host */
	#define CONFIG_USB_EHCI
	#define CONFIG_USB_EHCI_RK
#elif defined(RKUSB_UMS_BOOT_FROM_DWC2_HOST) || defined(RKUSB_UMS_BOOT_FROM_DWC2_OTG)
	/* dwc2 host or otg */
	#define CONFIG_USB_DWC_HCD
#endif


/* enable usb config for usb host */
#define CONFIG_CMD_USB
#define CONFIG_USB_STORAGE
#define CONFIG_PARTITIONS
#endif /* CONFIG_RK_UMS_BOOT_EN */


#define CONFIG_SYS_ICACHE_OFF
#define CONFIG_SYS_DCACHE_OFF
#define CONFIG_SYS_L2CACHE_OFF

/* variant1: use ehci host port */
#define CONFIG_USB_EHCI
#define CONFIG_USB_EHCI_RK

/* vairant2: use uboots dwc2 driver for otg port */
/* #define CONFIG_USB_DWC2
#define CONFIG_USB_DWC2_REG_ADDR RKIO_USBOTG_PHYS */

/* variant3 use rk's dwc2 driver for otg port */
/*#define CONFIG_USB_DWC_HCD */

#define CONFIG_CMD_USB
#define CONFIG_USB_STORAGE
#define CONFIG_PARTITIONS
#define CONFIG_DOS_PARTITION
#define CONFIG_EFI_PARTITION
#define CONFIG_PARTITION_UUIDS

#define CONFIG_CMD_PART
#define CONFIG_CMD_ELF
#define CONFIG_CMD_FAT
#define CONFIG_CMD_EXT2
#define CONFIG_CMD_EXT4

#define CONFIG_USB_HOST_ETHER	/* Enable USB Ethernet adapters */
#define CONFIG_USB_ETHER_ASIX
#define CONFIG_USB_ETHER_SMSC95XX

#define CONFIG_BOOTP_SUBNETMASK
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME
#define CONFIG_BOOTP_BOOTPATH
#define CONFIG_CMD_NET
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_PING
#define CONFIG_BOOTP_SUBNETMASK
#define CONFIG_BOOTP_GATEWAY

/*#define CONFIG_SERVERIP		192.168.140.1 */
#define CONFIG_SERVERIP		192.168.138.241
#define CONFIG_BOOTFILE		"hstuebner/r88.vmlinuz"

/* more config for display */
#ifdef CONFIG_LCD

#define CONFIG_RK33_FB

#ifdef CONFIG_RK_HDMI
#define CONFIG_RK_HDMIV2
#endif

#ifdef CONFIG_RK_TVE
#define CONFIG_RK1000_TVE
#undef CONFIG_GM7122_TVE
#endif

#define CONFIG_RK32_DSI

#undef CONFIG_UBOOT_CHARGE

#else

#undef CONFIG_RK_FB
#undef CONFIG_RK_PWM_BL
#undef CONFIG_RK_HDMI
#undef CONFIG_RK_TVE
#undef CONFIG_CMD_BMP
#undef CONFIG_UBOOT_CHARGE

#endif /* CONFIG_LCD */


/* more config for charge */
#ifdef CONFIG_UBOOT_CHARGE

#define CONFIG_CMD_CHARGE_ANIM
#define CONFIG_CHARGE_DEEP_SLEEP

#ifdef CONFIG_CHARGE_DEEP_SLEEP
#define CONFIG_CHARGE_TIMER_WAKEUP
#endif
#endif /* CONFIG_UBOOT_CHARGE */


/* more config for power */
#ifdef CONFIG_RK_POWER

#define CONFIG_POWER
#define CONFIG_POWER_I2C

#define CONFIG_POWER_PMIC
/* if box product, undefine fg and battery */
#ifndef CONFIG_PRODUCT_BOX
#define CONFIG_POWER_FG
#define CONFIG_POWER_BAT
#endif /* CONFIG_PRODUCT_BOX */

#define CONFIG_SCREEN_ON_VOL_THRESD	0
#define CONFIG_SYSTEM_ON_VOL_THRESD	0

/******** pwm regulator driver ********/
#define CONFIG_POWER_PWM_REGULATOR

/******** pmic driver ********/
#ifdef CONFIG_POWER_PMIC
#undef CONFIG_POWER_RK_SAMPLE
#define CONFIG_POWER_RICOH619
#define CONFIG_POWER_RK808
#define CONFIG_POWER_RK818
#define CONFIG_POWER_ACT8846
#endif /* CONFIG_POWER_PMIC */

/******** charger driver ********/
#ifdef CONFIG_POWER_FG
#define CONFIG_POWER_FG_CW201X
#endif /* CONFIG_POWER_FG */

/******** battery driver ********/
#ifdef CONFIG_POWER_BAT
#undef CONFIG_BATTERY_RK_SAMPLE
#undef CONFIG_BATTERY_BQ27541
#undef CONFIG_BATTERY_RICOH619
#endif /* CONFIG_POWER_BAT */

#endif /* CONFIG_RK_POWER */

#endif /* __RK33PLAT_CONFIG_H */
