/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef _ASM_ARCH_DDR_RK3288_H
#define _ASM_ARCH_DDR_RK3288_H

#include <arm/arch/dw_upctl.h>
#include <arm/arch/dw_publ.h>

struct rk3288_msch {
	u32 coreid;
	u32 revisionid;
	u32 ddrconf;
	u32 ddrtiming;
	u32 ddrmode;
	u32 readlatency;
	u32 reserved1[8];
	u32 activate;
	u32 devtodev;
};
check_member(rk3288_msch, devtodev, 0x003c);

/* MSCH_DEVTODEV */
#define BUSWRTORD_SHIFT			4
#define BUSRDTOWR_SHIFT			2
#define BUSRDTORD_SHIFT			0

/* mr1 for ddr3 */
#define DDR3_DLL_DISABLE		1

/*
 *TODO(sjg@chromium.org): We use a PMU register to store SDRAM information for
 * passing from SPL to U-Boot. It would probably be better to use a normal C
 * structure in SRAM.
 *
 * sys_reg bitfield struct
 * [31] row_3_4_ch1
 * [30] row_3_4_ch0
 * [29:28] chinfo
 * [27] rank_ch1
 * [26:25] col_ch1
 * [24] bk_ch1
 * [23:22] cs0_row_ch1
 * [21:20] cs1_row_ch1
 * [19:18] bw_ch1
 * [17:16] dbw_ch1;
 * [15:13] ddrtype
 * [12] channelnum
 * [11] rank_ch0
 * [10:9] col_ch0
 * [8] bk_ch0
 * [7:6] cs0_row_ch0
 * [5:4] cs1_row_ch0
 * [3:2] bw_ch0
 * [1:0] dbw_ch0
*/
#define SYS_REG_DDRTYPE_SHIFT		13
#define SYS_REG_DDRTYPE_MASK		7
#define SYS_REG_NUM_CH_SHIFT		12
#define SYS_REG_NUM_CH_MASK		1
#define SYS_REG_ROW_3_4_SHIFT(ch)	(30 + (ch))
#define SYS_REG_ROW_3_4_MASK		1
#define SYS_REG_CHINFO_SHIFT(ch)	(28 + (ch))
#define SYS_REG_RANK_SHIFT(ch)		(11 + (ch) * 16)
#define SYS_REG_RANK_MASK		1
#define SYS_REG_COL_SHIFT(ch)		(9 + (ch) * 16)
#define SYS_REG_COL_MASK		3
#define SYS_REG_BK_SHIFT(ch)		(8 + (ch) * 16)
#define SYS_REG_BK_MASK			1
#define SYS_REG_CS0_ROW_SHIFT(ch)	(6 + (ch) * 16)
#define SYS_REG_CS0_ROW_MASK		3
#define SYS_REG_CS1_ROW_SHIFT(ch)	(4 + (ch) * 16)
#define SYS_REG_CS1_ROW_MASK		3
#define SYS_REG_BW_SHIFT(ch)		(2 + (ch) * 16)
#define SYS_REG_BW_MASK			3
#define SYS_REG_DBW_SHIFT(ch)		((ch) * 16)
#define SYS_REG_DBW_MASK		3

#endif
