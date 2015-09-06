/*
 * Copyright (c) 2015 Google, Inc
 *
 * Copyright 2014 Rockchip Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef _ASM_ARCH_TIMER_H__
#define _ASM_ARCH_TIMER_H__

struct rockchip_timer {
	u32 timer_load_count0;
	u32 timer_load_count1;
	u32 timer_curr_value0;
	u32 timer_curr_value1;
	u32 timer_ctrl_reg;
	u32 timer_int_status;
};

#endif
