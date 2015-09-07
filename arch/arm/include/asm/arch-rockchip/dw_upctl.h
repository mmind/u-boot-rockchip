/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef _ASM_ARCH_DW_UPCTL_H
#define _ASM_ARCH_DW_UPCTL_H

struct dw_upctl {
	u32 scfg;
	u32 sctl;
	u32 stat;
	u32 intrstat;
	u32 reserved0[12];
	u32 mcmd;
	u32 powctl;
	u32 powstat;
	u32 cmdtstat;
	u32 tstaten;
	u32 reserved1[3];
	u32 mrrcfg0;
	u32 mrrstat0;
	u32 mrrstat1;
	u32 reserved2[4];
	u32 mcfg1;
	u32 mcfg;
	u32 ppcfg;
	u32 mstat;
	u32 lpddr2zqcfg;
	u32 reserved3;
	u32 dtupdes;
	u32 dtuna;
	u32 dtune;
	u32 dtuprd0;
	u32 dtuprd1;
	u32 dtuprd2;
	u32 dtuprd3;
	u32 dtuawdt;
	u32 reserved4[3];
	u32 togcnt1u;
	u32 tinit;
	u32 trsth;
	u32 togcnt100n;
	u32 trefi;
	u32 tmrd;
	u32 trfc;
	u32 trp;
	u32 trtw;
	u32 tal;
	u32 tcl;
	u32 tcwl;
	u32 tras;
	u32 trc;
	u32 trcd;
	u32 trrd;
	u32 trtp;
	u32 twr;
	u32 twtr;
	u32 texsr;
	u32 txp;
	u32 txpdll;
	u32 tzqcs;
	u32 tzqcsi;
	u32 tdqs;
	u32 tcksre;
	u32 tcksrx;
	u32 tcke;
	u32 tmod;
	u32 trstl;
	u32 tzqcl;
	u32 tmrr;
	u32 tckesr;
	u32 tdpd;
	u32 reserved5[14];
	u32 ecccfg;
	u32 ecctst;
	u32 eccclr;
	u32 ecclog;
	u32 reserved6[28];
	u32 dtuwactl;
	u32 dturactl;
	u32 dtucfg;
	u32 dtuectl;
	u32 dtuwd0;
	u32 dtuwd1;
	u32 dtuwd2;
	u32 dtuwd3;
	u32 dtuwdm;
	u32 dturd0;
	u32 dturd1;
	u32 dturd2;
	u32 dturd3;
	u32 dtulfsrwd;
	u32 dtulfsrrd;
	u32 dtueaf;
	u32 dfitctrldelay;
	u32 dfiodtcfg;
	u32 dfiodtcfg1;
	u32 dfiodtrankmap;
	u32 dfitphywrdata;
	u32 dfitphywrlat;
	u32 reserved7[2];
	u32 dfitrddataen;
	u32 dfitphyrdlat;
	u32 reserved8[2];
	u32 dfitphyupdtype0;
	u32 dfitphyupdtype1;
	u32 dfitphyupdtype2;
	u32 dfitphyupdtype3;
	u32 dfitctrlupdmin;
	u32 dfitctrlupdmax;
	u32 dfitctrlupddly;
	u32 reserved9;
	u32 dfiupdcfg;
	u32 dfitrefmski;
	u32 dfitctrlupdi;
	u32 reserved10[4];
	u32 dfitrcfg0;
	u32 dfitrstat0;
	u32 dfitrwrlvlen;
	u32 dfitrrdlvlen;
	u32 dfitrrdlvlgateen;
	u32 dfiststat0;
	u32 dfistcfg0;
	u32 dfistcfg1;
	u32 reserved11;
	u32 dfitdramclken;
	u32 dfitdramclkdis;
	u32 dfistcfg2;
	u32 dfistparclr;
	u32 dfistparlog;
	u32 reserved12[3];
	u32 dfilpcfg0;
	u32 reserved13[3];
	u32 dfitrwrlvlresp0;
	u32 dfitrwrlvlresp1;
	u32 dfitrwrlvlresp2;
	u32 dfitrrdlvlresp0;
	u32 dfitrrdlvlresp1;
	u32 dfitrrdlvlresp2;
	u32 dfitrwrlvldelay0;
	u32 dfitrwrlvldelay1;
	u32 dfitrwrlvldelay2;
	u32 dfitrrdlvldelay0;
	u32 dfitrrdlvldelay1;
	u32 dfitrrdlvldelay2;
	u32 dfitrrdlvlgatedelay0;
	u32 dfitrrdlvlgatedelay1;
	u32 dfitrrdlvlgatedelay2;
	u32 dfitrcmd;
	u32 reserved14[46];
	u32 ipvr;
	u32 iptr;
};
check_member(dw_upctl, iptr, 0x03fc);

/* PCTL_DFISTCFG0 */
#define DFI_INIT_START			(1 << 0)

/* PCTL_DFISTCFG1 */
#define DFI_DRAM_CLK_SR_EN		(1 << 0)
#define DFI_DRAM_CLK_DPD_EN		(1 << 1)

/* PCTL_DFISTCFG2 */
#define DFI_PARITY_INTR_EN		(1 << 0)
#define DFI_PARITY_EN			(1 << 1)

/* PCTL_DFILPCFG0 */
#define TLP_RESP_TIME_SHIFT		16
#define LP_SR_EN			(1 << 8)
#define LP_PD_EN			(1 << 0)

/* PCTL_DFITCTRLDELAY */
#define TCTRL_DELAY_TIME_SHIFT		0

/* PCTL_DFITPHYWRDATA */
#define TPHY_WRDATA_TIME_SHIFT		0

/* PCTL_DFITPHYRDLAT */
#define TPHY_RDLAT_TIME_SHIFT		0

/* PCTL_DFITDRAMCLKDIS */
#define TDRAM_CLK_DIS_TIME_SHIFT	0

/* PCTL_DFITDRAMCLKEN */
#define TDRAM_CLK_EN_TIME_SHIFT		0

/* PCTL_DFIODTCFG */
#define RANK0_ODT_WRITE_SEL		(1 << 3)
#define RANK1_ODT_WRITE_SEL		(1 << 11)

/* PCTL_DFIODTCFG1 */
#define ODT_LEN_BL8_W_SHIFT		16

/* PCTL_SCTL */
#define INIT_STATE			0
#define CFG_STATE			1
#define GO_STATE			2
#define SLEEP_STATE			3
#define WAKEUP_STATE			4

/* PCTL_STAT */
#define LP_TRIG_SHIFT			4
#define LP_TRIG_MASK			7
#define PCTL_STAT_MSK			7
#define INIT_MEM			0
#define CONFIG				1
#define CONFIG_REQ			2
#define ACCESS				3
#define ACCESS_REQ			4
#define LOW_POWER			5
#define LOW_POWER_ENTRY_REQ		6
#define LOW_POWER_EXIT_REQ		7

/* PCTL_MCFG */
#define MDDR_LPDDR2_CLK_STOP_IDLE_SHIFT	24
#define PD_IDLE_SHIFT			8
#define MDDR_EN				(2 << 22)
#define LPDDR2_EN			(3 << 22)
#define DDR2_EN				(0 << 5)
#define DDR3_EN				(1 << 5)
#define LPDDR2_S2			(0 << 6)
#define LPDDR2_S4			(1 << 6)
#define MDDR_LPDDR2_BL_2		(0 << 20)
#define MDDR_LPDDR2_BL_4		(1 << 20)
#define MDDR_LPDDR2_BL_8		(2 << 20)
#define MDDR_LPDDR2_BL_16		(3 << 20)
#define DDR2_DDR3_BL_4			0
#define DDR2_DDR3_BL_8			1
#define TFAW_SHIFT			18
#define PD_EXIT_SLOW			(0 << 17)
#define PD_EXIT_FAST			(1 << 17)
#define PD_TYPE_SHIFT			16
#define BURSTLENGTH_SHIFT		20

/* PCTL_POWCTL */
#define POWER_UP_START			(1 << 0)

/* PCTL_POWSTAT */
#define POWER_UP_DONE			(1 << 0)

/* PCTL_MCMD */
enum {
	DESELECT_CMD			= 0,
	PREA_CMD,
	REF_CMD,
	MRS_CMD,
	ZQCS_CMD,
	ZQCL_CMD,
	RSTL_CMD,
	MRR_CMD				= 8,
	DPDE_CMD,
};

#define LPDDR2_MA_SHIFT			4
#define LPDDR2_MA_MASK			0xff
#define LPDDR2_OP_SHIFT			12
#define LPDDR2_OP_MASK			0xff

#define START_CMD			(1u << 31)

struct dw_upctl_sdram_timing {
	u32 togcnt1u;
	u32 tinit;
	u32 trsth;
	u32 togcnt100n;
	u32 trefi;
	u32 tmrd;
	u32 trfc;
	u32 trp;
	u32 trtw;
	u32 tal;
	u32 tcl;
	u32 tcwl;
	u32 tras;
	u32 trc;
	u32 trcd;
	u32 trrd;
	u32 trtp;
	u32 twr;
	u32 twtr;
	u32 texsr;
	u32 txp;
	u32 txpdll;
	u32 tzqcs;
	u32 tzqcsi;
	u32 tdqs;
	u32 tcksre;
	u32 tcksrx;
	u32 tcke;
	u32 tmod;
	u32 trstl;
	u32 tzqcl;
	u32 tmrr;
	u32 tckesr;
	u32 tdpd;
};
check_member(dw_upctl_sdram_timing, tdpd, 0x144 - 0xc0);

#endif
