/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <asm/arch/clock.h>
#include <asm/arch/timer.h>

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	struct udevice *dev;
	int ret;

	rockchip_timer_init();
	ret = rockchip_get_clk(&dev);
	if (ret < 0)
		return ret;

	return 0;
}

int dram_init(void)
{
	gd->ram_size = 0x40000000;

	return 0;
}
