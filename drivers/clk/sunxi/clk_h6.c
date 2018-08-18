// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2018 Amarula Solutions.
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <asm/arch/ccu.h>
#include <dt-bindings/clock/sun50i-h6-ccu.h>
#include <dt-bindings/reset/sun50i-h6-ccu.h>

static struct ccu_clk_map h6_clks[] = {
	[CLK_MMC0]		= { 0x830, BIT(31), &mmc_clk_set_rate },
	[CLK_MMC1]		= { 0x834, BIT(31), &mmc_clk_set_rate },
	[CLK_MMC2]		= { 0x834, BIT(31), &mmc_clk_set_rate },

	[CLK_BUS_MMC0]		= { 0x84c, BIT(0), NULL },
	[CLK_BUS_MMC1]		= { 0x84c, BIT(1), NULL },
	[CLK_BUS_MMC2]		= { 0x84c, BIT(2), NULL },
};

static struct ccu_reset_map h6_resets[] = {
	[CLK_BUS_MMC0]		= { 0x84c, BIT(16) },
	[CLK_BUS_MMC1]		= { 0x84c, BIT(17) },
	[CLK_BUS_MMC2]		= { 0x84c, BIT(18) },
};

static const struct ccu_desc sun50i_h6_ccu_desc = {
	.clks = h6_clks,
	.num_clks = ARRAY_SIZE(h6_clks),

	.resets = h6_resets,
	.num_resets =  ARRAY_SIZE(h6_resets),
};

static int h6_clk_probe(struct udevice *dev)
{
	struct sunxi_clk_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base)
		return -ENOMEM;

	priv->desc = (const struct ccu_desc *)dev_get_driver_data(dev);
	if (!priv->desc)
		return -EINVAL;

	return 0;
}

static int h6_clk_bind(struct udevice *dev)
{
	return sunxi_reset_bind(dev, 62);
}

static const struct udevice_id h6_clk_ids[] = {
	{ .compatible = "allwinner,sun50i-h6-ccu",
	  .data = (ulong)&sun50i_h6_ccu_desc },
	{ }
};

U_BOOT_DRIVER(clk_sun50i_h6) = {
	.name		= "sun50i_h6_ccu",
	.id		= UCLASS_CLK,
	.of_match	= h6_clk_ids,
	.priv_auto_alloc_size	= sizeof(struct sunxi_clk_priv),
	.ops		= &sunxi_clk_ops,
	.probe		= h6_clk_probe,
	.bind		= h6_clk_bind,
};
