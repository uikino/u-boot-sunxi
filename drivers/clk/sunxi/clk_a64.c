// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Amarula Solutions.
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <asm/arch/ccu.h>
#include <dt-bindings/clock/sun50i-a64-ccu.h>

static const struct ccu_clk_gate a64_gates[] = {
	[CLK_BUS_OTG]		= GATE(0x060, BIT(23)),
	[CLK_BUS_EHCI0]		= GATE(0x060, BIT(24)),
	[CLK_BUS_EHCI1]		= GATE(0x060, BIT(25)),
	[CLK_BUS_OHCI0]		= GATE(0x060, BIT(28)),
	[CLK_BUS_OHCI1]		= GATE(0x060, BIT(29)),

	[CLK_USB_PHY0]		= GATE(0x0cc, BIT(8)),
	[CLK_USB_PHY1]		= GATE(0x0cc, BIT(9)),
	[CLK_USB_HSIC]		= GATE(0x0cc, BIT(10)),
	[CLK_USB_HSIC_12M]	= GATE(0x0cc, BIT(11)),
	[CLK_USB_OHCI0]		= GATE(0x0cc, BIT(16)),
	[CLK_USB_OHCI1]		= GATE(0x0cc, BIT(17)),
};

static const struct ccu_desc a64_ccu_desc = {
	.gates = a64_gates,
};

static const struct udevice_id a64_ccu_ids[] = {
	{ .compatible = "allwinner,sun50i-a64-ccu",
	  .data = (ulong)&a64_ccu_desc },
	{ }
};

U_BOOT_DRIVER(clk_sun50i_a64) = {
	.name		= "sun50i_a64_ccu",
	.id		= UCLASS_CLK,
	.of_match	= a64_ccu_ids,
	.priv_auto_alloc_size	= sizeof(struct ccu_priv),
	.ops		= &sunxi_clk_ops,
	.probe		= sunxi_clk_probe,
};
