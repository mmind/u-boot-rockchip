/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <debug_uart.h>
#include <dm.h>
#include <fdtdec.h>
#include <led.h>
#include <malloc.h>
#include <ram.h>
#include <spl.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/hardware.h>
#include <asm/arch/periph.h>
#include <asm/arch/sdram.h>
#include <dm/pinctrl.h>
#include <dm/root.h>
#include <dm/test.h>
#include <dm/util.h>
#include <power/regulator.h>

DECLARE_GLOBAL_DATA_PTR;

u32 spl_boot_device(void)
{
	const void *blob = gd->fdt_blob;
	struct udevice *dev;
	const char *bootdev;
	int node;
	int ret;

	bootdev = fdtdec_get_config_string(blob, "u-boot,boot0");
	debug("Boot device %s\n", bootdev);
	if (!bootdev)
		goto fallback;

	node = fdt_path_offset(blob, bootdev);
	if (node < 0) {
		debug("node=%d\n", node);
		goto fallback;
	}
	ret = device_get_global_by_of_offset(node, &dev);
	if (ret) {
		debug("device at node %s/%d not found: %d\n", bootdev, node,
		      ret);
		goto fallback;
	}
	debug("Found device %s\n", dev->name);
	switch (device_get_uclass_id(dev)) {
	case UCLASS_SPI_FLASH:
		return BOOT_DEVICE_SPI;
	case UCLASS_MMC:
		return BOOT_DEVICE_MMC1;
	default:
		debug("Booting from device uclass '%s' not supported\n",
		      dev_get_uclass_name(dev));
	}

fallback:
	return BOOT_DEVICE_MMC1;
}

u32 spl_boot_mode(void)
{
	return MMCSD_MODE_RAW;
}

static int configure_emmc(struct udevice *pinctrl)
{
	struct gpio_desc desc;
	int ret;

	pinctrl_request_noflags(pinctrl, PERIPH_ID_EMMC);

	/*
	 * TODO(sjg@chromium.org): Pick this up from device tree or perhaps
	 * use the EMMC_PWREN setting.
	 */
	ret = dm_gpio_lookup_name("D9", &desc);
	if (ret) {
		debug("gpio ret=%d\n", ret);
		return ret;
	}
	ret = dm_gpio_request(&desc, "emmc_pwren");
	if (ret) {
		debug("gpio_request ret=%d\n", ret);
		return ret;
	}
	ret = dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT);
	if (ret) {
		debug("gpio dir ret=%d\n", ret);
		return ret;
	}
	ret = dm_gpio_set_value(&desc, 1);
	if (ret) {
		debug("gpio value ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int setup_led(void)
{
#ifdef CONFIG_SPL_LED
	struct udevice *dev;
	char *led_name;
	int ret;

	led_name = fdtdec_get_config_string(gd->fdt_blob, "u-boot,boot-led");
	if (!led_name)
		return 0;
	ret = led_get_by_label(led_name, &dev);
	if (ret) {
		debug("%s: get=%d\n", __func__, ret);
		return ret;
	}
	ret = led_set_on(dev, 1);
	if (ret)
		return ret;
#endif

	return 0;
}

void spl_board_init(void)
{
	struct udevice *pinctrl;
	int ret;

	ret = setup_led();

	if (ret) {
		debug("LED ret=%d\n", ret);
		hang();
	}

	ret = uclass_get_device(UCLASS_PINCTRL, 0, &pinctrl);
	if (ret) {
		debug("%s: Cannot find pinctrl device\n", __func__);
		goto err;
	}
	ret = pinctrl_request_noflags(pinctrl, PERIPH_ID_SDCARD);
	if (ret) {
		debug("%s: Failed to set up SD card\n", __func__);
		goto err;
	}
	ret = configure_emmc(pinctrl);
	if (ret) {
		debug("%s: Failed to set up eMMC\n", __func__);
		goto err;
	}

	/* Enable debug UART */
	ret = pinctrl_request_noflags(pinctrl, PERIPH_ID_UART_DBG);
	if (ret) {
		debug("%s: Failed to set up console UART\n", __func__);
		goto err;
	}

	preloader_console_init();
	return;
err:
	printf("spl_board_init: Error %d\n", ret);

	/* No way to report error here */
	hang();
}
