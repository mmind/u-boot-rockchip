/*
 * (C) Copyright 2017 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include "of_plat.h"

u64 of_plat_get_number(const u32 *ptr, unsigned int cells)
{
	u64 number = 0;

	while (cells--)
		number = (number << 32) | (*ptr++);

	return number;
}
