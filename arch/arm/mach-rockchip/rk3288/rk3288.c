// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2016 Rockchip Electronics Co., Ltd
 */
#include <common.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>

DECLARE_GLOBAL_DATA_PTR;

#define GRF_SOC_CON2 0xff77024c

int dram_init_banksize(void)
{
#ifdef CONFIG_SPL_ATF
	size_t max_size = min((unsigned long)gd->ram_size, gd->ram_top);

	/* Reserve 0x200000 for ATF bl32 */
	gd->bd->bi_dram[0].start = 0x0200000;
	gd->bd->bi_dram[0].size = max_size - gd->bd->bi_dram[0].start;
#endif

	return 0;
}

int arch_cpu_init(void)
{
	/* We do some SoC one time setting here. */

	/* Use rkpwm by default */
	rk_setreg(GRF_SOC_CON2, 1 << 0);

	return 0;
}
