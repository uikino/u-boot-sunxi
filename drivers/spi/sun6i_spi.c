// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2018 Amarula Solutions.
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <errno.h>
#include <reset.h>
#include <spi.h>

#include <asm/bitops.h>
#include <asm/gpio.h>
#include <asm/io.h>

#include <asm/arch/clock.h>

DECLARE_GLOBAL_DATA_PTR;

#define SUN8I_FIFO_DEPTH			64

#define SUN6I_GBL_CTL_BUS_ENABLE		BIT(0)
#define SUN6I_GBL_CTL_MASTER			BIT(1)
#define SUN6I_GBL_CTL_TP			BIT(7)
#define SUN6I_GBL_CTL_RST			BIT(31)

#define SUN6I_TFR_CTL_CPHA			BIT(0)
#define SUN6I_TFR_CTL_CPOL			BIT(1)
#define SUN6I_TFR_CTL_SPOL			BIT(2)
#define SUN6I_TFR_CTL_CS_MASK			0x30
#define SUN6I_TFR_CTL_CS(cs)			(((cs) << 4) & SUN6I_TFR_CTL_CS_MASK)
#define SUN6I_TFR_CTL_CS_MANUAL			BIT(6)
#define SUN6I_TFR_CTL_CS_LEVEL			BIT(7)
#define SUN6I_TFR_CTL_DHB			BIT(8)
#define SUN6I_TFR_CTL_FBS			BIT(12)
#define SUN6I_TFR_CTL_XCH_MASK			0x80000000
#define SUN6I_TFR_CTL_XCH			BIT(31)

#define SUN6I_FIFO_CTL_RF_RST			BIT(15)
#define SUN6I_FIFO_CTL_TF_RST			BIT(31)

#define SUN6I_FIFO_STA_RF_CNT_MASK		0xff
#define SUN6I_FIFO_STA_RF_CNT_BITS		0

#define SUN6I_CLK_CTL_CDR2_MASK			0xff
#define SUN6I_CLK_CTL_CDR2(div)			(((div) & SUN6I_CLK_CTL_CDR2_MASK) << 0)
#define SUN6I_CLK_CTL_CDR1_MASK			0xf
#define SUN6I_CLK_CTL_CDR1(div)			(((div) & SUN6I_CLK_CTL_CDR1_MASK) << 8)
#define SUN6I_CLK_CTL_DRS			BIT(12)

#define SUN6I_MAX_XFER_SIZE			0xffffff
#define SUN6I_BURST_CNT(cnt)			((cnt) & SUN6I_MAX_XFER_SIZE)
#define SUN6I_XMIT_CNT(cnt)			((cnt) & SUN6I_MAX_XFER_SIZE)
#define SUN6I_BURST_CTL_CNT_STC(cnt)		((cnt) & SUN6I_MAX_XFER_SIZE)

#define SUNXI_SPI_FIFO_RF_CNT_MASK      0xff
#define SUNXI_SPI_FIFO_RF_CNT_BITS      0
#define SUNXI_SPI_FIFO_TF_CNT_MASK      0xff
#define SUNXI_SPI_FIFO_TF_CNT_BITS      16

#define SUN6I_SPI_MAX_RATE		(24 * 1000 * 1000)
#define SUN6I_SPI_MIN_RATE		(3 * 1000)

struct sun6i_spi_regs {
	u32 res0;
	u32 gblctl;	/* 0x04 */
	u32 tfrctl;	/* 0x08 */
	u32 res1;
	u32 intctl;	/* 0x10 */
	u32 intsta;	/* 0x14 */
	u32 fifoctl;	/* 0x18 */
	u32 fifosta;	/* 0x1c */
	u32 res2;	/* 0x20 */
	u32 clkctl;	/* 0x24 */
	u32 res3[2];
	u32 burstcnt;	/* 0x30 */
	u32 xmitcnt;	/* 0x34 */
	u32 burstctl;	/* 0x38 */
	u32 res4[113];
	u32 txdata;	/* 0x200 */
	u32 res5[63];
	u32 rxdata;	/* 0x300 */
};

struct sun6i_spi_priv {
	struct sun6i_spi_regs *regs;
	u32 fifo_depth;
	struct clk clk_ahb;
	struct clk clk_mod;
	struct reset_ctl reset;

	const u8 *tx_buf;
	u8 *rx_buf;
};

struct sun6i_spi_platdata {
	u32 base;
};

static void sun6i_spi_drain_fifo(struct sun6i_spi_priv *priv, int len)
{
	u8 byte;

	while (len--) {
		byte = readb(&priv->regs->rxdata);
		if (priv->rx_buf)
			*priv->rx_buf++ = byte;
	}
}

static void sun6i_spi_fill_fifo(struct sun6i_spi_priv *priv, int len)
{
	u8 byte;

	while (len--) {
		byte = priv->tx_buf ? *priv->tx_buf++ : 0;
		writeb(byte, &priv->regs->txdata);
	}
}

static inline u32 sun6i_spi_get_rx_fifo_count(struct sun6i_spi_priv *priv)
{
	u32 reg = readl(&priv->regs->fifosta);

	reg >>= SUN6I_FIFO_STA_RF_CNT_BITS;

	return reg & SUN6I_FIFO_STA_RF_CNT_MASK;
}

static void sun6i_spi_set_cs(struct udevice *dev, u8 cs, bool enable)
{
	struct udevice *bus = dev->parent;
	struct sun6i_spi_priv *priv = dev_get_priv(bus);
	u32 reg;

	reg = readl(&priv->regs->tfrctl);
	reg &= ~SUN6I_TFR_CTL_CS_MASK;

	if (enable)
		reg &= ~SUN6I_TFR_CTL_CS_LEVEL;

	if (enable)
		reg |= SUN6I_TFR_CTL_CS(cs);
	else
		reg |= SUN6I_TFR_CTL_CS_LEVEL;

	writel(reg, &priv->regs->tfrctl);
}

static int sun6i_spi_claim_bus(struct udevice *dev)
{
	struct udevice *bus = dev->parent;
	struct sun6i_spi_priv *priv = dev_get_priv(bus);
	int ret;

	ret = clk_enable(&priv->clk_ahb);
	if (ret) {
		dev_err(dev, "failed to enable ahb clock\n");
		return ret;
	}

	ret = clk_enable(&priv->clk_mod);
	if (ret) {
		dev_err(dev, "failed to enable mod clock\n");
		goto err_ahb;
	}

	ret = reset_deassert(&priv->reset);
	if (ret) {
		dev_err(dev, "failed to deassert reset\n");
		goto err_mod;
	}

	setbits_le32(&priv->regs->gblctl, SUN6I_GBL_CTL_MASTER |
		     SUN6I_GBL_CTL_BUS_ENABLE | SUN6I_GBL_CTL_TP |
		     SUN6I_GBL_CTL_RST);
	setbits_le32(&priv->regs->tfrctl, SUN6I_TFR_CTL_CS_MANUAL |
		     SUN6I_TFR_CTL_CS_LEVEL);

	return 0;

err_mod:
	clk_disable(&priv->clk_mod);
err_ahb:
	clk_disable(&priv->clk_ahb);
	return ret;
}

static int sun6i_spi_release_bus(struct udevice *dev)
{
	struct udevice *bus = dev->parent;
	struct sun6i_spi_priv *priv = dev_get_priv(bus);

	clrbits_le32(&priv->regs->gblctl, SUN6I_GBL_CTL_BUS_ENABLE);
	clk_disable(&priv->clk_ahb);
	clk_disable(&priv->clk_mod);
	reset_assert(&priv->reset);

	return 0;
}

static int sun6i_spi_xfer(struct udevice *dev, unsigned int bitlen,
			  const void *dout, void *din, unsigned long flags)
{
	struct udevice *bus = dev->parent;
	struct sun6i_spi_priv *priv = dev_get_priv(bus);
	struct dm_spi_slave_platdata *slave_plat = dev_get_parent_platdata(dev);
	u32 len = bitlen / 8;
	u32 rx_fifocnt;
	u8 nbytes;

	priv->tx_buf = dout;
	priv->rx_buf = din;

	if (bitlen % 8) {
		debug("%s: non byte-aligned SPI transfer.\n", __func__);
		return -EINVAL;
	}

	if (flags & SPI_XFER_BEGIN)
		sun6i_spi_set_cs(dev, slave_plat->cs, true);

	/* Reset FIFOs */
	setbits_le32(&priv->regs->fifoctl, SUN6I_FIFO_CTL_RF_RST |
		     SUN6I_FIFO_CTL_TF_RST);

	while (len) {
		/* Setup the transfer now... */
		nbytes = min(len, priv->fifo_depth - 1);

		/* Setup the counters */
		writel(SUN6I_BURST_CNT(nbytes), &priv->regs->burstcnt);
		writel(SUN6I_XMIT_CNT(nbytes), &priv->regs->xmitcnt);
		writel(SUN6I_BURST_CTL_CNT_STC(nbytes), &priv->regs->burstctl);

		/* Fill the TX FIFO */
		sun6i_spi_fill_fifo(priv, nbytes);

		/* Start the transfer */
		setbits_le32(&priv->regs->tfrctl, SUN6I_TFR_CTL_XCH);

		/**
		 * Wait transfer to complete
		 *
		 * readl_poll_timeout cannot work since it require
		 * an extra delay between reads.
		 */
		do {
			rx_fifocnt = sun6i_spi_get_rx_fifo_count(priv);
		} while (rx_fifocnt < nbytes);

		/* Drain the RX FIFO */
		sun6i_spi_drain_fifo(priv, nbytes);

		len -= nbytes;
	}

	if (flags & SPI_XFER_END)
		sun6i_spi_set_cs(dev, slave_plat->cs, false);

	return 0;
}

static int sun6i_spi_set_speed(struct udevice *bus, uint speed)
{
	struct sun6i_spi_priv *priv = dev_get_priv(bus);
	unsigned int div;
	u32 reg;

	speed = min(speed, (unsigned int)SUN6I_SPI_MAX_RATE);
	speed = max((unsigned int)SUN6I_SPI_MIN_RATE, speed);

	/*
	 * Setup clock divider.
	 *
	 * We have two choices there. Either we can use the clock
	 * divide rate 1, which is calculated thanks to this formula:
	 * SPI_CLK = MOD_CLK / (2 ^ cdr)
	 * Or we can use CDR2, which is calculated with the formula:
	 * SPI_CLK = MOD_CLK / (2 * (cdr + 1))
	 * Whether we use the former or the latter is set through the
	 * DRS bit.
	 *
	 * First try CDR2, and if we can't reach the expected
	 * frequency, fall back to CDR1.
	 */
	reg = readl(&priv->regs->clkctl);
	div = SUN6I_SPI_MAX_RATE / (2 * speed);
	if (div <= (SUN6I_CLK_CTL_CDR2_MASK + 1)) {
		if (div > 0)
			div--;

		reg |= SUN6I_CLK_CTL_CDR2(div) | SUN6I_CLK_CTL_DRS;
	} else {
		div = __ilog2(SUN6I_SPI_MAX_RATE) - __ilog2(speed);
		reg |= SUN6I_CLK_CTL_CDR1(div);
	}

	writel(reg, &priv->regs->clkctl);

	return 0;
}

static int sun6i_spi_set_mode(struct udevice *bus, uint mode)
{
	struct sun6i_spi_priv *priv = dev_get_priv(bus);
	u32 reg;

	reg = readl(&priv->regs->tfrctl);
	reg &= ~(SUN6I_TFR_CTL_CPOL | SUN6I_TFR_CTL_CPHA |
		 SUN6I_TFR_CTL_FBS);

	if (mode & SPI_CPOL)
		reg |= SUN6I_TFR_CTL_CPOL;

	if (mode & SPI_CPHA)
		reg |= SUN6I_TFR_CTL_CPHA;

	if (mode & SPI_LSB_FIRST)
		reg |= SUN6I_TFR_CTL_FBS;

	writel(reg, &priv->regs->tfrctl);

	return 0;
}

static int sun6i_spi_probe(struct udevice *bus)
{
	struct sun6i_spi_platdata *plat = dev_get_platdata(bus);
	struct sun6i_spi_priv *priv = dev_get_priv(bus);
	unsigned int pin_function = SUNXI_GPC_SPI0;
	int pin, ret;

	priv->regs = (struct sun6i_spi_regs *)(uintptr_t)plat->base;
	priv->fifo_depth = dev_get_driver_data(bus);

	/* TODO This pin functions will drop, once sunxi pinctrl in Mainline */
	if (IS_ENABLED(CONFIG_MACH_SUN50I))
		pin_function = SUN50I_GPC_SPI0;

	for (pin = SUNXI_GPC(0); pin <= SUNXI_GPC(3); pin++)
		sunxi_gpio_set_cfgpin(pin, pin_function);

	ret = clk_get_by_name(bus, "ahb", &priv->clk_ahb);
	if (ret) {
		dev_err(dev, "failed to get ahb clock\n");
		return ret;
	}

	ret = clk_get_by_name(bus, "mod", &priv->clk_mod);
	if (ret) {
		dev_err(dev, "failed to get mod clock\n");
		return ret;
	}

	ret = reset_get_by_index(bus, 0, &priv->reset);
	if (ret) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}

	return ret;
}

static int sun6i_spi_ofdata_to_platdata(struct udevice *bus)
{
	struct sun6i_spi_platdata *plat = dev_get_platdata(bus);

	plat->base = devfdt_get_addr(bus);

	return 0;
}

static const struct dm_spi_ops sun6i_spi_ops = {
	.claim_bus	= sun6i_spi_claim_bus,
	.release_bus	= sun6i_spi_release_bus,
	.xfer		= sun6i_spi_xfer,
	.set_speed	= sun6i_spi_set_speed,
	.set_mode	= sun6i_spi_set_mode,
};

static const struct udevice_id sun6i_spi_ids[] = {
	{ .compatible = "allwinner,sun8i-h3-spi", .data = SUN8I_FIFO_DEPTH },
	{ }
};

U_BOOT_DRIVER(sun6i_spi) = {
	.name	= "sun6i_spi",
	.id	= UCLASS_SPI,
	.of_match = sun6i_spi_ids,
	.ops	= &sun6i_spi_ops,
	.ofdata_to_platdata = sun6i_spi_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct sun6i_spi_platdata),
	.priv_auto_alloc_size = sizeof(struct sun6i_spi_priv),
	.probe	= sun6i_spi_probe,
};
