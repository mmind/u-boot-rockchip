/*
 * Copyright (c) 2015 Google, Inc
 *
 * Copyright 2014 Rockchip Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef _ASM_ARCH_RK3288_SDRAM_H__
#define _ASM_ARCH_RK3288_SDRAM_H__

#include <asm/arch/dw_upctl.h>
#include <asm/arch/dw_publ.h>

enum {
	DDR3 = 3,
	LPDDR3 = 6,
	UNUSED = 0xFF,
};

struct rk3288_sdram_channel {
	u8 rank;
	u8 col;
	u8 bk;
	u8 bw;
	u8 dbw;
	u8 row_3_4;
	u8 cs0_row;
	u8 cs1_row;
};

struct rk3288_base_params {
	u32 noc_timing;
	u32 noc_activate;
	u32 ddrconfig;
	u32 ddr_freq;
	u32 dramtype;
	u32 stride;
	u32 odt;
};

struct rk3288_sdram_params {
	struct rk3288_sdram_channel ch[2];
	struct dw_upctl_sdram_timing pctl_timing;
	struct dw_publ_sdram_timing phy_timing;
	struct rk3288_base_params base;
	int num_channels;
};

#endif
