// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2015 Rockchip Electronics Co., Ltd
 */

#include <common.h>
#include <debug_uart.h>
#include <asm/io.h>
#include <asm/arch/bootrom.h>
#include <asm/arch/grf_rk3128.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sdram_rk3128.h>
#include <asm/arch/timer.h>
#include <asm/arch/uart.h>

#define GRF_BASE	0x20008000

#define DEBUG_UART_BASE	0x20068000

void puts(const char *s)
{
#ifdef CONFIG_DEBUG_UART
	while (*s) {
		int ch = *s++;

		printch(ch);
	}
	return;
#endif
}

void board_debug_uart_init(void)
{
	struct rk3128_grf * const grf = (void *)GRF_BASE;
	enum {
		GPIO1C3_SHIFT		= 6,
		GPIO1C3_MASK		= 3 << GPIO1C3_SHIFT,
		GPIO1C3_GPIO		= 0,
		GPIO1C3_UART2_SOUT	= 2,

		GPIO1C2_SHIFT		= 4,
		GPIO1C2_MASK		= 3 << GPIO1C2_SHIFT ,
		GPIO1C2_GPIO		= 0,
		GPIO1C2_UART2_SIN	= 2,
	};

	/*
	 * NOTE: sd card and debug uart use same iomux in rk3128,
	 * so if you enable uart,
	 * you can not boot from sdcard
	 */
	rk_clrsetreg(&grf->gpio1c_iomux,
		     GPIO1C3_MASK << GPIO1C3_SHIFT |
		     GPIO1C2_MASK << GPIO1C2_SHIFT,
		     GPIO1C3_UART2_SOUT << GPIO1C3_SHIFT |
		     GPIO1C2_UART2_SIN << GPIO1C2_SHIFT);
}

void board_init_f(ulong dummy)
{
#ifdef EARLY_DEBUG
	debug_uart_init();
	printascii("U-Boot SPL board init");
#endif

	rockchip_timer_init();
	sdram_init();

	/* return to maskrom */
	back_to_bootrom(BROM_BOOT_NEXTSTAGE);
}

/* Place Holders */
void board_init_r(gd_t *id, ulong dest_addr)
{
	/*
	 * Function attribute is no-return
	 * This Function never executes
	 */
	while (1)
		;
}
