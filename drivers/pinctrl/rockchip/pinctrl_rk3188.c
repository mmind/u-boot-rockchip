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
#include <asm/arch/grf_rk3188.h>
#include <asm/arch/hardware.h>
#include <asm/arch/periph.h>
#include <asm/arch/pmu_rk3188.h>
#include <dm/pinctrl.h>

DECLARE_GLOBAL_DATA_PTR;

struct rk3188_pinctrl_priv {
	struct rk3188_grf *grf;
	struct rk3188_pmu *pmu;
};

static void pinctrl_rk3188_pwm_config(struct rk3188_grf *grf, int pwm_id)
{
	switch (pwm_id) {
	case PERIPH_ID_PWM0:
		rk_clrsetreg(&grf->gpio3d_iomux, GPIO3D3_MASK << GPIO3D3_SHIFT,
			     GPIO3D3_PWM_0 << GPIO3D3_SHIFT);
		break;
	case PERIPH_ID_PWM1:
		rk_clrsetreg(&grf->gpio3d_iomux, GPIO3D4_MASK << GPIO3D4_SHIFT,
			     GPIO3D4_PWM_1 << GPIO3D4_SHIFT);
		break;
	case PERIPH_ID_PWM2:
		rk_clrsetreg(&grf->gpio3d_iomux, GPIO3D5_MASK << GPIO3D5_SHIFT,
			     GPIO3D5_PWM_2 << GPIO3D5_SHIFT);
		break;
	case PERIPH_ID_PWM3:
		rk_clrsetreg(&grf->gpio3d_iomux, GPIO3D6_MASK << GPIO3D6_SHIFT,
			     GPIO3D6_PWM_3 << GPIO3D6_SHIFT);
		break;
	default:
		debug("pwm id = %d iomux error!\n", pwm_id);
		break;
	}
}

static void pinctrl_rk3188_i2c_config(struct rk3188_grf *grf,
				      struct rk3188_pmu *pmu, int i2c_id)
{
	switch (i2c_id) {
	case PERIPH_ID_I2C0:
		rk_clrsetreg(&grf->gpio1d_iomux,
				GPIO1D1_MASK << GPIO1D1_SHIFT |
				GPIO1D0_MASK << GPIO1D0_SHIFT,
				GPIO1D1_I2C0_SCL << GPIO1D1_SHIFT |
				GPIO1D0_I2C0_SDA << GPIO1D0_SHIFT);
		break;
	case PERIPH_ID_I2C1:
		rk_clrsetreg(&grf->gpio1d_iomux,
				GPIO1D3_MASK << GPIO1D3_SHIFT |
				GPIO1D2_MASK << GPIO1D2_SHIFT,
				GPIO1D3_I2C1_SCL << GPIO1D2_SHIFT |
				GPIO1D2_I2C1_SDA << GPIO1D2_SHIFT);
		break;
	case PERIPH_ID_I2C2:
		rk_clrsetreg(&grf->gpio1d_iomux,
				GPIO1D5_MASK << GPIO1D5_SHIFT |
				GPIO1D4_MASK << GPIO1D4_SHIFT,
				GPIO1D5_I2C2_SCL << GPIO1D5_SHIFT |
				GPIO1D4_I2C2_SDA << GPIO1D4_SHIFT);
		break;
	case PERIPH_ID_I2C3:
		rk_clrsetreg(&grf->gpio3b_iomux,
				GPIO3B7_MASK << GPIO3B7_SHIFT |
				GPIO3B6_MASK << GPIO3B6_SHIFT,
				GPIO3B7_I2C3_SCL << GPIO3B7_SHIFT |
				GPIO3B6_I2C3_SDA << GPIO3B6_SHIFT);
		break;
	case PERIPH_ID_I2C4:
		rk_clrsetreg(&grf->gpio1d_iomux,
				GPIO1D7_MASK << GPIO1D7_SHIFT |
				GPIO1D6_MASK << GPIO1D6_SHIFT,
				GPIO1D7_I2C4_SCL << GPIO1D7_SHIFT |
				GPIO1D6_I2C4_SDA << GPIO1D6_SHIFT);
		break;
	default:
		debug("i2c id = %d iomux error!\n", i2c_id);
		break;
	}
}

static void pinctrl_rk3188_lcdc_config(struct rk3188_grf *grf, int lcd_id)
{
	switch (lcd_id) {
/*	case PERIPH_ID_LCDC0:
		rk_clrsetreg(&grf->gpio1d_iomux,
			     GPIO1D3_MASK << GPIO1D0_SHIFT |
			     GPIO1D2_MASK << GPIO1D2_SHIFT |
			     GPIO1D1_MASK << GPIO1D1_SHIFT |
			     GPIO1D0_MASK << GPIO1D0_SHIFT,
			     GPIO1D3_LCDC0_DCLK << GPIO1D3_SHIFT |
			     GPIO1D2_LCDC0_DEN << GPIO1D2_SHIFT |
			     GPIO1D1_LCDC0_VSYNC << GPIO1D1_SHIFT |
			     GPIO1D0_LCDC0_HSYNC << GPIO1D0_SHIFT);
		break;*/
	default:
		debug("lcdc id = %d iomux error!\n", lcd_id);
		break;
	}
}

static int pinctrl_rk3188_spi_config(struct rk3188_grf *grf,
				     enum periph_id spi_id, int cs)
{
	switch (spi_id) {
	case PERIPH_ID_SPI0:
		switch (cs) {
		case 0:
			rk_clrsetreg(&grf->gpio1a_iomux,
				     GPIO1A7_MASK << GPIO1A7_SHIFT,
				     GPIO1A7_SPI0_CSN0 << GPIO1A7_SHIFT);
			break;
		case 1:
			rk_clrsetreg(&grf->gpio1b_iomux,
				     GPIO1B7_MASK << GPIO1B7_SHIFT,
				     GPIO1B7_SPI0_CSN1 << GPIO1B7_SHIFT);
			break;
		default:
			goto err;
		}
		rk_clrsetreg(&grf->gpio1a_iomux,
			     GPIO1A4_MASK << GPIO1A4_SHIFT |
			     GPIO1A5_MASK << GPIO1A5_SHIFT |
			     GPIO1A6_MASK << GPIO1A6_SHIFT,
			     GPIO1A4_SPI0_RXD << GPIO1A4_SHIFT |
			     GPIO1A5_SPI0_TXD << GPIO1A5_SHIFT |
			     GPIO1A6_SPI0_CLK << GPIO1A6_SHIFT);
		break;
	case PERIPH_ID_SPI1:
		switch (cs) {
		case 0:
			rk_clrsetreg(&grf->gpio0d_iomux,
				     GPIO0D7_MASK << GPIO0D7_SHIFT,
				     GPIO0D7_SPI1_CSN0 << GPIO0D7_SHIFT);
			break;
		case 1:
			rk_clrsetreg(&grf->gpio1b_iomux,
				     GPIO1B6_MASK << GPIO1B6_SHIFT,
				     GPIO1B6_SPI1_CSN1 << GPIO1B6_SHIFT);
			break;
		default:
			goto err;
		}
		rk_clrsetreg(&grf->gpio0d_iomux,
			     GPIO0D4_MASK << GPIO0D4_SHIFT |
			     GPIO0D5_MASK << GPIO0D5_SHIFT |
			     GPIO0D6_MASK << GPIO0D6_SHIFT,
			     GPIO0D4_SPI0_RXD << GPIO0D4_SHIFT |
			     GPIO0D5_SPI1_TXD << GPIO0D5_SHIFT |
			     GPIO0D6_SPI1_CLK << GPIO0D6_SHIFT);
		break;
	default:
		goto err;
	}

	return 0;
err:
	debug("rkspi: periph%d cs=%d not supported", spi_id, cs);
	return -ENOENT;
}

static void pinctrl_rk3188_uart_config(struct rk3188_grf *grf, int uart_id)
{
	switch (uart_id) {
	case PERIPH_ID_UART0:
		rk_clrsetreg(&grf->gpio1a_iomux,
			     GPIO1A3_MASK << GPIO1A3_SHIFT |
			     GPIO1A2_MASK << GPIO1A2_SHIFT |
			     GPIO1A1_MASK << GPIO1A1_SHIFT |
			     GPIO1A0_MASK << GPIO1A0_SHIFT,
			     GPIO1A3_UART0_RTS_N << GPIO1A3_SHIFT |
			     GPIO1A2_UART0_CTS_N << GPIO1A2_SHIFT |
			     GPIO1A1_UART0_SOUT << GPIO1A1_SHIFT |
			     GPIO1A0_UART0_SIN << GPIO1A0_SHIFT);
		break;
	case PERIPH_ID_UART1:
		rk_clrsetreg(&grf->gpio1a_iomux,
			     GPIO1A7_MASK << GPIO1A7_SHIFT |
			     GPIO1A6_MASK << GPIO1A6_SHIFT |
			     GPIO1A5_MASK << GPIO1A5_SHIFT |
			     GPIO1A4_MASK << GPIO1A4_SHIFT,
			     GPIO1A7_UART1_RTS_N << GPIO1A7_SHIFT |
			     GPIO1A6_UART1_CTS_N << GPIO1A6_SHIFT |
			     GPIO1A5_UART1_SOUT << GPIO1A5_SHIFT |
			     GPIO1A4_UART1_SIN << GPIO1A4_SHIFT);
		break;
	case PERIPH_ID_UART2:
		rk_clrsetreg(&grf->gpio1b_iomux,
			     GPIO1B1_MASK << GPIO1B1_SHIFT |
			     GPIO1B0_MASK << GPIO1B0_SHIFT,
			     GPIO1B1_UART2_SOUT << GPIO1B1_SHIFT |
			     GPIO1B0_UART2_SIN << GPIO1B0_SHIFT);
		break;
	case PERIPH_ID_UART3:
		rk_clrsetreg(&grf->gpio1b_iomux,
			     GPIO1B5_MASK << GPIO1B5_SHIFT |
			     GPIO1B4_MASK << GPIO1B4_SHIFT |
			     GPIO1B3_MASK << GPIO1B3_SHIFT |
			     GPIO1B2_MASK << GPIO1B2_SHIFT,
			     GPIO1B5_UART3_RTS_N << GPIO1B5_SHIFT |
			     GPIO1B4_UART3_CTS_N << GPIO1B4_SHIFT |
			     GPIO1B3_UART3_SOUT << GPIO1B3_SHIFT |
			     GPIO1B2_UART3_SIN << GPIO1B2_SHIFT);
		break;
	default:
		debug("uart id = %d iomux error!\n", uart_id);
		break;
	}
}

static void pinctrl_rk3188_sdmmc_config(struct rk3188_grf *grf, int mmc_id)
{
	switch (mmc_id) {
/*	case PERIPH_ID_EMMC:
		rk_clrsetreg(&grf->gpio3a_iomux, 0xffff,
			     GPIO3A7_EMMC_DATA7 << GPIO3A7_SHIFT |
			     GPIO3A6_EMMC_DATA6 << GPIO3A6_SHIFT |
			     GPIO3A5_EMMC_DATA5 << GPIO3A5_SHIFT |
			     GPIO3A4_EMMC_DATA4 << GPIO3A4_SHIFT |
			     GPIO3A3_EMMC_DATA3 << GPIO3A3_SHIFT |
			     GPIO3A2_EMMC_DATA2 << GPIO3A2_SHIFT |
			     GPIO3A1_EMMC_DATA1 << GPIO3A1_SHIFT |
			     GPIO3A0_EMMC_DATA0 << GPIO3A0_SHIFT);
		rk_clrsetreg(&grf->gpio3b_iomux, GPIO3B1_MASK << GPIO3B1_SHIFT,
			     GPIO3B1_EMMC_PWREN << GPIO3B1_SHIFT);
		rk_clrsetreg(&grf->gpio3c_iomux,
			     GPIO3C0_MASK << GPIO3C0_SHIFT,
			     GPIO3C0_EMMC_CMD << GPIO3C0_SHIFT);
		break;
	case PERIPH_ID_SDCARD:
		rk_clrsetreg(&grf->gpio6c_iomux, 0xffff,
			     GPIO6C6_SDMMC0_DECTN << GPIO6C6_SHIFT |
			     GPIO6C5_SDMMC0_CMD << GPIO6C5_SHIFT |
			     GPIO6C4_SDMMC0_CLKOUT << GPIO6C4_SHIFT |
			     GPIO6C3_SDMMC0_DATA3 << GPIO6C3_SHIFT |
			     GPIO6C2_SDMMC0_DATA2 << GPIO6C2_SHIFT |
			     GPIO6C1_SDMMC0_DATA1 << GPIO6C1_SHIFT |
			     GPIO6C0_SDMMC0_DATA0 << GPIO6C0_SHIFT);
		break;*/
	default:
		debug("mmc id = %d iomux error!\n", mmc_id);
		break;
	}
}

static int rk3188_pinctrl_request(struct udevice *dev, int func, int flags)
{
	struct rk3188_pinctrl_priv *priv = dev_get_priv(dev);

	debug("%s: func=%x, flags=%x\n", __func__, func, flags);
	switch (func) {
	case PERIPH_ID_PWM0:
	case PERIPH_ID_PWM1:
	case PERIPH_ID_PWM2:
	case PERIPH_ID_PWM3:
	case PERIPH_ID_PWM4:
		pinctrl_rk3188_pwm_config(priv->grf, func);
		break;
	case PERIPH_ID_I2C0:
	case PERIPH_ID_I2C1:
	case PERIPH_ID_I2C2:
	case PERIPH_ID_I2C3:
	case PERIPH_ID_I2C4:
	case PERIPH_ID_I2C5:
		pinctrl_rk3188_i2c_config(priv->grf, priv->pmu, func);
		break;
	case PERIPH_ID_SPI0:
	case PERIPH_ID_SPI1:
	case PERIPH_ID_SPI2:
		pinctrl_rk3188_spi_config(priv->grf, func, flags);
		break;
	case PERIPH_ID_UART0:
	case PERIPH_ID_UART1:
	case PERIPH_ID_UART2:
	case PERIPH_ID_UART3:
	case PERIPH_ID_UART4:
		pinctrl_rk3188_uart_config(priv->grf, func);
		break;
	case PERIPH_ID_LCDC0:
	case PERIPH_ID_LCDC1:
		pinctrl_rk3188_lcdc_config(priv->grf, func);
		break;
	case PERIPH_ID_SDMMC0:
	case PERIPH_ID_SDMMC1:
		pinctrl_rk3188_sdmmc_config(priv->grf, func);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rk3188_pinctrl_get_periph_id(struct udevice *dev,
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
	case 46:
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

static int rk3188_pinctrl_set_state_simple(struct udevice *dev,
					   struct udevice *periph)
{
	int func;

	func = rk3188_pinctrl_get_periph_id(dev, periph);
	if (func < 0)
		return func;
	return rk3188_pinctrl_request(dev, func, 0);
}

static struct pinctrl_ops rk3188_pinctrl_ops = {
	.set_state_simple	= rk3188_pinctrl_set_state_simple,
	.request	= rk3188_pinctrl_request,
	.get_periph_id	= rk3188_pinctrl_get_periph_id,
};

static int rk3188_pinctrl_probe(struct udevice *dev)
{
	struct rk3188_pinctrl_priv *priv = dev_get_priv(dev);

	priv->grf = syscon_get_first_range(ROCKCHIP_SYSCON_GRF);
	priv->pmu = syscon_get_first_range(ROCKCHIP_SYSCON_PMU);
	debug("%s: grf=%p, pmu=%p\n", __func__, priv->grf, priv->pmu);

	return 0;
}

static const struct udevice_id rk3188_pinctrl_ids[] = {
	{ .compatible = "rockchip,rk3188-pinctrl" },
	{ }
};

U_BOOT_DRIVER(pinctrl_rk3188) = {
	.name		= "pinctrl_rk3188",
	.id		= UCLASS_PINCTRL,
	.of_match	= rk3188_pinctrl_ids,
	.priv_auto_alloc_size = sizeof(struct rk3188_pinctrl_priv),
	.ops		= &rk3188_pinctrl_ops,
	.probe		= rk3188_pinctrl_probe,
};
