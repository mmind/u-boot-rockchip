/*
 * (C) Copyright 2017 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __of_plat_h
#define __of_plat_h

#include "libfdt_env.h"
/**
 * Get a variable-sized number from a property
 *
 * This reads a number from one or more cells.
 *
 * @param ptr	Pointer to property
 * @param cells	Number of cells containing the number
 * @return the value in the cells
 */
u64 of_plat_get_number(const fdt32_t *ptr, unsigned int cells);

#endif
