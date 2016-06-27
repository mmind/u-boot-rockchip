/*
 * Copyright (C) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <syscon.h>
#include <asm/arch/clock.h>

static const struct udevice_id rk3368_syscon_ids[] = {
	{ .compatible = "rockchip,rk3368-noc", .data = ROCKCHIP_SYSCON_NOC },
	{ .compatible = "rockchip,rk3368-grf", .data = ROCKCHIP_SYSCON_GRF },
	{ .compatible = "rockchip,rk3368-sgrf", .data = ROCKCHIP_SYSCON_SGRF },
	{ .compatible = "rockchip,rk3368-pmu", .data = ROCKCHIP_SYSCON_PMU },
	{ .compatible = "rockchip,rk3368-pmugrf", .data = ROCKCHIP_SYSCON_PMUGRF },

	{ }
};

U_BOOT_DRIVER(syscon_rk3368) = {
	.name = "rk3368_syscon",
	.id = UCLASS_SYSCON,
	.of_match = rk3368_syscon_ids,
};
