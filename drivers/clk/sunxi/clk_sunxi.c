// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Amarula Solutions.
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/ccu.h>
#include <linux/log2.h>

static const struct ccu_clk_gate *priv_to_gate(struct ccu_priv *priv,
					       unsigned long id)
{
	return &priv->desc->gates[id];
}

static const struct ccu_clk_tree *priv_to_tree(struct ccu_priv *priv,
					       unsigned long id)
{
	return &priv->desc->tree[id];
}

static int sunxi_get_parent_idx(const struct ccu_clk_tree *tree, void *base)
{
	u32 reg, idx;

	reg = readl(base + tree->off);
	idx = reg >> tree->mux.shift;
	idx &= (1 << tree->mux.width) - 1;

	return idx;
}

static ulong sunxi_fixed_get_rate(struct clk *clk, unsigned long id)
{
	struct ccu_priv *priv = dev_get_priv(clk->dev);
	const struct ccu_clk_tree *tree = priv_to_tree(priv, id);

	if (!(tree->flags & CCU_CLK_F_INIT_DONE)) {
		printf("%s: (CLK#%ld) unhandled\n", __func__, clk->id);
		return 0;
	}

	return tree->fixed_rate;
}

static ulong sunxi_nk_get_parent_rate(struct clk *clk, unsigned long id)
{
	struct ccu_priv *priv = dev_get_priv(clk->dev);
	const struct ccu_clk_tree *tree = priv_to_tree(priv, id);
	ulong rate = 0;

	switch (tree->type) {
	case CCU_CLK_TYPE_FIXED:
		rate = sunxi_fixed_get_rate(clk, id);
		break;
	default:
		printf("%s: Unknown (TYPE#%d)\n", __func__, tree->type);
		break;
	}

	return rate;
}

static ulong sunxi_nk_get_rate(struct clk *clk, unsigned long id)
{
	struct ccu_priv *priv = dev_get_priv(clk->dev);
	const struct ccu_clk_tree *tree = priv_to_tree(priv, id);
	ulong rate, parent_rate;
	unsigned long n, k;
	u32 reg;

	parent_rate = sunxi_nk_get_parent_rate(clk, tree->parent[0]);

	reg = readl(priv->base + tree->off);

	n = reg >> tree->n.shift;
	n &= (1 << tree->n.width) - 1;
	n += tree->n.offset;
	if (!n)
		n++;

	k = reg >> tree->k.shift;
	k &= (1 << tree->k.width) - 1;
	k += tree->k.offset;
	if (!k)
		k++;

	rate = parent_rate * n * k;
	if (tree->flags & CCU_CLK_F_POSTDIV)
		rate /= tree->postdiv;

	return rate;
}

static ulong sunxi_mp_get_parent_rate(struct clk *clk, unsigned long id)
{
	struct ccu_priv *priv = dev_get_priv(clk->dev);
	const struct ccu_clk_tree *tree = priv_to_tree(priv, id);
	ulong rate = 0;

	if (!(tree->flags & CCU_CLK_F_INIT_DONE)) {
		printf("%s: (CLK#%ld) unhandled\n", __func__, clk->id);
		return 0;
	}

	switch (tree->type) {
	case CCU_CLK_TYPE_FIXED:
		rate = sunxi_fixed_get_rate(clk, id);
		break;
	case CCU_CLK_TYPE_NK:
		rate = sunxi_nk_get_rate(clk, id);
		break;
	default:
		printf("%s: (TYPE#%d) unhandled\n", __func__, tree->type);
		break;
	}

	return rate;
}

static ulong sunxi_mp_get_rate(struct clk *clk, unsigned long id)
{
	struct ccu_priv *priv = dev_get_priv(clk->dev);
	const struct ccu_clk_tree *tree = priv_to_tree(priv, id);
	unsigned int m, p;
	ulong parent_rate;
	u32 reg, idx;

	idx = sunxi_get_parent_idx(tree, priv->base);
	if (idx < 0) {
		printf("%s: Wrong parent index %d\n", __func__, idx);
		return 0;
	}

	parent_rate = sunxi_mp_get_parent_rate(clk, tree->parent[idx]);

	reg = readl(priv->base + tree->off);

	m = reg >> tree->m.shift;
	m &= (1 << tree->m.width) - 1;
	m += tree->m.offset;
	if (!m)
		m++;

	p = reg >> tree->p.shift;
	p &= (1 << tree->p.width) - 1;

	return (parent_rate >> p) / m;
}

static ulong sunxi_misc_get_rate(struct clk *clk, unsigned long id)
{
	struct ccu_priv *priv = dev_get_priv(clk->dev);
	const struct ccu_clk_tree *tree = priv_to_tree(priv, id);
	ulong rate = 0;

	if (!(tree->flags & CCU_CLK_F_INIT_DONE)) {
		printf("%s: (CLK#%ld) unhandled\n", __func__, clk->id);
		return 0;
	}

	switch (tree->type) {
	case CCU_CLK_TYPE_MP:
		rate = sunxi_mp_get_rate(clk, id);
		break;
	default:
		printf("%s: (TYPE#%d) unhandled\n", __func__, tree->type);
		break;
	}

	return rate;
}

static ulong sunxi_clk_get_rate(struct clk *clk)
{
	struct ccu_priv *priv = dev_get_priv(clk->dev);
	const struct ccu_clk_tree *tree = priv_to_tree(priv, clk->id);
	ulong rate = 0;

	if (!(tree->flags & CCU_CLK_F_INIT_DONE)) {
		printf("%s: (CLK#%ld) unhandled\n", __func__, clk->id);
		return 0;
	}

	switch (tree->type) {
	case CCU_CLK_TYPE_MISC:
		rate = sunxi_misc_get_rate(clk, tree->parent[0]);
		break;
	default:
		printf("%s: (TYPE#%d) unhandled\n", __func__, tree->type);
		break;
	}

	return rate;
}

static int sunxi_set_gate(struct clk *clk, bool on)
{
	struct ccu_priv *priv = dev_get_priv(clk->dev);
	const struct ccu_clk_gate *gate = priv_to_gate(priv, clk->id);
	u32 reg;

	if (!(gate->flags & CCU_CLK_F_INIT_DONE)) {
		printf("%s: (CLK#%ld) unhandled\n", __func__, clk->id);
		return 0;
	}

	debug("%s: (CLK#%ld) off#0x%x, BIT(%d)\n", __func__,
	      clk->id, gate->off, ilog2(gate->bit));

	reg = readl(priv->base + gate->off);
	if (on)
		reg |= gate->bit;
	else
		reg &= ~gate->bit;

	writel(reg, priv->base + gate->off);

	return 0;
}

static int sunxi_clk_enable(struct clk *clk)
{
	return sunxi_set_gate(clk, true);
}

static int sunxi_clk_disable(struct clk *clk)
{
	return sunxi_set_gate(clk, false);
}

struct clk_ops sunxi_clk_ops = {
	.enable = sunxi_clk_enable,
	.disable = sunxi_clk_disable,
	.get_rate = sunxi_clk_get_rate,
};

int sunxi_clk_probe(struct udevice *dev)
{
	struct ccu_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base)
		return -ENOMEM;

	priv->desc = (const struct ccu_desc *)dev_get_driver_data(dev);
	if (!priv->desc)
		return -EINVAL;

	return 0;
}
