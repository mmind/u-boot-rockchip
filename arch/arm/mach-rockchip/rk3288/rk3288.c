// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2016 Rockchip Electronics Co., Ltd
 */
#include <common.h>
#include <asm/io.h>
#include <asm/arch-rockchip/hardware.h>
#include <asm/arch-rockchip/grf_rk3288.h>

DECLARE_GLOBAL_DATA_PTR;

#define GRF_BASE	0xff770000

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
	struct rk3288_grf * const grf = (void *)GRF_BASE;

	/* Use rkpwm by default */
	rk_setreg(&grf->soc_con2, 1 << 0);

	return 0;
}

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
void board_debug_uart_init(void)
{
	/* Enable early UART on the RK3288 */
	struct rk3288_grf * const grf = (void *)GRF_BASE;

	rk_clrsetreg(&grf->gpio7ch_iomux, GPIO7C7_MASK << GPIO7C7_SHIFT |
		     GPIO7C6_MASK << GPIO7C6_SHIFT,
		     GPIO7C7_UART2DBG_SOUT << GPIO7C7_SHIFT |
		     GPIO7C6_UART2DBG_SIN << GPIO7C6_SHIFT);
}
#endif
