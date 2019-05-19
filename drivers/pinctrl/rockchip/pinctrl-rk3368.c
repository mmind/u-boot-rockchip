// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2019 Rockchip Electronics Co., Ltd
 * (C) Copyright 2019 Theobroma Systems Design und Consulting GmbH
 */

#include <common.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/arch-rockchip/grf_rk3368.h>
#include <asm/arch-rockchip/periph.h>

#include "pinctrl-rockchip.h"

#define RK3368_PULL_GRF_OFFSET		0x100
#define RK3368_PULL_PMU_OFFSET		0x10

static void rk3368_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					 int pin_num, struct regmap **regmap,
					 int *reg, u8 *bit)
{
	struct rockchip_pinctrl_priv *priv = bank->priv;

	/* The first 32 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		*regmap = priv->regmap_pmu;
		*reg = RK3368_PULL_PMU_OFFSET;

		*reg += ((pin_num / ROCKCHIP_PULL_PINS_PER_REG) * 4);
		*bit = pin_num % ROCKCHIP_PULL_PINS_PER_REG;
		*bit *= ROCKCHIP_PULL_BITS_PER_PIN;
	} else {
		*regmap = priv->regmap_base;
		*reg = RK3368_PULL_GRF_OFFSET;

		/* correct the offset, as we're starting with the 2nd bank */
		*reg -= 0x10;
		*reg += bank->bank_num * ROCKCHIP_PULL_BANK_STRIDE;
		*reg += ((pin_num / ROCKCHIP_PULL_PINS_PER_REG) * 4);

		*bit = (pin_num % ROCKCHIP_PULL_PINS_PER_REG);
		*bit *= ROCKCHIP_PULL_BITS_PER_PIN;
	}
}

#define RK3368_DRV_PMU_OFFSET		0x20
#define RK3368_DRV_GRF_OFFSET		0x200

static void rk3368_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl_priv *priv = bank->priv;

	/* The first 32 pins of the first bank are located in PMU */
	if (bank->bank_num == 0) {
		*regmap = priv->regmap_pmu;
		*reg = RK3368_DRV_PMU_OFFSET;

		*reg += ((pin_num / ROCKCHIP_DRV_PINS_PER_REG) * 4);
		*bit = pin_num % ROCKCHIP_DRV_PINS_PER_REG;
		*bit *= ROCKCHIP_DRV_BITS_PER_PIN;
	} else {
		*regmap = priv->regmap_base;
		*reg = RK3368_DRV_GRF_OFFSET;

		/* correct the offset, as we're starting with the 2nd bank */
		*reg -= 0x10;
		*reg += bank->bank_num * ROCKCHIP_DRV_BANK_STRIDE;
		*reg += ((pin_num / ROCKCHIP_DRV_PINS_PER_REG) * 4);

		*bit = (pin_num % ROCKCHIP_DRV_PINS_PER_REG);
		*bit *= ROCKCHIP_DRV_BITS_PER_PIN;
	}
}

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

static struct rockchip_pin_ctrl rk3368_pin_ctrl = {
		.pin_banks		= rk3368_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3368_pin_banks),
		.label			= "RK3368-GPIO",
		.type			= RK3368,
		.grf_mux_offset		= 0x0,
		.pmu_mux_offset		= 0x0,
		.pull_calc_reg		= rk3368_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3368_calc_drv_reg_and_bit,
};

enum {
	GPIO1C7_SHIFT           = 14,
	GPIO1C7_MASK            = GENMASK(GPIO1C7_SHIFT + 1, GPIO1C7_SHIFT),
	GPIO1C7_GPIO            = 0,
	GPIO1C7_EMMC_DATA5      = (2 << GPIO1C7_SHIFT),
	GPIO1C7_SPI0_TXD        = (3 << GPIO1C7_SHIFT),

	GPIO1C6_SHIFT           = 12,
	GPIO1C6_MASK            = GENMASK(GPIO1C6_SHIFT + 1, GPIO1C6_SHIFT),
	GPIO1C6_GPIO            = 0,
	GPIO1C6_EMMC_DATA4      = (2 << GPIO1C6_SHIFT),
	GPIO1C6_SPI0_RXD        = (3 << GPIO1C6_SHIFT),

	GPIO1C5_SHIFT           = 10,
	GPIO1C5_MASK            = GENMASK(GPIO1C5_SHIFT + 1, GPIO1C5_SHIFT),
	GPIO1C5_GPIO            = 0,
	GPIO1C5_EMMC_DATA3      = (2 << GPIO1C5_SHIFT),

	GPIO1C4_SHIFT           = 8,
	GPIO1C4_MASK            = GENMASK(GPIO1C4_SHIFT + 1, GPIO1C4_SHIFT),
	GPIO1C4_GPIO            = 0,
	GPIO1C4_EMMC_DATA2      = (2 << GPIO1C4_SHIFT),

	GPIO1C3_SHIFT           = 6,
	GPIO1C3_MASK            = GENMASK(GPIO1C3_SHIFT + 1, GPIO1C3_SHIFT),
	GPIO1C3_GPIO            = 0,
	GPIO1C3_EMMC_DATA1      = (2 << GPIO1C3_SHIFT),

	GPIO1C2_SHIFT           = 4,
	GPIO1C2_MASK            = GENMASK(GPIO1C2_SHIFT + 1, GPIO1C2_SHIFT),
	GPIO1C2_GPIO            = 0,
	GPIO1C2_EMMC_DATA0      = (2 << GPIO1C2_SHIFT),
};

enum {
	GPIO1D3_SHIFT           = 6,
	GPIO1D3_MASK            = GENMASK(GPIO1D3_SHIFT + 1, GPIO1D3_SHIFT),
	GPIO1D3_GPIO            = 0,
	GPIO1D3_EMMC_PWREN      = (2 << GPIO1D3_SHIFT),

	GPIO1D2_SHIFT           = 4,
	GPIO1D2_MASK            = GENMASK(GPIO1D2_SHIFT + 1, GPIO1D2_SHIFT),
	GPIO1D2_GPIO            = 0,
	GPIO1D2_EMMC_CMD        = (2 << GPIO1D2_SHIFT),

	GPIO1D1_SHIFT           = 2,
	GPIO1D1_MASK            = GENMASK(GPIO1D1_SHIFT + 1, GPIO1D1_SHIFT),
	GPIO1D1_GPIO            = 0,
	GPIO1D1_EMMC_DATA7      = (2 << GPIO1D1_SHIFT),
	GPIO1D1_SPI0_CSN1       = (3 << GPIO1D1_SHIFT),

	GPIO1D0_SHIFT           = 0,
	GPIO1D0_MASK            = GENMASK(GPIO1D0_SHIFT + 1, GPIO1D0_SHIFT),
	GPIO1D0_GPIO            = 0,
	GPIO1D0_EMMC_DATA6      = (2 << GPIO1D0_SHIFT),
	GPIO1D0_SPI0_CSN0       = (3 << GPIO1D0_SHIFT),
};

enum {
	GPIO2A7_SHIFT           = 14,
	GPIO2A7_MASK            = GENMASK(GPIO2A7_SHIFT + 1, GPIO2A7_SHIFT),
	GPIO2A7_GPIO            = 0,
	GPIO2A7_SDMMC0_D2       = (1 << GPIO2A7_SHIFT),
	GPIO2A7_JTAG_TCK        = (2 << GPIO2A7_SHIFT),

	GPIO2A6_SHIFT           = 12,
	GPIO2A6_MASK            = GENMASK(GPIO2A6_SHIFT + 1, GPIO2A6_SHIFT),
	GPIO2A6_GPIO            = 0,
	GPIO2A6_SDMMC0_D1       = (1 << GPIO2A6_SHIFT),
	GPIO2A6_UART2_SIN       = (2 << GPIO2A6_SHIFT),

	GPIO2A5_SHIFT           = 10,
	GPIO2A5_MASK            = GENMASK(GPIO2A5_SHIFT + 1, GPIO2A5_SHIFT),
	GPIO2A5_GPIO            = 0,
	GPIO2A5_SDMMC0_D0       = (1 << GPIO2A5_SHIFT),
	GPIO2A5_UART2_SOUT      = (2 << GPIO2A5_SHIFT),

	GPIO2A4_SHIFT           = 8,
	GPIO2A4_MASK            = GENMASK(GPIO2A4_SHIFT + 1, GPIO2A4_SHIFT),
	GPIO2A4_GPIO            = 0,
	GPIO2A4_FLASH_DQS       = (1 << GPIO2A4_SHIFT),
	GPIO2A4_EMMC_CLKOUT     = (2 << GPIO2A4_SHIFT),

	GPIO2A3_SHIFT           = 6,
	GPIO2A3_MASK            = GENMASK(GPIO2A3_SHIFT + 1, GPIO2A3_SHIFT),
	GPIO2A3_GPIO            = 0,
	GPIO2A3_FLASH_CSN3      = (1 << GPIO2A3_SHIFT),
	GPIO2A3_EMMC_RSTNOUT    = (2 << GPIO2A3_SHIFT),
};

enum {
	GPIO2B3_SHIFT           = 6,
	GPIO2B3_MASK            = GENMASK(GPIO2B3_SHIFT + 1, GPIO2B3_SHIFT),
	GPIO2B3_GPIO            = 0,
	GPIO2B3_SDMMC0_DTECTN   = (1 << GPIO2B3_SHIFT),

	GPIO2B2_SHIFT           = 4,
	GPIO2B2_MASK            = GENMASK(GPIO2B2_SHIFT + 1, GPIO2B2_SHIFT),
	GPIO2B2_GPIO            = 0,
	GPIO2B2_SDMMC0_CMD      = (1 << GPIO2B2_SHIFT),

	GPIO2B1_SHIFT           = 2,
	GPIO2B1_MASK            = GENMASK(GPIO2B1_SHIFT + 1, GPIO2B1_SHIFT),
	GPIO2B1_GPIO            = 0,
	GPIO2B1_SDMMC0_CLKOUT   = (1 << GPIO2B1_SHIFT),

	GPIO2B0_SHIFT           = 0,
	GPIO2B0_MASK            = GENMASK(GPIO2B0_SHIFT + 1, GPIO2B0_SHIFT),
	GPIO2B0_GPIO            = 0,
	GPIO2B0_SDMMC0_D3       = (1 << GPIO2B0_SHIFT),
};

static int rk3368_pinctrl_get_periph_id(struct udevice *dev,
					struct udevice *periph)
{
	switch (dev_read_addr(periph)) {
	case 0xff690000:
		return PERIPH_ID_UART2;
	case 0xff810000:
		return PERIPH_ID_UART0;
	case 0xff0f0000:
		return PERIPH_ID_EMMC;
	case 0xff0c0000:
		return PERIPH_ID_SDCARD;
	}

	return -ENOENT;
}

static void pinctrl_rk3368_sdmmc_config(struct udevice *dev, int mmc_id)
{
	struct rockchip_pinctrl_priv *priv = dev_get_priv(dev);

	switch (mmc_id) {
	case PERIPH_ID_EMMC:
		debug("mmc id = %d setting registers!\n", mmc_id);
		regmap_write(priv->regmap_base,
			     offsetof(struct rk3368_grf, gpio1c_iomux),
			     ((GPIO1C2_MASK | GPIO1C3_MASK |
			       GPIO1C4_MASK | GPIO1C5_MASK |
			       GPIO1C6_MASK | GPIO1C7_MASK) << 16) |
			     (GPIO1C2_EMMC_DATA0 |
			      GPIO1C3_EMMC_DATA1 |
			      GPIO1C4_EMMC_DATA2 |
			      GPIO1C5_EMMC_DATA3 |
			      GPIO1C6_EMMC_DATA4 |
			      GPIO1C7_EMMC_DATA5));
		regmap_write(priv->regmap_base,
			     offsetof(struct rk3368_grf, gpio1d_iomux),
			     ((GPIO1D0_MASK | GPIO1D1_MASK |
			       GPIO1D2_MASK | GPIO1D3_MASK) << 16) |
			     (GPIO1D0_EMMC_DATA6 |
			      GPIO1D1_EMMC_DATA7 |
			      GPIO1D2_EMMC_CMD |
			      GPIO1D3_EMMC_PWREN));
		regmap_write(priv->regmap_base,
			     offsetof(struct rk3368_grf, gpio2a_iomux),
			     (GPIO2A3_MASK | GPIO2A4_MASK) << 16 |
			     (GPIO2A3_EMMC_RSTNOUT | GPIO2A4_EMMC_CLKOUT));
		break;
	case PERIPH_ID_SDCARD:
		debug("mmc id = %d setting registers!\n", mmc_id);
		regmap_write(priv->regmap_base,
			     offsetof(struct rk3368_grf, gpio2a_iomux),
			     ((GPIO2A5_MASK | GPIO2A6_MASK | GPIO2A7_MASK) << 16) |
			     (GPIO2A5_SDMMC0_D0 | GPIO2A6_SDMMC0_D1 |
			      GPIO2A7_SDMMC0_D2));
		regmap_write(priv->regmap_base,
			     offsetof(struct rk3368_grf, gpio2b_iomux),
			     ((GPIO2B0_MASK | GPIO2B1_MASK |
			       GPIO2B2_MASK | GPIO2B3_MASK) << 16) |
			     (GPIO2B0_SDMMC0_D3 | GPIO2B1_SDMMC0_CLKOUT |
			      GPIO2B2_SDMMC0_CMD | GPIO2B3_SDMMC0_DTECTN));
		break;
	default:
		debug("mmc id = %d iomux error!\n", mmc_id);
		break;
	}
}

static void pinctrl_rk3368_uart_config(struct udevice *dev, int uart_id)
{
	struct rockchip_pinctrl_priv *priv = dev_get_priv(dev);

	switch (uart_id) {
	case PERIPH_ID_UART2:
		regmap_write(priv->regmap_base,
			     offsetof(struct rk3368_grf, gpio2a_iomux),
			     (GPIO2A6_MASK | GPIO2A5_MASK) << 16 |
			     (GPIO2A6_UART2_SIN | GPIO2A5_UART2_SOUT));
		break;
	default:
		debug("uart id = %d iomux error!\n", uart_id);
		break;
	}
}

static int rk3368_pinctrl_request(struct udevice *dev, int func, int flags)
{
	debug("%s: func=%x, flags=%x\n", __func__, func, flags);
	switch (func) {
	case PERIPH_ID_UART2:
		pinctrl_rk3368_uart_config(dev, func);
		break;
	case PERIPH_ID_EMMC:
	case PERIPH_ID_SDCARD:
		pinctrl_rk3368_sdmmc_config(dev, func);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rk3368_pinctrl_set_state_simple(struct udevice *dev,
					   struct udevice *periph)
{
	int func = rk3368_pinctrl_get_periph_id(dev, periph);

	debug("%s: func %d\n", __func__, func);

	if (func < 0)
		return func;

	return rk3368_pinctrl_request(dev, func, 0);
}

const struct pinctrl_ops rk3368_pinctrl_simple_ops = {
	.set_state_simple = rk3368_pinctrl_set_state_simple,
};

static const struct udevice_id rk3368_pinctrl_ids[] = {
	{
		.compatible = "rockchip,rk3368-pinctrl",
		.data = (ulong)&rk3368_pin_ctrl
	},
	{ }
};

U_BOOT_DRIVER(pinctrl_rk3368) = {
	.name		= "rockchip_rk3368_pinctrl",
	.id		= UCLASS_PINCTRL,
	.of_match	= rk3368_pinctrl_ids,
	.priv_auto_alloc_size = sizeof(struct rockchip_pinctrl_priv),
#if CONFIG_IS_ENABLED(PINCTRL_FULL)
	.ops		= &rockchip_pinctrl_ops,
#else
	.ops            = &rk3368_pinctrl_simple_ops,
#endif
#if !CONFIG_IS_ENABLED(OF_PLATDATA)
	.bind		= dm_scan_fdt_dev,
#endif
	.probe		= rockchip_pinctrl_probe,
};
