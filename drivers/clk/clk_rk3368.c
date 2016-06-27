/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <syscon.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/cru_rk3368.h>
#include <asm/arch/grf_rk3368.h>
#include <asm/arch/hardware.h>
#include <dt-bindings/clock/rk3368-cru.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>

DECLARE_GLOBAL_DATA_PTR;

struct rk3368_clk_priv {
	struct rk3368_grf *grf;
	struct rk3368_cru *cru;
	ulong rate;
};

struct pll_div {
	u32 nr;
	u32 nf;
	u32 no;
};

enum {
	VCO_MAX_HZ	= 2200U * 1000000,
	VCO_MIN_HZ	= 440 * 1000000,
	OUTPUT_MAX_HZ	= 2200U * 1000000,
	OUTPUT_MIN_HZ	= 27500000,
	FREF_MAX_HZ	= 2200U * 1000000,
	FREF_MIN_HZ	= 269 * 1000000,
};

enum {
	/* PLL CON0 */
	PLL_OD_MASK		= 0x0f,

	/* PLL CON1 */
	PLL_NF_MASK		= 0x1fff,

	/* PLL CON2 */
	PLL_BWADJ_MASK		= 0x0fff,

	/* PLL CON3 */
	PLL_RESET_SHIFT		= 5,

	/* CLKSEL0 */
/*	CORE_SEL_PLL_MASK	= 1,
	CORE_SEL_PLL_SHIFT	= 15,
	A17_DIV_MASK		= 0x1f,
	A17_DIV_SHIFT		= 8,
	MP_DIV_MASK		= 0xf,
	MP_DIV_SHIFT		= 4,
	M0_DIV_MASK		= 0xf,
	M0_DIV_SHIFT		= 0,*/

	/* CLKSEL1: pd bus clk pll sel: codec or general */
/*	PD_BUS_SEL_PLL_MASK	= 15,
	PD_BUS_SEL_CPLL		= 0,
	PD_BUS_SEL_GPLL,*/

	/* pd bus pclk div: pclk = pd_bus_aclk /(div + 1) */
/*	PD_BUS_PCLK_DIV_SHIFT	= 12,
	PD_BUS_PCLK_DIV_MASK	= 7,*/

	/* pd bus hclk div: aclk_bus: hclk_bus = 1:1 or 2:1 or 4:1 */
//	PD_BUS_HCLK_DIV_SHIFT	= 8,
//	PD_BUS_HCLK_DIV_MASK	= 3,

	/* pd bus aclk div: pd_bus_aclk = pd_bus_src_clk /(div0 * div1) */
//	PD_BUS_ACLK_DIV0_SHIFT	= 3,
//	PD_BUS_ACLK_DIV0_MASK	= 0x1f,
//	PD_BUS_ACLK_DIV1_SHIFT	= 0,
//	PD_BUS_ACLK_DIV1_MASK	= 0x7,

	/*
	 * CLKSEL10
	 * peripheral bus pclk div:
	 * aclk_bus: pclk_bus = 1:1 or 2:1 or 4:1 or 8:1
	 */
//	PERI_SEL_PLL_MASK	 = 1,
//	PERI_SEL_PLL_SHIFT	 = 15,
//	PERI_SEL_CPLL		= 0,
//	PERI_SEL_GPLL,

//	PERI_PCLK_DIV_SHIFT	= 12,
//	PERI_PCLK_DIV_MASK	= 3,

	/* peripheral bus hclk div: aclk_bus: hclk_bus = 1:1 or 2:1 or 4:1 */
//	PERI_HCLK_DIV_SHIFT	= 8,
//	PERI_HCLK_DIV_MASK	= 3,

	/*
	 * peripheral bus aclk div:
	 *    aclk_periph = periph_clk_src / (peri_aclk_div_con + 1)
	 */
//	PERI_ACLK_DIV_SHIFT	= 0,
//	PERI_ACLK_DIV_MASK	= 0x1f,

	SOCSTS_APLLL_LOCK	= 1 << 0,
	SOCSTS_APLLB_LOCK	= 1 << 1,
	SOCSTS_DPLL_LOCK	= 1 << 2,
	SOCSTS_CPLL_LOCK	= 1 << 3,
	SOCSTS_GPLL_LOCK	= 1 << 4,
	SOCSTS_NPLL_LOCK	= 1 << 5,
};

#define RATE_TO_DIV(input_rate, output_rate) \
	((input_rate) / (output_rate) - 1);

#define DIV_TO_RATE(input_rate, div)	((input_rate) / ((div) + 1))

#define PLL_DIVISORS(hz, _nr, _no) {\
	.nr = _nr, .nf = (u32)((u64)hz * _nr * _no / OSC_HZ), .no = _no};\
	_Static_assert(((u64)hz * _nr * _no / OSC_HZ) * OSC_HZ /\
		       (_nr * _no) == hz, #hz "Hz cannot be hit with PLL "\
		       "divisors on line " __stringify(__LINE__));

/* Keep divisors as low as possible to reduce jitter and power usage */
static const struct pll_div apllb_init_cfg = PLL_DIVISORS(APLLB_HZ, 1, 2);
static const struct pll_div aplll_init_cfg = PLL_DIVISORS(APLLL_HZ, 1, 2);
static const struct pll_div gpll_init_cfg = PLL_DIVISORS(GPLL_HZ, 2, 2);
static const struct pll_div cpll_init_cfg = PLL_DIVISORS(CPLL_HZ, 1, 2);

void *rockchip_get_cru(void)
{
	struct rk3368_clk_priv *priv;
	struct udevice *dev;
	int ret;

	ret = uclass_get_device(UCLASS_CLK, 0, &dev);
	if (ret)
		return ERR_PTR(ret);

	priv = dev_get_priv(dev);

	return priv->cru;
}

static int rkclk_set_pll(struct rk3368_cru *cru, enum rk_clk_id clk_id,
			 const struct pll_div *div)
{
	int pll_id = rk_pll_id(clk_id);
	struct rk3368_pll *pll = &cru->pll[pll_id];
	/* All PLLs have same VCO and output frequency range restrictions. */
	uint vco_hz = OSC_HZ / 1000 * div->nf / div->nr * 1000;
	uint output_hz = vco_hz / div->no;

	debug("PLL at %x: nf=%d, nr=%d, no=%d, vco=%u Hz, output=%u Hz\n",
	      (uint)pll, div->nf, div->nr, div->no, vco_hz, output_hz);
	assert(vco_hz >= VCO_MIN_HZ && vco_hz <= VCO_MAX_HZ &&
	       output_hz >= OUTPUT_MIN_HZ && output_hz <= OUTPUT_MAX_HZ &&
	       (div->no == 1 || !(div->no % 2)));

	/* enter reset */
	rk_setreg(&pll->con3, 1 << PLL_RESET_SHIFT);

	rk_clrsetreg(&pll->con0,
		     CLKR_MASK << CLKR_SHIFT | PLL_OD_MASK,
		     ((div->nr - 1) << CLKR_SHIFT) | (div->no - 1));
	rk_clrsetreg(&pll->con1, CLKF_MASK, div->nf - 1);
	rk_clrsetreg(&pll->con2, PLL_BWADJ_MASK, (div->nf >> 1) - 1);

	udelay(10);

	/* return from reset */
	rk_clrreg(&pll->con3, 1 << PLL_RESET_SHIFT);

	return 0;
}

static inline unsigned int log2(unsigned int value)
{
	return fls(value) - 1;
}

static int rockchip_mac_set_clk(struct rk3368_cru *cru,
				  int periph, uint freq)
{
	/* Assuming mac_clk is fed by an external clock */
	rk_clrsetreg(&cru->cru_clksel_con[43],
		     RMII_EXTCLK_MASK << RMII_EXTCLK_SHIFT,
		     RMII_EXTCLK_SELECT_EXT_CLK << RMII_EXTCLK_SHIFT);

	 return 0;
}

void rkclk_set_pll_mode(struct rk3368_cru *cru, enum rk_clk_id clk_id, int mode)
{
	int pll_id = rk_pll_id(CLK_ARML);
	struct rk3368_pll *pll = &cru->pll[pll_id];

	rk_clrsetreg(&pll->con3,
		     PLLMODE_MASK << PLLMODE_SHIFT,
		     mode << PLLMODE_SHIFT);
}

static void rkclk_init(struct rk3368_cru *cru, struct rk3368_grf *grf)
{
	u32 aclk_div;
	u32 hclk_div;
	u32 pclk_div;

	/* pll enter slow-mode */
	rkclk_set_pll_mode(cru, CLK_GENERAL, PLLMODE_SLOW);
	rkclk_set_pll_mode(cru, CLK_CODEC, PLLMODE_SLOW);

	/* init pll */
	rkclk_set_pll(cru, CLK_GENERAL, &gpll_init_cfg);
	rkclk_set_pll(cru, CLK_CODEC, &cpll_init_cfg);

	/* waiting for pll lock */
	while ((readl(&grf->soc_status[0]) &
			(SOCSTS_CPLL_LOCK | SOCSTS_GPLL_LOCK)) !=
			(SOCSTS_CPLL_LOCK | SOCSTS_GPLL_LOCK))
		udelay(1);

	/*
	 * pd_bus clock pll source selection and
	 * set up dependent divisors for PCLK/HCLK and ACLK clocks.
	 */
/*	aclk_div = GPLL_HZ / PD_BUS_ACLK_HZ - 1;
	assert((aclk_div + 1) * PD_BUS_ACLK_HZ == GPLL_HZ && aclk_div < 0x1f);
	hclk_div = PD_BUS_ACLK_HZ / PD_BUS_HCLK_HZ - 1;
	assert((hclk_div + 1) * PD_BUS_HCLK_HZ ==
		PD_BUS_ACLK_HZ && (hclk_div < 0x4) && (hclk_div != 0x2));

	pclk_div = PD_BUS_ACLK_HZ / PD_BUS_PCLK_HZ - 1;
	assert((pclk_div + 1) * PD_BUS_PCLK_HZ ==
		PD_BUS_ACLK_HZ && pclk_div < 0x7);

	rk_clrsetreg(&cru->cru_clksel_con[1],
		     PD_BUS_PCLK_DIV_MASK << PD_BUS_PCLK_DIV_SHIFT |
		     PD_BUS_HCLK_DIV_MASK << PD_BUS_HCLK_DIV_SHIFT |
		     PD_BUS_ACLK_DIV0_MASK << PD_BUS_ACLK_DIV0_SHIFT |
		     PD_BUS_ACLK_DIV1_MASK << PD_BUS_ACLK_DIV1_SHIFT,
		     pclk_div << PD_BUS_PCLK_DIV_SHIFT |
		     hclk_div << PD_BUS_HCLK_DIV_SHIFT |
		     aclk_div << PD_BUS_ACLK_DIV0_SHIFT |
		     0 << 0);*/

	/*
	 * peri clock pll source selection and
	 * set up dependent divisors for PCLK/HCLK and ACLK clocks.
	 */
/*	aclk_div = GPLL_HZ / PERI_ACLK_HZ - 1;
	assert((aclk_div + 1) * PERI_ACLK_HZ == GPLL_HZ && aclk_div < 0x1f);

	hclk_div = log2(PERI_ACLK_HZ / PERI_HCLK_HZ);
	assert((1 << hclk_div) * PERI_HCLK_HZ ==
		PERI_ACLK_HZ && (hclk_div < 0x4));

	pclk_div = log2(PERI_ACLK_HZ / PERI_PCLK_HZ);
	assert((1 << pclk_div) * PERI_PCLK_HZ ==
		PERI_ACLK_HZ && (pclk_div < 0x4));

	rk_clrsetreg(&cru->cru_clksel_con[10],
		     PERI_PCLK_DIV_MASK << PERI_PCLK_DIV_SHIFT |
		     PERI_HCLK_DIV_MASK << PERI_HCLK_DIV_SHIFT |
		     PERI_ACLK_DIV_MASK << PERI_ACLK_DIV_SHIFT,
		     PERI_SEL_GPLL << PERI_SEL_PLL_SHIFT |
		     pclk_div << PERI_PCLK_DIV_SHIFT |
		     hclk_div << PERI_HCLK_DIV_SHIFT |
		     aclk_div << PERI_ACLK_DIV_SHIFT);*/

	/* PLL enter normal-mode */
	rkclk_set_pll_mode(cru, CLK_GENERAL, PLLMODE_NORMAL);
	rkclk_set_pll_mode(cru, CLK_CODEC, PLLMODE_NORMAL);


	/* pll enter slow-mode */
	rkclk_set_pll_mode(cru, CLK_ARML, PLLMODE_SLOW);
	rkclk_set_pll_mode(cru, CLK_ARMB, PLLMODE_SLOW);

	rkclk_set_pll(cru, CLK_ARML, &aplll_init_cfg);
	rkclk_set_pll(cru, CLK_ARMB, &apllb_init_cfg);

	/* waiting for pll lock */
	while ((readl(&grf->soc_status[0]) &
			(SOCSTS_APLLL_LOCK | SOCSTS_APLLB_LOCK)) !=
			(SOCSTS_APLLL_LOCK | SOCSTS_APLLB_LOCK))
		udelay(1);

	/*
	 * core clock pll source selection and
	 * set up dependent divisors for MPAXI/M0AXI and ARM clocks.
	 * core clock select apll, apll clk = 1800MHz
	 * arm clk = 1800MHz, mpclk = 450MHz, m0clk = 900MHz
	 */
/*	rk_clrsetreg(&cru->cru_clksel_con[0],
		     CORE_SEL_PLL_MASK << CORE_SEL_PLL_SHIFT |
		     A17_DIV_MASK << A17_DIV_SHIFT |
		     MP_DIV_MASK << MP_DIV_SHIFT |
		     M0_DIV_MASK << M0_DIV_SHIFT,
		     0 << A17_DIV_SHIFT |
		     3 << MP_DIV_SHIFT |
		     1 << M0_DIV_SHIFT);*/

	/*
	 * set up dependent divisors for L2RAM/ATCLK and PCLK clocks.
	 * l2ramclk = 900MHz, atclk = 450MHz, pclk_dbg = 450MHz
	 */
/*	rk_clrsetreg(&cru->cru_clksel_con[37],
		     CLK_L2RAM_DIV_MASK << CLK_L2RAM_DIV_SHIFT |
		     ATCLK_CORE_DIV_CON_MASK << ATCLK_CORE_DIV_CON_SHIFT |
		     PCLK_CORE_DBG_DIV_MASK >> PCLK_CORE_DBG_DIV_SHIFT,
		     1 << CLK_L2RAM_DIV_SHIFT |
		     3 << ATCLK_CORE_DIV_CON_SHIFT |
		     3 << PCLK_CORE_DBG_DIV_SHIFT);*/

	/* PLL enter normal-mode */
	rkclk_set_pll_mode(cru, CLK_ARML, PLLMODE_NORMAL);
	rkclk_set_pll_mode(cru, CLK_ARMB, PLLMODE_NORMAL);
}

/* Get pll rate by id */
static uint32_t rkclk_pll_get_rate(struct rk3368_cru *cru,
				   enum rk_clk_id clk_id)
{
	uint32_t nr, no, nf;
	uint32_t con;
	int pll_id = rk_pll_id(clk_id);
	struct rk3368_pll *pll = &cru->pll[pll_id];

	con = readl(&pll->con3);
	switch ((con >> PLLMODE_SHIFT) & PLLMODE_MASK) {
	case PLLMODE_SLOW:
		return OSC_HZ;
	case PLLMODE_NORMAL:
		/* normal mode */
		con = readl(&pll->con0);
		no = ((con >> CLKOD_SHIFT) & CLKOD_MASK) + 1;
		nr = ((con >> CLKR_SHIFT) & CLKR_MASK) + 1;
		con = readl(&pll->con1);
		nf = ((con >> CLKF_SHIFT) & CLKF_MASK) + 1;

		return (24 * nf / (nr * no)) * 1000000;
	case PLLMODE_DEEP:
	default:
		return 32768;
	}
}

static ulong rockchip_mmc_get_clk(struct rk3368_cru *cru, uint gclk_rate,
				  int periph)
{
	uint src_rate;
	uint div, mux;
	u32 con;

	switch (periph) {
	case HCLK_EMMC:
		con = readl(&cru->cru_clksel_con[51]);
		mux = (con >> EMMC_PLL_SHIFT) & EMMC_PLL_MASK;
		div = (con >> EMMC_DIV_SHIFT) & EMMC_DIV_MASK;
		break;
	case HCLK_SDMMC:
		con = readl(&cru->cru_clksel_con[50]);
		mux = (con >> MMC0_PLL_SHIFT) & MMC0_PLL_MASK;
		div = (con >> MMC0_DIV_SHIFT) & MMC0_DIV_MASK;
		break;
	case HCLK_SDIO0:
		con = readl(&cru->cru_clksel_con[48]);
		mux = (con >> SDIO0_PLL_SHIFT) & SDIO0_PLL_MASK;
		div = (con >> SDIO0_DIV_SHIFT) & SDIO0_DIV_MASK;
		break;
	default:
		return -EINVAL;
	}

	src_rate = mux == EMMC_PLL_SELECT_24MHZ ? OSC_HZ : gclk_rate;
	return DIV_TO_RATE(src_rate, div);
}

static ulong rockchip_mmc_set_clk(struct rk3368_cru *cru, uint gclk_rate,
				  int  periph, uint freq)
{
	int src_clk_div;
	int mux;

	debug("%s: gclk_rate=%u\n", __func__, gclk_rate);
	src_clk_div = RATE_TO_DIV(gclk_rate, freq);

	if (src_clk_div > 0x7f) {
		src_clk_div = RATE_TO_DIV(OSC_HZ, freq);
		mux = EMMC_PLL_SELECT_24MHZ;
		assert((int)EMMC_PLL_SELECT_24MHZ ==
		       (int)MMC0_PLL_SELECT_24MHZ);
	} else {
		mux = EMMC_PLL_SELECT_GENERAL;
		assert((int)EMMC_PLL_SELECT_GENERAL ==
		       (int)MMC0_PLL_SELECT_GENERAL);
	}
	switch (periph) {
	case HCLK_EMMC:
		rk_clrsetreg(&cru->cru_clksel_con[51],
			     EMMC_PLL_MASK << EMMC_PLL_SHIFT |
			     EMMC_DIV_MASK << EMMC_DIV_SHIFT,
			     mux << EMMC_PLL_SHIFT |
			     (src_clk_div - 1) << EMMC_DIV_SHIFT);
		break;
	case HCLK_SDMMC:
		rk_clrsetreg(&cru->cru_clksel_con[50],
			     MMC0_PLL_MASK << MMC0_PLL_SHIFT |
			     MMC0_DIV_MASK << MMC0_DIV_SHIFT,
			     mux << MMC0_PLL_SHIFT |
			     (src_clk_div - 1) << MMC0_DIV_SHIFT);
		break;
	case HCLK_SDIO0:
		rk_clrsetreg(&cru->cru_clksel_con[48],
			     SDIO0_PLL_MASK << SDIO0_PLL_SHIFT |
			     SDIO0_DIV_MASK << SDIO0_DIV_SHIFT,
			     mux << SDIO0_PLL_SHIFT |
			     (src_clk_div - 1) << SDIO0_DIV_SHIFT);
		break;
	default:
		return -EINVAL;
	}

	return rockchip_mmc_get_clk(cru, gclk_rate, periph);
}

static ulong rockchip_spi_get_clk(struct rk3368_cru *cru, uint gclk_rate,
				  int periph)
{
/*	uint div, mux;
	u32 con;

	switch (periph) {
	case SCLK_SPI0:
		con = readl(&cru->cru_clksel_con[25]);
		mux = (con >> SPI0_PLL_SHIFT) & SPI0_PLL_MASK;
		div = (con >> SPI0_DIV_SHIFT) & SPI0_DIV_MASK;
		break;
	case SCLK_SPI1:
		con = readl(&cru->cru_clksel_con[25]);
		mux = (con >> SPI1_PLL_SHIFT) & SPI1_PLL_MASK;
		div = (con >> SPI1_DIV_SHIFT) & SPI1_DIV_MASK;
		break;
	case SCLK_SPI2:
		con = readl(&cru->cru_clksel_con[39]);
		mux = (con >> SPI2_PLL_SHIFT) & SPI2_PLL_MASK;
		div = (con >> SPI2_DIV_SHIFT) & SPI2_DIV_MASK;
		break;
	default:
		return -EINVAL;
	}
	assert(mux == SPI0_PLL_SELECT_GENERAL);

	return DIV_TO_RATE(gclk_rate, div);*/
return 0;
}

static ulong rockchip_spi_set_clk(struct rk3368_cru *cru, uint gclk_rate,
				  int periph, uint freq)
{
/*	int src_clk_div;

	debug("%s: clk_general_rate=%u\n", __func__, gclk_rate);
	src_clk_div = RATE_TO_DIV(gclk_rate, freq);
	switch (periph) {
	case SCLK_SPI0:
		rk_clrsetreg(&cru->cru_clksel_con[25],
			     SPI0_PLL_MASK << SPI0_PLL_SHIFT |
			     SPI0_DIV_MASK << SPI0_DIV_SHIFT,
			     SPI0_PLL_SELECT_GENERAL << SPI0_PLL_SHIFT |
			     src_clk_div << SPI0_DIV_SHIFT);
		break;
	case SCLK_SPI1:
		rk_clrsetreg(&cru->cru_clksel_con[25],
			     SPI1_PLL_MASK << SPI1_PLL_SHIFT |
			     SPI1_DIV_MASK << SPI1_DIV_SHIFT,
			     SPI1_PLL_SELECT_GENERAL << SPI1_PLL_SHIFT |
			     src_clk_div << SPI1_DIV_SHIFT);
		break;
	case SCLK_SPI2:
		rk_clrsetreg(&cru->cru_clksel_con[39],
			     SPI2_PLL_MASK << SPI2_PLL_SHIFT |
			     SPI2_DIV_MASK << SPI2_DIV_SHIFT,
			     SPI2_PLL_SELECT_GENERAL << SPI2_PLL_SHIFT |
			     src_clk_div << SPI2_DIV_SHIFT);
		break;
	default:
		return -EINVAL;
	}

	return rockchip_spi_get_clk(cru, gclk_rate, periph);*/
return 0;
}

static ulong rk3368_clk_get_rate(struct clk *clk)
{
	struct rk3368_clk_priv *priv = dev_get_priv(clk->dev);
	ulong new_rate, gclk_rate;

	gclk_rate = rkclk_pll_get_rate(priv->cru, CLK_GENERAL);
	switch (clk->id) {
	case 0 ... 63:
		new_rate = rkclk_pll_get_rate(priv->cru, clk->id);
		break;
	case HCLK_EMMC:
	case HCLK_SDMMC:
	case HCLK_SDIO0:
		new_rate = rockchip_mmc_get_clk(priv->cru, gclk_rate, clk->id);
		break;
	case SCLK_SPI0:
	case SCLK_SPI1:
	case SCLK_SPI2:
		new_rate = rockchip_spi_get_clk(priv->cru, gclk_rate, clk->id);
		break;
	case PCLK_I2C0:
	case PCLK_I2C1:
	case PCLK_I2C2:
	case PCLK_I2C3:
	case PCLK_I2C4:
	case PCLK_I2C5:
		return gclk_rate;
	default:
		return -ENOENT;
	}

	return new_rate;
}

static ulong rk3368_clk_set_rate(struct clk *clk, ulong rate)
{
	struct rk3368_clk_priv *priv = dev_get_priv(clk->dev);
	struct rk3368_cru *cru = priv->cru;
	ulong new_rate, gclk_rate;

	gclk_rate = rkclk_pll_get_rate(priv->cru, CLK_GENERAL);
	switch (clk->id) {
	case CLK_DDR:
		/* not supported, as we're a 2nd stage loader */
		return -EINVAL;
	case HCLK_EMMC:
	case HCLK_SDMMC:
	case HCLK_SDIO0:
		new_rate = rockchip_mmc_set_clk(cru, gclk_rate, clk->id, rate);
		break;
	case SCLK_SPI0:
	case SCLK_SPI1:
	case SCLK_SPI2:
		new_rate = rockchip_spi_set_clk(cru, gclk_rate, clk->id, rate);
		break;
#ifndef CONFIG_SPL_BUILD
	case SCLK_MAC:
		new_rate = rockchip_mac_set_clk(priv->cru, clk->id, rate);
		break;
#endif
	default:
		return -ENOENT;
	}

	return new_rate;
}

static struct clk_ops rk3368_clk_ops = {
	.get_rate	= rk3368_clk_get_rate,
	.set_rate	= rk3368_clk_set_rate,
};

static int rk3368_clk_probe(struct udevice *dev)
{
	struct rk3368_clk_priv *priv = dev_get_priv(dev);

	priv->cru = (struct rk3368_cru *)dev_get_addr(dev);
	priv->grf = syscon_get_first_range(ROCKCHIP_SYSCON_GRF);
	rkclk_init(priv->cru, priv->grf);

	return 0;
}

static int rk3368_clk_bind(struct udevice *dev)
{
	int ret;

	/* The reset driver does not have a device node, so bind it here */
	ret = device_bind_driver(gd->dm_root, "rk3368_sysreset", "reset", &dev);
	if (ret)
		debug("Warning: No RK3368 reset driver: ret=%d\n", ret);

	return 0;
}

static const struct udevice_id rk3368_clk_ids[] = {
	{ .compatible = "rockchip,rk3368-cru" },
	{ }
};

U_BOOT_DRIVER(clk_rk3368) = {
	.name		= "clk_rk3368",
	.id		= UCLASS_CLK,
	.of_match	= rk3368_clk_ids,
	.priv_auto_alloc_size = sizeof(struct rk3368_clk_priv),
	.ops		= &rk3368_clk_ops,
	.bind		= rk3368_clk_bind,
	.probe		= rk3368_clk_probe,
};
