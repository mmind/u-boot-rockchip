/*
 * (C) Copyright 2015 Google, Inc
 *
 * (C) Copyright 2008-2014 Rockchip Electronics
 * Peter, Software Engineering, <superpeter.cai@gmail.com>.
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */
#ifndef _ASM_ARCH_CRU_RK3288_H
#define _ASM_ARCH_CRU_RK3288_H

#define OSC_HZ		(24 * 1000 * 1000)

#define APLL_HZ		(1608 * 1000000)
#define GPLL_HZ		(594 * 1000000)
#define CPLL_HZ		(384 * 1000000)

/* The SRAM is clocked off aclk_cpu, so we want to max it out for boot speed */
#define CPU_ACLK_HZ	297000000
#define CPU_HCLK_HZ	148500000
#define CPU_PCLK_HZ	74250000

#define PERI_ACLK_HZ	148500000
#define PERI_HCLK_HZ	148500000
#define PERI_PCLK_HZ	74250000

struct rk3188_cru {
	struct rk3188_pll {
		u32 con0;
		u32 con1;
		u32 con2;
		u32 con3;
	} pll[4];
	u32 cru_mode_con;
	u32 cru_clksel_con[35];
	u32 cru_clkgate_con[10];
	u32 reserved1[2];
	u32 cru_glb_srst_fst_value;
	u32 cru_glb_srst_snd_value;
	u32 reserved2[2];
	u32 cru_softrst_con[9];
	u32 cru_misc_con;
	u32 reserved3[2];
	u32 cru_glb_cnt_th;
};
check_member(rk3188_cru, cru_glb_cnt_th, 0x0140);

/* CRU_CLKSEL11_CON */
enum {
	HSICPHY_DIV_SHIFT	= 8,
	HSICPHY_DIV_MASK	= 0x3f,

	MMC0_DIV_SHIFT		= 0,
	MMC0_DIV_MASK		= 0x3f,
};

/* CRU_CLKSEL12_CON */
enum {
	EMMC_DIV_SHIFT		= 8,
	EMMC_DIV_MASK		= 0x3f,

	SDIO0_DIV_SHIFT		= 0,
	SDIO0_DIV_MASK		= 0x3f,
};

/* CRU_CLKSEL25_CON */
/*enum {
	SPI1_PLL_SHIFT		= 0xf,
	SPI1_PLL_MASK		= 1,
	SPI1_PLL_SELECT_CODEC	= 0,
	SPI1_PLL_SELECT_GENERAL,

	SPI1_DIV_SHIFT		= 8,
	SPI1_DIV_MASK		= 0x7f,

	SPI0_PLL_SHIFT		= 7,
	SPI0_PLL_MASK		= 1,
	SPI0_PLL_SELECT_CODEC	= 0,
	SPI0_PLL_SELECT_GENERAL,

	SPI0_DIV_SHIFT		= 0,
	SPI0_DIV_MASK		= 0x7f,
};*/

/* CRU_CLKSEL39_CON */
/*enum {
	ACLK_HEVC_PLL_SHIFT	= 0xe,
	ACLK_HEVC_PLL_MASK	= 3,
	ACLK_HEVC_PLL_SELECT_CODEC = 0,
	ACLK_HEVC_PLL_SELECT_GENERAL,
	ACLK_HEVC_PLL_SELECT_NEW,

	ACLK_HEVC_DIV_SHIFT	= 8,
	ACLK_HEVC_DIV_MASK	= 0x1f,

	SPI2_PLL_SHIFT		= 7,
	SPI2_PLL_MASK		= 1,
	SPI2_PLL_SELECT_CODEC	= 0,
	SPI2_PLL_SELECT_GENERAL,

	SPI2_DIV_SHIFT		= 0,
	SPI2_DIV_MASK		= 0x7f,
};*/

/* CRU_MODE_CON */
enum {
	GPLL_WORK_SHIFT		= 12,
	GPLL_WORK_MASK		= 3,
	GPLL_WORK_SLOW		= 0,
	GPLL_WORK_NORMAL,
	GPLL_WORK_DEEP,

	CPLL_WORK_SHIFT		= 8,
	CPLL_WORK_MASK		= 3,
	CPLL_WORK_SLOW		= 0,
	CPLL_WORK_NORMAL,
	CPLL_WORK_DEEP,

	DPLL_WORK_SHIFT		= 4,
	DPLL_WORK_MASK		= 3,
	DPLL_WORK_SLOW		= 0,
	DPLL_WORK_NORMAL,
	DPLL_WORK_DEEP,

	APLL_WORK_SHIFT		= 0,
	APLL_WORK_MASK		= 3,
	APLL_WORK_SLOW		= 0,
	APLL_WORK_NORMAL,
	APLL_WORK_DEEP,
};

/* CRU_APLL_CON0 */
enum {
	CLKR_SHIFT		= 8,
	CLKR_MASK		= 0x3f,

	CLKOD_SHIFT		= 0,
	CLKOD_MASK		= 0xf,
};

/* CRU_APLL_CON1 */
enum {
	LOCK_SHIFT		= 0x1f,
	LOCK_MASK		= 1,
	LOCK_UNLOCK		= 0,
	LOCK_LOCK,

	CLKF_SHIFT		= 0,
	CLKF_MASK		= 0x1fff,
};

#endif
