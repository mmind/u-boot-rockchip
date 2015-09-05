/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef _ASM_ARCH_DW_PUBL_H
#define _ASM_ARCH_DW_PUBL_H

struct dw_publ_datx {
	u32 dxgcr;
	u32 dxgsr[2];
	u32 dxdllcr;
	u32 dxdqtr;
	u32 dxdqstr;
	u32 reserved[10];
};

struct dw_publ {
	u32 ridr;
	u32 pir;
	u32 pgcr;
	u32 pgsr;
	u32 dllgcr;
	u32 acdllcr;
	u32 ptr[3];
	u32 aciocr;
	u32 dxccr;
	u32 dsgcr;
	u32 dcr;
	u32 dtpr[3];
	u32 mr[4];
	u32 odtcr;
	u32 dtar;
	u32 dtdr[2];
	u32 reserved1[24];
	u32 dcuar;
	u32 dcudr;
	u32 dcurr;
	u32 dculr;
	u32 dcugcr;
	u32 dcutpr;
	u32 dcusr[2];
	u32 reserved2[8];
	u32 bist[17];
	u32 reserved3[15];
	u32 zq0cr[2];
	u32 zq0sr[2];
	u32 zq1cr[2];
	u32 zq1sr[2];
	u32 zq2cr[2];
	u32 zq2sr[2];
	u32 zq3cr[2];
	u32 zq3sr[2];
	struct dw_publ_datx datx8[4];
};
check_member(dw_publ, datx8[3].dxdqstr, 0x0294);

/* PUBL_ACDLLCR */
#define ACDLLCR_DLLDIS			(1 << 31)
#define ACDLLCR_DLLSRST			(1 << 30)

/* PUBL_DXDLLCR */
#define DXDLLCR_DLLDIS			(1 << 31)
#define DXDLLCR_DLLSRST			(1 << 30)

/* PUBL_DLLGCR */
#define DLLGCR_SBIAS			(1 << 30)

/* PUBL_DXGCR */
#define DQSRTT				(1 << 9)
#define DQRTT				(1 << 10)

/* PUBL_PIR */
#define PIR_INIT			(1 << 0)
#define PIR_DLLSRST			(1 << 1)
#define PIR_DLLLOCK			(1 << 2)
#define PIR_ZCAL			(1 << 3)
#define PIR_ITMSRST			(1 << 4)
#define PIR_DRAMRST			(1 << 5)
#define PIR_DRAMINIT			(1 << 6)
#define PIR_QSTRN			(1 << 7)
#define PIR_RVTRN			(1 << 8)
#define PIR_ICPC			(1 << 16)
#define PIR_DLLBYP			(1 << 17)
#define PIR_CTLDINIT			(1 << 18)
#define PIR_CLRSR			(1 << 28)
#define PIR_LOCKBYP			(1 << 29)
#define PIR_ZCALBYP			(1 << 30)
#define PIR_INITBYP			(1u << 31)

/* PUBL_PGCR */
#define PGCR_DFTLMT_SHIFT		3
#define PGCR_DFTCMP_SHIFT		2
#define PGCR_DQSCFG_SHIFT		1
#define PGCR_ITMDMD_SHIFT		0

/* PUBL_PGSR */
#define PGSR_IDONE			(1 << 0)
#define PGSR_DLDONE			(1 << 1)
#define PGSR_ZCDONE			(1 << 2)
#define PGSR_DIDONE			(1 << 3)
#define PGSR_DTDONE			(1 << 4)
#define PGSR_DTERR			(1 << 5)
#define PGSR_DTIERR			(1 << 6)
#define PGSR_DFTERR			(1 << 7)
#define PGSR_RVERR			(1 << 8)
#define PGSR_RVEIRR			(1 << 9)

/* PUBL_PTR0 */
#define PRT_ITMSRST_SHIFT		18
#define PRT_DLLLOCK_SHIFT		6
#define PRT_DLLSRST_SHIFT		0

/* PUBL_PTR1 */
#define PRT_DINIT0_SHIFT		0
#define PRT_DINIT1_SHIFT		19

/* PUBL_PTR2 */
#define PRT_DINIT2_SHIFT		0
#define PRT_DINIT3_SHIFT		17

/* PUBL_DCR */
#define DDRMD_LPDDR			0
#define DDRMD_DDR			1
#define DDRMD_DDR2			2
#define DDRMD_DDR3			3
#define DDRMD_LPDDR2_LPDDR3		4
#define DDRMD_MASK			7
#define DDRMD_SHIFT			0
#define PDQ_MASK			7
#define PDQ_SHIFT			4

/* PUBL_DXCCR */
#define DQSNRES_MASK			0xf
#define DQSNRES_SHIFT			8
#define DQSRES_MASK			0xf
#define DQSRES_SHIFT			4

/* PUBL_DTPR */
#define TDQSCKMAX_SHIFT			27
#define TDQSCKMAX_MASK			7
#define TDQSCK_SHIFT			24
#define TDQSCK_MASK			7

/* PUBL_DSGCR */
#define DQSGX_SHIFT			5
#define DQSGX_MASK			7
#define DQSGE_SHIFT			8
#define DQSGE_MASK			7

/* PUBL_ZQCR*/
#define PD_OUTPUT_SHIFT			0
#define PU_OUTPUT_SHIFT			5
#define PD_ONDIE_SHIFT			10
#define PU_ONDIE_SHIFT			15
#define ZDEN_SHIFT			28

/* PUBL_DDLGCR */
#define SBIAS_BYPASS			(1 << 23)

#endif
