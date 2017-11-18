/*
 * (C) Copyright 2017 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <configs/rk322x_common.h>

/* Store env in emmc */
#undef CONFIG_ENV_SIZE
#define CONFIG_ENV_SIZE                 (32 << 10)
#define CONFIG_SYS_MMC_ENV_DEV          0

#endif
