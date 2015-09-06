/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <debug_uart.h>
#include <dm.h>
#include <fdtdec.h>
#include <led.h>
#include <malloc.h>
#include <ram.h>
#include <spl.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/hardware.h>
#include <asm/arch/periph.h>
#include <asm/arch/sdram.h>
#include <asm/arch/timer.h>
#include <dm/pinctrl.h>
#include <dm/root.h>
#include <dm/test.h>
#include <dm/util.h>
#include <power/regulator.h>

DECLARE_GLOBAL_DATA_PTR;

/* read L2 control register (L2CTLR) */
static inline uint32_t read_l2ctlr(void)
{
	uint32_t val = 0;

	asm volatile ("mrc p15, 1, %0, c9, c0, 2" : "=r" (val));

	return val;
}

/* write L2 control register (L2CTLR) */
static inline void write_l2ctlr(uint32_t val)
{
	/*
	 * Note: L2CTLR can only be written when the L2 memory system
	 * is idle, ie before the MMU is enabled.
	 */
	asm volatile("mcr p15, 1, %0, c9, c0, 2" : : "r" (val) : "memory");
	isb();
}

static void configure_l2ctlr(void)
{
	uint32_t l2ctlr;

	l2ctlr = read_l2ctlr();
	l2ctlr &= 0xfffc0000; /* clear bit0~bit17 */

	/*
	* Data RAM write latency: 2 cycles
	* Data RAM read latency: 2 cycles
	* Data RAM setup latency: 1 cycle
	* Tag RAM write latency: 1 cycle
	* Tag RAM read latency: 1 cycle
	* Tag RAM setup latency: 1 cycle
	*/
	l2ctlr |= (1 << 3 | 1 << 0);
	write_l2ctlr(l2ctlr);
}

void init_timer(void)
{
	struct rockchip_timer * const timer7_ptr = (void *)RK3288_TIMER7_BASE;

	writel(0xffffffff, &timer7_ptr->timer_load_count0);
	writel(0xffffffff, &timer7_ptr->timer_load_count1);
	writel(1, &timer7_ptr->timer_ctrl_reg);
}

void board_init_f(ulong dummy)
{
	struct udevice *pinctrl;
	struct udevice *dev;
	int ret;

	/* Example code showing how to enable the debug UART on RK3288 */
#ifdef EARLY_UART
#include <asm/arch/grf_rk3288.h>
	/* Enable early UART on the RK3288 */
#define GRF_BASE	0xff770000
	struct rk3288_grf * const grf = (void *)GRF_BASE;

	rk_clrsetreg(&grf->gpio7ch_iomux, GPIO7C7_MASK << GPIO7C7_SHIFT |
		     GPIO7C6_MASK << GPIO7C6_SHIFT,
		     GPIO7C7_UART2DBG_SOUT << GPIO7C7_SHIFT |
		     GPIO7C6_UART2DBG_SIN << GPIO7C6_SHIFT);
	/*
	 * Debug UART can be used from here if required:
	 *
	 * debug_uart_init();
	 * printch('a');
	 * printhex8(0x1234);
	 * printascii("string");
	 */
	debug_uart_init();
#endif

	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}

	init_timer();
	configure_l2ctlr();

	ret = uclass_get_device(UCLASS_CLK, 0, &dev);
	if (ret) {
		debug("CLK init failed: %d\n", ret);
		return;
	}

	ret = uclass_get_device(UCLASS_PINCTRL, 0, &pinctrl);
	if (ret) {
		debug("Pinctrl init failed: %d\n", ret);
		return;
	}

	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret) {
		debug("DRAM init failed: %d\n", ret);
		return;
	}
}
