// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * (C) Copyright 2016-2017 Rockchip Inc.
 *
 * Adapted from coreboot.
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <dt-structs.h>
#include <ram.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/sdram_common.h>
#include <asm/arch/sdram_rk3399.h>
#include <asm/arch/cru_rk3399.h>
#include <asm/arch/grf_rk3399.h>
#include <asm/arch/hardware.h>
#include <linux/err.h>
//#include <linux/kernel.h>
#include <time.h>
#include <i2c_eeprom.h>
#include "dram_spec_timing.h"

struct chan_info {
	struct rk3399_ddr_pctl_regs *pctl;
	struct rk3399_ddr_pi_regs *pi;
	struct rk3399_ddr_publ_regs *publ;
	struct rk3399_msch_regs *msch;
};

struct dram_info {
#ifdef CONFIG_SPL_BUILD
	struct chan_info chan[2];
	struct clk ddr_clk;
	struct rk3399_cru *cru;
	struct rk3399_pmucru *pmucru;
	struct rk3399_pmusgrf_regs *pmusgrf;
	struct rk3399_ddr_cic_regs *cic;
#endif
	struct ram_info info;
	struct rk3399_pmugrf_regs *pmugrf;
};

#define PRESET_SGRF_HOLD(n)	((0x1 << (6 + 16)) | ((n) << 6))
#define PRESET_GPIO0_HOLD(n)	((0x1 << (7 + 16)) | ((n) << 7))
#define PRESET_GPIO1_HOLD(n)	((0x1 << (8 + 16)) | ((n) << 8))

/*
 * Calculating back from the below drive-strength/ODT table,
 * the following resistor array is used:
 *   0b1000 = 120 ohm
 *   0b0100 = 120 ohm
 *   0b0010 = 120 ohm
 *   0b0001 = 240 ohm
 */

#define PHY_DRV_ODT_Hi_Z	0x0
#define PHY_DRV_ODT_240		0x1
#define PHY_DRV_ODT_120		0x8
#define PHY_DRV_ODT_80		0x9
#define PHY_DRV_ODT_60		0xc
#define PHY_DRV_ODT_48		0xd
#define PHY_DRV_ODT_40		0xe
#define PHY_DRV_ODT_34_3	0xf

#define ENPER_CS_TRAINING_FREQ	(666)
#define TDFI_LAT_THRESHOLD_FREQ	(933) /* TODO: actually 928 */
#define PHY_DLL_BYPASS_FREQ	(260)

#define PI_REGS_DIMM_SUPPORT	(0)
#define PI_ADD_LATENCY	(0)
#define PI_DOUBLEFREEK	(1)

#define PI_PAD_DELAY_PS_VALUE	(1000)
#define PI_IE_ENABLE_VALUE	(3000)
#define PI_TSEL_ENABLE_VALUE	(700)

#ifdef CONFIG_SPL_BUILD

static int spd_decode_ddr3(const u8 *spd, const u32 freq, struct dram_timing_t *timing);

#if 0

#undef clrbits_le32
#undef setbits_le32

static inline void setbits_le32(u32 *addr, u32 set)
{
	*addr |= set;
}

static inline void clrbits_le32(u32 *addr, u32 clr)
{
	*addr &= ~clr;
}

#undef clrsetbits_le32
static inline void clrsetbits_le32(u32 *addr, u32 clr, u32 set)
{
	*addr &= ~clr;
	*addr |= set;
}

#undef writel
#define writel writel_relaxed

#endif

struct rockchip_dmc_plat {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_rockchip_rk3399_dmc dtplat;
#else
	struct rk3399_sdram_params sdram_params;
	u8 spd[128];
#endif
	struct regmap *map;
};

static uint32_t wrdqs_delay_val[2][2][4];
static uint32_t rddqs_delay_ps;

static void phy_dll_bypass_set(struct rk3399_ddr_publ_regs *ddr_publ_regs,
			       u32 freq, u32 ch, u32 index, u32 dram_type)
{
	u32 *denali_phy = ddr_publ_regs->denali_phy;
	u32 sw_master_mode = 0;
	u32 rddqs_gate_delay, rddqs_latency, total_delay;
	u32 i;

	debug("%s\n", __func__);

	if (dram_type == DDR3)
		total_delay = PI_PAD_DELAY_PS_VALUE;
	else if (dram_type == LPDDR3)
		total_delay = PI_PAD_DELAY_PS_VALUE + 2500;
	else
		total_delay = PI_PAD_DELAY_PS_VALUE + 1500;

	/* total_delay + 0.55tck */
	total_delay +=  (55 * 10000)/freq;
	rddqs_latency = total_delay * freq / 1000000;
	total_delay -= rddqs_latency * 1000000 / freq;
	rddqs_gate_delay = total_delay * 0x200 * freq / 1000000;

	if (freq <= PHY_DLL_BYPASS_FREQ) {
		sw_master_mode = 0xc;

		setbits_le32(&denali_phy[514], 1);
		setbits_le32(&denali_phy[642], 1);
		setbits_le32(&denali_phy[770], 1);

		/* setting bypass mode slave delay */
		for (i = 0; i < 4; i++) {
			/* wr dq delay = -180deg + (0x60 / 4) * 20ps */
			clrsetbits_le32(&denali_phy[1 + 128 * i],
					0x7ff << 8, 0x4a0 << 8);
			/* rd dqs/dq delay = (0x60 / 4) * 20ps */
			clrsetbits_le32(&denali_phy[11 + 128 * i],
					0x3ff, 0xa0);
			/* rd rddqs_gate delay */
			clrsetbits_le32(&denali_phy[2 + 128 * i],
					0x3ff, rddqs_gate_delay);
			clrsetbits_le32(&denali_phy[78 + 128 * i],
					0xf, rddqs_latency);
		}

		/* adr delay */
		for (i = 0; i < 3; i++)
			clrsetbits_le32(&denali_phy[513 + 128 * i],
					0x7ff << 16, 0x80 << 16);

		if ((readl(&denali_phy[86]) & 0xc00) == 0) {
			/*
			 * old status is normal mode,
			 * and saving the wrdqs slave delay
			 */
			for (i = 0; i < 4; i++) {
				u32 reg = readl(&denali_phy[63 + i * 128]);

				/* save and clear wr dqs slave delay */
				wrdqs_delay_val[ch][index][i] =
					        0x3ff &(reg >> 16);
				clrsetbits_le32(&denali_phy[63 + i * 128],
						0x3ff << 16, 0 << 16);
				/*
				 * in normal mode the cmd may delay 1cycle by
				 * wrlvl and in bypass mode making dqs also
				 * delay 1cycle.
				 */
				clrsetbits_le32(&denali_phy[78 + i * 128],
						0x7 << 8, 0x1 << 8);
			}
		}
	} else if (readl(&denali_phy[86]) & 0xc00) {
		/* old status is bypass mode and restore wrlvl resume */
		for (i = 0; i < 4; i++) {
			clrsetbits_le32(&denali_phy[63 + i * 128],
					0x03ff << 16,
					(wrdqs_delay_val[ch][index][i] &
					 0x3ff) << 16);
			/* resume phy_write_path_lat_add */
			clrbits_le32(&denali_phy[78 + i * 128], 0x7 << 8);
		}
	}

	/* phy_sw_master_mode_X PHY_86/214/342/470 4bits offset_8 */
	clrsetbits_le32(&denali_phy[86], 0xf << 8, sw_master_mode << 8);
	clrsetbits_le32(&denali_phy[214], 0xf << 8, sw_master_mode << 8);
	clrsetbits_le32(&denali_phy[342], 0xf << 8, sw_master_mode << 8);
	clrsetbits_le32(&denali_phy[470], 0xf << 8, sw_master_mode << 8);

	/* phy_adrctl_sw_master_mode PHY_547/675/803 4bits offset_16 */
	clrsetbits_le32(&denali_phy[547], 0xf << 16, sw_master_mode << 16);
	clrsetbits_le32(&denali_phy[675], 0xf << 16, sw_master_mode << 16);
	clrsetbits_le32(&denali_phy[803], 0xf << 16, sw_master_mode << 16);
}

static void set_memory_map(const struct chan_info *chan, u32 channel,
			   const struct rk3399_sdram_params *sdram_params)
{
	const struct rk3399_sdram_channel *sdram_ch =
		&sdram_params->ch[channel];
	u32 *denali_ctl = chan->pctl->denali_ctl;
	u32 *denali_pi = chan->pi->denali_pi;
	u32 cs_map;
	u32 reduc;
	u32 row;

	/* Get row number from ddrconfig setting */
	if (sdram_ch->ddrconfig < 2 || sdram_ch->ddrconfig == 4)
		row = 16;
	else if (sdram_ch->ddrconfig == 3)
		row = 14;
	else
		row = 15;

	cs_map = (sdram_ch->rank > 1) ? 3 : 1;
	reduc = (sdram_ch->bw == 2) ? 0 : 1;

	/* Set the dram configuration */
	const u8 coldiff = 12 - sdram_ch->col;
	clrsetbits_le32(&denali_ctl[191], 0xf, coldiff);
	clrsetbits_le32(&denali_pi[199], 0xf, coldiff);

	clrsetbits_le32(&denali_ctl[190], (0x3 << 16) | (0x7 << 24),
			((3 - sdram_ch->bk) << 16) |
			((16 - row) << 24));

	/* Is this a 16bit wide bus? */
	if (reduc) {
		setbits_le32(&denali_ctl[196], BIT(16));
		setbits_le32(&denali_pi[58], BIT(8));
	} else {
		clrbits_le32(&denali_ctl[196], BIT(16));
		clrbits_le32(&denali_pi[58], BIT(8));
	}

	/* active chip selects */
	clrsetbits_le32(&denali_ctl[196], 0x3, cs_map);

	/* PI_155 PI_ROW_DIFF:RW:24:3 PI_BANK_DIFF:RW:16:2 */
	clrsetbits_le32(&denali_pi[155], (0x3 << 16) | (0x7 << 24),
			((3 - sdram_ch->bk) << 16) |
			((16 - row) << 24));
	/* PI_41 PI_CS_MAP:RW:24:4 */
	clrsetbits_le32(&denali_pi[41], 0xf << 24, cs_map << 24);
	if ((sdram_ch->rank == 1) && (sdram_params->base.dramtype == DDR3))
		writel(0x2EC7FFFF, &denali_pi[34]);
}

static void set_ds_odt(const struct chan_info *chan,
		       const struct rk3399_sdram_params *sdram_params)
{
	u32 *denali_phy = chan->publ->denali_phy;

	u32 tsel_idle_en, tsel_wr_en, tsel_rd_en;
	u32 tsel_idle_select_p, tsel_wr_select_p, tsel_rd_select_p;
	u32 ca_tsel_wr_select_p, ca_tsel_wr_select_n;
	u32 tsel_idle_select_n, tsel_wr_select_n, tsel_rd_select_n;
	u32 reg_value;

	if (sdram_params->base.dramtype == LPDDR4) {
		tsel_rd_select_p = PHY_DRV_ODT_Hi_Z;
		tsel_wr_select_p = PHY_DRV_ODT_40;
		ca_tsel_wr_select_p = PHY_DRV_ODT_40;
		tsel_idle_select_p = PHY_DRV_ODT_Hi_Z;

		tsel_rd_select_n = PHY_DRV_ODT_240;
		tsel_wr_select_n = PHY_DRV_ODT_40;
		ca_tsel_wr_select_n = PHY_DRV_ODT_40;
		tsel_idle_select_n = PHY_DRV_ODT_240;
	} else if (sdram_params->base.dramtype == LPDDR3) {
		tsel_rd_select_p = PHY_DRV_ODT_240;
		tsel_wr_select_p = PHY_DRV_ODT_34_3;
		ca_tsel_wr_select_p = PHY_DRV_ODT_48;
		tsel_idle_select_p = PHY_DRV_ODT_240;

		tsel_rd_select_n = PHY_DRV_ODT_Hi_Z;
		tsel_wr_select_n = PHY_DRV_ODT_34_3;
		ca_tsel_wr_select_n = PHY_DRV_ODT_48;
		tsel_idle_select_n = PHY_DRV_ODT_Hi_Z;
	} else {
		tsel_rd_select_p = PHY_DRV_ODT_240;
		tsel_wr_select_p = PHY_DRV_ODT_34_3;
		ca_tsel_wr_select_p = PHY_DRV_ODT_34_3;
		tsel_idle_select_p = PHY_DRV_ODT_240;

		tsel_rd_select_n = PHY_DRV_ODT_240;
		tsel_wr_select_n = PHY_DRV_ODT_34_3;
		ca_tsel_wr_select_n = PHY_DRV_ODT_34_3;
		tsel_idle_select_n = PHY_DRV_ODT_240;

#if 1
		tsel_rd_select_p = PHY_DRV_ODT_60;
		tsel_wr_select_p = PHY_DRV_ODT_34_3;
		ca_tsel_wr_select_p = PHY_DRV_ODT_34_3;
		tsel_idle_select_p = PHY_DRV_ODT_60;

		tsel_rd_select_n = PHY_DRV_ODT_60;
		tsel_wr_select_n = PHY_DRV_ODT_34_3;
		ca_tsel_wr_select_n = PHY_DRV_ODT_34_3;
		tsel_idle_select_n = PHY_DRV_ODT_60;

#endif
	}

	if (sdram_params->base.odt == 1)
		tsel_rd_en = 1;
	else
		tsel_rd_en = 0;

	tsel_wr_en = 0;
	tsel_idle_en = 0;

	/*
	 * phy_dq_tsel_select_X 24bits DENALI_PHY_6/134/262/390 offset_0
	 * sets termination values for read/idle cycles and drive strength
	 * for write cycles for DQ/DM
	 */
	reg_value = tsel_rd_select_n | (tsel_rd_select_p << 0x4) |
		    (tsel_wr_select_n << 8) | (tsel_wr_select_p << 12) |
		    (tsel_idle_select_n << 16) | (tsel_idle_select_p << 20);
	clrsetbits_le32(&denali_phy[6], 0xffffff, reg_value);
	clrsetbits_le32(&denali_phy[134], 0xffffff, reg_value);
	clrsetbits_le32(&denali_phy[262], 0xffffff, reg_value);
	clrsetbits_le32(&denali_phy[390], 0xffffff, reg_value);

	/*
	 * phy_dqs_tsel_select_X 24bits DENALI_PHY_7/135/263/391 offset_0
	 * sets termination values for read/idle cycles and drive strength
	 * for write cycles for DQS
	 */
	clrsetbits_le32(&denali_phy[7], 0xffffff, reg_value);
	clrsetbits_le32(&denali_phy[135], 0xffffff, reg_value);
	clrsetbits_le32(&denali_phy[263], 0xffffff, reg_value);
	clrsetbits_le32(&denali_phy[391], 0xffffff, reg_value);

	/* phy_adr_tsel_select_ 8bits DENALI_PHY_544/672/800 offset_0 */
	reg_value = ca_tsel_wr_select_n | (ca_tsel_wr_select_p << 0x4);
	clrsetbits_le32(&denali_phy[544], 0xff, reg_value);
	clrsetbits_le32(&denali_phy[672], 0xff, reg_value);
	clrsetbits_le32(&denali_phy[800], 0xff, reg_value);

	/* phy_pad_addr_drive 8bits DENALI_PHY_928 offset_0 */
	clrsetbits_le32(&denali_phy[928], 0xff, reg_value);

	/* phy_pad_rst_drive 8bits DENALI_PHY_937 offset_0 */
	clrsetbits_le32(&denali_phy[937], 0xff, reg_value);

	/* phy_pad_cke_drive 8bits DENALI_PHY_935 offset_0 */
	clrsetbits_le32(&denali_phy[935], 0xff, reg_value);

	/* phy_pad_cs_drive 8bits DENALI_PHY_939 offset_0 */
	clrsetbits_le32(&denali_phy[939], 0xff, reg_value);

	/* phy_pad_clk_drive 8bits DENALI_PHY_929 offset_0 */
	clrsetbits_le32(&denali_phy[929], 0xff, reg_value);

	/* phy_pad_fdbk_drive 23bit DENALI_PHY_924/925 */
	clrsetbits_le32(&denali_phy[924], 0xff,
			tsel_wr_select_n | (tsel_wr_select_p << 4));
	clrsetbits_le32(&denali_phy[925], 0xff,
			tsel_rd_select_n | (tsel_rd_select_p << 4));

	/* phy_dq_tsel_enable_X 3bits DENALI_PHY_5/133/261/389 offset_16 */
	reg_value = (tsel_rd_en | (tsel_wr_en << 1) | (tsel_idle_en << 2))
		<< 16;
	clrsetbits_le32(&denali_phy[5], 0x7 << 16, reg_value);
	clrsetbits_le32(&denali_phy[133], 0x7 << 16, reg_value);
	clrsetbits_le32(&denali_phy[261], 0x7 << 16, reg_value);
	clrsetbits_le32(&denali_phy[389], 0x7 << 16, reg_value);

	/* phy_dqs_tsel_enable_X 3bits DENALI_PHY_6/134/262/390 offset_24 */
	reg_value = (tsel_rd_en | (tsel_wr_en << 1) | (tsel_idle_en << 2))
		<< 24;
	clrsetbits_le32(&denali_phy[6], 0x7 << 24, reg_value);
	clrsetbits_le32(&denali_phy[134], 0x7 << 24, reg_value);
	clrsetbits_le32(&denali_phy[262], 0x7 << 24, reg_value);
	clrsetbits_le32(&denali_phy[390], 0x7 << 24, reg_value);

	/* phy_adr_tsel_enable_ 1bit DENALI_PHY_518/646/774 offset_8 */
	reg_value = tsel_wr_en << 8;
	clrsetbits_le32(&denali_phy[518], 0x1 << 8, reg_value);
	clrsetbits_le32(&denali_phy[646], 0x1 << 8, reg_value);
	clrsetbits_le32(&denali_phy[774], 0x1 << 8, reg_value);

	/* phy_pad_addr_term tsel 1bit DENALI_PHY_933 offset_17 */
	reg_value = tsel_wr_en << 17;
	clrsetbits_le32(&denali_phy[933], 0x1 << 17, reg_value);
	/*
	 * pad_rst/cke/cs/clk_term tsel 1bits
	 * DENALI_PHY_938/936/940/934 offset_17
	 */
	clrsetbits_le32(&denali_phy[938], 0x1 << 17, reg_value);
	clrsetbits_le32(&denali_phy[936], 0x1 << 17, reg_value);
	clrsetbits_le32(&denali_phy[940], 0x1 << 17, reg_value);
	clrsetbits_le32(&denali_phy[934], 0x1 << 17, reg_value);

	/* phy_pad_fdbk_term 1bit DENALI_PHY_930 offset_17 */
	clrsetbits_le32(&denali_phy[930], 0x1 << 17, reg_value);
}

static int phy_io_config(const struct chan_info *chan,
			  const struct rk3399_sdram_params *sdram_params)
{
	u32 *denali_phy = chan->publ->denali_phy;
	u32 vref_mode_dq, vref_value_dq, vref_mode_ac, vref_value_ac;
	u32 mode_sel;
	u32 reg_value;
	u32 drv_value, odt_value;
	u32 speed;

	/* vref setting */
	if (sdram_params->base.dramtype == LPDDR4) {
		/* LPDDR4 */
		vref_mode_dq = 0x6;
		vref_value_dq = 0x1f;
		vref_mode_ac = 0x6;
		vref_value_ac = 0x1f;
	} else if (sdram_params->base.dramtype == LPDDR3) {
		if (sdram_params->base.odt == 1) {
			vref_mode_dq = 0x5;  /* LPDDR3 ODT */
			drv_value = (readl(&denali_phy[6]) >> 12) & 0xf;
			odt_value = (readl(&denali_phy[6]) >> 4) & 0xf;
			if (drv_value == PHY_DRV_ODT_48) {
				switch (odt_value) {
				case PHY_DRV_ODT_240:
					vref_value_dq = 0x16;
					break;
				case PHY_DRV_ODT_120:
					vref_value_dq = 0x26;
					break;
				case PHY_DRV_ODT_60:
					vref_value_dq = 0x36;
					break;
				default:
					debug("Invalid ODT value.\n");
					return -EINVAL;
				}
			} else if (drv_value == PHY_DRV_ODT_40) {
				switch (odt_value) {
				case PHY_DRV_ODT_240:
					vref_value_dq = 0x19;
					break;
				case PHY_DRV_ODT_120:
					vref_value_dq = 0x23;
					break;
				case PHY_DRV_ODT_60:
					vref_value_dq = 0x31;
					break;
				default:
					debug("Invalid ODT value.\n");
					return -EINVAL;
				}
			} else if (drv_value == PHY_DRV_ODT_34_3) {
				switch (odt_value) {
				case PHY_DRV_ODT_240:
					vref_value_dq = 0x17;
					break;
				case PHY_DRV_ODT_120:
					vref_value_dq = 0x20;
					break;
				case PHY_DRV_ODT_60:
					vref_value_dq = 0x2e;
					break;
				default:
					debug("Invalid ODT value.\n");
					return -EINVAL;
				}
			} else {
				debug("Invalid DRV value.\n");
				return -EINVAL;
			}
		} else {
			vref_mode_dq = 0x2;  /* LPDDR3 */
			vref_value_dq = 0x1f;
		}
		vref_mode_ac = 0x2;
		vref_value_ac = 0x1f;
	} else if (sdram_params->base.dramtype == DDR3) {
		/* DDR3L */
		vref_mode_dq = 0x1;
		vref_value_dq = 0x1f;
		vref_mode_ac = 0x1;
		vref_value_ac = 0x1f;
	} else {
		debug("Unknown DRAM type.\n");
		return -EINVAL;
	}

	reg_value = (vref_mode_dq << 9) | (0x1 << 8) | vref_value_dq;

	/* PHY_913 PHY_PAD_VREF_CTRL_DQ_0 12bits offset_8 */
	clrsetbits_le32(&denali_phy[913], 0xfff << 8, reg_value << 8);
	/* PHY_914 PHY_PAD_VREF_CTRL_DQ_1 12bits offset_0 */
	clrsetbits_le32(&denali_phy[914], 0xfff, reg_value);
	/* PHY_914 PHY_PAD_VREF_CTRL_DQ_2 12bits offset_16 */
	clrsetbits_le32(&denali_phy[914], 0xfff << 16, reg_value << 16);
	/* PHY_915 PHY_PAD_VREF_CTRL_DQ_3 12bits offset_0 */
	clrsetbits_le32(&denali_phy[915], 0xfff, reg_value);

	reg_value = (vref_mode_ac << 9) | (0x1 << 8) | vref_value_ac;

	/* PHY_915 PHY_PAD_VREF_CTRL_AC 12bits offset_16 */
	clrsetbits_le32(&denali_phy[915], 0xfff << 16, reg_value << 16);

	if (sdram_params->base.dramtype == LPDDR4)
		mode_sel = 0x6;
	else if (sdram_params->base.dramtype == LPDDR3)
		mode_sel = 0x0;
	else if (sdram_params->base.dramtype == DDR3)
		mode_sel = 0x1;
	else
		return -EINVAL;

	/* PHY_924 PHY_PAD_FDBK_DRIVE */
	clrsetbits_le32(&denali_phy[924], 0x7 << 15, mode_sel << 15);
	/* PHY_926 PHY_PAD_DATA_DRIVE */
	clrsetbits_le32(&denali_phy[926], 0x7 << 6, mode_sel << 6);
	/* PHY_927 PHY_PAD_DQS_DRIVE */
	clrsetbits_le32(&denali_phy[927], 0x7 << 6, mode_sel << 6);
	/* PHY_928 PHY_PAD_ADDR_DRIVE */
	clrsetbits_le32(&denali_phy[928], 0x7 << 14, mode_sel << 14);
	/* PHY_929 PHY_PAD_CLK_DRIVE */
	clrsetbits_le32(&denali_phy[929], 0x7 << 14, mode_sel << 14);
	/* PHY_935 PHY_PAD_CKE_DRIVE */
	clrsetbits_le32(&denali_phy[935], 0x7 << 14, mode_sel << 14);
	/* PHY_937 PHY_PAD_RST_DRIVE */
	clrsetbits_le32(&denali_phy[937], 0x7 << 14, mode_sel << 14);
	/* PHY_939 PHY_PAD_CS_DRIVE */
	clrsetbits_le32(&denali_phy[939], 0x7 << 14, mode_sel << 14);


	/* speed setting */
	if (sdram_params->base.ddr_freq < 400)
		speed = 0x0;
	else if (sdram_params->base.ddr_freq < 800)
		speed = 0x1;
	else if (sdram_params->base.ddr_freq < 1200)
		speed = 0x2;
	else
		speed = 0x3;

	/* PHY_924 PHY_PAD_FDBK_DRIVE */
	clrsetbits_le32(&denali_phy[924], 0x3 << 21, speed << 21);
	/* PHY_926 PHY_PAD_DATA_DRIVE */
	clrsetbits_le32(&denali_phy[926], 0x3 << 9, speed << 9);
	/* PHY_927 PHY_PAD_DQS_DRIVE */
	clrsetbits_le32(&denali_phy[927], 0x3 << 9, speed << 9);
	/* PHY_928 PHY_PAD_ADDR_DRIVE */
	clrsetbits_le32(&denali_phy[928], 0x3 << 17, speed << 17);
	/* PHY_929 PHY_PAD_CLK_DRIVE */
	clrsetbits_le32(&denali_phy[929], 0x3 << 17, speed << 17);
	/* PHY_935 PHY_PAD_CKE_DRIVE */
	clrsetbits_le32(&denali_phy[935], 0x3 << 17, speed << 17);
	/* PHY_937 PHY_PAD_RST_DRIVE */
	clrsetbits_le32(&denali_phy[937], 0x3 << 17, speed << 17);
	/* PHY_939 PHY_PAD_CS_DRIVE */
	clrsetbits_le32(&denali_phy[939], 0x3 << 17, speed << 17);

	return 0;
}

static uint32_t get_pi_rdlat_adj(struct dram_timing_t *pdram_timing)
{
	/*[DLLSUBTYPE2] == "STD_DENALI_HS" */
	uint32_t rdlat, delay_adder, ie_enable, hs_offset, tsel_adder,
	    extra_adder, tsel_enable;

	ie_enable = PI_IE_ENABLE_VALUE;
	tsel_enable = PI_TSEL_ENABLE_VALUE;

	rdlat = pdram_timing->cl + PI_ADD_LATENCY;
	delay_adder = ie_enable / (1000000 / pdram_timing->mhz);
	if ((ie_enable % (1000000 / pdram_timing->mhz)) != 0)
		delay_adder++;
	hs_offset = 0;
	tsel_adder = 0;
	extra_adder = 0;
	/* rdlat = rdlat - (PREAMBLE_SUPPORT & 0x1); */
	tsel_adder = tsel_enable / (1000000 / pdram_timing->mhz);
	if ((tsel_enable % (1000000 / pdram_timing->mhz)) != 0)
		tsel_adder++;
	delay_adder = delay_adder - 1;
	if (tsel_adder > delay_adder)
		extra_adder = tsel_adder - delay_adder;
	else
		extra_adder = 0;
	if (PI_REGS_DIMM_SUPPORT && PI_DOUBLEFREEK)
		hs_offset = 2;
	else
		hs_offset = 1;

	if (delay_adder > (rdlat - 1 - hs_offset)) {
		rdlat = rdlat - tsel_adder;
	} else {
		if ((rdlat - delay_adder) < 2)
			rdlat = 2;
		else
			rdlat = rdlat - delay_adder - extra_adder;
	}

	return rdlat;
}

static uint32_t get_pi_wrlat(struct dram_timing_t *pdram_timing,
			     struct timing_related_config *timing_config)
{
	uint32_t tmp;

	if (timing_config->dram_type == LPDDR3) {
		tmp = pdram_timing->cl;
		if (tmp >= 14)
			tmp = 8;
		else if (tmp >= 10)
			tmp = 6;
		else if (tmp == 9)
			tmp = 5;
		else if (tmp == 8)
			tmp = 4;
		else if (tmp == 6)
			tmp = 3;
		else
			tmp = 1;
	} else {
		tmp = 1;
	}

	return tmp;
}

static uint32_t get_pi_wrlat_adj(struct dram_timing_t *pdram_timing,
				 struct timing_related_config *timing_config)
{
	return get_pi_wrlat(pdram_timing, timing_config) + PI_ADD_LATENCY - 1;
}

static uint32_t get_pi_tdfi_phy_rdlat(struct dram_timing_t *pdram_timing,
			struct timing_related_config *timing_config)
{
	/* [DLLSUBTYPE2] == "STD_DENALI_HS" */
	uint32_t cas_lat, delay_adder, ie_enable, hs_offset, ie_delay_adder;
	uint32_t mem_delay_ps, round_trip_ps;
	uint32_t phy_internal_delay, lpddr_adder, dfi_adder, rdlat_delay;

	ie_enable = PI_IE_ENABLE_VALUE;

	delay_adder = ie_enable / (1000000 / pdram_timing->mhz);
	if ((ie_enable % (1000000 / pdram_timing->mhz)) != 0)
		delay_adder++;
	delay_adder = delay_adder - 1;
	if (PI_REGS_DIMM_SUPPORT && PI_DOUBLEFREEK)
		hs_offset = 2;
	else
		hs_offset = 1;

	cas_lat = pdram_timing->cl + PI_ADD_LATENCY;

	if (delay_adder > (cas_lat - 1 - hs_offset)) {
		ie_delay_adder = 0;
	} else {
		ie_delay_adder = ie_enable / (1000000 / pdram_timing->mhz);
		if ((ie_enable % (1000000 / pdram_timing->mhz)) != 0)
			ie_delay_adder++;
	}

	if (timing_config->dram_type == DDR3) {
		mem_delay_ps = 0;
	} else if (timing_config->dram_type == LPDDR4) {
		mem_delay_ps = 3600;
	} else if (timing_config->dram_type == LPDDR3) {
		mem_delay_ps = 5500;
	} else {
		printf("get_pi_tdfi_phy_rdlat:dramtype unsupport\n");
		return 0;
	}
	round_trip_ps = 1100 + 500 + mem_delay_ps + 500 + 600;
	delay_adder = round_trip_ps / (1000000 / pdram_timing->mhz);
	if ((round_trip_ps % (1000000 / pdram_timing->mhz)) != 0)
		delay_adder++;

	phy_internal_delay = 5 + 2 + 4;
	lpddr_adder = mem_delay_ps / (1000000 / pdram_timing->mhz);
	if ((mem_delay_ps % (1000000 / pdram_timing->mhz)) != 0)
		lpddr_adder++;
	dfi_adder = 0;
	phy_internal_delay = phy_internal_delay + 2;
	rdlat_delay = delay_adder + phy_internal_delay +
	    ie_delay_adder + lpddr_adder + dfi_adder;

	rdlat_delay = rdlat_delay + 2;
	return rdlat_delay;
}

static uint32_t get_pi_todtoff_min(struct dram_timing_t *pdram_timing,
				   struct timing_related_config *timing_config)
{
	uint32_t tmp, todtoff_min_ps;

	if (timing_config->dram_type == LPDDR3)
		todtoff_min_ps = 2500;
	else if (timing_config->dram_type == LPDDR4)
		todtoff_min_ps = 1500;
	else
		todtoff_min_ps = 0;
	/* todtoff_min */
	tmp = todtoff_min_ps / (1000000 / pdram_timing->mhz);
	if ((todtoff_min_ps % (1000000 / pdram_timing->mhz)) != 0)
		tmp++;
	return tmp;
}

static uint32_t get_pi_todtoff_max(struct dram_timing_t *pdram_timing,
				   struct timing_related_config *timing_config)
{
	uint32_t tmp, todtoff_max_ps;

	if ((timing_config->dram_type == LPDDR4)
	    || (timing_config->dram_type == LPDDR3))
		todtoff_max_ps = 3500;
	else
		todtoff_max_ps = 0;

	/* todtoff_max */
	tmp = todtoff_max_ps / (1000000 / pdram_timing->mhz);
	if ((todtoff_max_ps % (1000000 / pdram_timing->mhz)) != 0)
		tmp++;
	return tmp;
}

struct lat_adj_pair {
	uint32_t cl;
	uint32_t rdlat_adj;
	uint32_t cwl;
	uint32_t wrlat_adj;
};

const struct lat_adj_pair ddr3_lat_adj[] = {
	{6, 5, 5, 4},
	{8, 7, 6, 5},
	{10, 9, 7, 6},
	{11, 9, 8, 7},
	{13, 0xb, 9, 8},
	{14, 0xb, 0xa, 9}
};

static uint32_t get_rdlat_adj(uint32_t dram_type, uint32_t cl)
{
	const struct lat_adj_pair *p;
	uint32_t cnt;
	uint32_t i;

	if (dram_type == DDR3) {
		p = ddr3_lat_adj;
		cnt = ARRAY_SIZE(ddr3_lat_adj);
	}
#if 0 // TODO
	else if (dram_type == LPDDR3) {
		p = lpddr3_lat_adj;
		cnt = ARRAY_SIZE(lpddr3_lat_adj);
	} else {
		p = lpddr4_lat_adj;
		cnt = ARRAY_SIZE(lpddr4_lat_adj);
	}
#endif

	for (i = 0; i < cnt; i++) {
		if (cl == p[i].cl)
			return p[i].rdlat_adj;
	}
	/* fail */
	return 0xff;
}

static uint32_t get_wrlat_adj(uint32_t dram_type, uint32_t cwl)
{
	const struct lat_adj_pair *p;
	uint32_t cnt;
	uint32_t i;

	if (dram_type == DDR3) {
		p = ddr3_lat_adj;
		cnt = ARRAY_SIZE(ddr3_lat_adj);
	}
#if 0
	else if (dram_type == LPDDR3) {
		p = lpddr3_lat_adj;
		cnt = ARRAY_SIZE(lpddr3_lat_adj);
	} else {
		p = lpddr4_lat_adj;
		cnt = ARRAY_SIZE(lpddr4_lat_adj);
	}
#endif

	for (i = 0; i < cnt; i++) {
		if (cwl == p[i].cwl)
			return p[i].wrlat_adj;
	}
	/* fail */
	return 0xff;
}

static void gen_rk3399_ctl_params_f0(const struct chan_info *chan,
				     struct timing_related_config *timing_config,
				     struct dram_timing_t *pdram_timing)
{
	u32 *denali_ctl = chan->pctl->denali_ctl;
	//	u32 *denali_pi = chan->pi->denali_pi;
	//	u32 *denali_phy = chan->publ->denali_phy;
	uint32_t i;
	uint32_t tmp, tmp1;

	for (i = 0; i < timing_config->ch_cnt; i++) {
		if (timing_config->dram_type == DDR3) {
			tmp = (700000 * timing_config->freq + 999) / 1000;
			tmp += pdram_timing->txsnr + (pdram_timing->tmrd * 3) +
			    pdram_timing->tmod + pdram_timing->tzqinit;
			writel(tmp, &denali_ctl[5]);

			clrsetbits_le32(&denali_ctl[22], 0xffff, pdram_timing->tdllk);

			tmp = (pdram_timing->tmod << 8) | pdram_timing->tmrd;
			writel(tmp, &denali_ctl[32]);

			clrsetbits_le32(&denali_ctl[59], 0xffff << 16,
					(pdram_timing->txsr - pdram_timing->trcd) << 16);
#if 0
		} else if (timing_config->dram_type == LPDDR4) {
			mmio_write_32(CTL_REG(i, 5), pdram_timing->tinit1 +
						     pdram_timing->tinit3);
			mmio_write_32(CTL_REG(i, 32),
				      (pdram_timing->tmrd << 8) |
				      pdram_timing->tmrd);
			mmio_clrsetbits_32(CTL_REG(i, 59), 0xffff << 16,
					   pdram_timing->txsr << 16);
		} else {
			mmio_write_32(CTL_REG(i, 5), pdram_timing->tinit1);
			mmio_write_32(CTL_REG(i, 7), pdram_timing->tinit4);
			mmio_write_32(CTL_REG(i, 32),
				      (pdram_timing->tmrd << 8) |
				      pdram_timing->tmrd);
			mmio_clrsetbits_32(CTL_REG(i, 59), 0xffff << 16,
					   pdram_timing->txsr << 16);
#endif
		}
		writel(pdram_timing->tinit3, &denali_ctl[6]);
		writel(pdram_timing->tinit5, &denali_ctl[8]);
		clrsetbits_le32(&denali_ctl[23], (0x7f << 16),
				((pdram_timing->cl * 2) << 16));
		clrsetbits_le32(&denali_ctl[23], (0x1f << 24),
				(pdram_timing->cwl << 24));
		clrsetbits_le32(&denali_ctl[24], 0x3f, pdram_timing->al);
		clrsetbits_le32(&denali_ctl[26], 0xffff << 16,
				(pdram_timing->trc << 24) |
				(pdram_timing->trrd << 16));
		writel(((pdram_timing->tfaw << 24) |
			(pdram_timing->trppb << 16) |
			(pdram_timing->twtr << 8) |
			pdram_timing->tras_min),
		       &denali_ctl[27]);

		clrsetbits_le32(&denali_ctl[31], 0xff << 24,
				max(4, pdram_timing->trtp) << 24);
		writel(((pdram_timing->tcke << 24) |
			pdram_timing->tras_max),
		       &denali_ctl[33]);

		clrsetbits_le32(&denali_ctl[34], 0xff,
				   max(1, pdram_timing->tckesr));
		clrsetbits_le32(&denali_ctl[39],
				(0x3f << 16) | (0xff << 8),
				(pdram_timing->twr << 16) |
				(pdram_timing->trcd << 8));
		clrsetbits_le32(&denali_ctl[42], 0x1f << 16,
			      pdram_timing->tmrz << 16);
		tmp = pdram_timing->tdal ? pdram_timing->tdal :
		      (pdram_timing->twr + pdram_timing->trp);
		clrsetbits_le32(&denali_ctl[44], 0xff, tmp);
		clrsetbits_le32(&denali_ctl[45], 0xff, pdram_timing->trp);
		writel(((pdram_timing->trefi - 8) << 16) | pdram_timing->trfc,
		       &denali_ctl[48]);
		clrsetbits_le32(&denali_ctl[52], 0xffff, pdram_timing->txp);
		clrsetbits_le32(&denali_ctl[53], 0xffff << 16,
				pdram_timing->txpdll << 16);
		clrsetbits_le32(&denali_ctl[55], 0xf << 24,
				pdram_timing->tcscke << 24);
		clrsetbits_le32(&denali_ctl[55], 0xff,
				pdram_timing->tmrri);
		writel((pdram_timing->tzqcke << 24) |
		       (pdram_timing->tmrwckel << 16) |
		       (pdram_timing->tckehcs << 8) |
		       pdram_timing->tckelcs,
		       &denali_ctl[56]);
		clrsetbits_le32(&denali_ctl[60], 0xffff, pdram_timing->txsnr);
		clrsetbits_le32(&denali_ctl[62], 0xffff << 16,
				   (pdram_timing->tckehcmd << 24) |
				   (pdram_timing->tckelcmd << 16));
		writel((pdram_timing->tckelpd << 24) |
		       (pdram_timing->tescke << 16) |
		       (pdram_timing->tsr << 8) |
		       pdram_timing->tckckel,
		       &denali_ctl[63]);

		clrsetbits_le32(&denali_ctl[64], 0xfff,
				(pdram_timing->tcmdcke << 8) |
				pdram_timing->tcsckeh);
		clrsetbits_le32(&denali_ctl[92], 0xffff << 8,
				(pdram_timing->tcksrx << 16) |
				(pdram_timing->tcksre << 8));
		clrsetbits_le32(&denali_ctl[108], 0x1 << 24,
				(timing_config->dllbp << 24));
		clrsetbits_le32(&denali_ctl[122], 0x3ff << 16,
				(pdram_timing->tvrcg_enable << 16));
		writel((pdram_timing->tfc_long << 16) |
		       pdram_timing->tvrcg_disable,
		       &denali_ctl[123]);
		writel((pdram_timing->tvref_long << 16) |
		       (pdram_timing->tckfspx << 8) |
		       pdram_timing->tckfspe,
		       &denali_ctl[124]);

		writel((pdram_timing->mr[1] << 16) |
		       pdram_timing->mr[0], &denali_ctl[133]);
		clrsetbits_le32(&denali_ctl[134], 0xffff,
				pdram_timing->mr[2]);
		clrsetbits_le32(&denali_ctl[138], 0xffff,
				pdram_timing->mr[3]);
		clrsetbits_le32(&denali_ctl[139], 0xff << 24,
				pdram_timing->mr11 << 24);
		writel((pdram_timing->mr[1] << 16) |
		       pdram_timing->mr[0],
		       &denali_ctl[147]);
		clrsetbits_le32(&denali_ctl[148], 0xffff,
				pdram_timing->mr[2]);
		clrsetbits_le32(&denali_ctl[152], 0xffff,
				pdram_timing->mr[3]);
		clrsetbits_le32(&denali_ctl[153], 0xff << 24,
				pdram_timing->mr11 << 24);
		/*
		if (timing_config->dram_type == LPDDR4) {
			mmio_clrsetbits_32(CTL_REG(i, 140), 0xffff << 16,
					   pdram_timing->mr12 << 16);
			mmio_clrsetbits_32(CTL_REG(i, 142), 0xffff << 16,
					   pdram_timing->mr14 << 16);
			mmio_clrsetbits_32(CTL_REG(i, 145), 0xffff << 16,
					   pdram_timing->mr22 << 16);
			mmio_clrsetbits_32(CTL_REG(i, 154), 0xffff << 16,
					   pdram_timing->mr12 << 16);
			mmio_clrsetbits_32(CTL_REG(i, 156), 0xffff << 16,
					   pdram_timing->mr14 << 16);
			mmio_clrsetbits_32(CTL_REG(i, 159), 0xffff << 16,
					   pdram_timing->mr22 << 16);
		}
		*/

		clrsetbits_32(&denali_ctl[153], 0xfff << 8,
			      pdram_timing->tzqinit << 8);
		writel((pdram_timing->tzqcs << 16) | (pdram_timing->tzqinit / 2),
		       &denali_ctl[180]);
		writel((pdram_timing->tzqlat << 16) | pdram_timing->tzqcal,
		       &denali_ctl[181]);
		clrsetbits_32(&denali_ctl[212], 0xff << 8,
			      pdram_timing->todton << 8);

		if (timing_config->odt) {
			setbits_le32(&denali_ctl[213], BIT(16));
			if (timing_config->freq < 400)
				tmp = 4;
			else
				tmp = 8;
		} else {
			clrbits_32(&denali_ctl[213], BIT(16));
			tmp = 2;
		}

		/* Additional delay between reads to different chip selects */
		clrsetbits_32(&denali_ctl[216], 0x1f << 24, tmp << 24);

		clrsetbits_32(&denali_ctl[221], (0x3 << 16) | (0xf << 8),
			      (pdram_timing->tdqsck << 16) |
			      (pdram_timing->tdqsck_max << 8));

		tmp =
		    (get_wrlat_adj(timing_config->dram_type, pdram_timing->cwl)
		     << 8) | get_rdlat_adj(timing_config->dram_type,
					   pdram_timing->cl);

		clrsetbits_le32(&denali_ctl[284], 0xffff, tmp);

		clrsetbits_le32(&denali_ctl[82], 0xffff << 16,
				(4 * pdram_timing->trefi) << 16);

		clrsetbits_le32(&denali_ctl[83], 0xffff,
				(2 * pdram_timing->trefi) & 0xffff);

		if ((timing_config->dram_type == LPDDR3) ||
		    (timing_config->dram_type == LPDDR4)) {
			tmp = get_pi_wrlat(pdram_timing, timing_config);
			tmp1 = get_pi_todtoff_max(pdram_timing, timing_config);
			tmp = (tmp > tmp1) ? (tmp - tmp1) : 0;
		} else {
			tmp = 0;
		}
		clrsetbits_le32(&denali_ctl[214], 0x3f << 16,
				   (tmp & 0x3f) << 16);

		if ((timing_config->dram_type == LPDDR3) ||
		    (timing_config->dram_type == LPDDR4)) {
			/* min_rl_preamble = cl+TDQSCK_MIN -1 */
			tmp = pdram_timing->cl +
			    get_pi_todtoff_min(pdram_timing, timing_config) - 1;
			/* todtoff_max */
			tmp1 = get_pi_todtoff_max(pdram_timing, timing_config);
			tmp = (tmp > tmp1) ? (tmp - tmp1) : 0;
		} else {
			tmp = pdram_timing->cl - pdram_timing->cwl;
		}
		clrsetbits_le32(&denali_ctl[215], 0x3f << 8,
				   (tmp & 0x3f) << 8);

		clrsetbits_le32(&denali_ctl[275], 0xff << 16,
				   (get_pi_tdfi_phy_rdlat(pdram_timing,
							  timing_config) &
				    0xff) << 16);

		clrsetbits_le32(&denali_ctl[277], 0xffff,
				   (2 * pdram_timing->trefi) & 0xffff);

		clrsetbits_le32(&denali_ctl[282], 0xffff,
				   (2 * pdram_timing->trefi) & 0xffff);

		writel(20 * pdram_timing->trefi,
		       &denali_ctl[283]);

		/* CTL_308 TDFI_CALVL_CAPTURE_F0:RW:16:10 */
		tmp1 = 20000 / (1000000 / pdram_timing->mhz) + 1;
		if ((20000 % (1000000 / pdram_timing->mhz)) != 0)
			tmp1++;
		tmp = (tmp1 >> 1) + (tmp1 % 2) + 5;
		clrsetbits_le32(&denali_ctl[308], 0x3ff << 16, tmp << 16);

		/* CTL_308 TDFI_CALVL_CC_F0:RW:0:10 */
		tmp = tmp + 18;
		clrsetbits_le32(&denali_ctl[308], 0x3ff, tmp);

		/* CTL_314 TDFI_WRCSLAT_F0:RW:8:8 */
		tmp1 = get_pi_wrlat_adj(pdram_timing, timing_config);
		if (timing_config->freq <= TDFI_LAT_THRESHOLD_FREQ) {
			if (tmp1 == 0)
				tmp = 0;
			else if (tmp1 < 5)
				tmp = tmp1 - 1;
			else
				tmp = tmp1 - 5;
		} else {
			tmp = tmp1 - 2;
		}
		clrsetbits_le32(&denali_ctl[314], 0xff << 8, tmp << 8);

		/* CTL_314 TDFI_RDCSLAT_F0:RW:0:8 */
		if ((timing_config->freq <= TDFI_LAT_THRESHOLD_FREQ) &&
		    (pdram_timing->cl >= 5))
			tmp = pdram_timing->cl - 5;
		else
			tmp = pdram_timing->cl - 2;
		clrsetbits_le32(&denali_ctl[314], 0xff, tmp);
	}
}

static void gen_rk3399_pi_params_f0(const struct chan_info *chan,
				    struct timing_related_config *timing_config,
				    struct dram_timing_t *pdram_timing)
{
	u32 *denali_pi = chan->pi->denali_pi;
	u32 tmp;

	/* PI_02 PI_TDFI_PHYMSTR_MAX_F0:RW:0:32 */
	tmp = 4 * pdram_timing->trefi;
	writel(tmp, &denali_pi[2]);
	/* PI_03 PI_TDFI_PHYMSTR_RESP_F0:RW:0:16 */
	tmp = 2 * pdram_timing->trefi;
	clrsetbits_le32(&denali_pi[3], 0xffff, tmp);
	/* PI_07 PI_TDFI_PHYUPD_RESP_F0:RW:16:16 */
	clrsetbits_le32(&denali_pi[7], 0xffff << 16, tmp << 16);

	tmp = 0;
	tmp += (pdram_timing->bl / 2) + 4 +
		(get_pi_rdlat_adj(pdram_timing) - 2) +
		get_pi_tdfi_phy_rdlat(pdram_timing, timing_config);
	clrsetbits_le32(&denali_pi[42], 0xff, tmp);
	clrsetbits_le32(&denali_pi[43], 0x7f << 16, (pdram_timing->cl * 2) << 16);

	/* PI_46 PI_TREF_F0:RW:16:16 */
	clrsetbits_le32(&denali_pi[46], 0xffff << 16,
			pdram_timing->trefi << 16);
	/* PI_46 PI_TRFC_F0:RW:0:10 */
	clrsetbits_le32(&denali_pi[46], 0x3ff, pdram_timing->trfc);

	/* PI_72 PI_WR_TO_ODTH_F0:RW:16:6 */
#if 0
	if ((timing_config->dram_type == LPDDR3) ||
	    (timing_config->dram_type == LPDDR4)) {
		tmp1 = get_pi_wrlat(pdram_timing, timing_config);
		tmp2 = get_pi_todtoff_max(pdram_timing, timing_config);
		if (tmp1 > tmp2)
			tmp = tmp1 - tmp2;
		else
			tmp = 0;
	} else if (timing_config->dram_type == DDR3) {
		tmp = 0;
	}
#endif
	tmp = 0;
	clrsetbits_le32(&denali_pi[72], 0x3f << 16, tmp << 16);
	/* PI_73 PI_RD_TO_ODTH_F0:RW:8:6 */
#if 0
	if ((timing_config->dram_type == LPDDR3) ||
	    (timing_config->dram_type == LPDDR4)) {
		/* min_rl_preamble = cl + TDQSCK_MIN - 1 */
		tmp1 = pdram_timing->cl;
		tmp1 += get_pi_todtoff_min(pdram_timing, timing_config);
		tmp1--;
		/* todtoff_max */
		tmp2 = get_pi_todtoff_max(pdram_timing, timing_config);
		if (tmp1 > tmp2)
			tmp = tmp1 - tmp2;
		else
			tmp = 0;
	} else if (timing_config->dram_type == DDR3) {
		tmp = pdram_timing->cl - pdram_timing->cwl;
	}
#endif
	tmp = pdram_timing->cl - pdram_timing->cwl;
	clrsetbits_le32(&denali_pi[73], 0x3f << 8, tmp << 8);
	/* PI_89 PI_RDLAT_ADJ_F0:RW:16:8 */
	tmp = get_pi_rdlat_adj(pdram_timing);
	clrsetbits_le32(&denali_pi[89], 0xff << 16, tmp << 16);
	/* PI_90 PI_WRLAT_ADJ_F0:RW:16:8 */
	tmp = get_pi_wrlat_adj(pdram_timing, timing_config);
	clrsetbits_le32(&denali_pi[90], 0xff << 16, tmp << 16);
	/* PI_125 PI_MR1_DATA_F0_0:RW+:8:16 */
	clrsetbits_le32(&denali_pi[125], 0xffff << 8,
			pdram_timing->mr[1] << 8);
	/* PI_133 PI_MR1_DATA_F0_1:RW+:0:16 */
	clrsetbits_le32(&denali_pi[133], 0xffff, pdram_timing->mr[1]);
#if 0 // LPDDR4 only
	/* PI_140 PI_MR1_DATA_F0_2:RW+:16:16 */
	clrsetbits_le32(&denali_pi[140], 0xffff << 16,
			pdram_timing->mr[1] << 16);
	/* PI_148 PI_MR1_DATA_F0_3:RW+:0:16 */
	clrsetbits_le32(&denali_pi[148], 0xffff, pdram_timing->mr[1]);
#endif
	/* PI_126 PI_MR2_DATA_F0_0:RW+:0:16 */
	clrsetbits_le32(&denali_pi[126], 0xffff, pdram_timing->mr[2]);
	/* PI_133 PI_MR2_DATA_F0_1:RW+:16:16 */
	clrsetbits_le32(&denali_pi[133], 0xffff << 16,
			pdram_timing->mr[2] << 16);
#if 0 // LPDDR4
	/* PI_141 PI_MR2_DATA_F0_2:RW+:0:16 */
	clrsetbits_le32(&denali_pi[141], 0xffff, pdram_timing->mr[2]);
	/* PI_148 PI_MR2_DATA_F0_3:RW+:16:16 */
	clrsetbits_le32(&denali_pi[148], 0xffff << 16,
			pdram_timing->mr[2] << 16);
#endif
	/* PI_156 PI_TFC_F0:RW:0:10 */
	clrsetbits_le32(&denali_pi[156], 0x3ff,
			pdram_timing->tfc_long);
	/* PI_158 PI_TWR_F0:RW:24:6 */
	clrsetbits_le32(&denali_pi[158], 0x3f << 24,
			pdram_timing->twr << 24);

	/* PI_158 PI_TWTR_F0:RW:16:6 */
	clrsetbits_le32(&denali_pi[158], 0x3f << 16,
			pdram_timing->twtr << 16);
	/* PI_158 PI_TRCD_F0:RW:8:8 */
	clrsetbits_le32(&denali_pi[158], 0xff << 8,
			pdram_timing->trcd << 8);
	/* PI_158 PI_TRP_F0:RW:0:8 */
	clrsetbits_le32(&denali_pi[158], 0xff, pdram_timing->trp);
	/* PI_157 PI_TRTP_F0:RW:24:8 */
	clrsetbits_le32(&denali_pi[157], 0xff << 24,
			pdram_timing->trtp << 24);
	/* PI_159 PI_TRAS_MIN_F0:RW:24:8 */
	clrsetbits_le32(&denali_pi[159], 0xff << 24,
			pdram_timing->tras_min << 24);
	/* PI_159 PI_TRAS_MAX_F0:RW:0:17 */
	tmp = pdram_timing->tras_max * 99 / 100;
	clrsetbits_le32(&denali_pi[159], 0x1ffff, tmp);
	/* PI_160 PI_TMRD_F0:RW:16:6 */
	clrsetbits_le32(&denali_pi[160], 0x3f << 16,
			pdram_timing->tmrd << 16);
	/*PI_160 PI_TDQSCK_MAX_F0:RW:0:4 */
	clrsetbits_le32(&denali_pi[160], 0xf,
			pdram_timing->tdqsck_max);
	/* PI_187 PI_TDFI_CTRLUPD_MAX_F0:RW:8:16 */
	clrsetbits_le32(&denali_pi[187], 0xffff << 8,
			(2 * pdram_timing->trefi) << 8);
	/* PI_188 PI_TDFI_CTRLUPD_INTERVAL_F0:RW:0:32 */
	writel(20 * pdram_timing->trefi, &denali_pi[188]);

#if 0
	/* PI_43 PI_WRLAT_F0:RW:0:5 */
	if (timing_config->dram_type == LPDDR3) {
		tmp = get_pi_wrlat(pdram_timing, timing_config);
		mmio_clrsetbits_32(PI_REG(i, 43), 0x1f, tmp);
	}
#endif

	/* PI_43 PI_ADDITIVE_LAT_F0:RW:8:6 */
	clrsetbits_le32(&denali_pi[43], 0x3f << 8,
			PI_ADD_LATENCY << 8);
	/* PI_43 PI_CASLAT_LIN_F0:RW:16:7 */
	clrsetbits_le32(&denali_pi[43], 0x7f << 16,
			(pdram_timing->cl * 2) << 16);

#if 0
	/* PI_66 PI_TODTL_2CMD_F0:RW:24:8 */
	if (timing_config->dram_type == LPDDR3) {
		tmp = get_pi_todtoff_max(pdram_timing, timing_config);
		mmio_clrsetbits_32(PI_REG(i, 66), 0xff << 24,
				   tmp << 24);
	}
#endif

	/* PI_91 PI_TDFI_WRCSLAT_F0:RW:16:8 */
	tmp = get_pi_wrlat_adj(pdram_timing, timing_config);;
	u32 tmp1 = tmp;  // TODO: can be removed
	if (tmp1 == 0)
		tmp = 0;
	else if (tmp1 < 5)
		tmp = tmp1 - 1;
	else
		tmp = tmp1 - 5;
	clrsetbits_le32(&denali_pi[91], 0xff << 16, tmp << 16);

	/* PI_95 PI_TDFI_CALVL_CAPTURE_F0:RW:16:10 */
	tmp1 = 20000 / (1000000 / pdram_timing->mhz) + 1;
	if ((20000 % (1000000 / pdram_timing->mhz)) != 0)
		tmp1++;
	tmp = (tmp1 >> 1) + (tmp1 % 2) + 5;
	clrsetbits_le32(&denali_pi[95], 0x3ff << 16, tmp << 16);
	/* PI_95 PI_TDFI_CALVL_CC_F0:RW:0:10 */
	clrsetbits_le32(&denali_pi[95], 0x3ff, tmp + 18);

	/* PI_102 PI_TMRZ_F0:RW:8:5 */
	clrsetbits_le32(&denali_pi[102], 0x1f << 8,
			pdram_timing->tmrz << 8);
	/* PI_111 PI_TDFI_CALVL_STROBE_F0:RW:8:4 */
	tmp1 = 2 * 1000 / (1000000 / pdram_timing->mhz);
	if ((2 * 1000 % (1000000 / pdram_timing->mhz)) != 0)
		tmp1++;
	/* pi_tdfi_calvl_strobe=tds_train+5 */
	tmp = tmp1 + 5;
	clrsetbits_le32(&denali_pi[111], 0xf << 8, tmp << 8);
	/* PI_116 PI_TCKEHDQS_F0:RW:16:6 */
	tmp = 10000 / (1000000 / pdram_timing->mhz);
	if ((10000 % (1000000 / pdram_timing->mhz)) != 0)
		tmp++;
	if (pdram_timing->mhz <= 100)
		tmp = tmp + 1;
	else
		tmp = tmp + 8;
	clrsetbits_le32(&denali_pi[116], 0x3f << 16, tmp << 16);

}

static void gen_rk3399_ctl_params_f1(const struct chan_info *chan,
				     struct timing_related_config *timing_config,
				     struct dram_timing_t *pdram_timing)
{
	u32 *denali_ctl = chan->pctl->denali_ctl;
	//	u32 *denali_pi = chan->pi->denali_pi;
	//	u32 *denali_phy = chan->publ->denali_phy;
	uint32_t i;
	uint32_t tmp, tmp1;

	// TODO: don't loop here?
	for (i = 0; i < timing_config->ch_cnt; i++) {
		if (timing_config->dram_type == DDR3) {
			tmp = ((700000 + 10) * timing_config->freq +
				999) / 1000;
			tmp += pdram_timing->txsnr + (pdram_timing->tmrd * 3) +
			    pdram_timing->tmod + pdram_timing->tzqinit;
			writel(tmp, &denali_ctl[9]);

			clrsetbits_le32(&denali_ctl[22], 0xffff << 16, pdram_timing->tdllk << 16);

			tmp = (pdram_timing->tmod << 24) |
			      (pdram_timing->tmrd << 16) |
			      (pdram_timing->trtp << 8);
			writel(tmp, &denali_ctl[34]);

			clrsetbits_le32(&denali_ctl[60], 0xffff << 16,
					(pdram_timing->txsr - pdram_timing->trcd) << 16);
#if 0
		} else if (timing_config->dram_type == LPDDR4) {
			mmio_write_32(CTL_REG(i, 5), pdram_timing->tinit1 +
						     pdram_timing->tinit3);
			mmio_write_32(CTL_REG(i, 32),
				      (pdram_timing->tmrd << 8) |
				      pdram_timing->tmrd);
			mmio_clrsetbits_32(CTL_REG(i, 59), 0xffff << 16,
					   pdram_timing->txsr << 16);
		} else {
			mmio_write_32(CTL_REG(i, 5), pdram_timing->tinit1);
			mmio_write_32(CTL_REG(i, 7), pdram_timing->tinit4);
			mmio_write_32(CTL_REG(i, 32),
				      (pdram_timing->tmrd << 8) |
				      pdram_timing->tmrd);
			mmio_clrsetbits_32(CTL_REG(i, 59), 0xffff << 16,
					   pdram_timing->txsr << 16);
#endif
		}
		writel(pdram_timing->tinit3, &denali_ctl[10]);
		writel(pdram_timing->tinit5, &denali_ctl[12]);
		clrsetbits_le32(&denali_ctl[24], (0x7f << 8),
				((pdram_timing->cl * 2) << 8));
		clrsetbits_le32(&denali_ctl[24], (0x1f << 16),
				(pdram_timing->cwl << 16));
		clrsetbits_le32(&denali_ctl[24], 0x3f << 24, pdram_timing->al << 24);
		clrsetbits_le32(&denali_ctl[28], 0xffffff << 8,
				(pdram_timing->tras_min << 24) |
				(pdram_timing->trc << 16) |
				(pdram_timing->trrd << 8));
		clrsetbits_le32(&denali_ctl[29], 0xffffff,
				(pdram_timing->tfaw << 16) |
				(pdram_timing->trppb << 8) |
				(pdram_timing->twtr));

		writel(pdram_timing->tras_max | (pdram_timing->tcke << 24),
		       &denali_ctl[35]);

		clrsetbits_le32(&denali_ctl[36], 0xff,
				   max(1, pdram_timing->tckesr));
		clrsetbits_le32(&denali_ctl[39],
				(0xff << 24),
				(pdram_timing->trcd << 24));
		clrsetbits_le32(&denali_ctl[40], 0x3f, pdram_timing->twr);
		clrsetbits_le32(&denali_ctl[42], 0x1f << 24,
			      pdram_timing->tmrz << 24);
		tmp = pdram_timing->tdal ? pdram_timing->tdal :
		      (pdram_timing->twr + pdram_timing->trp);
		clrsetbits_le32(&denali_ctl[44], 0xff << 8, tmp << 8);
		clrsetbits_le32(&denali_ctl[45], 0xff << 8, pdram_timing->trp << 8);
		writel(((pdram_timing->trefi - 8) << 16) | pdram_timing->trfc,
		       &denali_ctl[49]);
		clrsetbits_le32(&denali_ctl[52], 0xffff << 16, pdram_timing->txp << 16);
		clrsetbits_le32(&denali_ctl[54], 0xffff, pdram_timing->txpdll);
		//		clrsetbits_le32(&denali_ctl[55], 0xf << 24,
		//				pdram_timing->tcscke << 24);
		clrsetbits_le32(&denali_ctl[55], 0xff << 8,
				pdram_timing->tmrri << 8);
		writel((pdram_timing->tzqcke << 24) |
		       (pdram_timing->tmrwckel << 16) |
		       (pdram_timing->tckehcs << 8) |
		       pdram_timing->tckelcs,
		       &denali_ctl[57]);
		//
		clrsetbits_le32(&denali_ctl[61], 0xffff, pdram_timing->txsnr);
		clrsetbits_le32(&denali_ctl[64], 0xffff << 16,
				   (pdram_timing->tckehcmd << 24) |
				   (pdram_timing->tckelcmd << 16));
		writel((pdram_timing->tckelpd << 24) |
		       (pdram_timing->tescke << 16) |
		       (pdram_timing->tsr << 8) |
		       pdram_timing->tckckel,
		       &denali_ctl[65]);

		clrsetbits_le32(&denali_ctl[66], 0xfff,
				(pdram_timing->tcmdcke << 8) |
				pdram_timing->tcsckeh);
		clrsetbits_le32(&denali_ctl[92], 0xff << 24,
				(pdram_timing->tcksre << 24));
		clrsetbits_le32(&denali_ctl[93], 0xff, pdram_timing->tcksrx);
		clrsetbits_le32(&denali_ctl[108], 0x1 << 25,
				(timing_config->dllbp << 25));
		clrsetbits_le32(&denali_ctl[125], 0x3ff << 16,
				(pdram_timing->tvrcg_enable << 16));
		writel(pdram_timing->tfc_long |
		       (pdram_timing->tckfspx << 24) |
		       (pdram_timing->tckfspe << 16),
		       &denali_ctl[126]);
		clrsetbits_le32(&denali_ctl[127], 0xffff, pdram_timing->tvref_long);

		clrsetbits_le32(&denali_ctl[134], 0xffff << 16,
				pdram_timing->mr[0] << 16);
		writel((pdram_timing->mr[2] << 16) |
		       pdram_timing->mr[1], &denali_ctl[135]);
		clrsetbits_le32(&denali_ctl[138], 0xffff << 16,
				pdram_timing->mr[3] << 16);
		clrsetbits_le32(&denali_ctl[140], 0xff,
				pdram_timing->mr11);
		clrsetbits_le32(&denali_ctl[148], 0xffff << 16,
				pdram_timing->mr[0] << 16);
		writel((pdram_timing->mr[2] << 16) |
		       pdram_timing->mr[1],
		       &denali_ctl[149]);
		clrsetbits_le32(&denali_ctl[152], 0xffff << 16,
				pdram_timing->mr[3] << 16);
		clrsetbits_le32(&denali_ctl[154], 0xff,
				pdram_timing->mr11);
		/*
		if (timing_config->dram_type == LPDDR4) {
			mmio_clrsetbits_32(CTL_REG(i, 140), 0xffff << 16,
					   pdram_timing->mr12 << 16);
			mmio_clrsetbits_32(CTL_REG(i, 142), 0xffff << 16,
					   pdram_timing->mr14 << 16);
			mmio_clrsetbits_32(CTL_REG(i, 145), 0xffff << 16,
					   pdram_timing->mr22 << 16);
			mmio_clrsetbits_32(CTL_REG(i, 154), 0xffff << 16,
					   pdram_timing->mr12 << 16);
			mmio_clrsetbits_32(CTL_REG(i, 156), 0xffff << 16,
					   pdram_timing->mr14 << 16);
			mmio_clrsetbits_32(CTL_REG(i, 159), 0xffff << 16,
					   pdram_timing->mr22 << 16);
		}
		*/
		writel(((pdram_timing->tzqinit / 2) << 16) |
		       pdram_timing->tzqinit,
		       &denali_ctl[182]);
		writel((pdram_timing->tzqcal << 16) | pdram_timing->tzqcs,
		       &denali_ctl[183]);

		clrsetbits_le32(&denali_ctl[184], 0x3f, pdram_timing->tzqlat);
		clrsetbits_le32(&denali_ctl[188], 0xfff, pdram_timing->tzqreset);
		clrsetbits_le32(&denali_ctl[212], 0xff << 16,
				pdram_timing->todton << 16);

		if (timing_config->odt) {
			setbits_le32(&denali_ctl[213], BIT(24));
			if (timing_config->freq < 400)
				tmp = 4;
			else
				tmp = 8;
		} else {
			clrbits_le32(&denali_ctl[213], BIT(24));
			tmp = 2;
		}

		/* Additional delay between reads to different chip selects */
		clrsetbits_le32(&denali_ctl[217], 0x1f << 24, tmp << 24);

                clrsetbits_le32(&denali_ctl[221], 0xf << 24,
				(pdram_timing->tdqsck_max << 24));
		clrsetbits_le32(&denali_ctl[222], 0x3, pdram_timing->tdqsck);

		tmp =
		    (get_wrlat_adj(timing_config->dram_type, pdram_timing->cwl)
		     << 8) | get_rdlat_adj(timing_config->dram_type,
					   pdram_timing->cl);

		clrsetbits_le32(&denali_ctl[291], 0xffff, tmp);

		clrsetbits_le32(&denali_ctl[84], 0xffff,
				(4 * pdram_timing->trefi));
		clrsetbits_le32(&denali_ctl[84], 0xffff << 16,
				((2 * pdram_timing->trefi) & 0xffff) << 16);

#if 0
		if ((timing_config->dram_type == LPDDR3) ||
		    (timing_config->dram_type == LPDDR4)) {
			tmp = get_pi_wrlat(pdram_timing, timing_config);
			tmp1 = get_pi_todtoff_max(pdram_timing, timing_config);
			tmp = (tmp > tmp1) ? (tmp - tmp1) : 0;
		} else {
			tmp = 0;
		}
#endif
		tmp = 0;
		clrsetbits_le32(&denali_ctl[214], 0x3f << 24,
				   (tmp & 0x3f) << 24);

		if ((timing_config->dram_type == LPDDR3) ||
		    (timing_config->dram_type == LPDDR4)) {
			/* min_rl_preamble = cl+TDQSCK_MIN -1 */
			tmp = pdram_timing->cl +
			    get_pi_todtoff_min(pdram_timing, timing_config) - 1;
			/* todtoff_max */
			tmp1 = get_pi_todtoff_max(pdram_timing, timing_config);
			tmp = (tmp > tmp1) ? (tmp - tmp1) : 0;
		} else {
			tmp = pdram_timing->cl - pdram_timing->cwl;
		}
		clrsetbits_le32(&denali_ctl[215], 0x3f << 24,
				   (tmp & 0x3f) << 24);

		clrsetbits_le32(&denali_ctl[275], 0xff << 24,
				   (get_pi_tdfi_phy_rdlat(pdram_timing,
							  timing_config) &
				    0xff) << 24);

		clrsetbits_le32(&denali_ctl[284], 0xffff << 16,
				((2 * pdram_timing->trefi) & 0xffff) << 16);

		clrsetbits_le32(&denali_ctl[289], 0xffff,
				   (2 * pdram_timing->trefi) & 0xffff);

		writel(20 * pdram_timing->trefi,
		       &denali_ctl[290]);

		/* CTL_308 TDFI_CALVL_CAPTURE_F0:RW:16:10 */
		tmp1 = 20000 / (1000000 / pdram_timing->mhz) + 1;
		if ((20000 % (1000000 / pdram_timing->mhz)) != 0)
			tmp1++;
		tmp = (tmp1 >> 1) + (tmp1 % 2) + 5;
		clrsetbits_le32(&denali_ctl[309], 0x3ff << 16, tmp << 16);

		/* CTL_308 TDFI_CALVL_CC_F1:RW:0:10 */
		tmp = tmp + 18;
		clrsetbits_le32(&denali_ctl[309], 0x3ff, tmp);

		// TODO: Merge w/ PI init
		/* CTL_314 TDFI_WRCSLAT_F1:RW:24:8 */
		tmp1 = get_pi_wrlat_adj(pdram_timing, timing_config);
		if (timing_config->freq <= TDFI_LAT_THRESHOLD_FREQ) {
			if (tmp1 == 0)
				tmp = 0;
			else if (tmp1 < 5)
				tmp = tmp1 - 1;
			else
				tmp = tmp1 - 5;
		} else {
			tmp = tmp1 - 2;
		}
		clrsetbits_le32(&denali_ctl[314], 0xff << 24, tmp << 24);

		/* CTL_314 TDFI_RDCSLAT_F1:RW:16:8 */
		if ((timing_config->freq <= TDFI_LAT_THRESHOLD_FREQ) &&
		    (pdram_timing->cl >= 5))
			tmp = pdram_timing->cl - 5;
		else
			tmp = pdram_timing->cl - 2;
		clrsetbits_le32(&denali_ctl[314], 0xff << 16, tmp << 16);
	}
}

static void gen_rk3399_pi_params_f1(const struct chan_info *chan,
				    struct timing_related_config *timing_config,
				    struct dram_timing_t *pdram_timing)
{
	u32 *denali_pi = chan->pi->denali_pi;
	u32 tmp;

	/* PI_04 PI_TDFI_PHYMSTR_MAX_F1:RW:0:32 */
	tmp = 4 * pdram_timing->trefi;
	writel(tmp, &denali_pi[4]);
	/* PI_05 PI_TDFI_PHYMSTR_RESP_F1:RW:0:16 */
	tmp = 2 * pdram_timing->trefi;
	clrsetbits_le32(&denali_pi[5], 0xffff, tmp);
	/* PI_12 PI_TDFI_PHYUPD_RESP_F1:RW:0:16 */
	clrsetbits_le32(&denali_pi[12], 0xffff, tmp);

#if 0
	/* PI_42 PI_TDELAY_RDWR_2_BUS_IDLE_F1:RW:8:8 */
	if (timing_config->dram_type == LPDDR4)
		tmp = 2;
	else
		tmp = 0;
#endif
	tmp = 0;
	tmp += (pdram_timing->bl / 2) + 4 +
		(get_pi_rdlat_adj(pdram_timing) - 2) +
		get_pi_tdfi_phy_rdlat(pdram_timing, timing_config);
	clrsetbits_le32(&denali_pi[42], 0xff << 8, tmp << 8);

	/* PI_47 PI_TREF_F1:RW:16:16 */
	clrsetbits_le32(&denali_pi[47], 0xffff << 16,
			pdram_timing->trefi << 16);
	/* PI_47 PI_TRFC_F1:RW:0:10 */
	clrsetbits_le32(&denali_pi[47], 0x3ff, pdram_timing->trfc);

	/* PI_72 PI_WR_TO_ODTH_F1:RW:24:6 */
#if 0
	if ((timing_config->dram_type == LPDDR3) ||
	    (timing_config->dram_type == LPDDR4)) {
		tmp1 = get_pi_wrlat(pdram_timing, timing_config);
		tmp2 = get_pi_todtoff_max(pdram_timing, timing_config);
		if (tmp1 > tmp2)
			tmp = tmp1 - tmp2;
		else
			tmp = 0;
	} else if (timing_config->dram_type == DDR3) {
		tmp = 0;
	}
#endif
	tmp = 0;
	clrsetbits_le32(&denali_pi[72], 0x3f << 24, tmp << 24);
	/* PI_73 PI_RD_TO_ODTH_F1:RW:16:6 */
#if 0
	if ((timing_config->dram_type == LPDDR3) ||
	    (timing_config->dram_type == LPDDR4)) {
		/* min_rl_preamble = cl + TDQSCK_MIN - 1 */
		tmp1 = pdram_timing->cl +
			get_pi_todtoff_min(pdram_timing, timing_config);
		tmp1--;
		/* todtoff_max */
		tmp2 = get_pi_todtoff_max(pdram_timing, timing_config);
		if (tmp1 > tmp2)
			tmp = tmp1 - tmp2;
		else
			tmp = 0;
	} else if (timing_config->dram_type == DDR3)
		tmp = pdram_timing->cl - pdram_timing->cwl;
#endif
	tmp = pdram_timing->cl - pdram_timing->cwl;
	clrsetbits_le32(&denali_pi[73], 0x3f << 16, tmp << 16);

	/*P I_89 PI_RDLAT_ADJ_F1:RW:24:8 */
	tmp = get_pi_rdlat_adj(pdram_timing);
	clrsetbits_le32(&denali_pi[89], 0xff << 24, tmp << 24);
	/* PI_90 PI_WRLAT_ADJ_F1:RW:24:8 */
	tmp = get_pi_wrlat_adj(pdram_timing, timing_config);
	clrsetbits_le32(&denali_pi[90], 0xff << 24, tmp << 24);

	/* PI_128 PI_MR1_DATA_F1_0:RW+:0:16 */
	clrsetbits_le32(&denali_pi[128], 0xffff, pdram_timing->mr[1]);
	/* PI_135 PI_MR1_DATA_F1_1:RW+:8:16 */
	clrsetbits_le32(&denali_pi[135], 0xffff << 8,
			pdram_timing->mr[1] << 8);
#if 0
	/* PI_143 PI_MR1_DATA_F1_2:RW+:0:16 */
	clrsetbits_le32(&denali_pi[143], 0xffff, pdram_timing->mr[1]);
	/* PI_150 PI_MR1_DATA_F1_3:RW+:8:16 */
	clrsetbits_le32(&denali_pi[150], 0xffff << 8,
			pdram_timing->mr[1] << 8);
#endif
	/* PI_128 PI_MR2_DATA_F1_0:RW+:16:16 */
	clrsetbits_le32(&denali_pi[128], 0xffff << 16,
			pdram_timing->mr[2] << 16);
	/* PI_136 PI_MR2_DATA_F1_1:RW+:0:16 */
	clrsetbits_le32(&denali_pi[136], 0xffff, pdram_timing->mr[2]);
#if 0
	/* PI_143 PI_MR2_DATA_F1_2:RW+:16:16 */
	clrsetbits_le32(&denali_pi[143], 0xffff << 16,
			pdram_timing->mr[2] << 16);
	/* PI_151 PI_MR2_DATA_F1_3:RW+:0:16 */
	clrsetbits_le32(&denali_pi[151], 0xffff, pdram_timing->mr[2]);
#endif
	/* PI_156 PI_TFC_F1:RW:16:10 */
	clrsetbits_le32(&denali_pi[156], 0x3ff << 16,
			pdram_timing->tfc_long << 16);
	/* PI_162 PI_TWR_F1:RW:8:6 */
	clrsetbits_le32(&denali_pi[162], 0x3f << 8,
			pdram_timing->twr << 8);
	/* PI_162 PI_TWTR_F1:RW:0:6 */
	clrsetbits_le32(&denali_pi[162], 0x3f, pdram_timing->twtr);
	/* PI_161 PI_TRCD_F1:RW:24:8 */
	clrsetbits_le32(&denali_pi[161], 0xff << 24,
			pdram_timing->trcd << 24);
	/* PI_161 PI_TRP_F1:RW:16:8 */
	clrsetbits_le32(&denali_pi[161], 0xff << 16,
			pdram_timing->trp << 16);
	/* PI_161 PI_TRTP_F1:RW:8:8 */
	clrsetbits_le32(&denali_pi[161], 0xff << 8,
			pdram_timing->trtp << 8);
	/* PI_163 PI_TRAS_MIN_F1:RW:24:8 */
	clrsetbits_le32(&denali_pi[163], 0xff << 24,
			pdram_timing->tras_min << 24);
	/* PI_163 PI_TRAS_MAX_F1:RW:0:17 */
	clrsetbits_le32(&denali_pi[163], 0x1ffff,
			pdram_timing->tras_max * 99 / 100);
	/* PI_164 PI_TMRD_F1:RW:16:6 */
	clrsetbits_le32(&denali_pi[164], 0x3f << 16,
			pdram_timing->tmrd << 16);
	/* PI_164 PI_TDQSCK_MAX_F1:RW:0:4 */
	clrsetbits_le32(&denali_pi[164], 0xf,
			pdram_timing->tdqsck_max);
	/* PI_189 PI_TDFI_CTRLUPD_MAX_F1:RW:0:16 */
	clrsetbits_le32(&denali_pi[189], 0xffff,
			2 * pdram_timing->trefi);
	/* PI_190 PI_TDFI_CTRLUPD_INTERVAL_F1:RW:0:32 */
	writel(20 * pdram_timing->trefi, &denali_pi[190]);

#if 0
	/* PI_43 PI_WRLAT_F1:RW:24:5 */
	if (timing_config->dram_type == LPDDR3) {
		tmp = get_pi_wrlat(pdram_timing, timing_config);
		mmio_clrsetbits_32(PI_REG(i, 43), 0x1f << 24,
				   tmp << 24);
	}
#endif
	/* PI_44 PI_ADDITIVE_LAT_F1:RW:0:6 */
	clrsetbits_le32(&denali_pi[44], 0x3f, PI_ADD_LATENCY);
	/* PI_44 PI_CASLAT_LIN_F1:RW:8:7:=0x18 */
	clrsetbits_le32(&denali_pi[44], 0x7f << 8,
			(pdram_timing->cl * 2) << 8);
#if 0
	/* PI_67 PI_TODTL_2CMD_F1:RW:8:8 */
	if (timing_config->dram_type == LPDDR3) {
		tmp = get_pi_todtoff_max(pdram_timing, timing_config);
		mmio_clrsetbits_32(PI_REG(i, 67), 0xff << 8, tmp << 8);
	}
#endif
	/* PI_91 PI_TDFI_WRCSLAT_F1:RW:24:8 */
	tmp = get_pi_wrlat_adj(pdram_timing, timing_config);
	u32 tmp1 = tmp; // TODO: can be removed
	if (tmp1 == 0)
		tmp = 0;
	else if (tmp1 < 5)
		tmp = tmp1 - 1;
	else
		tmp = tmp1 - 5;
	clrsetbits_le32(&denali_pi[91], 0xff << 24, tmp << 24);

	/*PI_96 PI_TDFI_CALVL_CAPTURE_F1:RW:16:10 */
	/* tadr=20ns */
	tmp1 = 20000 / (1000000 / pdram_timing->mhz) + 1;
	if ((20000 % (1000000 / pdram_timing->mhz)) != 0)
		tmp1++;
	tmp = (tmp1 >> 1) + (tmp1 % 2) + 5;
	clrsetbits_le32(&denali_pi[96], 0x3ff << 16, tmp << 16);
	/* PI_96 PI_TDFI_CALVL_CC_F1:RW:0:10 */
	tmp = tmp + 18;
	clrsetbits_le32(&denali_pi[96], 0x3ff, tmp);
	/*PI_103 PI_TMRZ_F1:RW:0:5 */
	clrsetbits_le32(&denali_pi[103], 0x1f, pdram_timing->tmrz);
	/*PI_111 PI_TDFI_CALVL_STROBE_F1:RW:16:4 */
	/* tds_train=ceil(2/ns) */
	tmp1 = 2 * 1000 / (1000000 / pdram_timing->mhz);
	if ((2 * 1000 % (1000000 / pdram_timing->mhz)) != 0)
		tmp1++;
	/* pi_tdfi_calvl_strobe=tds_train+5 */
	tmp = tmp1 + 5;
	clrsetbits_le32(&denali_pi[111], 0xf << 16,
			tmp << 16);
	/* PI_116 PI_TCKEHDQS_F1:RW:24:6 */
	tmp = 10000 / (1000000 / pdram_timing->mhz);
	if ((10000 % (1000000 / pdram_timing->mhz)) != 0)
		tmp++;
	if (pdram_timing->mhz <= 100)
		tmp = tmp + 1;
	else
		tmp = tmp + 8;
	clrsetbits_le32(&denali_pi[116], 0x3f << 24,
			tmp << 24);
}

static void gen_rk3399_phy_params(const struct chan_info *chan,
				  struct timing_related_config *timing_config,
				  /*				  struct drv_odt_lp_config *drv_config, */
				  struct dram_timing_t *pdram_timing,
  uint32_t fn)
{
	uint32_t tmp, i, div, j;
	uint32_t mem_delay_ps, pad_delay_ps, total_delay_ps, delay_frac_ps;
	uint32_t trpre_min_ps, gate_delay_ps, gate_delay_frac_ps;
	uint32_t ie_enable, tsel_enable, cas_lat, rddata_en_ie_dly, tsel_adder;
	uint32_t extra_adder, delta, hs_offset;
	//	u32 fn = 0;
	u32 *denali_phy = chan->publ->denali_phy;

	for (i = 0; i < timing_config->ch_cnt; i++) {

		pad_delay_ps = PI_PAD_DELAY_PS_VALUE;
		ie_enable = PI_IE_ENABLE_VALUE;
		tsel_enable = PI_TSEL_ENABLE_VALUE;

		clrsetbits_le32(&denali_phy[896], (0x3 << 8) | 1, fn << 8);

		/* PHY_LOW_FREQ_SEL */
		/* DENALI_PHY_913 1bit offset_0 */
		if (timing_config->freq > 400)
			clrbits_le32(&denali_phy[913], 1);
		else
			setbits_le32(&denali_phy[913], 1);

		/* PHY_RPTR_UPDATE_x */
		/* DENALI_PHY_87/215/343/471 4bit offset_16 */
		tmp = 2500 / (1000000 / pdram_timing->mhz) + 3;
		if ((2500 % (1000000 / pdram_timing->mhz)) != 0)
			tmp++;
		clrsetbits_le32(&denali_phy[87], 0xf << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[215], 0xf << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[343], 0xf << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[471], 0xf << 16, tmp << 16);

		/* PHY_PLL_CTRL */
		/* DENALI_PHY_911 13bits offset_0 */
		/* PHY_LP4_BOOT_PLL_CTRL */
		/* DENALI_PHY_919 13bits offset_0 */
		tmp = (1 << 12) | (2 << 7) | (1 << 1);
		clrsetbits_le32(&denali_phy[911], 0x1fff, tmp);
		clrsetbits_le32(&denali_phy[919], 0x1fff, tmp);

		/* PHY_PLL_CTRL_CA */
		/* DENALI_PHY_911 13bits offset_16 */
		/* PHY_LP4_BOOT_PLL_CTRL_CA */
		/* DENALI_PHY_919 13bits offset_16 */
		tmp = (2 << 7) | (1 << 5) | (1 << 1);
		clrsetbits_le32(&denali_phy[911], 0x1fff << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[919], 0x1fff << 16, tmp << 16);

		/* PHY_TCKSRE_WAIT */
		/* DENALI_PHY_922 4bits offset_24 */
		if (pdram_timing->mhz <= 400)
			tmp = 1;
		else if (pdram_timing->mhz <= 800)
			tmp = 3;
		else if (pdram_timing->mhz <= 1000)
			tmp = 4;
		else
			tmp = 5;
		clrsetbits_le32(&denali_phy[922], 0xf << 24, tmp << 24);
		/* PHY_CAL_CLK_SELECT_0:RW8:3 */
		div = pdram_timing->mhz / (2 * 20);
		for (j = 2, tmp = 1; j <= 128; j <<= 1, tmp++) {
			if (div < j)
				break;
		}
		clrsetbits_le32(&denali_phy[947], 0x7 << 8, tmp << 8);

		if (timing_config->dram_type == DDR3) {
			mem_delay_ps = 0;
			trpre_min_ps = 1000;
		} else if (timing_config->dram_type == LPDDR4) {
			mem_delay_ps = 1500;
			trpre_min_ps = 900;
		} else if (timing_config->dram_type == LPDDR3) {
			mem_delay_ps = 2500;
			trpre_min_ps = 900;
		} else {
			printf("gen_rk3399_phy_params:dramtype unsupport\n");
			return;
		}
		total_delay_ps = mem_delay_ps + pad_delay_ps;
		delay_frac_ps = 1000 * total_delay_ps /
				(1000000 / pdram_timing->mhz);
		gate_delay_ps = delay_frac_ps + 1000 - (trpre_min_ps / 2);
		gate_delay_frac_ps = gate_delay_ps % 1000;
		tmp = gate_delay_frac_ps * 0x200 / 1000;
		/* PHY_RDDQS_GATE_SLAVE_DELAY */
		/* DENALI_PHY_77/205/333/461 10bits offset_16 */
		clrsetbits_le32(&denali_phy[77], 0x2ff << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[205], 0x2ff << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[333], 0x2ff << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[461], 0x2ff << 16, tmp << 16);

		tmp = gate_delay_ps / 1000;
		/* PHY_LP4_BOOT_RDDQS_LATENCY_ADJUST */
		/* DENALI_PHY_10/138/266/394 4bit offset_0 */
		clrsetbits_le32(&denali_phy[10], 0xf, tmp);
		clrsetbits_le32(&denali_phy[138], 0xf, tmp);
		clrsetbits_le32(&denali_phy[266], 0xf, tmp);
		clrsetbits_le32(&denali_phy[394], 0xf, tmp);
		/* PHY_GTLVL_LAT_ADJ_START */
		/* DENALI_PHY_80/208/336/464 4bits offset_16 */
		tmp = rddqs_delay_ps / (1000000 / pdram_timing->mhz) + 2;
		clrsetbits_le32(&denali_phy[80], 0xf << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[208], 0xf << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[336], 0xf << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[464], 0xf << 16, tmp << 16);

		cas_lat = pdram_timing->cl + PI_ADD_LATENCY;
		rddata_en_ie_dly = ie_enable / (1000000 / pdram_timing->mhz);
		if ((ie_enable % (1000000 / pdram_timing->mhz)) != 0)
			rddata_en_ie_dly++;
		rddata_en_ie_dly = rddata_en_ie_dly - 1;
		tsel_adder = tsel_enable / (1000000 / pdram_timing->mhz);
		if ((tsel_enable % (1000000 / pdram_timing->mhz)) != 0)
			tsel_adder++;
		if (rddata_en_ie_dly > tsel_adder)
			extra_adder = rddata_en_ie_dly - tsel_adder;
		else
			extra_adder = 0;
		delta = cas_lat - rddata_en_ie_dly;
		if (PI_REGS_DIMM_SUPPORT && PI_DOUBLEFREEK)
			hs_offset = 2;
		else
			hs_offset = 1;
		if (rddata_en_ie_dly > (cas_lat - 1 - hs_offset))
			tmp = 0;
		else if ((delta == 2) || (delta == 1))
			tmp = rddata_en_ie_dly - 0 - extra_adder;
		else
			tmp = extra_adder;
		/* PHY_LP4_BOOT_RDDATA_EN_TSEL_DLY */
		/* DENALI_PHY_9/137/265/393 4bit offset_16 */
		clrsetbits_le32(&denali_phy[9], 0xf << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[137], 0xf << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[265], 0xf << 16, tmp << 16);
		clrsetbits_le32(&denali_phy[393], 0xf << 16, tmp << 16);
		/* PHY_RDDATA_EN_TSEL_DLY */
		/* DENALI_PHY_86/214/342/470 4bit offset_0 */
		clrsetbits_le32(&denali_phy[86], 0xf, tmp);
		clrsetbits_le32(&denali_phy[214], 0xf, tmp);
		clrsetbits_le32(&denali_phy[342], 0xf, tmp);
		clrsetbits_le32(&denali_phy[470], 0xf, tmp);

		if (tsel_adder > rddata_en_ie_dly)
			extra_adder = tsel_adder - rddata_en_ie_dly;
		else
			extra_adder = 0;
		if (rddata_en_ie_dly > (cas_lat - 1 - hs_offset))
			tmp = tsel_adder;
		else
			tmp = rddata_en_ie_dly - 0 + extra_adder;
		/* PHY_LP4_BOOT_RDDATA_EN_DLY */
		/* DENALI_PHY_9/137/265/393 4bit offset_8 */
		clrsetbits_le32(&denali_phy[9], 0xf << 8, tmp << 8);
		clrsetbits_le32(&denali_phy[137], 0xf << 8, tmp << 8);
		clrsetbits_le32(&denali_phy[265], 0xf << 8, tmp << 8);
		clrsetbits_le32(&denali_phy[393], 0xf << 8, tmp << 8);
		/* PHY_RDDATA_EN_DLY */
		/* DENALI_PHY_85/213/341/469 4bit offset_24 */
		clrsetbits_le32(&denali_phy[85], 0xf << 24, tmp << 24);
		clrsetbits_le32(&denali_phy[213], 0xf << 24, tmp << 24);
		clrsetbits_le32(&denali_phy[341], 0xf << 24, tmp << 24);
		clrsetbits_le32(&denali_phy[469], 0xf << 24, tmp << 24);

		if (pdram_timing->mhz <= ENPER_CS_TRAINING_FREQ) {
			/*
			 * Note:Per-CS Training is not compatible at speeds
			 * under 533 MHz. If the PHY is running at a speed
			 * less than 533MHz, all phy_per_cs_training_en_X
			 * parameters must be cleared to 0.
			 */

			/*DENALI_PHY_84/212/340/468 1bit offset_16 */
		        clrbits_le32(&denali_phy[84], 0x1 << 16);
			clrbits_le32(&denali_phy[212], 0x1 << 16);
			clrbits_le32(&denali_phy[340], 0x1 << 16);
			clrbits_le32(&denali_phy[468], 0x1 << 16);
		} else {
			setbits_le32(&denali_phy[84], 0x1 << 16);
			setbits_le32(&denali_phy[212], 0x1 << 16);
			setbits_le32(&denali_phy[340], 0x1 << 16);
			setbits_le32(&denali_phy[468], 0x1 << 16);
		}
		//		gen_rk3399_phy_dll_bypass(pdram_timing->mhz, i, fn,
		//					  timing_config->dram_type);
		phy_dll_bypass_set(chan->publ, pdram_timing->mhz, i, fn, timing_config->dram_type);
	}
}

static void gen_rk3399_enable_training(u32 *denali_ctl, uint32_t nmhz)
{
	uint32_t tmp;

	if (nmhz <= PHY_DLL_BYPASS_FREQ)
		tmp = 0;
	else
		tmp = 1;

	clrsetbits_le32(&denali_ctl[305], 1 << 16, tmp << 16);
	clrsetbits_le32(&denali_ctl[71], 1, tmp);
	clrsetbits_le32(&denali_ctl[70], 1 << 8, 1 << 8);
}

static int pctl_cfg(struct udevice *dev,
		    const struct chan_info *chan, u32 channel,
		    const struct rk3399_sdram_params *sdram_params)
{
	/* const struct rk3399_sdram_channel *sdram_ch =
		&sdram_params->ch[channel]; */
	u32 *denali_ctl = chan->pctl->denali_ctl;
	u32 *denali_pi = chan->pi->denali_pi;
	u32 *denali_phy = chan->publ->denali_phy;
	u32 tmp, tmp1, tmp2;
	u32 pwrup_srefresh_exit;
	int ret;
	const ulong timeout_ms = 200;
	struct dram_timing_t dram_timing;
	struct timing_related_config timing_config;
#if !CONFIG_IS_ENABLED(OF_PLATDATA)
	struct rockchip_dmc_plat *plat = dev_get_platdata(dev);
#endif

	timing_config.dram_type = DDR3;
	timing_config.dram_info[0].speed_rate = DDR3_1866M; //DDR3_1600K;
	//	timing_config.dram_info[0].speed_rate = DDR3_1600K;
	timing_config.dram_info[0].cs_cnt = 1;
	timing_config.dram_info[1].speed_rate = DDR3_1866M; //DDR3_1600K;
	//	timing_config.dram_info[1].speed_rate = DDR3_1600K;
	timing_config.dram_info[1].cs_cnt = 1;
	timing_config.ch_cnt = 1;
	timing_config.odt = 1;
	timing_config.freq = sdram_params->base.ddr_freq;
	timing_config.bl = 8;
	timing_config.ap = 0;
	/*
	if (mhz < 300)
		rk3399_dram_status.timing_config.dllbp = 1;
	else
		rk3399_dram_status.timing_config.dllbp = 0;
	 */
	timing_config.dllbp = 0;
	timing_config.dramds = 34;
	timing_config.dramodt = 60;
	//	timing_config.dramodt = 40;
	/*	timing_config.caodt = 120;  */ /* NOT USED FOR DDR3 */

	//	dram_get_parameter(&timing_config, &dram_timing);
	spd_decode_ddr3(&plat->spd[0], timing_config.freq, &dram_timing);

	/*
	 * work around controller bug:
	 * Do not program DRAM_CLASS until NO_PHY_IND_TRAIN_INT is programmed
	 */
	// 40: ok w/ spec
	// 60: ok w/ spec(gcc)
	// 130: ok w/ spec(gcc)
	// 182: ok w/ spec(gcc)
	// 201: ok w/ spec(gcc)
	// 225: ok w/ spec(gcc)
	// 284
	// 299
	// 300
	//

#if 1
	/* initialize PCTL */

	/*
	 * Disable MRR commands during initialization.
	 * Set to 1 to disable.
	 */
	/* DENALI_CTL_16 */
	const u32 NO_AUTO_MRR_INIT = BIT(24);

	/* DENALI_CTL_18 */
	/*
	 * Enable PHY independent training mode commands during initialization.
	 * Set to 1 to enable.
	 */
	const u32 PHY_INDEP_TRAIN_MODE = BIT(8);
	/*
	 * Disable PHY Independent Training during initialization.
	 * Set to 1 to disable.
	 */
	const u32 NO_PHY_IND_TRAIN_INIT = BIT(0);

	setbits_le32(&denali_ctl[16], NO_AUTO_MRR_INIT);
	dmb();

	setbits_le32(&denali_ctl[18], PHY_INDEP_TRAIN_MODE |
		                      NO_PHY_IND_TRAIN_INIT);

	/* 1:1 mapping for DFIBUS frequencies at F0..F2 */
	clrsetbits_le32(&denali_ctl[19], 0x1f1f1f, 0x020100);

	/* for ddr3 */
	/* tRST_PWRON */
	writel(dram_timing.trstl, &denali_ctl[20]);
	/* CKE-inactive */
	writel(dram_timing.trsth, &denali_ctl[21]);

	/* LPC_SR_ZQ_EN */
	const u32 LPC_SR_ZQ_EN = BIT(16);
	const u32 LPC_SR_PHYMSTR_EN = BIT(0);
	setbits_le32(&denali_ctl[107], LPC_SR_ZQ_EN | LPC_SR_PHYMSTR_EN);

	/* TBST_INT_INTERVAL */
	clrsetbits_le32(&denali_ctl[25], 0x7 << 24, 0x4 << 24);
	/* BST_LEN */
	/*
	 * Encoded burst length sent to DRAMs during initialization.
	 * Program to 1 for BL2, program to 2 for BL4, or program to 3 for BL8.
	 */
	clrsetbits_le32(&denali_ctl[44], 0x1f << 24, 3 << 24);

	/* TREF_ENABLE - start refreshes */
	const u32 TREF_ENABLE = BIT(16);
	setbits_le32(&denali_ctl[47], TREF_ENABLE);
	// CHECK: [47] := 0x03 << 24;

	/* CONCURRENTAP */
	const u32 CONCURRENTAP = BIT(16);
	setbits_le32(&denali_ctl[43], CONCURRENTAP);

	/* Basic settings for DFS and DFI */
	const u32 DFS_ENABLE = BIT(16);
	setbits_le32(&denali_ctl[108], DFS_ENABLE);
	/* */
	const u32 TDFI_INIT_START_F0 = 0x20;
	const u32 TDFI_INIT_START_F1 = 0x20;
	const u32 TDFI_INIT_START_F2 = 0x20;
	const u32 TDFI_INIT_COMPLETE_F0 = 0x400;
	const u32 TDFI_INIT_COMPLETE_F1 = 0x400;
	const u32 TDFI_INIT_COMPLETE_F2 = 0x400;
	const u32 DFS_PHY_REG_WRITE_EN = BIT(24);

	writel((TDFI_INIT_START_F1 << 24) |
	       (TDFI_INIT_COMPLETE_F0 << 8) |
	       TDFI_INIT_START_F0, &denali_ctl[109]);
	writel((TDFI_INIT_START_F2 << 16) | TDFI_INIT_COMPLETE_F1,
	       &denali_ctl[110]);
	clrsetbits_le32(&denali_ctl[111], 0x0100ffff,
			DFS_PHY_REG_WRITE_EN | TDFI_INIT_COMPLETE_F2);

	writel(0xb80, &denali_ctl[112]); /* DFS_PHY_REG_WRITE_ADDR */
	writel(0x0, &denali_ctl[113]);   /* DFS_PHY_REG_WRITE_DATA_F0 */
	writel(0x1, &denali_ctl[114]);   /* DFS_PHY_REG_WRITE_DATA_F1 */
	writel(0x2, &denali_ctl[115]);   /* DFS_PHY_REG_WRITE_DATA_F2 */
	writel(0xe, &denali_ctl[116]);   /* DFS_PHY_REG_WRITE_MASK */

	/* AREF_MAX_DEFICIT, AREF_HIGH_THRESHOLD, AREF_NORM_THRESHOLD */
	writel(0x18151100, &denali_ctl[164]);
	/* AREF_MAX_CREDIT */
	clrsetbits_le32(&denali_ctl[165], 0x1f, 0xc);

	const u32 ZQCS_ROTATE = BIT(8);
	setbits_le32(&denali_ctl[189], ZQCS_ROTATE);  /* TODO: via DTS */

	//	const u32 COL_DIFF = (12 - sdram_ch->col); // was: 2
	//	clrsetbits_le32(&denali_ctl[191], 0xf, COL_DIFF);
	// TODO: collocate w/ pi[199]

	/* COMMAND_AGE_COUNT and AGE_COUNT */
	// TODO - configurable
	const u32 COMMAND_AGE_COUNT = 0x7;
	const u32 AGE_COUNT = 0x7;
	clrsetbits_le32(&denali_ctl[192], 0x0f0f,
			(COMMAND_AGE_COUNT << 8) | AGE_COUNT);
	// CHECK:
	setbits_le32(&denali_ctl[192], BIT(24));

	/* configurable command queuing */
	// TODO - make configurable
	const u32 RW_SAME_EN = BIT(24);
	const u32 PRIORITY_EN = BIT(16);
	const u32 PLACEMENT_EN = BIT(8);
	const u32 BANK_SPLIT_EN = BIT(0);
	clrbits_le32(&denali_ctl[193],
		     RW_SAME_EN | PRIORITY_EN |
		     PLACEMENT_EN | BANK_SPLIT_EN);

	/* TODO: more command queuing policy in #194 */
	// TODO - make configurable
	const u32 DISABLE_RD_INTERLEAVE = BIT(16);
	clrsetbits_le32(&denali_ctl[195], 0x7, DISABLE_RD_INTERLEAVE | 0x3); /* NUM_Q_ENTRIES_ACT_DISABLE */

	/* BC# is A12 for DDR3 */
	clrsetbits_le32(&denali_ctl[196], 0xf << 8, 12 << 8);

	/* TODO: Programm MEMDATA_RATIO_0 for dual-rank */
	// clrsetbits_le32(&denali_ctl[197], 7, 1);

	/*
	 * Force the controller to accept commands in the order that
	 * they are placed in the command queue.
	 */
	// TODO: make configurable
	const u32 IN_ORDER_ACCEPT = BIT(24);
	setbits_le32(&denali_ctl[199], IN_ORDER_ACCEPT);

	/*
	 * Enable Enable an automatic controller-initiated update
	 * (dfi_ctrlupd_req) after every refresh.
	 */
	const u32 CTRLUPD_REQ_PER_AREF_EN = BIT(16);
	setbits_le32(&denali_ctl[200], CTRLUPD_REQ_PER_AREF_EN);

	const u32 RD_PREAMBLE_TRAINING_EN = BIT(0);
	setbits_le32(&denali_ctl[201], RD_PREAMBLE_TRAINING_EN);

	/* Enable ODT on writes */
	// TODO: sync/colocate w/ pi_odt_wr_map_cs1, pi_odt_wr_map_cs0
	// CHECK THESE:
	setbits_le32(&denali_ctl[211], BIT(16)); /* on CS0 for writes to CS0 */
	setbits_le32(&denali_ctl[212], BIT(1));  /* on CS1 for writes to CS1 */

	/* TODTL_2CMD_F{0,1,2} */
	// CHECK THESE AGAINST JEDEC:
	// TODO: should this be ODTLoff (WL-2) ???
	// clrsetbits_le32(&denali_ctl[212], 0xffffff << 8, 0x070707 << 8);

	/* ODT high time*/
	// TODO
	// This is true for DDR3 w/ BL8 only
	/* const u32 tODTH4 = 4; */
	const u32 tODTH8 = 6;
	clrsetbits_le32(&denali_ctl[213], 0xf << 8, tODTH8 << 8); // TODTH_RD
	clrsetbits_le32(&denali_ctl[213], 0xf << 0, tODTH8 << 0); // TODTH_WR

	/* TODO: Make this DTS-configurable */
	const u32 EN_ODT_ASSERT_EXCEPT_RD = BIT(8);
	clrbits_le32(&denali_ctl[214], EN_ODT_ASSERT_EXCEPT_RD);

	/* RD_TO_ODTH_F{0,1,2} */
	// TODO: This doesn't match up with anything in JESD-79 for me ... */
	// TODO: 040404 for 933, 030303 ok for 800
        clrsetbits_le32(&denali_ctl[215], 0xffffff << 8, 0x040404 << 8);

	/* CTL_217: {W2W,W2R,R2W}_DIFFCS_DLY_F0 */
	// TODO: Check againstt JESD-79
	clrsetbits_le32(&denali_ctl[217], 0xffffff, 0x050303); // TODO: F0
	/* CTL_217: {W2W,W2R,R2W}_DIFFCS_DLY_F1 */
	clrsetbits_le32(&denali_ctl[218], 0xffffff, 0x050303); // TODO: F1

	/* WLDQSEN --- DDR3: 28 nCK */
	const u32 PHYUPD_APPEND_EN = BIT(0);
	clrsetbits_le32(&denali_ctl[225], 0x3f << 24, 28 << 24);
	setbits_le32(&denali_ctl[225], PHYUPD_APPEND_EN);

	/* WLMRD --- DDR3: 40 nCK */
	const u32 DFI_PHY_WRLVL_MODE = BIT(16);
	clrsetbits_le32(&denali_ctl[226], 0x3f, 40);
	setbits_le32(&denali_ctl[226], DFI_PHY_WRLVL_MODE);

	/* WRLVL_CS_MAP */
	// TODO: check number of ranks/chip-selects
	clrsetbits_le32(&denali_ctl[227], 0x3, 0x3);

	const u32 DFI_PHY_RDLVL_GATE_MODE = BIT(16);
	const u32 DFI_PHY_RDLVL_MODE = BIT(8);
	setbits_le32(&denali_ctl[237], DFI_PHY_RDLVL_GATE_MODE |
		                       DFI_PHY_RDLVL_MODE);

	const u32 RDLVL_AREF_EN = BIT(24);
	setbits_le32(&denali_ctl[238], RDLVL_AREF_EN);

	const u32 RDLVL_GATE_AREF_EN = BIT(0);
	setbits_le32(&denali_ctl[239], RDLVL_GATE_AREF_EN);

	/* RDLVL_GATE_CS_MAP and RDLVL_CS_MAP */
	// TODO: match up against single-rank vs. dual-rank
	clrsetbits_le32(&denali_ctl[240], 0x303, 0x303);

	/* CA training patterns */
	clrsetbits_le32(&denali_ctl[256], 0xfffff, 0x556aa);
	clrsetbits_le32(&denali_ctl[257], 0xfffff, 0xaaaaa);
	clrsetbits_le32(&denali_ctl[258], 0xfffff, 0xaa955);
	clrsetbits_le32(&denali_ctl[259], 0xfffff, 0x55555);
	clrsetbits_le32(&denali_ctl[260], 0xfffff, 0xb3133);
	clrsetbits_le32(&denali_ctl[261], 0xfffff, 0x4cd33);
	clrsetbits_le32(&denali_ctl[262], 0xfffff, 0x4cecc);
	clrsetbits_le32(&denali_ctl[263], 0xfffff, 0xb32cc);

	const u32 CALVL_SEQ_EN = BIT(16);
	const u32 CALVL_SEQ_ALL_PATTERNS = 3 << 8;
	setbits_le32(&denali_ctl[264], CALVL_SEQ_EN | CALVL_SEQ_ALL_PATTERNS);

	const u32 CALVL_CS_MAP = 3 << 24;
	const u32 CALVL_AREF_EN = BIT(8);
	setbits_le32(&denali_ctl[265], CALVL_CS_MAP | CALVL_AREF_EN);

	/* DLL_RST_ADJ_DLY */
	// TODO: why?
	clrsetbits_le32(&denali_ctl[274], 0xffff << 8, 0xffff);

	/* tPHY_WRLAT -- calculated by HW? */
	//	clrsetbits_le32(&denali_ctl[275], 0xff, 6);
	//	clrsetbits_le32(&denali_ctl[275], 0xff, 2);

	// TODO: 298: tCTRL_DELAY_F0, tCTRL_DELAY_F1: 2 vs 3

	/* TDFI_WRLVL_WW */
	clrsetbits_le32(&denali_ctl[303], 0x3ff, 20);
	/* TDFI_RDLVL_RR - minimum # cycles between read commands; tunable? */
	// TODO: verify that 0 is ok; also check pi[78]
	/* TDFI_RDLVL_EN */
	clrsetbits_le32(&denali_ctl[303], 0x3ffff, 20 << 8 | 3);
	/* TDFI_CALVL_EN */
	clrsetbits_le32(&denali_ctl[307], 0xff << 16, 3 << 16);
	/* TDFI_PHY_WRDATA */
	clrsetbits_le32(&denali_ctl[313], 0x07 << 24, 1 << 24);
	/* TDFI_WRDATA_DELAY */
	clrsetbits_le32(&denali_ctl[315], 0xff << 16, 5 << 16);

	/* DENALI_CTL324 */
	const u32 MULTI_CHANNEL_ZQ_CAL_MASTER = BIT(24);
	/* const u32 BL_ON_FLY_ENABLE = BIT(16); */
	const u32 DISABLE_MEMORY_MASKED_WRITE = BIT(8);
	/* const u32 EN_1T_TIMING = BIT(0); */

	// TODO: make 2t-timing an option via DTS
	setbits_le32(&denali_ctl[324], /* EN_1T_TIMING | */
		                       MULTI_CHANNEL_ZQ_CAL_MASTER |
		                       DISABLE_MEMORY_MASKED_WRITE);

	/* reg 325 is missing from the documentation */
	// writel(&denali_ctl[325], 0x01010101);   // breaks 933mhz

#endif

	/* TODO: rank count need to set for init */

	gen_rk3399_ctl_params_f0(chan, &timing_config, &dram_timing);
	gen_rk3399_ctl_params_f1(chan, &timing_config, &dram_timing);

	/* common for f0..f2 */
	clrsetbits_le32(&denali_ctl[26], 0x1f, dram_timing.tccd);

	/* DRAM class */
	enum {
	  DDR3 = 6,
	  LPDDR3 = 7,
	  LPDDR4 = 11,
	};

	clrsetbits_le32(&denali_ctl[0], 0xf << 8, DDR3 << 8);
	clrsetbits_le32(&denali_pi[0], 0xf << 8, DDR3 << 8);

#if 0
	if (channel == 0) {
	  printf ("/* PI - preinit */\n");
	  for (int i = 0; i < sizeof(struct rk3399_ddr_pi_regs) / 4; ++i)
	    printf("PI_%d = 0x%08x\n", i, readl(&denali_pi[i]));
	}
#endif

	gen_rk3399_pi_params_f0(chan, &timing_config, &dram_timing);
	gen_rk3399_pi_params_f1(chan, &timing_config, &dram_timing);
	set_memory_map(chan, channel, sdram_params);

	// PI_23: TODO
	setbits_le32(&denali_pi[23], 7);
	// for DDR3:
	writel(0x110f0000, &denali_pi[24]);
	// writel(0x1f0f0000, &denali_pi[24]);
	writel(0x3c020000 | dram_timing.mr[2], &denali_pi[25]);
	// CS0 + CS1; write MRS2
	writel(0x3fffffff, &denali_pi[26]);
	writel(0x3c030000 | dram_timing.mr[3], &denali_pi[27]);
	// CS0 + CS1; write MRS3
	writel(0x3fffffff, &denali_pi[28]);
	writel(0x3c010000 | dram_timing.mr[1], &denali_pi[29]);
	// CS0 + CS1; write MRS1
	writel(0x3fffffff, &denali_pi[30]);
	writel(0x3c010000 | dram_timing.mr[0], &denali_pi[31]);
	// CS0 + CS1; write MRS0
	writel(0x3fffffff, &denali_pi[32]);
	writel(0x3c300400, &denali_pi[33]); // CS0 + C1; ZQCL

	// PI_49: pi_tref_interval -- cycles between ref to different CS
	// TODO: sync with DIFFCS_DLY ?? (e.g. ctl[217] and ctl[218]?
	clrsetbits_le32(&denali_pi[49], 0xffff << 8, 5 << 8);

	// pi_tdfi_ctrl_delay_f0,f1
	// TODO: match up with CTL[298]
	clrsetbits_le32(&denali_pi[58], 0x1f << 24, 4 << 24);
	clrsetbits_le32(&denali_pi[58], 0x1f << 16, 4 << 16);

	// pi_wldqsen PI_59
	// TODO: match up with ctl[225]
	// TODO: collocate w/ ctl[225]
	clrsetbits_le32(&denali_pi[59], 0x3f << 24, 28 << 24);

	// pi_wlmrd
	// TODO: match up with ctl[226] and collocate
	clrsetbits_le32(&denali_pi[60], 0x3f, 40);

	// tDFI_WRLVL_EN ctl[299] match up
	clrsetbits_le32(&denali_pi[62], 0xff << 16, 0);
	// pi_wrlvl_cs_map ctl[???] match up
	clrsetbits_le32(&denali_pi[62], 3, 3);

	// TDFI_WRLVL_WW
	// TODO: ctl[303]
	clrsetbits_le32(&denali_pi[63], 0x3ff, 20);

	// TODO: pi_wrlvl_strobe_num (LPDDR4 only): can't be 0 even for DDR3
	setbits_le32(&denali_pi[66], 2);

	// pi_odt_en_f1 and pi_odt_en_f0
	// TODO: ctl[213]
	if (timing_config.odt) {
		setbits_le32(&denali_pi[67], BIT(16)); // pi_odt_en_f1
		setbits_le32(&denali_pi[67], BIT(0));  // pi_odt_en_f0
	}

	// TODO: ctl[211] and ctl[212]
	setbits_le32(&denali_pi[69], BIT(17)); // pi_odt_wr_map_cs1
	setbits_le32(&denali_pi[69], BIT(0));  // pi_odt_wr_map_cs0

	// pi_rdlvl_gate_cs_map, pi_rdlvl_cs_map
	// TODO: ctl[240]
	clrsetbits_le32(&denali_pi[77], 0x303 << 8, 0x303 << 8);

	// pi_tdfi_rdlvl_rr
	// TODO: ctl[303]
	clrsetbits_le32(&denali_pi[78], 0x3ff, 20);

	// pi_tdfi_rdlvl_en
	// TODO: ctl[303]
	// CHECK: read-leveling is LPDDR4 only, right?
	clrsetbits_le32(&denali_pi[80], 0xff << 8, 3 << 8);

	// pi_rd_preamble_training_en
	// TODO: ctl[201]
	setbits_le32(&denali_pi[89], BIT(0));

	// pi_calvl_seq_en (matches up w/ CALVL_SEQ_ALL_PATTERNS)
	// TODO: ctl[264]
	clrsetbits_le32(&denali_pi[93], 3 << 16, 3 << 16);

	// pi_calvl_cs_map (matches up w/ CALVL_CS_MAP)
	// TODO: ctl[264]
	clrsetbits_le32(&denali_pi[94], 3 << 16, 3 << 16);

#if 0
	if (channel == 0) {
	  printf ("/* PI - postinit */\n");
	  for (int i = 0; i < sizeof(struct rk3399_ddr_pi_regs) / 4; ++i)
	    printf("PI_%d = 0x%08x\n", i, readl(&denali_pi[i]));
	}
#endif

	/* PHY PLL params */
	writel(0x64 << 8, &denali_phy[910]); // PHY_PLL_WAIT
	writel(0x01121102, &denali_phy[911]);
	clrbits_le32(&denali_phy[912], 0xf); // clear PHY_PLL_BYPASS

	pwrup_srefresh_exit = readl(&denali_ctl[68]) & PWRUP_SREFRESH_EXIT;
	clrbits_le32(&denali_ctl[68], PWRUP_SREFRESH_EXIT);

	/* PHY_DLL_RST_EN */
	clrsetbits_le32(&denali_phy[957], 0x3 << 24, 1 << 24);

	dmb();
	setbits_le32(&denali_pi[0], START);
	dmb();
	setbits_le32(&denali_ctl[0], START);
	dmb();

	/* Wating for phy DLL lock */
	while (1) {
		tmp = readl(&denali_phy[920]);
		tmp1 = readl(&denali_phy[921]);
		tmp2 = readl(&denali_phy[922]);
		if ((((tmp >> 16) & 0x1) == 0x1) &&
		    (((tmp1 >> 16) & 0x1) == 0x1) &&
		    (((tmp1 >> 0) & 0x1) == 0x1) &&
		    (((tmp2 >> 0) & 0x1) == 0x1))
			break;
	}

	for (int i = 0; i < 4; ++i) {
		/* Establish a 1:1 mapping for DM/DQ bits */
		writel(0x76543210, &denali_phy[0 + i * 128]);
		writel(0x00000008, &denali_phy[1 + i * 128]);
		setbits_le32(&denali_phy[6 + i * 128], BIT(24));  // tsel during read

		// TODO: PHY_CLK_WR_BYPASS_SLAVE_DELAY_0
		//	clrsetbits(&denali_phy[1], 0x7ff << 8, 0x4c0);

		//	PHY_PER_CS_TRAINING_MULTICAST_E
		setbits_le32(&denali_phy[8 + i * 128], BIT(16));
		setbits_le32(&denali_phy[8 + i * 128], 3 << 8);  // TODO: cs_map;

		// PHY_LPDDR_TYPE_0  // TODO
		//	clrsetbits_le32(&denali_phy[12], 3 << 8, 0); // DDR3
		// PHY_LPDDR_0 - even though 'LPDDR', required for DDR3
		setbits_le32(&denali_phy[12 + i * 128], 1);
		// PHY_GATE_ERROR_DELAY_SELECT_0
		clrsetbits_le32(&denali_phy[11 + i * 128], 0x1f << 16, 0x1f);
		// PHY_GATE_SMPL1_SLAVE_DELAY_0
		clrsetbits_le32(&denali_phy[12 + i * 128], 0x1ff << 16, 0xcc << 16);
		// PHY_GATE_SMPL2_SLAVE_DELAY_0
		clrsetbits_le32(&denali_phy[13 + i * 128], 0x1ff, 0x66);

		// PHY_WRLVL_UPDT_WAIT_CNT_0, PHY_WRLVL_CAPTURE_CNT_0
		clrsetbits_le32(&denali_phy[22 + i * 128], 0x0f3f << 16, 0x408 << 16);
		// PHY_RDLVL_UPDT_WAIT_CNT_0, PHY_RDLVL_CAPTURE_CNT_0
		clrsetbits_le32(&denali_phy[23 + i * 128], 0x0f3f << 16, 0x408 << 16);
		// PHY_GTLVL_UPDT_WAIT_CNT_0
		clrsetbits_le32(&denali_phy[23 + i * 128], 0xf << 8, 4 << 8);
		// PHY_WDQLVL_BURST_CNT_0 (TODO: should this be burst-length?)
		clrsetbits_le32(&denali_phy[24 + i * 128], 0x3f << 24, 8 << 24);

		// PHY_WDQLVL_UPDT_WAIT_CNT_0
		clrsetbits_le32(&denali_phy[25 + i * 128], 0xf << 24, 0xc << 24);
		// PHY_WDQLVL_DQDM_SLV_DLY_JUMP_OFFSET_0
		clrsetbits_le32(&denali_phy[25 + i * 128], 0x7ff << 8, 0xc0 <<  8);
		// PHY_WDQLVL_PATT_0
		setbits_le32(&denali_phy[25 + i * 128], 7); // TODO: all patterns
		// PHY_WDQLVL_QTR_DLY_STEP_0
		clrsetbits_le32(&denali_phy[26 + i * 128], 0xf << 8, 1 << 8);

		/* We have only 8bits in DM (i.e. no  ECC), so mask bit 8 */
		setbits_le32(&denali_phy[27 + i * 128], BIT(8));

		// PHY_USER_PATT0_0
		writel(0x55555555, &denali_phy[28 + i * 128]);
		writel(0xaaaaaaaa, &denali_phy[29 + i * 128]);
		writel(0x55555555, &denali_phy[30 + i * 128]);
		writel(0xaaaaaaaa, &denali_phy[31 + i * 128]);
		writel(0x00005555, &denali_phy[32 + i * 128]);

		/* clock slave delays */
		// TODO: write as a series  ox 0x270 writes
		// TODO/CHECK: 270 on lpddr3, 280 on ddr3
		writel(0x2800280, &denali_phy[59 + i * 128]);
		writel(0x2800280, &denali_phy[60 + i * 128]);
		writel(0x2800280, &denali_phy[61 + i * 128]);
		writel(0x2800280, &denali_phy[62 + i * 128]);
		writel(0x280, &denali_phy[63 + i * 128]);

#if 1
		// CHECK: this should not be needed
		writel(0x0080000, &denali_phy[68 + i * 128]);
		for (int j = 69; j < 76; ++j)
			writel(0x0080080, &denali_phy[j + i * 128]);
		writel(0x0080, &denali_phy[77 + i * 128]);
#endif

		// PHY_RDDQS_LATENCY_ADJUST_0
		clrsetbits_le32(&denali_phy[78 + i * 128], 0xf, 1);

		// timings -- TODO: clean up
		writel(0x51413152, &denali_phy[83 + i * 128]); // includes the "add half-cycle"
		writel(0x80004130, &denali_phy[84 + i * 128]); // note: per-cs training set elsewhere (i.e. not here)
		writel(0x80, &denali_phy[85 + i * 128]);

		writel(0x100001,  &denali_phy[86 + i * 128]);
		writel(0x07004208, &denali_phy[87 + i * 128]);  // PHY_RPTR_UPDATE_0 will be generated later
		writel(0x000f0c0f, &denali_phy[88 + i * 128]);  // TODO: split into fields
		writel(0x01000140, &denali_phy[89 + i * 128]);
		writel(0x00000c20, &denali_phy[90 + i * 128]);
	}

	writel(0x0a418820, &denali_phy[541]); // LPDDR4, but required?
	writel(0x16a4a0e6, &denali_phy[669]); // LPDDR4, but required?
	writel(0x1ee6b16a, &denali_phy[797]);
	for (int i = 0; i < 3; ++i) {
		writel(0x103f0000, &denali_phy[542 + i * 128]); //

		/* Delay lines */ // TODO: 0x380 on LPDDR3, 0x300 on DDR3
		const u32 slave_delay = 0x380;
		clrsetbits_le32(&denali_phy[544 + i * 128],
				0x7ff << 8, slave_delay << 8);
		clrsetbits_le32(&denali_phy[545 + i * 128],
				0x7ff << 16, slave_delay << 16);
		clrsetbits_le32(&denali_phy[545 + i * 128],
				0x7ff, slave_delay);
		clrsetbits_le32(&denali_phy[546 + i * 128],
				0x7ff << 16, slave_delay << 16);
		clrsetbits_le32(&denali_phy[546 + i * 128],
				0x7ff, slave_delay);
		clrsetbits_le32(&denali_phy[547 + i * 128],
				0x7ff, slave_delay);

		writel(0x42080010, &denali_phy[548 + i * 128]);
		writel(3, &denali_phy[549 + i * 128]); // no CALVL, but required?
	}

	// PHY_DFI_PHYUPD_TYPE, PHY_ADRCTL_LPDDR
	// TODO: same as PHY_LPDDR_0 ??
	writel(0x10100, &denali_phy[907]);

	/* Delay lines */
	const u32 slave_delay = 0x380;
	clrsetbits_le32(&denali_phy[916],
			0x7ff << 8, slave_delay << 8);
	clrsetbits_le32(&denali_phy[917],
			0x7ff << 16, slave_delay << 16);
	clrsetbits_le32(&denali_phy[917],
			0x7ff, slave_delay);
	clrsetbits_le32(&denali_phy[918],
			0x7ff, slave_delay);

	// PHY_TCKSRE_WAIT (CHECK: rockchip has 3, 0xf seems a safe default
	clrsetbits_le32(&denali_phy[922], 0xf << 24, 0xf);

	// TODO: check?
	clrsetbits_le32(&denali_phy[944], 0xff << 24, 0x6 << 24);
	writel(0x508, &denali_phy[947]);  // TODO: more pad-calibration

	writel(0xe4 << 24, &denali_phy[954]); // data-slice byte-swap
	setbits_le32(&denali_phy[957], BIT(16) | BIT(24));

	set_ds_odt(chan, sdram_params);

	ret = phy_io_config(chan, sdram_params);

	gen_rk3399_phy_params(chan, &timing_config, &dram_timing, 0);
	gen_rk3399_phy_params(chan, &timing_config, &dram_timing, 1);

	if (ret)
		return ret;

#if defined(DEBUG)
	{
		u32 trefi0 = (readl(&denali_ctl[48]) >> 16) + 8;
		u32 trefi1 = (readl(&denali_ctl[49]) >> 16) + 8;

		printf("trefi0     %08x\n", trefi0);
		printf("trefi1     %08x\n", trefi1);
		printf("tinit1+2   %08x (txsnr, 3*mrd, tmod, tzqinit)) ... 700us\n",
		       readl(&denali_ctl[5]));
		printf("tinit3     %08x\n", readl(&denali_ctl[6]));
		printf("tinit5     %08x\n", readl(&denali_ctl[8]));
		printf("tCL * 2    %08x (%d)\n",
		       ((readl(&denali_ctl[23]) >> 16) & 0x7f),
		       ((readl(&denali_ctl[23]) >> 16) & 0x7f));
		printf("timing.CL %d CWL %d\n", dram_timing.cl, dram_timing.cwl);
		printf("tCWL       %d\n", ((readl(&denali_ctl[23]) >> 24) & 0x1f));
		printf("tAL        %d\n", (readl(&denali_ctl[24]) & 0x3f));
		printf("tRC        %d\n", (readl(&denali_ctl[26]) >> 24) & 0xff);
		printf("tRRD       %d\n", (readl(&denali_ctl[26]) >> 16) & 0xff);
		printf("tMOD       %d\n", (readl(&denali_ctl[32]) >> 8) & 0xff);
		printf("tMRD       %d\n", (readl(&denali_ctl[32]) >> 0) & 0xff);
		printf("MR[0]      %04x (tCL, tWR)\n",
		       (readl(&denali_ctl[133]) >> 0) & 0xffff);
		printf("MR[1]      %04x\n", (readl(&denali_ctl[133]) >> 16) & 0xffff);
		printf("MR[2]      %04x (tCWL)\n",
		       (readl(&denali_ctl[134]) >> 0) & 0xffff);
		printf("MR[3]      %04x\n", (readl(&denali_ctl[138]) >> 0) & 0xffff);
		//		printf("
		printf("CTL284: wrlat_adj %d rdlat_adj %d (depends on CWL, CL)\n",
		       (readl(&denali_ctl[284]) >> 8) & 0xff,
		       (readl(&denali_ctl[284]) >> 0) & 0xff);

	}
#endif

	/* PHY_DLL_RST_EN */
	clrsetbits_le32(&denali_phy[957], 0x3 << 24, 0x2 << 24);

	gen_rk3399_enable_training(denali_ctl, timing_config.freq);

	/* Wating for PHY and DRAM init complete */
	tmp = get_timer(0);
	do {
		if (get_timer(tmp) > timeout_ms) {
			pr_err("DRAM (%s): phy failed to lock within  %ld ms\n",
			      __func__, timeout_ms);
			return -ETIME;
		}
	} while (!(readl(&denali_ctl[203]) & (1 << 3)));
	debug("DRAM (%s): phy locked after %ld ms\n", __func__, get_timer(tmp));

	clrsetbits_le32(&denali_ctl[68], PWRUP_SREFRESH_EXIT,
			pwrup_srefresh_exit);

#if 0
	printf ("/* PCTL */\n");
	for (int i = 0; i < sizeof(struct rk3399_ddr_pctl_regs) / 4; ++i)
	  printf("\t\t0x%08x\n", readl(&denali_ctl[i]));

	printf ("/* PI */\n");
	for (int i = 0; i < sizeof(struct rk3399_ddr_pi_regs) / 4; ++i)
	  printf("\t\t0x%08x\n", readl(&denali_pi[i]));

	printf ("/* PHY */\n");
	for (int i = 0; i < 958; ++i)
	  printf("\t\t0x%08x\n", readl(&denali_phy[i]));
#endif

	return 0;
}

static void select_per_cs_training_index(const struct chan_info *chan,
					 u32 rank)
{
	u32 *denali_phy = chan->publ->denali_phy;

	/* PHY_84 PHY_PER_CS_TRAINING_EN_0 1bit offset_16 */
	if ((readl(&denali_phy[84])>>16) & 1) {
		/*
		 * PHY_8/136/264/392
		 * phy_per_cs_training_index_X 1bit offset_24
		 */
		clrsetbits_le32(&denali_phy[8], 0x1 << 24, rank << 24);
		clrsetbits_le32(&denali_phy[136], 0x1 << 24, rank << 24);
		clrsetbits_le32(&denali_phy[264], 0x1 << 24, rank << 24);
		clrsetbits_le32(&denali_phy[392], 0x1 << 24, rank << 24);
	}
}

static void override_write_leveling_value(const struct chan_info *chan)
{
	u32 *denali_ctl = chan->pctl->denali_ctl;
	u32 *denali_phy = chan->publ->denali_phy;
	u32 byte;

	/* PHY_896 PHY_FREQ_SEL_MULTICAST_EN 1bit offset_0 */
	setbits_le32(&denali_phy[896], 1);

	/*
	 * PHY_8/136/264/392
	 * phy_per_cs_training_multicast_en_X 1bit offset_16
	 */
	clrsetbits_le32(&denali_phy[8], 0x1 << 16, 1 << 16);
	clrsetbits_le32(&denali_phy[136], 0x1 << 16, 1 << 16);
	clrsetbits_le32(&denali_phy[264], 0x1 << 16, 1 << 16);
	clrsetbits_le32(&denali_phy[392], 0x1 << 16, 1 << 16);

	for (byte = 0; byte < 4; byte++)
		clrsetbits_le32(&denali_phy[63 + (128 * byte)], 0xffff << 16,
				0x200 << 16);

	/* PHY_896 PHY_FREQ_SEL_MULTICAST_EN 1bit offset_0 */
	clrbits_le32(&denali_phy[896], 1);

	/* CTL_200 ctrlupd_req 1bit offset_8 */
	clrsetbits_le32(&denali_ctl[200], 0x1 << 8, 0x1 << 8);
}

static int data_training_ca(const struct chan_info *chan, u32 channel,
			    const struct rk3399_sdram_params *sdram_params)
{
	u32 *denali_pi = chan->pi->denali_pi;
	u32 *denali_phy = chan->publ->denali_phy;
	u32 i, tmp;
	u32 obs_0, obs_1, obs_2, obs_err = 0;
	u32 rank = sdram_params->ch[channel].rank;

	for (i = 0; i < rank; i++) {
		select_per_cs_training_index(chan, i);
		/* PI_100 PI_CALVL_EN:RW:8:2 */
		clrsetbits_le32(&denali_pi[100], 0x3 << 8, 0x2 << 8);
		/* PI_92 PI_CALVL_REQ:WR:16:1,PI_CALVL_CS:RW:24:2 */
		clrsetbits_le32(&denali_pi[92],
				(0x1 << 16) | (0x3 << 24),
				(0x1 << 16) | (i << 24));

		/* Waiting for training complete */
		while (1) {
			/* PI_174 PI_INT_STATUS:RD:8:18 */
			tmp = readl(&denali_pi[174]) >> 8;
			/*
			 * check status obs
			 * PHY_532/660/789 phy_adr_calvl_obs1_:0:32
			 */
			obs_0 = readl(&denali_phy[532]);
			obs_1 = readl(&denali_phy[660]);
			obs_2 = readl(&denali_phy[788]);
			if (((obs_0 >> 30) & 0x3) ||
			    ((obs_1 >> 30) & 0x3) ||
			    ((obs_2 >> 30) & 0x3))
				obs_err = 1;
			if ((((tmp >> 11) & 0x1) == 0x1) &&
			    (((tmp >> 13) & 0x1) == 0x1) &&
			    (((tmp >> 5) & 0x1) == 0x0) &&
			    (obs_err == 0))
				break;
			else if ((((tmp >> 5) & 0x1) == 0x1) ||
				 (obs_err == 1))
				return -EIO;
		}
		/* clear interrupt,PI_175 PI_INT_ACK:WR:0:17 */
		writel(0x00003f7c, (&denali_pi[175]));
	}
	clrbits_le32(&denali_pi[100], 0x3 << 8);

	return 0;
}

static int data_training_wl(const struct chan_info *chan, u32 channel,
			    const struct rk3399_sdram_params *sdram_params)
{
	u32 *denali_pi = chan->pi->denali_pi;
	u32 *denali_phy = chan->publ->denali_phy;
	u32 i, tmp;
	u32 obs_0, obs_1, obs_2, obs_3, obs_err = 0;
	u32 rank = sdram_params->ch[channel].rank;

	for (i = 0; i < rank; i++) {
#if 0
		// TODO: remove => this was needed due to a bug in RK template
		/* PI_60 PI_WRLVL_EN:RW:8:2 */
		clrsetbits_le32(&denali_pi[59], 0xff << 24, 0xff << 24);
		debug("denali_pi[59] %08x\n", readl(&denali_pi[59]));
		debug("denali_pi[60] %08x\n", readl(&denali_pi[60]));
#endif
		clrsetbits_le32(&denali_pi[60], 0x3 << 8, 0x3 << 8);
		/* PI_59 PI_WRLVL_REQ:WR:8:1,PI_WRLVL_CS:RW:16:2 */
		clrsetbits_le32(&denali_pi[59],
				(0x1 << 8) | (0x3 << 16),
				(0x1 << 8) | (i << 16));

		select_per_cs_training_index(chan, i);

		/* Waiting for training complete */
		while (1) {
			/* PI_174 PI_INT_STATUS:RD:8:18 */
			tmp = readl(&denali_pi[174]) >> 8;

			/*
			 * check status obs, if error maybe can not
			 * get leveling done PHY_40/168/296/424
			 * phy_wrlvl_status_obs_X:0:13
			 */
			obs_0 = readl(&denali_phy[40]);
			obs_1 = readl(&denali_phy[168]);
			obs_2 = readl(&denali_phy[296]);
			obs_3 = readl(&denali_phy[424]);
			if (((obs_0 >> 12) & 0x1) ||
			    ((obs_1 >> 12) & 0x1) ||
			    ((obs_2 >> 12) & 0x1) ||
			    ((obs_3 >> 12) & 0x1))
				obs_err = 1;
			if ((((tmp >> 10) & 0x1) == 0x1) &&
			    (((tmp >> 13) & 0x1) == 0x1) &&
			    (((tmp >> 4) & 0x1) == 0x0) &&
			    (obs_err == 0))
				break;
			else if ((((tmp >> 4) & 0x1) == 0x1) ||
				 (obs_err == 1)) {
				printf("%s: -EIO\n", __func__);
				return -EIO;
			}
		}
		/* clear interrupt,PI_175 PI_INT_ACK:WR:0:17 */
		writel(0x00003f7c, (&denali_pi[175]));
	}

	override_write_leveling_value(chan);
	clrbits_le32(&denali_pi[60], 0x3 << 8);
	// TODO/CHECK: should this not remain enabled for non-init?

	return 0;
}

static int data_training_rg(const struct chan_info *chan, u32 channel,
			    const struct rk3399_sdram_params *sdram_params)
{
	u32 *denali_pi = chan->pi->denali_pi;
	u32 *denali_phy = chan->publ->denali_phy;
	u32 i, tmp;
	u32 obs_0, obs_1, obs_2, obs_3, obs_err = 0;
	u32 rank = sdram_params->ch[channel].rank;

	for (i = 0; i < rank; i++) {
		select_per_cs_training_index(chan, i);
		/* PI_80 PI_RDLVL_GATE_EN:RW:24:2 */
		clrsetbits_le32(&denali_pi[80], 0x3 << 24, 0x2 << 24);
		/*
		 * PI_74 PI_RDLVL_GATE_REQ:WR:16:1
		 * PI_RDLVL_CS:RW:24:2
		 */
		clrsetbits_le32(&denali_pi[74],
				(0x1 << 16) | (0x3 << 24),
				(0x1 << 16) | (i << 24));

		/* Waiting for training complete */
		while (1) {
			/* PI_174 PI_INT_STATUS:RD:8:18 */
			tmp = readl(&denali_pi[174]) >> 8;

			/*
			 * check status obs
			 * PHY_43/171/299/427
			 *     PHY_GTLVL_STATUS_OBS_x:16:8
			 */
			obs_0 = readl(&denali_phy[43]);
			obs_1 = readl(&denali_phy[171]);
			obs_2 = readl(&denali_phy[299]);
			obs_3 = readl(&denali_phy[427]);
			if (((obs_0 >> (16 + 6)) & 0x3) ||
			    ((obs_1 >> (16 + 6)) & 0x3) ||
			    ((obs_2 >> (16 + 6)) & 0x3) ||
			    ((obs_3 >> (16 + 6)) & 0x3))
				obs_err = 1;
			if ((((tmp >> 9) & 0x1) == 0x1) &&
			    (((tmp >> 13) & 0x1) == 0x1) &&
			    (((tmp >> 3) & 0x1) == 0x0) &&
			    (obs_err == 0))
				break;
			else if ((((tmp >> 3) & 0x1) == 0x1) ||
				 (obs_err == 1))
				return -EIO;
		}
		/* clear interrupt,PI_175 PI_INT_ACK:WR:0:17 */
		writel(0x00003f7c, (&denali_pi[175]));
	}
	clrbits_le32(&denali_pi[80], 0x3 << 24);

	return 0;
}

static int data_training_rl(const struct chan_info *chan, u32 channel,
			    const struct rk3399_sdram_params *sdram_params)
{
	u32 *denali_pi = chan->pi->denali_pi;
	u32 i, tmp;
	u32 rank = sdram_params->ch[channel].rank;

	for (i = 0; i < rank; i++) {
		select_per_cs_training_index(chan, i);
		/* PI_80 PI_RDLVL_EN:RW:16:2 */
		clrsetbits_le32(&denali_pi[80], 0x3 << 16, 0x2 << 16);
		/* PI_74 PI_RDLVL_REQ:WR:8:1,PI_RDLVL_CS:RW:24:2 */
		clrsetbits_le32(&denali_pi[74],
				(0x1 << 8) | (0x3 << 24),
				(0x1 << 8) | (i << 24));

		/* Waiting for training complete */
		while (1) {
			/* PI_174 PI_INT_STATUS:RD:8:18 */
			tmp = readl(&denali_pi[174]) >> 8;

			/*
			 * make sure status obs not report error bit
			 * PHY_46/174/302/430
			 *     phy_rdlvl_status_obs_X:16:8
			 */
			if ((((tmp >> 8) & 0x1) == 0x1) &&
			    (((tmp >> 13) & 0x1) == 0x1) &&
			    (((tmp >> 2) & 0x1) == 0x0))
				break;
			else if (((tmp >> 2) & 0x1) == 0x1)
				return -EIO;
		}
		/* clear interrupt,PI_175 PI_INT_ACK:WR:0:17 */
		writel(0x00003f7c, (&denali_pi[175]));
	}
	clrbits_le32(&denali_pi[80], 0x3 << 16);

	return 0;
}

static int data_training_wdql(const struct chan_info *chan, u32 channel,
			      const struct rk3399_sdram_params *sdram_params)
{
	u32 *denali_pi = chan->pi->denali_pi;
	u32 i, tmp;
	u32 rank = sdram_params->ch[channel].rank;

	for (i = 0; i < rank; i++) {
		select_per_cs_training_index(chan, i);
		/*
		 * disable PI_WDQLVL_VREF_EN before wdq leveling?
		 * PI_181 PI_WDQLVL_VREF_EN:RW:8:1
		 */
		clrbits_le32(&denali_pi[181], 0x1 << 8);
		/* PI_124 PI_WDQLVL_EN:RW:16:2 */
		clrsetbits_le32(&denali_pi[124], 0x3 << 16, 0x2 << 16);
		/* PI_121 PI_WDQLVL_REQ:WR:8:1,PI_WDQLVL_CS:RW:16:2 */
		clrsetbits_le32(&denali_pi[121],
				(0x1 << 8) | (0x3 << 16),
				(0x1 << 8) | (i << 16));

		/* Waiting for training complete */
		while (1) {
			/* PI_174 PI_INT_STATUS:RD:8:18 */
			tmp = readl(&denali_pi[174]) >> 8;
			if ((((tmp >> 12) & 0x1) == 0x1) &&
			    (((tmp >> 13) & 0x1) == 0x1) &&
			    (((tmp >> 6) & 0x1) == 0x0))
				break;
			else if (((tmp >> 6) & 0x1) == 0x1)
				return -EIO;
		}
		/* clear interrupt,PI_175 PI_INT_ACK:WR:0:17 */
		writel(0x00003f7c, (&denali_pi[175]));
	}
	clrbits_le32(&denali_pi[124], 0x3 << 16);

	return 0;
}

static int data_training(const struct chan_info *chan, u32 channel,
			 const struct rk3399_sdram_params *sdram_params,
			 u32 training_flag)
{
	u32 *denali_phy = chan->publ->denali_phy;

	/* PHY_927 PHY_PAD_DQS_DRIVE  RPULL offset_22 */
	setbits_le32(&denali_phy[927], (1 << 22));

	if (training_flag == PI_FULL_TRAINING) {
		if (sdram_params->base.dramtype == LPDDR4) {
			training_flag = PI_CA_TRAINING | PI_WRITE_LEVELING |
					PI_READ_GATE_TRAINING |
					PI_READ_LEVELING | PI_WDQ_LEVELING;
		} else if (sdram_params->base.dramtype == LPDDR3) {
			training_flag = PI_CA_TRAINING | PI_WRITE_LEVELING |
					PI_READ_GATE_TRAINING;
		} else if (sdram_params->base.dramtype == DDR3) {
			training_flag = PI_WRITE_LEVELING |
					PI_READ_GATE_TRAINING |
					PI_READ_LEVELING;
		}
	}

	/* ca training(LPDDR4,LPDDR3 support) */
	if ((training_flag & PI_CA_TRAINING) == PI_CA_TRAINING)
		data_training_ca(chan, channel, sdram_params);

	/* write leveling(LPDDR4,LPDDR3,DDR3 support) */
	if ((training_flag & PI_WRITE_LEVELING) == PI_WRITE_LEVELING)
		data_training_wl(chan, channel, sdram_params);

	/* read gate training(LPDDR4,LPDDR3,DDR3 support) */
	if ((training_flag & PI_READ_GATE_TRAINING) == PI_READ_GATE_TRAINING)
		data_training_rg(chan, channel, sdram_params);

	/* read leveling(LPDDR4,LPDDR3,DDR3 support) */
	if ((training_flag & PI_READ_LEVELING) == PI_READ_LEVELING)
		data_training_rl(chan, channel, sdram_params);

	/* wdq leveling(LPDDR4 support) */
	if ((training_flag & PI_WDQ_LEVELING) == PI_WDQ_LEVELING)
		data_training_wdql(chan, channel, sdram_params);

	/* PHY_927 PHY_PAD_DQS_DRIVE  RPULL offset_22 */
	clrbits_le32(&denali_phy[927], (1 << 22));

	return 0;
}

static void set_ddrconfig(const struct chan_info *chan,
			  const struct rk3399_sdram_params *sdram_params,
			  unsigned char channel, u32 ddrconfig)
{
	/* only need to set ddrconfig */
	struct rk3399_msch_regs *ddr_msch_regs = chan->msch;
	unsigned int cs0_cap = 0;
	unsigned int cs1_cap = 0;

	cs0_cap = (1 << (sdram_params->ch[channel].cs0_row
			+ sdram_params->ch[channel].col
			+ sdram_params->ch[channel].bk
			+ sdram_params->ch[channel].bw - 20));
	if (sdram_params->ch[channel].rank > 1)
		cs1_cap = cs0_cap >> (sdram_params->ch[channel].cs0_row
				- sdram_params->ch[channel].cs1_row);
	if (sdram_params->ch[channel].row_3_4) {
		cs0_cap = cs0_cap * 3 / 4;
		cs1_cap = cs1_cap * 3 / 4;
	}

	writel(ddrconfig | (ddrconfig << 8), &ddr_msch_regs->ddrconf);
	writel(((cs0_cap / 32) & 0xff) | (((cs1_cap / 32) & 0xff) << 8),
	       &ddr_msch_regs->ddrsize);
}

static void dram_all_config(struct dram_info *dram,
			    const struct rk3399_sdram_params *sdram_params)
{
	u32 sys_reg = 0;
	unsigned int channel, idx;

	sys_reg |= sdram_params->base.dramtype << SYS_REG_DDRTYPE_SHIFT;
	sys_reg |= (sdram_params->base.num_channels - 1)
		    << SYS_REG_NUM_CH_SHIFT;
	for (channel = 0, idx = 0;
	     (idx < sdram_params->base.num_channels) && (channel < 2);
	     channel++) {
		const struct rk3399_sdram_channel *info =
			&sdram_params->ch[channel];
		struct rk3399_msch_regs *ddr_msch_regs;
		const struct rk3399_msch_timings *noc_timing;

		if (sdram_params->ch[channel].col == 0)
			continue;
		idx++;
		sys_reg |= info->row_3_4 << SYS_REG_ROW_3_4_SHIFT(channel);
		sys_reg |= 1 << SYS_REG_CHINFO_SHIFT(channel);
		sys_reg |= (info->rank - 1) << SYS_REG_RANK_SHIFT(channel);
		sys_reg |= (info->col - 9) << SYS_REG_COL_SHIFT(channel);
		sys_reg |= info->bk == 3 ? 0 : 1 << SYS_REG_BK_SHIFT(channel);
		sys_reg |= (info->cs0_row - 13) << SYS_REG_CS0_ROW_SHIFT(channel);
		sys_reg |= (info->cs1_row - 13) << SYS_REG_CS1_ROW_SHIFT(channel);
		sys_reg |= (2 >> info->bw) << SYS_REG_BW_SHIFT(channel);
		sys_reg |= (2 >> info->dbw) << SYS_REG_DBW_SHIFT(channel);

		ddr_msch_regs = dram->chan[channel].msch;
		noc_timing = &sdram_params->ch[channel].noc_timings;
		writel(noc_timing->ddrtiminga0,
		       &ddr_msch_regs->ddrtiminga0);
		writel(noc_timing->ddrtimingb0,
		       &ddr_msch_regs->ddrtimingb0);
		writel(noc_timing->ddrtimingc0,
		       &ddr_msch_regs->ddrtimingc0);
		writel(noc_timing->devtodev0,
		       &ddr_msch_regs->devtodev0);
		writel(noc_timing->ddrmode,
		       &ddr_msch_regs->ddrmode);

		/* rank 1 memory clock disable (dfi_dram_clk_disable = 1) */
		if (sdram_params->ch[channel].rank == 1)
			setbits_le32(&dram->chan[channel].pctl->denali_ctl[276],
				     1 << 17);
	}

	writel(sys_reg, &dram->pmugrf->os_reg2);
	rk_clrsetreg(&dram->pmusgrf->soc_con4, 0x1f << 10,
		     sdram_params->base.stride << 10);

	/* reboot hold register set */
	writel(PRESET_SGRF_HOLD(0) | PRESET_GPIO0_HOLD(1) |
		PRESET_GPIO1_HOLD(1),
		&dram->pmucru->pmucru_rstnhold_con[1]);
	clrsetbits_le32(&dram->cru->glb_rst_con, 0x3, 0x3);
}

static int switch_to_phy_index(struct dram_info *dram,
			       const struct rk3399_sdram_params *sdram_params,
			       u32 index)
{
	u32 channel;
	u32 *denali_phy;
	u32 ch_count = sdram_params->base.num_channels;
	int ret;
	int i = 0;

	writel(RK_CLRSETBITS(0x03 << 4 | 1 << 2 | 1,
			     (index & 0x3) << 4 | 1 << 2 | 1),
			&dram->cic->cic_ctrl0);
	while (!(readl(&dram->cic->cic_status0) & (1 << 2))) {
		mdelay(10);
		i++;
		if (i > 10) {
			debug("index1 frequency change overtime\n");
			return -ETIME;
		}
	}

	i = 0;
	writel(RK_CLRSETBITS(1 << 1, 1 << 1), &dram->cic->cic_ctrl0);
	while (!(readl(&dram->cic->cic_status0) & (1 << 0))) {
		mdelay(10);
		i++;
		if (i > 10) {
			debug("index1 frequency done overtime\n");
			return -ETIME;
		}
	}

	for (channel = 0; channel < ch_count; channel++) {
		denali_phy = dram->chan[channel].publ->denali_phy;
		clrsetbits_le32(&denali_phy[896], (0x3 << 8) | 1, (index & 3) << 8);
		debug("data training\n");
		ret = data_training(&dram->chan[channel], channel,
				    sdram_params, PI_FULL_TRAINING);
		if (ret) {
			debug("index1 training failed\n");
			return ret;
		}
	}

	return 0;
}

static int sdram_init(struct udevice *dev, struct dram_info *dram,
		      const struct rk3399_sdram_params *sdram_params)
{
	unsigned char dramtype = sdram_params->base.dramtype;
	unsigned int ddr_freq = sdram_params->base.ddr_freq;
	int channel;

	debug("Starting SDRAM initialization...\n");

	if ((dramtype == DDR3 && ddr_freq > 933) ||
	    (dramtype == LPDDR3 && ddr_freq > 933) ||
	    (dramtype == LPDDR4 && ddr_freq > 800)) {
		debug("SDRAM frequency is to high!");
		return -E2BIG;
	}

	for (channel = 0; channel < 2; channel++) {
		const struct chan_info *chan = &dram->chan[channel];
		struct rk3399_ddr_publ_regs *publ = chan->publ;

		phy_dll_bypass_set(publ, ddr_freq, channel, 0, dramtype);
		phy_dll_bypass_set(publ, ddr_freq, channel, 1, dramtype);

		if (channel >= sdram_params->base.num_channels)
			continue;

		if (pctl_cfg(dev, chan, channel, sdram_params) != 0) {
			printf("pctl_cfg fail, reset\n");
			return -EIO;
		}

		/* LPDDR2/LPDDR3 need to wait DAI complete, max 10us */
		if (dramtype == LPDDR3)
			udelay(10);

		if (data_training(chan, channel,
				  sdram_params, PI_FULL_TRAINING)) {
			printf("SDRAM initialization failed, reset\n");
			return -EIO;
		}

		set_ddrconfig(chan, sdram_params, channel,
			      sdram_params->ch[channel].ddrconfig);
	}
	dram_all_config(dram, sdram_params);
	switch_to_phy_index(dram, sdram_params, 1);

	debug("Finish SDRAM initialization...\n");
	return 0;
}

static inline u16 crc16(const u8 *ptr, int count)
{
	int crc = 0;
	int i;

	while (--count >= 0) {
		crc = crc ^ (int)*ptr++ << 8;
		for (i = 0; i < 8; ++i)
			if (crc & 0x8000)
				crc = crc << 1 ^ 0x1021;
			else
				crc = crc << 1;
	}

	return crc;
}

/*
 * CAS Latency Calculation (as per "Annex K")
 * -----------------------
 * CAS latency is not a purely analog value as DDR3 SDRAMs use the DLL
 * to synchronize data and strobe outputs with the clock. All possible
 * frequencies may not be tested, therefore an application should use
 * the next smaller JEDEC standard tCKmin value (2.5, 1.875, 1.5,
 * 1.25, 1.071, and 0.938 ns for DDR3 SDRAMs) when calculating CAS
 * Latency. This section shows how the BIOS may calculate CAS latency
 * based on Bytes 12 ~ 16, 34, and 35.
 *
 * Step 1: Determine the common set of supported CAS Latency values
 *         for all modules on the memory channel using the CAS
 *         Latencies Supported in SPD bytes 14 and 15.
 *
 * Step 2: Determine tAAmin(all) which is the largest tAAmin value for
 *         all modules on the memory channel (SPD bytes 16 and 35).
 *
 * Step 3: Determine tCKmin(all) which is the largest tCKmin value for
 *         all modules on the memory channel (SPD bytes 12 and 34).
 *
 * Step 4: For a proposed tCK value (tCKproposed) between tCKmin(all)
 *         and tCKmax, determine the desired CAS Latency. If
 *         tCKproposed is not a standard JEDEC value (2.5, 1.875, 1.5,
 *         1.25, 1.071, or 0.938 ns) then tCKproposed must be adjusted
 *         to the next lower standard tCK value for calculating
 *         CLdesired.
 *
 *         CLdesired = ceiling (tAAmin(all) / tCKproposed)
 *            where tAAmin is defined in Byte 16 and Byte 35.
 *
 *         The ceiling function requires that the quotient be rounded
 *         up always.
 *
 * Step 5: Chose an actual CAS Latency (CLactual) that is greater than
 *         or equal to CLdesired and is supported by all modules on
 *         the memory channel as determined in step 1. If no such
 *         value exists, choose a higher tCKproposed value and repeat
 *         steps 4 and 5 until a solution is found.
 *
 * Step 6: Once the calculation of CLactual is completed, the BIOS
 *         must also verify that this CAS Latency value does not
 *         exceed tAAmax, which is 20 ns for all DDR3 speed grades, by
 *         multiplying CLactual times tCKproposed. If not, choose a
 *         lower CL value and repeat steps 5 and 6 until a solution is
 *         found.
 */

static inline u32 subps_to_cycles(const u32 subps, const u32 tck)
{
	return DIV_ROUND_UP(subps, tck);
}

static inline u32 ns_to_cycles(const u16 ns, const u32 tck)
{
	const u32 ns_as_subps = ns * 10000;
	return subps_to_cycles(ns_as_subps, tck);
}

static int spd_decode_ddr3(const u8 *spd, const u32 freq, struct dram_timing_t *timing)
{
	int n_crc = 126;
	/* u32 val; */

	/* u32 capacity_shift; */
	/* u32 banks_shift; */
	u32 capacity, banks;
	u32 rows, cols;
	u32 width, bus_width;
	u32 ranks;
	/* u32 dramds; */
	/* u32 dramodt; */

#if defined(DEBUG)
	const u8 SPD_VDD_1_25V = (1 << 2);
	const u8 SPD_VDD_1_35V = (1 << 1);
	const u8 SPD_VDD_1_50V = (1 << 0);
	bool supports_1_25V = false;
	bool supports_1_35V = false;
	bool supports_1_50V = false;
#endif

	/* Check if the CRC covers only 0~116 instead of 0~125 */
	if (spd[0] & 0x80)
		n_crc = 117;

	debug("\nCRC expected: 0x%04x\n", crc16(spd, n_crc));

	/* Number of memory banks */
	banks = 1 << ((spd[4] >> 4) & 0x07);
	if (banks > 8) {
		debug("  Invalid number of memory banks\n");
		//		ret = SPD_STATUS_INVALID_FIELD;
	}

	/* SDRAM capacity */
	capacity = 256 << (spd[4] & 0x0f);
	if (capacity > 8192) {
		debug("  Invalid module capacity\n");
		//		ret = SPD_STATUS_INVALID_FIELD;
	}

	/* Row address bits */
	rows = 12 + ((spd[5] >> 3) & 0x07);
	if (rows > 16) {
		debug("  Invalid row address bits\n");
		//		ret = SPD_STATUS_INVALID_FIELD;
	}

	/* Column address bits */
	cols = 9 + (spd[5] & 0x07);
	if (cols > 12) {
		debug("  Invalid column address bits\n");
		//		ret = SPD_STATUS_INVALID_FIELD;
	}

#if defined(DEBUG)
	/* Module nominal voltage */
	if (spd[6] & SPD_VDD_1_25V)
		supports_1_25V = true;

	if (spd[6] & SPD_VDD_1_35V)
		supports_1_35V = true;

	if (spd[6] & SPD_VDD_1_50V)
		supports_1_50V = true;
#endif

	/* Number of ranks */
	ranks = 1 + ((spd[7] >> 3) & 0x07);
	if (ranks > 2) {
		debug("  Unsupported number of ranks\n");
		//		ret = SPD_STATUS_INVALID_FIELD;
	}

	/* SDRAM device width */
	width = 4 << (spd[7] & 0x07);
	if (width > 32) {
		debug("  Invalid SDRAM width\n");
		//		ret = SPD_STATUS_INVALID_FIELD;
	}
	//	dimm->width = (4 << val);
	//	printram("  SDRAM width        : %u\n", dimm->width);

	/* Bus extension */
	/* --> ECC not implemented on the RK3399 */

	/* Bus width */
	bus_width = 8 << (spd[8] & 0x07);
	if (bus_width > 32) {
		printf("  Unsupported bus width\n");
		//		ret = SPD_STATUS_INVALID_FIELD;
	}

#if defined(DEBUG)
	u32 size_mb = (capacity / 8) * (bus_width / width) * ranks;
#endif

        /* Fine Timebase (FTB) Dividend/Divisor */
	const u32 ftb_dividend = (spd[9] >> 4) & 0x0f;
	const u32 ftb_divisor = spd[9] & 0x0f;
	const u32 ftb = (ftb_dividend * 10) / ftb_divisor;

	/* Medium Timebase (MTB) */
	const u32 mtb = (((u32) spd[10]) * 10000) / spd[11];

	/* tXX = tXX(MTB) * MTB + tXX(FTB) * FTB */
	const u32 tCKmin = spd[12] * mtb + (s8)spd[34] * ftb;
	const u32 tAAmin = spd[16] * mtb + (s8)spd[35] * ftb;
	const u32 tWRmin = spd[17] * mtb;
	const u32 tRCDmin = spd[18] * mtb + (s8)spd[36] * ftb;
	const u32 tRRDmin = spd[19] * mtb;
#if defined(DEBUG)
	const u32 tRPmin = spd[20] * mtb + (s8)spd[37] * ftb;
#endif
	const u32 tRASmin = (((spd[21] & 0x0f) << 8) + spd[22]) * mtb;
	const u32 tRCmin = (((spd[21] >> 4) << 8) + spd[23]) * mtb + (s8)spd[38] * ftb;
	const u32 tRFCmin = (spd[24] + (spd[25] << 8)) * mtb;
	const u32 tWTRmin = spd[26] * mtb;
	const u32 tRTPmin = spd[27] * mtb;
	const u32 tFAWmin = (((spd[28] & 0x0f) << 8) + spd[29]) * mtb;
	/*
	 * Supported CAS latencies are encoded in a bitmap to be
	 * tested as
	 *   BIT(CL - 4)
	 */
	const u16 CL_supported = (spd[15] << 8) | spd[14];

	const u32 tCK = 10000000 / freq;

#if defined(DEBUG)
	printf("    tCK              : %u ps (@ %u MHz)\n", tCK / 10, freq);
	if (tCK < tCKmin) {
		printf("    => violates tCKmin\n");
		// ret = ...
	}
	printf("    tAAmin (cycles)  : %u tCK\n", (tAAmin + tCK - 1) / tCK);
	printf("    tCKmin           : %u ps\n", tCKmin / 10);
	printf("    tAAmin           : %u ps\n", tAAmin / 10);
	printf("    tWRmin           : %u ps\n", tWRmin / 10);
	printf("    tRCDmin          : %u ps\n", tRCDmin / 10);
	printf("    tRRDmin          : %u ps\n", tRRDmin / 10);
	printf("    tRPmin           : %u ps\n", tRPmin / 10);
	printf("    tRCmin           : %u ps\n", tRCmin / 10);

	printf("  Banks              : %u\n", banks);
	printf("  Number of ranks    : %u\n", ranks);
	printf("  Capacity           : %u Mib\n", capacity);
	printf("  Supported voltages : ");
	if (supports_1_25V)
		printf("1.25V ");
	if (supports_1_35V)
		printf("1.35V ");
	if (supports_1_50V)
		printf("1.50V ");
	printf("\n");
	printf("  Device width       : %u\n", width);
	printf("  Bus width          : %u\n", bus_width);
	printf("  Row    addr bits   : %u\n", rows);
	printf("  Column addr bits   : %u\n", cols);
	printf("  Channel Capacity   : %u MiB\n", size_mb);
#endif

	/* Calculate the CAS latency to be used */
	const u32 CLdesired = DIV_ROUND_UP(tAAmin, tCK);
	debug("CLdesired %d\n", CLdesired);
	/* TODO: find the smallest CL greater-equal CLdesired */
	u32 CLactual;
	for (CLactual = CLdesired; CLactual < 19; ++CLactual)
		if (CL_supported & BIT(CLactual - 4))
			break;

	if (CLactual == 19) {
		debug("no CAS latency found\n");
		return -1;
	}

	u32 tCL = CLactual * tCK;
	debug("-- tCL: %u ps (must be <= 20ns)\n", tCL / 10);
	debug("   CL: %d\n", CLactual);

	/*
	 * In DDR3, only one CWL is valid for a given clock frequency
	 * range.  For this reason, there is no entry encoding the CWL
	 * values in the SPD.
	 * The valid CWL values (in respect to the corresponding tCK
	 * values) are:
	 *   - CWL = 5, for tCKavg in [2.5ns; 3.3ns)
	 *   - CWL = 6, for tCKavg in [1.875ns; 2.5ns)
	 *   - CWL = 7, for tCKavg in [1.5ns; 1.875ns)
	 *   - CWL = 8, for tCKavg in [1.25ns; 1.5ns)
	 *   - CWL = 9, for tCKavg in [1.07ns; 1.25ns)
	 *   - CWL = 10, for tCKavg in [0.938ns; 1.07ns)
	 */

	u8 CWL = 0;
	if (tCK >= 25000)       /* 2500.0ps */
		CWL = 5;
	else if (tCK >= 18750)  /* 1875.0ps */
		CWL = 6;
	else if (tCK >= 15000)  /* 1500.0ps */
		CWL = 7;
	else if (tCK >= 12500)  /* 1250.0ps */
		CWL = 8;
	else if (tCK >= 10700)  /* 1070.0ps */
		CWL = 9;
	else if (tCK >= 9380)   /* 938.0ps */
		CWL = 10;

	debug(" CWL: %d\n", CWL);
	if (!CWL) {
		printf("could not find a valid CWL\n");
		// handle error
	}

	timing->mhz = freq;

	timing->trefi = ns_to_cycles(7800, tCK);  /* 7.8us */
	timing->al = 0;
	timing->bl = 8;

	timing->cl = CLactual;
	timing->cwl = CWL;

	timing->trstl = ns_to_cycles(100, tCK);            /* 100 ns */
	timing->trsth = DIV_ROUND_UP(500000 * freq, 1000); /* 500 us */

	timing->trcd = max(4, subps_to_cycles(tRCDmin, tCK));
	timing->trp = timing->cl;
	timing->trppb = timing->cl;
	timing->twr = subps_to_cycles(tWRmin, tCK);
	timing->tdal = timing->twr + timing->trp;
	timing->trtp = max(4, subps_to_cycles(tRTPmin, tCK));
	timing->trc = max(4, subps_to_cycles(tRCmin, tCK));
	timing->trrd = max(4, subps_to_cycles(tRRDmin, tCK));
	timing->tccd = 4;
	timing->twtr = max(4, subps_to_cycles(tWTRmin, tCK));
	timing->trtw = 0;
	timing->tras_max = 9 * timing->trefi;
	timing->tras_min = subps_to_cycles(tRASmin, tCK);
	timing->tfaw = subps_to_cycles(tFAWmin, tCK);
	timing->trfc = subps_to_cycles(tRFCmin, tCK);
	timing->txsnr = max(5, subps_to_cycles(tRFCmin + 10000, tCK));
	timing->tdqsck_max = 0;

	/* pd and sr */
	timing->tdllk = 512;  /* always 512 cycles for DDR3 */
	timing->txsr = timing->tdllk;
	timing->txp = max(3, ns_to_cycles(7, tCK));
	timing->txpdll = max(10, ns_to_cycles(10, tCK));

	if (tCKmin > 18750)  /* DDR-800 or slower */
		timing->tcke = max(3, subps_to_cycles(75000, tCK)); /* 7.5ns */
	else if (tCKmin > 12500) /* DDR-1066 */
		timing->tcke = max(3, subps_to_cycles(56250, tCK)); /* 5.625ns */
	else
		timing->tcke = max(3, subps_to_cycles(50000, tCK)); /* 5ns */

	timing->tckesr = timing->tcke + 1;  // CHECK
	timing->tcksre = max(5, ns_to_cycles(10, tCK));
	timing->tcksrx = timing->tcksre;

	/* mode register timing */
	timing->tmod = max(12, ns_to_cycles(15, tCK));
	timing->tmrd = 4; /* always 4 cycles for DDR3 */
	timing->tmrr = 0;

	/* ODT */
	timing->todton = timing->cwl - 2;

	/* ZQ */
	timing->tzqinit = max(512, ns_to_cycles(640, tCK));
	timing->tzqcs = max(64, ns_to_cycles(80, tCK));
	timing->tzqoper = max(256, ns_to_cycles(320, tCK));

	/* write leveling */
	timing->twlmrd = 40;
	timing->twldqsen = 25;
	timing->twlo = ns_to_cycles(9, tCK);

	timing->mr[0] =
		((timing->bl == 8) ? DDR3_BL8 : DDR3_BC4) |
		DDR3_CL(timing->cl) |
		DDR3_WR(timing->twr);
	timing->mr[1] = 0;

	/* TODO: set timing->odt = */

	u8 odt_offset = 72;
	/* extract ODT settings from SPD */
	if (tCK >= 18750)       /* 533MHz and less */
		odt_offset = 72;
		// 72, 73, 77
	else if (tCK >= 12500)  /* (533MHz; 800Mhz] */
		// 78, 79, 83
		odt_offset = 78;
	else if (tCK >= 9380)  /* (800MHz, 1066MHz] */
		// 84, 85, 89
		odt_offset = 84;

	debug("MR12 byte (spd[%d]): 0x%02x\n", odt_offset + 5, spd[odt_offset + 5]);

	switch (spd[odt_offset + 5] & 3) {
	case 0:
		/* dramds = 40; */
		timing->mr[1] = DDR3_DS_40;
		break;
	case 1:
		/* dramds = 34; */
		timing->mr[1] = DDR3_DS_34;
		break;
	default:
		// err
		break;
	}

	switch ((spd[odt_offset + 5] >> 2) & 0x7) {
	case 0:
		/* dramodt = 0; */ /* disabled */
		timing->mr[1] |= DDR3_RTT_NOM_DIS;
		break;
	case 1:
		/* dramodt = 60; */ /* RZQ/4 */
		timing->mr[1] |= DDR3_RTT_NOM_60;
		break;
	case 2:
		/* dramodt = 120; */ /* RZQ/2 */
		timing->mr[1] |= DDR3_RTT_NOM_120;
		break;
	case 3:
		/* dramodt = 40; */ /* RZQ/6 */
		timing->mr[1] |= DDR3_RTT_NOM_40;
		break;
	default:
		// err
		break;
	}

	/* mode register settings */
	timing->mr[2] = DDR3_MR2_CWL(timing->cwl);
	timing->mr[3] = 0;

	return 0;
}

static int rk3399_dmc_ofdata_to_platdata(struct udevice *dev)
{
#if !CONFIG_IS_ENABLED(OF_PLATDATA)
	struct rockchip_dmc_plat *plat = dev_get_platdata(dev);
	int ret;
	const void *spd_data;
	int spd_len;

	/*
	 * To support a single design with multiple different DRAM
	 * configurations (i.e. memory capacity and speed-grades) with
	 * a single U-Boot binary, the contents of a DDR3 SPD (based
	 * JEDEC document "Annex K: Serial Presence Detect (SPD) for
	 * DDR3 SDRAM Modules") can be retrieved from an EEPROM
	 * referenced by 'theobroma-systems,spd-eeprom'.  If SPD data
	 * is present, it is used to auto-configure the DRAM busses.
	 *
	 * Note the following RK3399-specific usage/subset of the SPD
	 * specification:
	 * - The "key byte" (byte 3), indicating the encoding of
	 *   'module specific section' (bytes 60~116) and the physical
	 *   dimensions should always be 0x00 (undefined).
	 *   However, the 'module specific section' should use the
	 *   encoding specified for an LRDIMM module for bytes 67~89
	 *   to store drive strength parameters, ODT control and MR1
	 *   and MR2 setting for various operating points using the
	 *   encodings defined for LRDIMM modules.
	 * - RK3399-specific data should be stored in bytes 150~175.
	 */

	struct udevice *spd_eeprom = NULL;
        /* get rockchip grf syscon phandle */
	ret = uclass_get_device_by_phandle(UCLASS_I2C_EEPROM, dev,
					   "theobroma-systems,spd-eeprom",
					   &spd_eeprom);
	if (ret)
		debug("unable to find theobroma-systems,spd-eeprom (%d)\n", ret);

	if (spd_eeprom) {
		i2c_eeprom_read(spd_eeprom, 0, &plat->spd[0], 128);
#if defined(DEBUG)
		print_buffer(0, &plat->spd[0], 1, 128, 16);
#endif
	}

	/*
	 * If we don't have a SPD EEPROM on board, try to retrieve SPD
	 * data (from a 'theobroma-systems,spd-data' property) from
	 * the DTS.  This fallback may also be used during factory
	 * programming, when the SPD has not yet been programmed.
	 */
	spd_data = dev_read_prop(dev, "theobroma-systems,spd-data", &spd_len);
	if (spd_data)
		memcpy(&plat->spd[0], spd_data, spd_len);

	/*
	 * And if all else fails, fall back to the (full) register-dump stored
	 * as 'rockchip,sdram-params'.
	 */
	ret = dev_read_u32_array(dev, "rockchip,sdram-params",
				 (u32 *)&plat->sdram_params,
				 sizeof(plat->sdram_params) / sizeof(u32));
	if (ret) {
		printf("%s: Cannot read rockchip,sdram-params %d\n",
		       __func__, ret);
		return ret;
	}
	ret = regmap_init_mem(dev_ofnode(dev), &plat->map);
	if (ret)
		printf("%s: regmap failed %d\n", __func__, ret);
#endif
	return 0;
}

#if CONFIG_IS_ENABLED(OF_PLATDATA)
static int conv_of_platdata(struct udevice *dev)
{
	struct rockchip_dmc_plat *plat = dev_get_platdata(dev);
	struct dtd_rockchip_rk3399_dmc *dtplat = &plat->dtplat;
	int ret;

	ret = regmap_init_mem_platdata(dev, dtplat->reg,
			ARRAY_SIZE(dtplat->reg) / 2,
			&plat->map);
	if (ret)
		return ret;

	return 0;
}
#endif

static int rk3399_dmc_init(struct udevice *dev)
{
	struct dram_info *priv = dev_get_priv(dev);
	struct rockchip_dmc_plat *plat = dev_get_platdata(dev);
	int ret;
#if !CONFIG_IS_ENABLED(OF_PLATDATA)
	struct rk3399_sdram_params *params = &plat->sdram_params;
#else
	struct dtd_rockchip_rk3399_dmc *dtplat = &plat->dtplat;
	struct rk3399_sdram_params *params =
					(void *)dtplat->rockchip_sdram_params;

	ret = conv_of_platdata(dev);
	if (ret)
		return ret;
#endif

	priv->cic = syscon_get_first_range(ROCKCHIP_SYSCON_CIC);
	priv->pmugrf = syscon_get_first_range(ROCKCHIP_SYSCON_PMUGRF);
	priv->pmusgrf = syscon_get_first_range(ROCKCHIP_SYSCON_PMUSGRF);
	priv->pmucru = rockchip_get_pmucru();
	priv->cru = rockchip_get_cru();
	priv->chan[0].pctl = regmap_get_range(plat->map, 0);
	priv->chan[0].pi = regmap_get_range(plat->map, 1);
	priv->chan[0].publ = regmap_get_range(plat->map, 2);
	priv->chan[0].msch = regmap_get_range(plat->map, 3);
	priv->chan[1].pctl = regmap_get_range(plat->map, 4);
	priv->chan[1].pi = regmap_get_range(plat->map, 5);
	priv->chan[1].publ = regmap_get_range(plat->map, 6);
	priv->chan[1].msch = regmap_get_range(plat->map, 7);

	debug("con reg %p %p %p %p %p %p %p %p\n",
	      priv->chan[0].pctl, priv->chan[0].pi,
	      priv->chan[0].publ, priv->chan[0].msch,
	      priv->chan[1].pctl, priv->chan[1].pi,
	      priv->chan[1].publ, priv->chan[1].msch);
	debug("cru %p, cic %p, grf %p, sgrf %p, pmucru %p\n", priv->cru,
	      priv->cic, priv->pmugrf, priv->pmusgrf, priv->pmucru);
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	ret = clk_get_by_index_platdata(dev, 0, dtplat->clocks, &priv->ddr_clk);
#else
	ret = clk_get_by_index(dev, 0, &priv->ddr_clk);
#endif
	if (ret) {
		printf("%s clk get failed %d\n", __func__, ret);
		return ret;
	}
	debug("base.ddr_freq %d\n", params->base.ddr_freq);
	ret = clk_set_rate(&priv->ddr_clk, params->base.ddr_freq * MHz);
	if (ret < 0) {
		printf("%s clk set failed %d\n", __func__, ret);
		return ret;
	}
	ret = sdram_init(dev, priv, params);
	if (ret < 0) {
		printf("%s DRAM init failed%d\n", __func__, ret);
		return ret;
	}

#if defined(DEBUG)
	{
		*(volatile u64*)0 = 0x0123456789abcdef;
		printf("%08x %08x\n", *(volatile u32*)0, *(volatile u32*)4);
	}
#endif

	return 0;
}
#endif

static int rk3399_dmc_probe(struct udevice *dev)
{
#ifdef CONFIG_SPL_BUILD
	if (rk3399_dmc_init(dev))
		do_reset(NULL, 0, 0, NULL);
#else
	struct dram_info *priv = dev_get_priv(dev);

	priv->pmugrf = syscon_get_first_range(ROCKCHIP_SYSCON_PMUGRF);
	debug("%s: pmugrf=%p\n", __func__, priv->pmugrf);
	priv->info.base = CONFIG_SYS_SDRAM_BASE;
	priv->info.size = rockchip_sdram_size(
			(phys_addr_t)&priv->pmugrf->os_reg2);
#endif
	return 0;
}

static int rk3399_dmc_get_info(struct udevice *dev, struct ram_info *info)
{
	struct dram_info *priv = dev_get_priv(dev);

	*info = priv->info;

	return 0;
}

static struct ram_ops rk3399_dmc_ops = {
	.get_info = rk3399_dmc_get_info,
};


static const struct udevice_id rk3399_dmc_ids[] = {
	{ .compatible = "rockchip,rk3399-dmc" },
	{ }
};

U_BOOT_DRIVER(dmc_rk3399) = {
	.name = "rockchip_rk3399_dmc",
	.id = UCLASS_RAM,
	.of_match = rk3399_dmc_ids,
	.ops = &rk3399_dmc_ops,
#ifdef CONFIG_SPL_BUILD
	.ofdata_to_platdata = rk3399_dmc_ofdata_to_platdata,
#endif
	.probe = rk3399_dmc_probe,
	.priv_auto_alloc_size = sizeof(struct dram_info),
#ifdef CONFIG_SPL_BUILD
	.platdata_auto_alloc_size = sizeof(struct rockchip_dmc_plat),
#endif
};
