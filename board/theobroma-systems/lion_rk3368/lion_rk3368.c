// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Theobroma Systems Design und Consulting GmbH
 */

#include <common.h>
#include <dm.h>
#include <ram.h>
#include <spl.h>
#include <syscon.h>
#include <asm/io.h>
#include <asm/arch-rockchip/clock.h>
#include <asm/arch-rockchip/grf_rk3368.h>
#include <asm/arch-rockchip/timer.h>
#include <power/regulator.h>

int mach_cpu_init(void)
{
	return 0;
}

int board_init(void)
{
	int ret;

	/*
	 * We need to call into regulators_enable_boot_on() again, as the call
	 * during SPL may have not included all regulators.
	 */
	ret = regulators_enable_boot_on(false);
	if (ret)
		debug("%s: Cannot enable boot on regulator\n", __func__);

	return 0;
}

void spl_board_init(void)
{
	int ret;

	debug("%s: calling regulators_enable_boot_on\n", __func__);

	ret = regulators_enable_boot_on(false);
	if (ret)
		debug("%s: Cannot enable boot on regulator\n", __func__);

	preloader_console_init();
}
