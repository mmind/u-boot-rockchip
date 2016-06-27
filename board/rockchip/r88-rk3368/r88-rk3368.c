/*
 * (C) Copyright 2015 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <asm/io.h>
#include <asm/arch/uart.h>
#include <asm/arch-rockchip/grf_rk3368.h>
#include <linux/sizes.h>
#include <asm/gpio.h>

DECLARE_GLOBAL_DATA_PTR;

#define GRF_BASE	0x20008000

int board_late_init(void)
{
//	struct rk3036_grf * const grf = (void *)GRF_BASE;
//	int boot_mode = readl(&grf->os_reg[4]);

	return 0;
}

int board_init(void)
{
	return 0;
}

int dram_init(void)
{
//	gd->ram_size = sdram_size();
	gd->ram_size = get_ram_size((void *)0, SZ_1G);

	return 0;
}

#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
	/* Enable D-cache. I-cache is already enabled in start.S */
	dcache_enable();
}
#endif
