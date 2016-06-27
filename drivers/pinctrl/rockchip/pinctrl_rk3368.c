/*
 * Pinctrl driver for Rockchip SoCs
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <syscon.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/grf_rk3368.h>
#include <asm/arch/hardware.h>
#include <asm/arch/periph.h>
#include <dm/pinctrl.h>
#include <dm/root.h>

DECLARE_GLOBAL_DATA_PTR;

struct rk3368_pinctrl_priv {
	struct rk3368_grf *grf;
	struct rk3368_pmugrf *pmugrf;
	int num_banks;
};

/**
 * Encode variants of iomux registers into a type variable
 */
#define IOMUX_GPIO_ONLY		BIT(0)
#define IOMUX_WIDTH_4BIT	BIT(1)
#define IOMUX_SOURCE_PMU	BIT(2)
#define IOMUX_UNROUTED		BIT(3)

/**
 * @type: iomux variant using IOMUX_* constants
 * @offset: if initialized to -1 it will be autocalculated, by specifying
 *	    an initial offset value the relevant source offset can be reset
 *	    to a new value for autocalculating the following iomux registers.
 */
struct rockchip_iomux {
	u8 type;
	s16 offset;
};

/**
 * @reg: register offset of the gpio bank
 * @nr_pins: number of pins in this bank
 * @bank_num: number of the bank, to account for holes
 * @name: name of the bank
 * @iomux: array describing the 4 iomux sources of the bank
 */
struct rockchip_pin_bank {
	u16 reg;
	u8 nr_pins;
	u8 bank_num;
	char *name;
	struct rockchip_iomux iomux[4];
};

#define PIN_BANK(id, pins, label)			\
	{						\
		.bank_num	= id,			\
		.nr_pins	= pins,			\
		.name		= label,		\
		.iomux		= {			\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
		},					\
	}

#define PIN_BANK_IOMUX_FLAGS(id, pins, label, iom0, iom1, iom2, iom3)	\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = -1 },			\
			{ .type = iom1, .offset = -1 },			\
			{ .type = iom2, .offset = -1 },			\
			{ .type = iom3, .offset = -1 },			\
		},							\
	}

#ifndef CONFIG_SPL_BUILD
static struct rockchip_pin_bank rk3368_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU
			    ),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
};
#endif

static void pinctrl_rk3368_pwm_config(struct rk3368_grf *grf,
				      struct rk3368_pmugrf *pmugrf,int pwm_id)
{
	switch (pwm_id) {
	case PERIPH_ID_PWM0:
		rk_clrsetreg(&grf->gpio3b_iomux, GPIO3B0_MASK << GPIO3B0_SHIFT,
			     GPIO3B0_PWM0 << GPIO3B0_SHIFT);
		break;
	case PERIPH_ID_PWM1:
		rk_clrsetreg(&pmugrf->gpio0b_iomux, GPIO0B0_MASK << GPIO0B0_SHIFT,
			     GPIO0B0_PWM1 << GPIO0B0_SHIFT);
		break;
	case PERIPH_ID_PWM2:
		debug("pwm id = %d iomux error - not controllable!\n", pwm_id);
		break;
	case PERIPH_ID_PWM3:
		rk_clrsetreg(&grf->gpio3d_iomux, GPIO3D6_MASK << GPIO3D6_SHIFT,
			     GPIO3D6_PWM3 << GPIO3D6_SHIFT);
		break;
	default:
		debug("pwm id = %d iomux error!\n", pwm_id);
		break;
	}
}

static void pinctrl_rk3368_i2c_config(struct rk3368_grf *grf,
				      struct rk3368_pmugrf *pmugrf, int i2c_id)
{
	switch (i2c_id) {
	case PERIPH_ID_I2C0:
		rk_clrsetreg(&pmugrf->gpio0a_iomux,
			     GPIO0A6_MASK << GPIO0A6_SHIFT |
			     GPIO0A7_MASK << GPIO0A7_SHIFT,
			     GPIO0A6_I2C0PMU_SDA << GPIO0A6_SHIFT |
			     GPIO0A7_I2C0PMU_SCL << GPIO0A7_SHIFT);
		break;
#ifndef CONFIG_SPL_BUILD
	case PERIPH_ID_I2C1:
		rk_clrsetreg(&grf->gpio2c_iomux,
			     GPIO2C5_MASK << GPIO2C5_SHIFT |
			     GPIO2C6_MASK << GPIO2C6_SHIFT,
			     GPIO2C5_I2C1AUDIO_SDA << GPIO2C5_SHIFT |
			     GPIO2C6_I2C1AUDIO_SCL << GPIO2C6_SHIFT);
		break;
	case PERIPH_ID_I2C2:
		rk_clrsetreg(&pmugrf->gpio0b_iomux,
			     GPIO0B1_MASK << GPIO0B1_SHIFT,
			     GPIO0B1_I2C2SENSOR_SCL << GPIO0B1_SHIFT);
		rk_clrsetreg(&grf->gpio3d_iomux,
			     GPIO3D7_MASK << GPIO3D7_SHIFT,
			     GPIO3D7_I2C2SENSOR_SDA << GPIO3D7_SHIFT);
		break;
	case PERIPH_ID_I2C3:
		rk_clrsetreg(&grf->gpio1c_iomux,
			     GPIO1C1_MASK << GPIO1C1_SHIFT |
			     GPIO1C0_MASK << GPIO1C0_SHIFT,
			     GPIO1C1_I2C3CAM_SDA << GPIO1C1_SHIFT |
			     GPIO1C0_I2C3CAM_SCL << GPIO1C0_SHIFT);
		break;
	case PERIPH_ID_I2C4:
		rk_clrsetreg(&grf->gpio3d_iomux,
			     GPIO3D0_MASK << GPIO3D0_SHIFT |
			     GPIO3D1_MASK << GPIO3D1_SHIFT,
			     GPIO3D0_I2C4TP_SDA << GPIO3D0_SHIFT |
			     GPIO3D1_I2C4TP_SCL << GPIO3D1_SHIFT);
		break;
	case PERIPH_ID_I2C5:
		rk_clrsetreg(&grf->gpio3d_iomux,
			     GPIO3D2_MASK << GPIO3D2_SHIFT |
			     GPIO3D3_MASK << GPIO3D3_SHIFT,
			     GPIO3D2_I2C5HDMI_SDA << GPIO3D2_SHIFT |
			     GPIO3D3_I2C5HDMI_SCL << GPIO3D3_SHIFT);
		break;
#endif
	default:
		debug("i2c id = %d iomux error!\n", i2c_id);
		break;
	}
}

static int pinctrl_rk3368_spi_config(struct rk3368_grf *grf,
				     enum periph_id spi_id, int cs)
{
/*	switch (spi_id) {
#ifndef CONFIG_SPL_BUILD
	case PERIPH_ID_SPI0:
		switch (cs) {
		case 0:
			rk_clrsetreg(&grf->gpio5b_iomux,
				     GPIO5B5_MASK << GPIO5B5_SHIFT,
				     GPIO5B5_SPI0_CSN0 << GPIO5B5_SHIFT);
			break;
		case 1:
			rk_clrsetreg(&grf->gpio5c_iomux,
				     GPIO5C0_MASK << GPIO5C0_SHIFT,
				     GPIO5C0_SPI0_CSN1 << GPIO5C0_SHIFT);
			break;
		default:
			goto err;
		}
		rk_clrsetreg(&grf->gpio5b_iomux,
			     GPIO5B7_MASK << GPIO5B7_SHIFT |
			     GPIO5B6_MASK << GPIO5B6_SHIFT |
			     GPIO5B4_MASK << GPIO5B4_SHIFT,
			     GPIO5B7_SPI0_RXD << GPIO5B7_SHIFT |
			     GPIO5B6_SPI0_TXD << GPIO5B6_SHIFT |
			     GPIO5B4_SPI0_CLK << GPIO5B4_SHIFT);
		break;
	case PERIPH_ID_SPI1:
		if (cs != 0)
			goto err;
		rk_clrsetreg(&grf->gpio7b_iomux,
			     GPIO7B6_MASK << GPIO7B6_SHIFT |
			     GPIO7B7_MASK << GPIO7B7_SHIFT |
			     GPIO7B5_MASK << GPIO7B5_SHIFT |
			     GPIO7B4_MASK << GPIO7B4_SHIFT,
			     GPIO7B6_SPI1_RXD << GPIO7B6_SHIFT |
			     GPIO7B7_SPI1_TXD << GPIO7B7_SHIFT |
			     GPIO7B5_SPI1_CSN0 << GPIO7B5_SHIFT |
			     GPIO7B4_SPI1_CLK << GPIO7B4_SHIFT);
		break;
#endif
	case PERIPH_ID_SPI2:
		switch (cs) {
		case 0:
			rk_clrsetreg(&grf->gpio8a_iomux,
				     GPIO8A7_MASK << GPIO8A7_SHIFT,
				     GPIO8A7_SPI2_CSN0 << GPIO8A7_SHIFT);
			break;
		case 1:
			rk_clrsetreg(&grf->gpio8a_iomux,
				     GPIO8A3_MASK << GPIO8A3_SHIFT,
				     GPIO8A3_SPI2_CSN1 << GPIO8A3_SHIFT);
			break;
		default:
			goto err;
		}
		rk_clrsetreg(&grf->gpio8b_iomux,
			     GPIO8B1_MASK << GPIO8B1_SHIFT |
			     GPIO8B0_MASK << GPIO8B0_SHIFT,
			     GPIO8B1_SPI2_TXD << GPIO8B1_SHIFT |
			     GPIO8B0_SPI2_RXD << GPIO8B0_SHIFT);
		rk_clrsetreg(&grf->gpio8a_iomux,
			     GPIO8A6_MASK << GPIO8A6_SHIFT,
			     GPIO8A6_SPI2_CLK << GPIO8A6_SHIFT);
		break;
	default:
		goto err;
	}*/

	return 0;
err:
	debug("rkspi: periph%d cs=%d not supported", spi_id, cs);
	return -ENOENT;
}

static void pinctrl_rk3368_uart_config(struct rk3368_grf *grf,
				       struct rk3368_pmugrf *pmugrf, int uart_id)
{
	switch (uart_id) {
#ifndef CONFIG_SPL_BUILD
	case PERIPH_ID_UART_BT:
		rk_clrsetreg(&grf->gpio2d_iomux,
			     GPIO2D3_MASK << GPIO2D3_SHIFT |
			     GPIO2D2_MASK << GPIO2D2_SHIFT |
			     GPIO2D1_MASK << GPIO2D1_SHIFT |
			     GPIO2D0_MASK << GPIO2D0_SHIFT,
			     GPIO2D3_UART0BT_RTSN << GPIO2D3_SHIFT |
			     GPIO2D2_UART0BT_CTSN << GPIO2D2_SHIFT |
			     GPIO2D1_UART0BT_SOUT << GPIO2D1_SHIFT |
			     GPIO2D0_UART0BT_SIN << GPIO2D0_SHIFT);
		break;
	case PERIPH_ID_UART_BB:
		rk_clrsetreg(&pmugrf->gpio0c_iomux,
			     GPIO0C7_MASK << GPIO0C7_SHIFT |
			     GPIO0C6_MASK << GPIO0C6_SHIFT |
			     GPIO0C5_MASK << GPIO0C5_SHIFT |
			     GPIO0C4_MASK << GPIO0C4_SHIFT,
			     GPIO0C7_UART1BB_RTSN << GPIO0C7_SHIFT |
			     GPIO0C6_UART1BB_CTSN << GPIO0C6_SHIFT |
			     GPIO0C5_UART1BB_SOUT << GPIO0C5_SHIFT |
			     GPIO0C4_UART1BB_SIN << GPIO0C4_SHIFT);
		break;
#endif
	case PERIPH_ID_UART_DBG:
		rk_clrsetreg(&grf->gpio2a_iomux,
			     GPIO2A5_MASK << GPIO2A5_SHIFT |
			     GPIO2A6_MASK << GPIO2A6_SHIFT,
			     GPIO2A5_UART2DBG_SOUT << GPIO2A5_SHIFT |
			     GPIO2A6_UART2DBG_SIN << GPIO2A6_SHIFT);
		break;
#ifndef CONFIG_SPL_BUILD
	case PERIPH_ID_UART_GPS:
		rk_clrsetreg(&grf->gpio3c_iomux,
			     GPIO3C1_MASK << GPIO3C1_SHIFT |
			     GPIO3C0_MASK << GPIO3C0_SHIFT,
			     GPIO3C1_UART3GPS_RTSN << GPIO3C1_SHIFT |
			     GPIO3C0_UART3GPS_CTSN << GPIO3C0_SHIFT);
		rk_clrsetreg(&grf->gpio3d_iomux,
			     GPIO3D6_MASK << GPIO3D6_SHIFT |
			     GPIO3D5_MASK << GPIO3D5_SHIFT,
			     GPIO3D6_UART3GPS_SOUT << GPIO3D6_SHIFT |
			     GPIO3D5_UART3GPS_SIN << GPIO3D5_SHIFT);
		break;
	case PERIPH_ID_UART_EXP:
		rk_clrsetreg(&pmugrf->gpio0d_iomux,
			     GPIO0D1_MASK << GPIO0D1_SHIFT |
			     GPIO0D0_MASK << GPIO0D0_SHIFT |
			     GPIO0D2_MASK << GPIO0D2_SHIFT |
			     GPIO0D3_MASK << GPIO0D3_SHIFT,
			     GPIO0D1_UART4EXP_RTSN << GPIO0D1_SHIFT |
			     GPIO0D0_UART4EXP_CTSN << GPIO0D0_SHIFT |
			     GPIO0D2_UART4EXP_SOUT << GPIO0D2_SHIFT |
			     GPIO0D3_UART4EXP_SIN << GPIO0D3_SHIFT);
		break;
#endif
	default:
		debug("uart id = %d iomux error!\n", uart_id);
		break;
	}
}

static void pinctrl_rk3368_sdmmc_config(struct rk3368_grf *grf, int mmc_id)
{
	switch (mmc_id) {
	case PERIPH_ID_EMMC:
		rk_clrsetreg(&grf->gpio1d_iomux,
			     GPIO1D3_MASK << GPIO1D3_SHIFT |
			     GPIO1D2_MASK << GPIO1D2_SHIFT |
			     GPIO1D1_MASK << GPIO1D1_SHIFT |
			     GPIO1D0_MASK << GPIO1D0_SHIFT,
			     GPIO1D3_EMMC_PWREN << GPIO1D3_SHIFT |
			     GPIO1D2_EMMC_CMD << GPIO1D2_SHIFT |
			     GPIO1D1_EMMC_DATA7 << GPIO1D1_SHIFT |
			     GPIO1D0_EMMC_DATA6 << GPIO1D0_SHIFT);
		rk_clrsetreg(&grf->gpio1c_iomux,
			     GPIO1C7_MASK << GPIO1C7_SHIFT |
			     GPIO1C6_MASK << GPIO1C6_SHIFT |
			     GPIO1C5_MASK << GPIO1C5_SHIFT |
			     GPIO1C4_MASK << GPIO1C4_SHIFT |
			     GPIO1C3_MASK << GPIO1C3_SHIFT |
			     GPIO1C2_MASK << GPIO1C2_SHIFT,
			     GPIO1C7_EMMC_DATA5 << GPIO1C7_SHIFT |
			     GPIO1C6_EMMC_DATA4 << GPIO1C6_SHIFT |
			     GPIO1C5_EMMC_DATA3 << GPIO1C5_SHIFT |
			     GPIO1C4_EMMC_DATA2 << GPIO1C4_SHIFT |
			     GPIO1C3_EMMC_DATA1 << GPIO1C3_SHIFT |
			     GPIO1C2_EMMC_DATA0 << GPIO1C2_SHIFT);
		rk_clrsetreg(&grf->gpio2a_iomux,
			     GPIO2A4_MASK << GPIO2A4_SHIFT,
			     GPIO2A4_EMMC_CLKOUT << GPIO2A4_SHIFT);
		break;
	case PERIPH_ID_SDCARD:
		rk_clrsetreg(&grf->gpio2b_iomux,
			     GPIO2B3_MASK << GPIO2B3_SHIFT |
			     GPIO2B2_MASK << GPIO2B2_SHIFT |
			     GPIO2B1_MASK << GPIO2B1_SHIFT |
			     GPIO2B0_MASK << GPIO2B0_SHIFT,
			     GPIO2B3_SDMMC0_DTECTN << GPIO2B3_SHIFT |
			     GPIO2B2_SDMMC0_CMD << GPIO2B2_SHIFT |
			     GPIO2B1_SDMMC0_CLKOUT << GPIO2B1_SHIFT |
			     GPIO2B0_SDMMC0_DATA3 << GPIO2B0_SHIFT);
		rk_clrsetreg(&grf->gpio2a_iomux,
			     GPIO2A7_MASK << GPIO2A7_SHIFT |
			     GPIO2A6_MASK << GPIO2A6_SHIFT |
			     GPIO2A5_MASK << GPIO2A5_SHIFT,
			     GPIO2A7_SDMMC0_DATA2 << GPIO2A7_SHIFT |
			     GPIO2A6_SDMMC0_DATA1 << GPIO2A6_SHIFT |
			     GPIO2A5_SDMMC0_DATA0 << GPIO2A5_SHIFT);

		/* use sdmmc0 io, disable JTAG function */
		rk_clrsetreg(&grf->soc_con15, 1 << GRF_FORCE_JTAG_SHIFT, 0);
		break;
	default:
		debug("mmc id = %d iomux error!\n", mmc_id);
		break;
	}
}

static int rk3368_pinctrl_request(struct udevice *dev, int func, int flags)
{
	struct rk3368_pinctrl_priv *priv = dev_get_priv(dev);

	debug("%s: func=%x, flags=%x\n", __func__, func, flags);
	switch (func) {
	case PERIPH_ID_PWM0:
	case PERIPH_ID_PWM1:
	case PERIPH_ID_PWM2:
	case PERIPH_ID_PWM3:
	case PERIPH_ID_PWM4:
		pinctrl_rk3368_pwm_config(priv->grf, priv->pmugrf, func);
		break;
	case PERIPH_ID_I2C0:
	case PERIPH_ID_I2C1:
	case PERIPH_ID_I2C2:
	case PERIPH_ID_I2C3:
	case PERIPH_ID_I2C4:
	case PERIPH_ID_I2C5:
		pinctrl_rk3368_i2c_config(priv->grf, priv->pmugrf, func);
		break;
	case PERIPH_ID_SPI0:
	case PERIPH_ID_SPI1:
	case PERIPH_ID_SPI2:
		pinctrl_rk3368_spi_config(priv->grf, func, flags);
		break;
	case PERIPH_ID_UART0:
	case PERIPH_ID_UART1:
	case PERIPH_ID_UART2:
	case PERIPH_ID_UART3:
	case PERIPH_ID_UART4:
		pinctrl_rk3368_uart_config(priv->grf, priv->pmugrf, func);
		break;
	case PERIPH_ID_SDMMC0:
	case PERIPH_ID_SDMMC1:
		pinctrl_rk3368_sdmmc_config(priv->grf, func);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rk3368_pinctrl_get_periph_id(struct udevice *dev,
					struct udevice *periph)
{
	u32 cell[3];
	int ret;

	ret = fdtdec_get_int_array(gd->fdt_blob, periph->of_offset,
				   "interrupts", cell, ARRAY_SIZE(cell));
	if (ret < 0)
		return -EINVAL;

	switch (cell[1]) {
	case 44:
		return PERIPH_ID_SPI0;
	case 45:
		return PERIPH_ID_SPI1;
	case 41: /* Note strange order */
		return PERIPH_ID_SPI2;
	case 60:
		return PERIPH_ID_I2C0;
	case 62: /* Note strange order */
		return PERIPH_ID_I2C1;
	case 61:
		return PERIPH_ID_I2C2;
	case 63:
		return PERIPH_ID_I2C3;
	case 64:
		return PERIPH_ID_I2C4;
	case 65:
		return PERIPH_ID_I2C5;
	}

	return -ENOENT;
}

static int rk3368_pinctrl_set_state_simple(struct udevice *dev,
					   struct udevice *periph)
{
	int func;

	func = rk3368_pinctrl_get_periph_id(dev, periph);
	if (func < 0)
		return func;
	return rk3368_pinctrl_request(dev, func, 0);
}

#ifndef CONFIG_SPL_BUILD
int rk3368_pinctrl_get_pin_info(struct rk3368_pinctrl_priv *priv,
				int banknum, int ind, u32 **addrp, uint *shiftp,
				uint *maskp)
{
	struct rockchip_pin_bank *bank = &rk3368_pin_banks[banknum];
	uint muxnum;
	u32 *addr;

	for (muxnum = 0; muxnum < 4; muxnum++) {
		struct rockchip_iomux *mux = &bank->iomux[muxnum];

		if (ind >= 8) {
			ind -= 8;
			continue;
		}

		if (mux->type & IOMUX_SOURCE_PMU)
			addr = &priv->pmugrf->gpio0a_iomux;
		else
			addr = (u32 *)priv->grf - 4;

		addr += mux->offset;
		*shiftp = ind & 7;
		*maskp = 3;
		*shiftp *= 2;

		debug("%s: addr=%p, mask=%x, shift=%x\n", __func__, addr,
		      *maskp, *shiftp);
		*addrp = addr;
		return 0;
	}

	return -EINVAL;
}

static int rk3368_pinctrl_get_gpio_mux(struct udevice *dev, int banknum,
				       int index)
{
	struct rk3368_pinctrl_priv *priv = dev_get_priv(dev);
	uint shift;
	uint mask;
	u32 *addr;
	int ret;

	ret = rk3368_pinctrl_get_pin_info(priv, banknum, index, &addr, &shift,
					  &mask);
	if (ret)
		return ret;
	return (readl(addr) & mask) >> shift;
}

static int rk3368_pinctrl_set_pins(struct udevice *dev, int banknum, int index,
				   int muxval, int flags)
{
	struct rk3368_pinctrl_priv *priv = dev_get_priv(dev);
	uint shift, ind = index;
	uint mask;
	u32 *addr;
	int ret;

	debug("%s: %x %x %x %x\n", __func__, banknum, index, muxval, flags);
	ret = rk3368_pinctrl_get_pin_info(priv, banknum, index, &addr, &shift,
					  &mask);
	if (ret)
		return ret;
	rk_clrsetreg(addr, mask << shift, muxval << shift);

	/* Handle pullup/pulldown */
	if (flags) {
		uint val = 0;

		if (flags & (1 << PIN_CONFIG_BIAS_PULL_UP))
			val = 1;
		else if (flags & (1 << PIN_CONFIG_BIAS_PULL_DOWN))
			val = 2;
		shift = (index & 7) * 2;
		ind = index >> 3;
		if (banknum == 0)
			addr = &priv->pmugrf->gpio0_p[ind];
		else
			addr = &priv->grf->gpio1_p[banknum - 1][ind];
		debug("%s: addr=%p, val=%x, shift=%x\n", __func__, addr, val,
		      shift);
		rk_clrsetreg(addr, 3 << shift, val << shift);
	}

	return 0;
}

static int rk3368_pinctrl_set_state(struct udevice *dev, struct udevice *config)
{
	const void *blob = gd->fdt_blob;
	int pcfg_node, ret, flags, count, i;
	u32 cell[60], *ptr;

	debug("%s: %s %s\n", __func__, dev->name, config->name);
	ret = fdtdec_get_int_array_count(blob, config->of_offset,
					 "rockchip,pins", cell,
					 ARRAY_SIZE(cell));
	if (ret < 0) {
		debug("%s: bad array %d\n", __func__, ret);
		return -EINVAL;
	}
	count = ret;
	for (i = 0, ptr = cell; i < count; i += 4, ptr += 4) {
		pcfg_node = fdt_node_offset_by_phandle(blob, ptr[3]);
		if (pcfg_node < 0)
			return -EINVAL;
		flags = pinctrl_decode_pin_config(blob, pcfg_node);
		if (flags < 0)
			return flags;

		ret = rk3368_pinctrl_set_pins(dev, ptr[0], ptr[1], ptr[2],
					      flags);
		if (ret)
			return ret;
	}

	return 0;
}
#endif

static struct pinctrl_ops rk3368_pinctrl_ops = {
#ifndef CONFIG_SPL_BUILD
	.set_state	= rk3368_pinctrl_set_state,
	.get_gpio_mux	= rk3368_pinctrl_get_gpio_mux,
#endif
	.set_state_simple	= rk3368_pinctrl_set_state_simple,
	.request	= rk3368_pinctrl_request,
	.get_periph_id	= rk3368_pinctrl_get_periph_id,
};

static int rk3368_pinctrl_bind(struct udevice *dev)
{
	/* scan child GPIO banks */
	return dm_scan_fdt_node(dev, gd->fdt_blob, dev->of_offset, false);
}

#ifndef CONFIG_SPL_BUILD
static int rk3368_pinctrl_parse_tables(struct rk3368_pinctrl_priv *priv,
				       struct rockchip_pin_bank *banks,
				       int count)
{
	struct rockchip_pin_bank *bank;
	uint reg, muxnum, banknum;

	reg = 0;
	for (banknum = 0; banknum < count; banknum++) {
		bank = &banks[banknum];
		bank->reg = reg;
		debug("%s: bank %d, reg %x\n", __func__, banknum, reg * 4);
		for (muxnum = 0; muxnum < 4; muxnum++) {
			struct rockchip_iomux *mux = &bank->iomux[muxnum];

			if (!(mux->type & IOMUX_UNROUTED))
				mux->offset = reg;
			if (mux->type & IOMUX_WIDTH_4BIT)
				reg += 2;
			else
				reg += 1;
		}
	}

	return 0;
}
#endif

static int rk3368_pinctrl_probe(struct udevice *dev)
{
	struct rk3368_pinctrl_priv *priv = dev_get_priv(dev);
	int ret = 0;

	priv->grf = syscon_get_first_range(ROCKCHIP_SYSCON_GRF);
	priv->pmugrf = syscon_get_first_range(ROCKCHIP_SYSCON_PMUGRF);
	debug("%s: grf=%p, pmu=%p\n", __func__, priv->grf, priv->pmugrf);
#ifndef CONFIG_SPL_BUILD
	ret = rk3368_pinctrl_parse_tables(priv, rk3368_pin_banks,
					  ARRAY_SIZE(rk3368_pin_banks));
#endif

	return ret;
}

static const struct udevice_id rk3368_pinctrl_ids[] = {
	{ .compatible = "rockchip,rk3368-pinctrl" },
	{ }
};

U_BOOT_DRIVER(pinctrl_rk3368) = {
	.name		= "pinctrl_rk3368",
	.id		= UCLASS_PINCTRL,
	.of_match	= rk3368_pinctrl_ids,
	.priv_auto_alloc_size = sizeof(struct rk3368_pinctrl_priv),
	.ops		= &rk3368_pinctrl_ops,
	.bind		= rk3368_pinctrl_bind,
	.probe		= rk3368_pinctrl_probe,
};
