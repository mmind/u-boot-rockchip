/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define ROCKCHIP_DEVICE_SETTINGS \
		"stdin=serial\0" \
		"stdout=serial\0" \
		"stderr=serial\0"

#include <configs/rk3188_common.h>

#define CONFIG_ENV_IS_NOWHERE
#define CONFIG_SPL_MMC_SUPPORT

#endif
