// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2016 Rockchip Electronics Co., Ltd
 */

#ifndef CONFIG_TPL_BUILD

#include <common.h>
#include <spl.h>

void board_boot_order(u32 *spl_boot_list)
{
	/* eMMC prior to sdcard. */
	spl_boot_list[0] = BOOT_DEVICE_MMC2;
	spl_boot_list[1] = BOOT_DEVICE_MMC1;
}

#endif

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	/* Just empty function now - can't decide what to choose */
	debug("%s: %s\n", __func__, name);

	return 0;
}
#endif
