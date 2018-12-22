// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Amarula Solutions.
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 */

#ifndef _ASM_ARCH_CCU_H
#define _ASM_ARCH_CCU_H

/**
 * enum ccu_clk_flags - ccu clock flags
 *
 * @CCU_CLK_F_INIT_DONE:		clock gate init done check
 */
enum ccu_clk_flags {
	CCU_CLK_F_INIT_DONE		= BIT(0),
};

/**
 * struct ccu_clk_gate - ccu clock gate
 * @off:	gate offset
 * @bit:	gate bit
 * @flags:	clock gate flags
 */
struct ccu_clk_gate {
	u16 off;
	u32 bit;
	enum ccu_clk_flags flags;
};

#define GATE(_off, _bit) {			\
	.off = _off,				\
	.bit = _bit,				\
	.flags = CCU_CLK_F_INIT_DONE,		\
}

/**
 * struct ccu_reset - ccu reset
 * @off:	reset offset
 * @bit:	reset bit
 * @flags:	reset flags
 */
struct ccu_reset {
	u16 off;
	u32 bit;
	enum ccu_clk_flags flags;
};

#define RESET(_off, _bit) {			\
	.off = _off,				\
	.bit = _bit,				\
	.flags = CCU_CLK_F_INIT_DONE,		\
}

/**
 * struct ccu_desc - clock control unit descriptor
 *
 * @gates:	clock gates
 * @resets:	reset unit
 */
struct ccu_desc {
	const struct ccu_clk_gate *gates;
	const struct ccu_reset *resets;
};

/**
 * struct ccu_priv - sunxi clock control unit
 *
 * @base:	base address
 * @desc:	ccu descriptor
 */
struct ccu_priv {
	void *base;
	const struct ccu_desc *desc;
};

/**
 * sunxi_clk_probe - common sunxi clock probe
 * @dev:	clock device
 */
int sunxi_clk_probe(struct udevice *dev);

extern struct clk_ops sunxi_clk_ops;

/**
 * sunxi_reset_bind() - reset binding
 *
 * @dev:       reset device
 * @count:     reset count
 * @return 0 success, or error value
 */
int sunxi_reset_bind(struct udevice *dev, ulong count);

#endif /* _ASM_ARCH_CCU_H */
