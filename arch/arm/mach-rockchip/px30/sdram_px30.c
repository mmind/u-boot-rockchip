// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2018 Rockchip Electronics Co., Ltd.
 */

#include <common.h>
#include <debug_uart.h>
#include <dm.h>
#include <ram.h>
#include <syscon.h>
#include <asm/io.h>
#include <asm/arch-rockchip/clock.h>
#include <asm/arch-rockchip/cru_px30.h>
#include <asm/arch-rockchip/grf_px30.h>
#include <asm/arch-rockchip/hardware.h>
#include <asm/arch-rockchip/sdram_common.h>
#include <asm/arch-rockchip/sdram_px30.h>

#define TIMER_CUR_VALUE0	0x08
#define TIMER_CUR_VALUE1	0x0c

static uint64_t rockchip_get_ticks(void)
{
	uint64_t timebase_h, timebase_l;

	timebase_l = readl(CONFIG_ROCKCHIP_STIMER_BASE + TIMER_CUR_VALUE0);
	timebase_h = readl(CONFIG_ROCKCHIP_STIMER_BASE + TIMER_CUR_VALUE1);

	return timebase_h << 32 | timebase_l;
}

void rockchip_udelay(unsigned int usec)
{
	uint64_t tmp;

	/* get timestamp */
	tmp = rockchip_get_ticks() + usec_to_tick(usec);

	/* loop till event */
	while (rockchip_get_ticks() < tmp+1)
		;
}

u8 ddr_cfg_2_rbc[] = {
	/*
	 * [6:4] max row: 13+n
	 * [3]  bank(0:4bank,1:8bank)
	 * [2:0]    col(10+n)
	 */
	((5 << 4) | (1 << 3) | 0), /* 0 */
	((5 << 4) | (1 << 3) | 1), /* 1 */
	((4 << 4) | (1 << 3) | 2), /* 2 */
	((3 << 4) | (1 << 3) | 3), /* 3 */
	((2 << 4) | (1 << 3) | 4), /* 4 */
	((5 << 4) | (0 << 3) | 2), /* 5 */
	((4 << 4) | (1 << 3) | 2), /* 6 */
};

#ifdef CONFIG_TPL_BUILD

/*
 * for ddr4 if ddrconfig=7, upctl should set 7 and noc should
 * set to 1 for more efficient.
 * noc ddrconf, upctl addrmap
 * 1  7
 * 2  8
 * 3  9
 * 12 10
 * 5  11
 */
static u8 d4_rbc_2_d3_rbc[] = {
	1, /* 7 */
	2, /* 8 */
	3, /* 9 */
	12, /* 10 */
	5, /* 11 */
};

/*
 * row higher than cs should be disabled by set to 0xf
 * rank addrmap calculate by real cap.
 */
static u32 addrmap[][8] = {
	/* map0 map1,   map2,       map3,       map4,      map5
	 * map6,        map7,       map8
	 * -------------------------------------------------------
	 * bk2-0       col 5-2     col 9-6    col 11-10   row 11-0
	 * row 15-12   row 17-16   bg1,0
	 * -------------------------------------------------------
	 * 4,3,2       5-2         9-6                    6
	 *                         3,2
	 */
	{0x00060606, 0x00000000, 0x1f1f0000, 0x00001f1f, 0x05050505,
		0x05050505, 0x00000505, 0x3f3f}, /* 0 */
	{0x00070707, 0x00000000, 0x1f000000, 0x00001f1f, 0x06060606,
		0x06060606, 0x06060606, 0x3f3f}, /* 1 */
	{0x00080808, 0x00000000, 0x00000000, 0x00001f1f, 0x07070707,
		0x07070707, 0x00000f07, 0x3f3f}, /* 2 */
	{0x00090909, 0x00000000, 0x00000000, 0x00001f00, 0x08080808,
		0x08080808, 0x00000f0f, 0x3f3f}, /* 3 */
	{0x000a0a0a, 0x00000000, 0x00000000, 0x00000000, 0x09090909,
		0x0f090909, 0x00000f0f, 0x3f3f}, /* 4 */
	{0x00080808, 0x00000000, 0x00000000, 0x00001f1f, 0x06060606,
		0x06060606, 0x00000606, 0x3f3f}, /* 5 */
	{0x00080808, 0x00000000, 0x00000000, 0x00001f1f, 0x07070707,
		0x07070707, 0x00000f0f, 0x3f3f}, /* 6 */
	{0x003f0808, 0x00000006, 0x1f1f0000, 0x00001f1f, 0x06060606,
		0x06060606, 0x00000606, 0x0600}, /* 7 */
	{0x003f0909, 0x00000007, 0x1f000000, 0x00001f1f, 0x07070707,
		0x07070707, 0x00000f07, 0x0700}, /* 8 */
	{0x003f0a0a, 0x01010100, 0x01010101, 0x00001f1f, 0x08080808,
		0x08080808, 0x00000f0f, 0x0801}, /* 9 */
	{0x003f0909, 0x01010100, 0x01010101, 0x00001f1f, 0x07070707,
		0x07070707, 0x00000f07, 0x3f01}, /* 10 */
	{0x003f0808, 0x00000007, 0x1f000000, 0x00001f1f, 0x06060606,
		0x06060606, 0x00000606, 0x3f00}, /* 11 */
	/* when ddr4 12 map to 10, when ddr3 12 unused */
	{0x003f0909, 0x01010100, 0x01010101, 0x00001f1f, 0x07070707,
		0x07070707, 0x00000f07, 0x3f01}, /* 10 */
	{0x00070706, 0x00000000, 0x1f010000, 0x00001f1f, 0x06060606,
		0x06060606, 0x00000606, 0x3f3f}, /* 13 */
};

DECLARE_GLOBAL_DATA_PTR;
struct dram_info {
	struct ddr_pctl_regs *pctl;
	struct ddr_phy_regs *phy;
	struct px30_cru *cru;
	struct px30_msch_regs *msch;
	struct px30_ddr_grf_regs *ddr_grf;
	struct px30_grf *grf;
	struct ram_info info;
	struct px30_pmugrf *pmugrf;
};

#define PMUGRF_BASE_ADDR		0xFF010000
#define CRU_BASE_ADDR			0xFF2B0000
#define GRF_BASE_ADDR			0xFF140000
#define DDRC_BASE_ADDR			0xFF600000
#define DDR_PHY_BASE_ADDR		0xFF2A0000
#define SERVER_MSCH0_BASE_ADDR		0xFF530000
#define DDR_GRF_BASE_ADDR		0xff630000

struct dram_info dram_info;

struct px30_sdram_params sdram_configs[] = {
#include	"sdram-px30-ddr3-detect-333.inc"
};

struct ddr_phy_skew skew = {
#include	"sdram-px30-ddr_skew.inc"
};

#define PATTERN				(0x5aa5f00f)

/*
 * cs: 0:cs0
 *	   1:cs1
 *     else cs0+cs1
 * note: it didn't consider about row_3_4
 */
u64 sdram_get_cs_cap(struct px30_sdram_channel *cap_info, u32 cs, u32 dram_type)
{
	u32 bg;
	u64 cap[2];

	if (dram_type == DDR4)
		/* DDR4 8bit dram BG = 2(4bank groups),
		 * 16bit dram BG = 1 (2 bank groups)
		 */
		bg = (cap_info->dbw == 0) ? 2 : 1;
	else
		bg = 0;
	cap[0] = 1llu << (cap_info->bw + cap_info->col +
		bg + cap_info->bk + cap_info->cs0_row);

	if (cap_info->rank == 2)
		cap[1] = 1llu << (cap_info->bw + cap_info->col +
			bg + cap_info->bk + cap_info->cs1_row);
	else
		cap[1] = 0;

	if (cs == 0)
		return cap[0];
	else if (cs == 1)
		return cap[1];
	else
		return (cap[0] + cap[1]);
}

/* n: Unit bytes */
void sdram_copy_to_reg(u32 *dest, const u32 *src, u32 n)
{
	int i;

	for (i = 0; i < n / sizeof(u32); i++) {
		writel(*src, dest);
		src++;
		dest++;
	}
}

static void sdram_phy_dll_bypass_set(void __iomem *phy_base, u32 freq)
{
	u32 tmp;
	u32 i, j;

	setbits_le32(PHY_REG(phy_base, 0x13), 1 << 4);
	clrbits_le32(PHY_REG(phy_base, 0x14), 1 << 3);
	for (i = 0; i < 4; i++) {
		j = 0x26 + i * 0x10;
		setbits_le32(PHY_REG(phy_base, j), 1 << 4);
		clrbits_le32(PHY_REG(phy_base, j + 0x1), 1 << 3);
	}

	if (freq <= (400000000))
		/* DLL bypass */
		setbits_le32(PHY_REG(phy_base, 0xa4), 0x1f);
	else
		clrbits_le32(PHY_REG(phy_base, 0xa4), 0x1f);

	if (freq <= (801000000))
		tmp = 2;
	else
		tmp = 1;

	for (i = 0; i < 4; i++) {
		j = 0x28 + i * 0x10;
		writel(tmp, PHY_REG(phy_base, j));
	}
}

static void sdram_phy_set_ds_odt(void __iomem *phy_base,
				 u32 dram_type)
{
	u32 cmd_drv, clk_drv, dqs_drv, dqs_odt;
	u32 i, j;

	if (dram_type == DDR3) {
		cmd_drv = PHY_DDR3_RON_RTT_34ohm;
		clk_drv = PHY_DDR3_RON_RTT_45ohm;
		dqs_drv = PHY_DDR3_RON_RTT_34ohm;
		dqs_odt = PHY_DDR3_RON_RTT_225ohm;
	} else {
		cmd_drv = PHY_DDR4_LPDDR3_RON_RTT_34ohm;
		clk_drv = PHY_DDR4_LPDDR3_RON_RTT_43ohm;
		dqs_drv = PHY_DDR4_LPDDR3_RON_RTT_34ohm;
		if (dram_type == LPDDR2)
			dqs_odt = PHY_DDR4_LPDDR3_RON_RTT_DISABLE;
		else
			dqs_odt = PHY_DDR4_LPDDR3_RON_RTT_240ohm;
	}
	/* DS */
	writel(cmd_drv, PHY_REG(phy_base, 0x11));
	clrsetbits_le32(PHY_REG(phy_base, 0x12), 0x1f << 3, cmd_drv << 3);
	writel(clk_drv, PHY_REG(phy_base, 0x16));
	writel(clk_drv, PHY_REG(phy_base, 0x18));

	for (i = 0; i < 4; i++) {
		j = 0x20 + i * 0x10;
		writel(dqs_drv, PHY_REG(phy_base, j));
		writel(dqs_drv, PHY_REG(phy_base, j + 0xf));
		/* ODT */
		writel(dqs_odt, PHY_REG(phy_base, j + 0x1));
		writel(dqs_odt, PHY_REG(phy_base, j + 0xe));
	}
}

static void phy_soft_reset(void __iomem *phy_base)
{
	clrbits_le32(PHY_REG(phy_base, 0), 0x3 << 2);
	udelay(1);
	setbits_le32(PHY_REG(phy_base, 0), ANALOG_DERESET);
	udelay(5);
	setbits_le32(PHY_REG(phy_base, 0), DIGITAL_DERESET);
	udelay(1);
}

static void phy_dram_set_bw(void __iomem *phy_base, u32 bw)
{
	if (bw == 2) {
		clrsetbits_le32(PHY_REG(phy_base, 0), 0xf << 4, 0xf << 4);
		setbits_le32(PHY_REG(phy_base, 0x46), 1 << 3);
		setbits_le32(PHY_REG(phy_base, 0x56), 1 << 3);
	} else if (bw == 1) {
		clrsetbits_le32(PHY_REG(phy_base, 0), 0xf << 4, 3 << 4);
		clrbits_le32(PHY_REG(phy_base, 0x46), 1 << 3);
		clrbits_le32(PHY_REG(phy_base, 0x56), 1 << 3);
	} else if (bw == 0) {
		clrsetbits_le32(PHY_REG(phy_base, 0), 0xf << 4, 1 << 4);
		clrbits_le32(PHY_REG(phy_base, 0x36), 1 << 3);
		clrbits_le32(PHY_REG(phy_base, 0x46), 1 << 3);
		clrbits_le32(PHY_REG(phy_base, 0x56), 1 << 3);
	}

	phy_soft_reset(phy_base);
}

static int phy_data_training(void __iomem *phy_base, u32 cs, u32 dramtype)
{
	u32 ret;
	u32 odt_val;
	u32 i, j;

	odt_val = readl(PHY_REG(phy_base, 0x2e));

	for (i = 0; i < 4; i++) {
		j = 0x20 + i * 0x10;
		writel(PHY_DDR3_RON_RTT_225ohm, PHY_REG(phy_base, j + 0x1));
		writel(0, PHY_REG(phy_base, j + 0xe));
	}

	if (dramtype == DDR4) {
		clrsetbits_le32(PHY_REG(phy_base, 0x29), 0x3, 0);
		clrsetbits_le32(PHY_REG(phy_base, 0x39), 0x3, 0);
		clrsetbits_le32(PHY_REG(phy_base, 0x49), 0x3, 0);
		clrsetbits_le32(PHY_REG(phy_base, 0x59), 0x3, 0);
	}
	/* choose training cs */
	clrsetbits_le32(PHY_REG(phy_base, 2), 0x33, (0x20 >> cs));
	/* enable gate training */
	clrsetbits_le32(PHY_REG(phy_base, 2), 0x33, (0x20 >> cs) | 1);
	udelay(50);
	ret = readl(PHY_REG(phy_base, 0xff));
	/* disable gate training */
	clrsetbits_le32(PHY_REG(phy_base, 2), 0x33, (0x20 >> cs) | 0);
	clrbits_le32(PHY_REG(phy_base, 2), 0x30);

	if (dramtype == DDR4) {
		clrsetbits_le32(PHY_REG(phy_base, 0x29), 0x3, 0x2);
		clrsetbits_le32(PHY_REG(phy_base, 0x39), 0x3, 0x2);
		clrsetbits_le32(PHY_REG(phy_base, 0x49), 0x3, 0x2);
		clrsetbits_le32(PHY_REG(phy_base, 0x59), 0x3, 0x2);
	}

	if (ret & 0x10) {
		ret = -1;
	} else {
		ret = (ret & 0xf) ^ (readl(PHY_REG(phy_base, 0)) >> 4);
		ret = (ret == 0) ? 0 : -1;
	}

	for (i = 0; i < 4; i++) {
		j = 0x20 + i * 0x10;
		writel(odt_val, PHY_REG(phy_base, j + 0x1));
		writel(odt_val, PHY_REG(phy_base, j + 0xe));
	}

	return ret;
}

static void phy_cfg(void __iomem *phy_base,
	     struct ddr_phy_regs *phy_regs, struct ddr_phy_skew *skew,
	     struct px30_base_params *base, u32 bw)
{
	u32 i;

	sdram_phy_dll_bypass_set(phy_base, base->ddr_freq);
	for (i = 0; phy_regs->phy[i][0] != 0xFFFFFFFF; i++) {
		writel(phy_regs->phy[i][1],
		       phy_base + phy_regs->phy[i][0]);
	}
	if (bw == 2) {
		clrsetbits_le32(PHY_REG(phy_base, 0), 0xf << 4, 0xf << 4);
	} else if (bw == 1) {
		clrsetbits_le32(PHY_REG(phy_base, 0), 0xf << 4, 3 << 4);
		/* disable DQS2,DQS3 tx dll  for saving power */
		clrbits_le32(PHY_REG(phy_base, 0x46), 1 << 3);
		clrbits_le32(PHY_REG(phy_base, 0x56), 1 << 3);
	} else {
		clrsetbits_le32(PHY_REG(phy_base, 0), 0xf << 4, 1 << 4);
		/* disable DQS2,DQS3 tx dll  for saving power */
		clrbits_le32(PHY_REG(phy_base, 0x36), 1 << 3);
		clrbits_le32(PHY_REG(phy_base, 0x46), 1 << 3);
		clrbits_le32(PHY_REG(phy_base, 0x56), 1 << 3);
	}
	sdram_phy_set_ds_odt(phy_base, base->dramtype);

	/* deskew */
	setbits_le32(PHY_REG(phy_base, 2), 8);
	sdram_copy_to_reg(PHY_REG(phy_base, 0xb0),
			  &skew->a0_a1_skew[0], 15 * 4);
	sdram_copy_to_reg(PHY_REG(phy_base, 0x70),
			  &skew->cs0_dm0_skew[0], 44 * 4);
	sdram_copy_to_reg(PHY_REG(phy_base, 0xc0),
			  &skew->cs1_dm0_skew[0], 44 * 4);
}

void sdram_org_config(struct px30_sdram_channel *info,
		      struct px30_base_params *base,
		      u32 *p_os_reg2, u32 *p_os_reg3, u32 channel)
{
	*p_os_reg2 |= base->dramtype << SYS_REG_DDRTYPE_SHIFT;
	*p_os_reg2 |= (base->num_channels - 1) << SYS_REG_NUM_CH_SHIFT;
	*p_os_reg2 |= info->row_3_4 << SYS_REG_ROW_3_4_SHIFT(channel);
	*p_os_reg2 |= 1 << SYS_REG_CHINFO_SHIFT(channel);
	*p_os_reg2 |= (info->rank - 1) << SYS_REG_RANK_SHIFT(channel);
	*p_os_reg2 |= (info->col - 9) << SYS_REG_COL_SHIFT(channel);
	*p_os_reg2 |= info->bk == 3 ? 0 : 1 << SYS_REG_BK_SHIFT(channel);
	*p_os_reg2 |= (info->cs0_row - 13) << SYS_REG_CS0_ROW_SHIFT(channel);
	*p_os_reg2 |= (info->cs1_row - 13) << SYS_REG_CS1_ROW_SHIFT(channel);
	*p_os_reg2 |= (2 >> info->bw) << SYS_REG_BW_SHIFT(channel);
	*p_os_reg2 |= (2 >> info->dbw) << SYS_REG_DBW_SHIFT(channel);
}

void sdram_msch_config(struct px30_msch_regs *msch,
		       struct px30_msch_timings *noc_timings,
		       struct px30_sdram_channel *cap_info,
		       struct px30_base_params *base)
{
	u64 cs_cap[2];

	cs_cap[0] = sdram_get_cs_cap(cap_info, 0, base->dramtype);
	cs_cap[1] = sdram_get_cs_cap(cap_info, 1, base->dramtype);
	writel(((((cs_cap[1] >> 20) / 64) & 0xff) << 8) |
			(((cs_cap[0] >> 20) / 64) & 0xff),
			&msch->devicesize);

	writel(noc_timings->ddrtiminga0, &msch->ddrtiminga0);
	writel(noc_timings->ddrtimingb0, &msch->ddrtimingb0);
	writel(noc_timings->ddrtimingc0, &msch->ddrtimingc0);
	writel(noc_timings->devtodev0, &msch->devtodev0);
	writel(noc_timings->ddrmode, &msch->ddrmode);
	writel(noc_timings->ddr4timing, &msch->ddr4timing);
	writel(noc_timings->agingx0, &msch->agingx0);
	writel(noc_timings->agingx0, &msch->aging0);
	writel(noc_timings->agingx0, &msch->aging1);
	writel(noc_timings->agingx0, &msch->aging2);
	writel(noc_timings->agingx0, &msch->aging3);
}

int sdram_detect_bw(struct px30_sdram_channel *cap_info)
{
	return 0;
}

int sdram_detect_cs(struct px30_sdram_channel *cap_info)
{
	return 0;
}

int sdram_detect_col(struct px30_sdram_channel *cap_info,
		     u32 coltmp)
{
	void __iomem *test_addr;
	u32 col;
	u32 bw = cap_info->bw;

	for (col = coltmp; col >= 9; col -= 1) {
		writel(0, CONFIG_SYS_SDRAM_BASE);
		test_addr = (void __iomem *)(CONFIG_SYS_SDRAM_BASE +
				(1ul << (col + bw - 1ul)));
		writel(PATTERN, test_addr);
		if ((readl(test_addr) == PATTERN) &&
		    (readl(CONFIG_SYS_SDRAM_BASE) == 0))
			break;
	}
	if (col == 8) {
		printascii("col error\n");
		return -1;
	}

	cap_info->col = col;

	return 0;
}

int sdram_detect_bank(struct px30_sdram_channel *cap_info,
		      u32 coltmp, u32 bktmp)
{
	void __iomem *test_addr;
	u32 bk;
	u32 bw = cap_info->bw;

	test_addr = (void __iomem *)(CONFIG_SYS_SDRAM_BASE +
			(1ul << (coltmp + bktmp + bw - 1ul)));
	writel(0, CONFIG_SYS_SDRAM_BASE);
	writel(PATTERN, test_addr);
	if ((readl(test_addr) == PATTERN) &&
	    (readl(CONFIG_SYS_SDRAM_BASE) == 0))
		bk = 3;
	else
		bk = 2;

	cap_info->bk = bk;

	return 0;
}

/* detect bg for ddr4 */
int sdram_detect_bg(struct px30_sdram_channel *cap_info,
		    u32 coltmp)
{
	void __iomem *test_addr;
	u32 dbw;
	u32 bw = cap_info->bw;

	test_addr = (void __iomem *)(CONFIG_SYS_SDRAM_BASE +
			(1ul << (coltmp + bw + 1ul)));
	writel(0, CONFIG_SYS_SDRAM_BASE);
	writel(PATTERN, test_addr);
	if ((readl(test_addr) == PATTERN) &&
	    (readl(CONFIG_SYS_SDRAM_BASE) == 0))
		dbw = 0;
	else
		dbw = 1;

	cap_info->dbw = dbw;

	return 0;
}

/* detect dbw for ddr3,lpddr2,lpddr3,lpddr4 */
int sdram_detect_dbw(struct px30_sdram_channel *cap_info, u32 dram_type)
{
	u32 row, col, bk, bw, cs_cap, cs;
	u32 die_bw_0 = 0, die_bw_1 = 0;

	if (dram_type == DDR3 || dram_type == LPDDR4) {
		cap_info->dbw = 1;
	} else if (dram_type == LPDDR3 || dram_type == LPDDR2) {
		row = cap_info->cs0_row;
		col = cap_info->col;
		bk = cap_info->bk;
		cs = cap_info->rank;
		bw = cap_info->bw;
		cs_cap = (1 << (row + col + bk + bw - 20));
		if (bw == 2) {
			if (cs_cap <= 0x2000000) /* 256Mb */
				die_bw_0 = (col < 9) ? 2 : 1;
			else if (cs_cap <= 0x10000000) /* 2Gb */
				die_bw_0 = (col < 10) ? 2 : 1;
			else if (cs_cap <= 0x40000000) /* 8Gb */
				die_bw_0 = (col < 11) ? 2 : 1;
			else
				die_bw_0 = (col < 12) ? 2 : 1;
			if (cs > 1) {
				row = cap_info->cs1_row;
				cs_cap = (1 << (row + col + bk + bw - 20));
				if (cs_cap <= 0x2000000) /* 256Mb */
					die_bw_0 = (col < 9) ? 2 : 1;
				else if (cs_cap <= 0x10000000) /* 2Gb */
					die_bw_0 = (col < 10) ? 2 : 1;
				else if (cs_cap <= 0x40000000) /* 8Gb */
					die_bw_0 = (col < 11) ? 2 : 1;
				else
					die_bw_0 = (col < 12) ? 2 : 1;
			}
		} else {
			die_bw_1 = 1;
			die_bw_0 = 1;
		}
		cap_info->dbw = (die_bw_0 > die_bw_1) ? die_bw_0 : die_bw_1;
	}

	return 0;
}

int sdram_detect_row(struct px30_sdram_channel *cap_info,
		     u32 coltmp, u32 bktmp, u32 rowtmp)
{
	u32 row;
	u32 bw = cap_info->bw;
	void __iomem *test_addr;

	for (row = rowtmp; row > 12; row--) {
		writel(0, CONFIG_SYS_SDRAM_BASE);
		test_addr = (void __iomem *)(CONFIG_SYS_SDRAM_BASE +
				(1ul << (row + bktmp + coltmp + bw - 1ul)));
		writel(PATTERN, test_addr);
		if ((readl(test_addr) == PATTERN) &&
		    (readl(CONFIG_SYS_SDRAM_BASE) == 0))
			break;
	}
	if (row == 12) {
		printascii("row error");
		return -1;
	}

	cap_info->cs0_row = row;

	return 0;
}

int sdram_detect_row_3_4(struct px30_sdram_channel *cap_info,
			 u32 coltmp, u32 bktmp)
{
	u32 row_3_4;
	u32 bw = cap_info->bw;
	u32 row = cap_info->cs0_row;
	void __iomem *test_addr, *test_addr1;

	test_addr = CONFIG_SYS_SDRAM_BASE;
	test_addr1 = (void __iomem *)(CONFIG_SYS_SDRAM_BASE +
			(0x3ul << (row + bktmp + coltmp + bw - 1ul - 1ul)));

	writel(0, test_addr);
	writel(PATTERN, test_addr1);
	if ((readl(test_addr) == 0) && (readl(test_addr1) == PATTERN))
		row_3_4 = 0;
	else
		row_3_4 = 1;

	cap_info->row_3_4 = row_3_4;

	return 0;
}

int sdram_detect_high_row(struct px30_sdram_channel *cap_info)
{
	cap_info->cs0_high16bit_row = cap_info->cs0_row;
	cap_info->cs1_high16bit_row = cap_info->cs1_row;

	return 0;
}

int sdram_detect_cs1_row(struct px30_sdram_channel *cap_info, u32 dram_type)
{
	void __iomem *test_addr;
	u32 row = 0, bktmp, coltmp, bw;
	ulong cs0_cap;
	u32 byte_mask;

	if (cap_info->rank == 2) {
		cs0_cap = sdram_get_cs_cap(cap_info, 0, dram_type);

		if (dram_type == DDR4) {
			if (cap_info->dbw == 0)
				bktmp = cap_info->bk + 2;
			else
				bktmp = cap_info->bk + 1;
		} else {
			bktmp = cap_info->bk;
		}
		bw = cap_info->bw;
		coltmp = cap_info->col;

		/*
		 * because px30 support axi split,min bandwidth
		 * is 8bit. if cs0 is 32bit, cs1 may 32bit or 16bit
		 * so we check low 16bit data when detect cs1 row.
		 * if cs0 is 16bit/8bit, we check low 8bit data.
		 */
		if (bw == 2)
			byte_mask = 0xFFFF;
		else
			byte_mask = 0xFF;

		/* detect cs1 row */
		for (row = cap_info->cs0_row; row > 12; row--) {
			test_addr = (void __iomem *)(CONFIG_SYS_SDRAM_BASE +
				    cs0_cap +
				    (1ul << (row + bktmp + coltmp + bw - 1ul)));
			writel(0, CONFIG_SYS_SDRAM_BASE + cs0_cap);
			writel(PATTERN, test_addr);

			if (((readl(test_addr) & byte_mask) ==
			     (PATTERN & byte_mask)) &&
			    ((readl(CONFIG_SYS_SDRAM_BASE + cs0_cap) &
			      byte_mask) == 0)) {
				break;
			}
		}
	}

	cap_info->cs1_row = row;

	return 0;
}


static void rkclk_ddr_reset(struct dram_info *dram,
			    u32 ctl_srstn, u32 ctl_psrstn,
			    u32 phy_srstn, u32 phy_psrstn)
{
	writel(upctl2_srstn_req(ctl_srstn) | upctl2_psrstn_req(ctl_psrstn) |
	       upctl2_asrstn_req(ctl_srstn),
	       &dram->cru->softrst_con[1]);
	writel(ddrphy_srstn_req(phy_srstn) | ddrphy_psrstn_req(phy_psrstn),
	       &dram->cru->softrst_con[2]);
}

static void rkclk_set_dpll(struct dram_info *dram, unsigned int hz)
{
	unsigned int refdiv, postdiv1, postdiv2, fbdiv;
	int delay = 1000;
	u32 mhz = hz / MHz;

	refdiv = 1;
	if (mhz <= 300) {
		postdiv1 = 4;
		postdiv2 = 2;
	} else if (mhz <= 400) {
		postdiv1 = 6;
		postdiv2 = 1;
	} else if (mhz <= 600) {
		postdiv1 = 4;
		postdiv2 = 1;
	} else if (mhz <= 800) {
		postdiv1 = 3;
		postdiv2 = 1;
	} else if (mhz <= 1600) {
		postdiv1 = 2;
		postdiv2 = 1;
	} else {
		postdiv1 = 1;
		postdiv2 = 1;
	}
	fbdiv = (mhz * refdiv * postdiv1 * postdiv2) / 24;

	writel(DPLL_MODE(CLOCK_FROM_XIN_OSC), &dram->cru->mode);

	writel(POSTDIV1(postdiv1) | FBDIV(fbdiv), &dram->cru->pll[1].con0);
	writel(DSMPD(1) | POSTDIV2(postdiv2) | REFDIV(refdiv),
	       &dram->cru->pll[1].con1);

	while (delay > 0) {
		rockchip_udelay(1);
		if (LOCK(readl(&dram->cru->pll[1].con1)))
			break;
		delay--;
	}

	writel(DPLL_MODE(CLOCK_FROM_PLL), &dram->cru->mode);
}

static void rkclk_configure_ddr(struct dram_info *dram,
				struct px30_sdram_params *sdram_params)
{
	/* for inno ddr phy need 2*freq */
	rkclk_set_dpll(dram,  sdram_params->base.ddr_freq * MHz * 2);
}

/* return ddrconfig value
 *       (-1), find ddrconfig fail
 *       other, the ddrconfig value
 * only support cs0_row >= cs1_row
 */
static unsigned int calculate_ddrconfig(struct px30_sdram_params *sdram_params)
{
	struct px30_sdram_channel *cap_info = &sdram_params->ch;
	u32 bw, die_bw, col, bank;
	u32 i, tmp;
	u32 ddrconf = -1;

	bw = cap_info->bw;
	die_bw = cap_info->dbw;
	col = cap_info->col;
	bank = cap_info->bk;

	if (sdram_params->base.dramtype == DDR4) {
		if (die_bw == 0)
			ddrconf = 7 + bw;
		else
			ddrconf = 12 - bw;
		ddrconf = d4_rbc_2_d3_rbc[ddrconf - 7];
	} else {
		tmp = ((bank - 2) << 3) | (col + bw - 10);
		for (i = 0; i < 7; i++)
			if ((ddr_cfg_2_rbc[i] & 0xf) == tmp) {
				ddrconf = i;
				break;
			}
		if (i > 6)
			printascii("calculate ddrconfig error\n");
	}

	return ddrconf;
}

/*
 * rank = 1: cs0
 * rank = 2: cs1
 */
static void pctl_read_mr(void __iomem *pctl_base, u32 rank, u32 mr_num)
{
	writel((rank << 4) | (1 << 0), pctl_base + DDR_PCTL2_MRCTRL0);
	writel((mr_num << 8), pctl_base + DDR_PCTL2_MRCTRL1);
	setbits_le32(pctl_base + DDR_PCTL2_MRCTRL0, 1u << 31);
	while (readl(pctl_base + DDR_PCTL2_MRCTRL0) & (1u << 31))
		continue;
	while (readl(pctl_base + DDR_PCTL2_MRSTAT) & MR_WR_BUSY)
		continue;
}

/* rank = 1: cs0
 * rank = 2: cs1
 * rank = 3: cs0 & cs1
 * note: be careful of keep mr original val
 */
static int pctl_write_mr(void __iomem *pctl_base, u32 rank, u32 mr_num, u32 arg,
		  u32 dramtype)
{
	while (readl(pctl_base + DDR_PCTL2_MRSTAT) & MR_WR_BUSY)
		continue;
	if (dramtype == DDR3 || dramtype == DDR4) {
		writel((mr_num << 12) | (rank << 4) | (0 << 0),
		       pctl_base + DDR_PCTL2_MRCTRL0);
		writel(arg, pctl_base + DDR_PCTL2_MRCTRL1);
	} else {
		writel((rank << 4) | (0 << 0),
		       pctl_base + DDR_PCTL2_MRCTRL0);
		writel((mr_num << 8) | (arg & 0xff),
		       pctl_base + DDR_PCTL2_MRCTRL1);
	}

	setbits_le32(pctl_base + DDR_PCTL2_MRCTRL0, 1u << 31);
	while (readl(pctl_base + DDR_PCTL2_MRCTRL0) & (1u << 31))
		continue;
	while (readl(pctl_base + DDR_PCTL2_MRSTAT) & MR_WR_BUSY)
		continue;

	return 0;
}

static int upctl2_update_ref_reg(void __iomem *pctl_base)
{
	u32 ret;

	ret = readl(pctl_base + DDR_PCTL2_RFSHCTL3) ^ (1 << 1);
	writel(ret, pctl_base + DDR_PCTL2_RFSHCTL3);

	return 0;
}

static u32 pctl_dis_zqcs_aref(void __iomem *pctl_base)
{
	u32 dis_auto_zq = 0;

	/* disable zqcs */
	if (!(readl(pctl_base + DDR_PCTL2_ZQCTL0) &
		(1ul << 31))) {
		dis_auto_zq = 1;
		setbits_le32(pctl_base + DDR_PCTL2_ZQCTL0, 1 << 31);
	}

	/* disable auto refresh */
	setbits_le32(pctl_base + DDR_PCTL2_RFSHCTL3, 1);

	upctl2_update_ref_reg(pctl_base);

	return dis_auto_zq;
}

static void pctl_rest_zqcs_aref(void __iomem *pctl_base, u32 dis_auto_zq)
{
	/* restore zqcs */
	if (dis_auto_zq)
		clrbits_le32(pctl_base + DDR_PCTL2_ZQCTL0, 1 << 31);

	/* restore auto refresh */
	clrbits_le32(pctl_base + DDR_PCTL2_RFSHCTL3, 1);

	upctl2_update_ref_reg(pctl_base);
}

/*
 * rank : 1:cs0, 2:cs1, 3:cs0&cs1
 * vrefrate: 4500: 45%,
 */
static int pctl_write_vrefdq(void __iomem *pctl_base, u32 rank, u32 vrefrate,
		      u32 dramtype)
{
	u32 tccd_l, value;
	u32 dis_auto_zq = 0;

	if (dramtype != DDR4 || vrefrate < 4500 ||
	    vrefrate > 9200)
		return (-1);

	tccd_l = (readl(pctl_base + DDR_PCTL2_DRAMTMG4) >> 16) & 0xf;
	tccd_l = (tccd_l - 4) << 10;

	if (vrefrate > 7500) {
		/* range 1 */
		value = ((vrefrate - 6000) / 65) | tccd_l;
	} else {
		/* range 2 */
		value = ((vrefrate - 4500) / 65) | tccd_l | (1 << 6);
	}

	dis_auto_zq = pctl_dis_zqcs_aref(pctl_base);

	/* enable vrefdq calibratin */
	pctl_write_mr(pctl_base, rank, 6, value | (1 << 7), dramtype);
	udelay(1);/* tvrefdqe */
	/* write vrefdq value */
	pctl_write_mr(pctl_base, rank, 6, value | (1 << 7), dramtype);
	udelay(1);/* tvref_time */
	pctl_write_mr(pctl_base, rank, 6, value | (0 << 7), dramtype);
	udelay(1);/* tvrefdqx */

	pctl_rest_zqcs_aref(pctl_base, dis_auto_zq);

	return 0;
}

static u32 pctl_remodify_sdram_params(struct ddr_pctl_regs *pctl_regs,
			       struct px30_sdram_channel *cap_info,
			       u32 dram_type)
{
	u32 tmp = 0, tmp_adr = 0, i;

	for (i = 0; pctl_regs->pctl[i][0] != 0xFFFFFFFF; i++) {
		if (pctl_regs->pctl[i][0] == 0) {
			tmp = pctl_regs->pctl[i][1];/* MSTR */
			tmp_adr = i;
		}
	}

	tmp &= ~((3ul << 30) | (3ul << 24) | (3ul << 12));

	switch (cap_info->dbw) {
	case 2:
		tmp |= (3ul << 30);
		break;
	case 1:
		tmp |= (2ul << 30);
		break;
	case 0:
	default:
		tmp |= (1ul << 30);
		break;
	}

	/*
	 * If DDR3 or DDR4 MSTR.active_ranks=1,
	 * it will gate memory clock when enter power down.
	 * Force set active_ranks to 3 to workaround it.
	 */
	if (cap_info->rank == 2 || dram_type == DDR3 ||
	    dram_type == DDR4)
		tmp |= 3 << 24;
	else
		tmp |= 1 << 24;

	tmp |= (2 - cap_info->bw) << 12;

	pctl_regs->pctl[tmp_adr][1] = tmp;

	return 0;
}

static int pctl_cfg(void __iomem *pctl_base, struct ddr_pctl_regs *pctl_regs,
	     u32 sr_idle, u32 pd_idle)
{
	u32 i;

	for (i = 0; pctl_regs->pctl[i][0] != 0xFFFFFFFF; i++) {
		writel(pctl_regs->pctl[i][1],
		       pctl_base + pctl_regs->pctl[i][0]);
	}
	clrsetbits_le32(pctl_base + DDR_PCTL2_PWRTMG,
			(0xff << 16) | 0x1f,
			((sr_idle & 0xff) << 16) | (pd_idle & 0x1f));

	clrsetbits_le32(pctl_base + DDR_PCTL2_HWLPCTL,
			0xfff << 16,
			5 << 16);
	/* disable zqcs */
	setbits_le32(pctl_base + DDR_PCTL2_ZQCTL0, 1u << 31);

	return 0;
}

/*
 * calculate controller dram address map, and setting to register.
 * argument sdram_params->ch.ddrconf must be right value before
 * call this function.
 */
static void set_ctl_address_map(struct dram_info *dram,
				struct px30_sdram_params *sdram_params)
{
	struct px30_sdram_channel *cap_info = &sdram_params->ch;
	void __iomem *pctl_base = dram->pctl;
	u32 cs_pst, bg, max_row, ddrconf;
	u32 i;

	if (sdram_params->base.dramtype == DDR4)
		/*
		 * DDR4 8bit dram BG = 2(4bank groups),
		 * 16bit dram BG = 1 (2 bank groups)
		 */
		bg = (cap_info->dbw == 0) ? 2 : 1;
	else
		bg = 0;

	cs_pst = cap_info->bw + cap_info->col +
		bg + cap_info->bk + cap_info->cs0_row;
	if (cs_pst >= 32 || cap_info->rank == 1)
		writel(0x1f, pctl_base + DDR_PCTL2_ADDRMAP0);
	else
		writel(cs_pst - 8, pctl_base + DDR_PCTL2_ADDRMAP0);

	ddrconf = cap_info->ddrconfig;
	if (sdram_params->base.dramtype == DDR4) {
		for (i = 0; i < ARRAY_SIZE(d4_rbc_2_d3_rbc); i++) {
			if (d4_rbc_2_d3_rbc[i] == ddrconf) {
				ddrconf = 7 + i;
				break;
			}
		}
	}

	sdram_copy_to_reg((u32 *)(pctl_base + DDR_PCTL2_ADDRMAP1),
			  &addrmap[ddrconf][0], 8 * 4);
	max_row = cs_pst - 1 - 8 - (addrmap[ddrconf][5] & 0xf);

	if (max_row < 12)
		printascii("set addrmap fail\n");
	/* need to disable row ahead of rank by set to 0xf */
	for (i = 17; i > max_row; i--)
		clrsetbits_le32(pctl_base + DDR_PCTL2_ADDRMAP6 +
			((i - 12) * 8 / 32) * 4,
			0xf << ((i - 12) * 8 % 32),
			0xf << ((i - 12) * 8 % 32));

	if ((sdram_params->base.dramtype == LPDDR3 ||
	     sdram_params->base.dramtype == LPDDR2) &&
		 cap_info->row_3_4)
		setbits_le32(pctl_base + DDR_PCTL2_ADDRMAP6, 1 << 31);
	if (sdram_params->base.dramtype == DDR4 && cap_info->bw != 0x2)
		setbits_le32(pctl_base + DDR_PCTL2_PCCFG, 1 << 8);
}

/*
 * rank = 1: cs0
 * rank = 2: cs1
 */
int read_mr(struct dram_info *dram, u32 rank, u32 mr_num)
{
	void __iomem *ddr_grf_base = dram->ddr_grf;

	pctl_read_mr(dram->pctl, rank, mr_num);

	return (readl(ddr_grf_base + DDR_GRF_STATUS(0)) & 0xff);
}

#define MIN(a, b)	(((a) > (b)) ? (b) : (a))
#define MAX(a, b)	(((a) > (b)) ? (a) : (b))
static u32 check_rd_gate(struct dram_info *dram)
{
	void __iomem *phy_base = dram->phy;

	u32 max_val = 0;
	u32 min_val = 0xff;
	u32 gate[4];
	u32 i, bw;

	bw = (readl(PHY_REG(phy_base, 0x0)) >> 4) & 0xf;
	switch (bw) {
	case 0x1:
		bw = 1;
		break;
	case 0x3:
		bw = 2;
		break;
	case 0xf:
	default:
		bw = 4;
		break;
	}

	for (i = 0; i < bw; i++) {
		gate[i] = readl(PHY_REG(phy_base, 0xfb + i));
		max_val = MAX(max_val, gate[i]);
		min_val = MIN(min_val, gate[i]);
	}

	if (max_val > 0x80 || min_val < 0x20)
		return -1;
	else
		return 0;
}

static int data_training(struct dram_info *dram, u32 cs, u32 dramtype)
{
	void __iomem *pctl_base = dram->pctl;
	u32 dis_auto_zq = 0;
	u32 pwrctl;
	u32 ret;

	/* disable auto low-power */
	pwrctl = readl(pctl_base + DDR_PCTL2_PWRCTL);
	writel(0, pctl_base + DDR_PCTL2_PWRCTL);

	dis_auto_zq = pctl_dis_zqcs_aref(dram->pctl);

	ret = phy_data_training(dram->phy, cs, dramtype);

	pctl_rest_zqcs_aref(dram->pctl, dis_auto_zq);

	/* restore auto low-power */
	writel(pwrctl, pctl_base + DDR_PCTL2_PWRCTL);

	return ret;
}

static void dram_set_bw(struct dram_info *dram, u32 bw)
{
	phy_dram_set_bw(dram->phy, bw);
}

static void set_ddrconfig(struct dram_info *dram, u32 ddrconfig)
{
	writel(ddrconfig | (ddrconfig << 8), &dram->msch->deviceconf);
	rk_clrsetreg(&dram->grf->soc_noc_con[1], 0x3 << 14, 0 << 14);
}

static void dram_all_config(struct dram_info *dram,
			    struct px30_sdram_params *sdram_params)
{
	struct px30_sdram_channel *cap_info = &sdram_params->ch;
	u32 sys_reg2 = 0;
	u32 sys_reg3 = 0;

	set_ddrconfig(dram, cap_info->ddrconfig);
	sdram_org_config(cap_info, &sdram_params->base, &sys_reg2,
			 &sys_reg3, 0);
	writel(sys_reg2, &dram->pmugrf->os_reg[2]);
	writel(sys_reg3, &dram->pmugrf->os_reg[3]);
	sdram_msch_config(dram->msch, &sdram_params->ch.noc_timings, cap_info,
			  &sdram_params->base);
}

static void enable_low_power(struct dram_info *dram,
			     struct px30_sdram_params *sdram_params)
{
	void __iomem *pctl_base = dram->pctl;
	void __iomem *phy_base = dram->phy;
	void __iomem *ddr_grf_base = dram->ddr_grf;
	u32 grf_lp_con;

	/*
	 * bit0: grf_upctl_axi_cg_en = 1 enable upctl2 axi clk auto gating
	 * bit1: grf_upctl_apb_cg_en = 1 ungated axi,core clk for apb access
	 * bit2: grf_upctl_core_cg_en = 1 enable upctl2 core clk auto gating
	 * bit3: grf_selfref_type2_en = 0 disable core clk gating when type2 sr
	 * bit4: grf_upctl_syscreq_cg_en = 1
	 *       ungating coreclk when c_sysreq assert
	 * bit8-11: grf_auto_sr_dly = 6
	 */
	writel(0x1f1f0617, &dram->ddr_grf->ddr_grf_con[1]);

	if (sdram_params->base.dramtype == DDR4)
		grf_lp_con = (0x7 << 16) | (1 << 1);
	else if (sdram_params->base.dramtype == DDR3)
		grf_lp_con = (0x7 << 16) | (1 << 0);
	else
		grf_lp_con = (0x7 << 16) | (1 << 2);

	/* en lpckdis_en */
	grf_lp_con = grf_lp_con | (0x1 << (9 + 16)) | (0x1 << 9);
	writel(grf_lp_con, ddr_grf_base + DDR_GRF_LP_CON);

	/* off digit module clock when enter power down */
	setbits_le32(PHY_REG(phy_base, 7), 1 << 7);

	/* enable sr, pd */
	if (PD_IDLE == 0)
		clrbits_le32(pctl_base + DDR_PCTL2_PWRCTL, (1 << 1));
	else
		setbits_le32(pctl_base + DDR_PCTL2_PWRCTL, (1 << 1));
	if (SR_IDLE == 0)
		clrbits_le32(pctl_base + DDR_PCTL2_PWRCTL, 1);
	else
		setbits_le32(pctl_base + DDR_PCTL2_PWRCTL, 1);
	setbits_le32(pctl_base + DDR_PCTL2_PWRCTL, (1 << 3));
}

/*
 * pre_init: 0: pre init for dram cap detect
 * 1: detect correct cap(except cs1 row)info, than reinit
 * 2: after reinit, we detect cs1_row, if cs1_row not equal
 *    to cs0_row and cs is in middle on ddrconf map, we need
 *    to reinit dram, than set the correct ddrconf.
 */
static int sdram_init_(struct dram_info *dram,
		       struct px30_sdram_params *sdram_params, u32 pre_init)
{
	struct px30_sdram_channel *cap_info = &sdram_params->ch;
	void __iomem *pctl_base = dram->pctl;

	rkclk_ddr_reset(dram, 1, 1, 1, 1);
	rockchip_udelay(10);
	/*
	 * dereset ddr phy psrstn to config pll,
	 * if using phy pll psrstn must be dereset
	 * before config pll
	 */
	rkclk_ddr_reset(dram, 1, 1, 1, 0);
	rkclk_configure_ddr(dram, sdram_params);

	/* release phy srst to provide clk to ctrl */
	rkclk_ddr_reset(dram, 1, 1, 0, 0);
	rockchip_udelay(10);
	phy_soft_reset(dram->phy);

	/* release ctrl presetn, and config ctl registers */
	rkclk_ddr_reset(dram, 1, 0, 0, 0);
	pctl_cfg(dram->pctl, &sdram_params->pctl_regs, SR_IDLE, PD_IDLE);
	cap_info->ddrconfig = calculate_ddrconfig(sdram_params);
	set_ctl_address_map(dram, sdram_params);
	phy_cfg(dram->phy, &sdram_params->phy_regs, sdram_params->skew,
		&sdram_params->base, cap_info->bw);

	/* enable dfi_init_start to init phy after ctl srstn deassert */
	setbits_le32(pctl_base + DDR_PCTL2_DFIMISC, (1 << 5) | (1 << 4));

	rkclk_ddr_reset(dram, 0, 0, 0, 0);
	/* wait for dfi_init_done and dram init complete */
	while ((readl(pctl_base + DDR_PCTL2_STAT) & 0x7) == 0)
		continue;

	if (sdram_params->base.dramtype == LPDDR3)
		pctl_write_mr(dram->pctl, 3, 11, 3, LPDDR3);

	/* do ddr gate training */
redo_cs0_training:
	if (data_training(dram, 0, sdram_params->base.dramtype) != 0) {
		if (pre_init != 0)
			printascii("DTT cs0 error\n");
		return -1;
	}
	if (check_rd_gate(dram)) {
		printascii("re training cs0");
		goto redo_cs0_training;
	}

	if (sdram_params->base.dramtype == LPDDR3) {
		if ((read_mr(dram, 1, 8) & 0x3) != 0x3)
			return -1;
	} else if (sdram_params->base.dramtype == LPDDR2) {
		if ((read_mr(dram, 1, 8) & 0x3) != 0x0)
			return -1;
	}

	/* for px30: when 2cs, both 2 cs should be training */
	if (pre_init != 0 && cap_info->rank == 2) {
redo_cs1_training:
		if (data_training(dram, 1, sdram_params->base.dramtype) != 0) {
			printascii("DTT cs1 error\n");
			return -1;
		}
		if (check_rd_gate(dram)) {
			printascii("re training cs1");
			goto redo_cs1_training;
		}
	}

	if (sdram_params->base.dramtype == DDR4)
		pctl_write_vrefdq(dram->pctl, 0x3, 5670,
				  sdram_params->base.dramtype);

	dram_all_config(dram, sdram_params);
	enable_low_power(dram, sdram_params);

	return 0;
}

static int dram_detect_cap(struct dram_info *dram,
			   struct px30_sdram_params *sdram_params,
			   unsigned char channel)
{
	struct px30_sdram_channel *cap_info = &sdram_params->ch;

	/*
	 * for ddr3: ddrconf = 3
	 * for ddr4: ddrconf = 12
	 * for lpddr3: ddrconf = 3
	 * default bw = 1
	 */
	u32 bk, bktmp;
	u32 col, coltmp;
	u32 rowtmp;
	u32 cs;
	u32 bw = 1;
	u32 dram_type = sdram_params->base.dramtype;

	if (dram_type != DDR4) {
		/* detect col and bk for ddr3/lpddr3 */
		coltmp = 12;
		bktmp = 3;
		if (dram_type == LPDDR2)
			rowtmp = 15;
		else
			rowtmp = 16;

		if (sdram_detect_col(cap_info, coltmp) != 0)
			goto cap_err;
		sdram_detect_bank(cap_info, coltmp, bktmp);
		sdram_detect_dbw(cap_info, dram_type);
	} else {
		/* detect bg for ddr4 */
		coltmp = 10;
		bktmp = 4;
		rowtmp = 17;

		col = 10;
		bk = 2;
		cap_info->col = col;
		cap_info->bk = bk;
		sdram_detect_bg(cap_info, coltmp);
	}

	/* detect row */
	if (sdram_detect_row(cap_info, coltmp, bktmp, rowtmp) != 0)
		goto cap_err;

	/* detect row_3_4 */
	sdram_detect_row_3_4(cap_info, coltmp, bktmp);

	/* bw and cs detect using data training */
	if (data_training(dram, 1, dram_type) == 0)
		cs = 1;
	else
		cs = 0;
	cap_info->rank = cs + 1;

	dram_set_bw(dram, 2);
	if (data_training(dram, 0, dram_type) == 0)
		bw = 2;
	else
		bw = 1;
	cap_info->bw = bw;

	cap_info->cs0_high16bit_row = cap_info->cs0_row;
	if (cs) {
		cap_info->cs1_row = cap_info->cs0_row;
		cap_info->cs1_high16bit_row = cap_info->cs0_row;
	} else {
		cap_info->cs1_row = 0;
		cap_info->cs1_high16bit_row = 0;
	}

	return 0;
cap_err:
	return -1;
}

static int sdram_init_detect(struct dram_info *dram,
			     struct px30_sdram_params *sdram_params)
{
	struct px30_sdram_channel *cap_info = &sdram_params->ch;
	u32 ret;
	u32 sys_reg = 0;
	u32 sys_reg3 = 0;

	if (sdram_init_(dram, sdram_params, 0) != 0)
		return -1;

	if (dram_detect_cap(dram, sdram_params, 0) != 0)
		return -1;

	/* modify bw, cs related timing */
	pctl_remodify_sdram_params(&sdram_params->pctl_regs, cap_info,
				   sdram_params->base.dramtype);
	/* reinit sdram by real dram cap */
	ret = sdram_init_(dram, sdram_params, 1);
	if (ret != 0)
		goto out;

	/* redetect cs1 row */
	sdram_detect_cs1_row(cap_info, sdram_params->base.dramtype);
	if (cap_info->cs1_row) {
		sys_reg = readl(&dram->pmugrf->os_reg[2]);
		sys_reg3 = readl(&dram->pmugrf->os_reg[3]);
/*		SYS_REG_ENC_CS1_ROW(cap_info->cs1_row,
				    sys_reg, sys_reg3, 0); */
		writel(sys_reg, &dram->pmugrf->os_reg[2]);
		writel(sys_reg3, &dram->pmugrf->os_reg[3]);
	}

	ret = sdram_detect_high_row(cap_info);

out:
	return ret;
}

struct px30_sdram_params *get_default_sdram_config(void)
{
	sdram_configs[0].skew = &skew;

	return &sdram_configs[0];
}

int sdram_init(void)
{
	struct px30_sdram_params *sdram_params;
	int ret = 0;

	dram_info.phy = (void *)DDR_PHY_BASE_ADDR;
	dram_info.pctl = (void *)DDRC_BASE_ADDR;
	dram_info.grf = (void *)GRF_BASE_ADDR;
	dram_info.cru = (void *)CRU_BASE_ADDR;
	dram_info.msch = (void *)SERVER_MSCH0_BASE_ADDR;
	dram_info.ddr_grf = (void *)DDR_GRF_BASE_ADDR;
	dram_info.pmugrf = (void *)PMUGRF_BASE_ADDR;

	sdram_params = get_default_sdram_config();
	ret = sdram_init_detect(&dram_info, sdram_params);
	if (ret)
		return ret;

	return ret;
}
#endif /* CONFIG_TPL_BUILD */
