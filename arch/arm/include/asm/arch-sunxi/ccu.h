// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Amarula Solutions.
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 */

#ifndef _ASM_ARCH_CCU_H
#define _ASM_ARCH_CCU_H

#define OSC_32K_ULL		32000ULL
#define OSC_24M_ULL		24000000ULL

/**
 * enum ccu_clk_type - ccu clock types
 *
 * @CCU_CLK_TYPE_MISC:			misc clock type
 * @CCU_CLK_TYPE_FIXED:			fixed clock type
 * @CCU_CLK_TYPE_MP:			mp clock type
 * @CCU_CLK_TYPE_NK:			nk clock type
 */
enum ccu_clk_type {
	CCU_CLK_TYPE_MISC		= 0,
	CCU_CLK_TYPE_FIXED		= 1,
	CCU_CLK_TYPE_MP			= 2,
	CCU_CLK_TYPE_NK			= 3,
};

/**
 * enum ccu_clk_flags - ccu clock flags
 *
 * @CCU_CLK_F_INIT_DONE:		clock tree/gate init done check
 * @CCU_CLK_F_POSTDIV:			clock post divider
 */
enum ccu_clk_flags {
	CCU_CLK_F_INIT_DONE		= BIT(0),
	CCU_CLK_F_POSTDIV		= BIT(1),
};

/**
 * struct ccu_mult - ccu clock multiplier
 *
 * @shift:		multiplier shift value
 * @width:		multiplier width value
 * @offset:		multiplier offset
 * @min:		minimum multiplier
 * @max:		maximum multiplier
 */
struct ccu_mult {
	u8 shift;
	u8 width;
	u8 offset;
	u8 min;
	u8 max;
};

#define _CCU_MULT_OFF_MIN_MAX(_shift, _width, _offset,		\
			      _min, _max) {			\
	.shift = _shift,					\
	.width = _width,					\
	.offset = _offset,					\
	.min = _min,						\
	.max = _max,						\
}

#define _CCU_MULT_MIN(_shift, _width, _min)			\
	_CCU_MULT_OFF_MIN_MAX(_shift, _width, 1, _min, 0)

#define _CCU_MULT(_shift, _width)				\
	_CCU_MULT_OFF_MIN_MAX(_shift, _width, 1, 1, 0)

/**
 * struct ccu_mux - ccu clock multiplexer
 *
 * @shift:		multiplexer shift value
 * @width:		multiplexer width value
 */
struct ccu_mux {
	u8 shift;
	u8 width;
};

#define _CCU_MUX(_shift, _width) {		\
	.shift = _shift,			\
	.width = _width,			\
}

/**
 * struct ccu_div - ccu clock divider
 *
 * @shift:		divider shift value
 * @width:		divider width value
 * @offset:		divider offset
 * @max:		maximum divider value
 */
struct ccu_div {
	u8 shift;
	u8 width;
	u32 offset;
	u32 max;
};

#define _CCU_DIV(_shift, _width) {		\
	.shift = _shift,			\
	.width = _width,			\
	.offset = 1,				\
	.max = 0,				\
}

/**
 * struct ccu_clk_tree - ccu clock tree
 *
 * @parent:		parent clock tree
 * @type:		clock type
 * @off:		clock tree offset
 * @m:			divider m
 * @p:			divider p
 * @mux:		multiplexer mux
 * @post:		post divider value
 * @n:			multiplier n
 * @k:			multiplier k
 * @fixed_rate:		fixed rate
 * @flags:		clock tree flags
 */
struct ccu_clk_tree {
	const unsigned long *parent;
	enum ccu_clk_type type;
	u16 off;

	struct ccu_div m;
	struct ccu_div p;
	struct ccu_mux mux;
	unsigned int postdiv;

	struct ccu_mult n;
	struct ccu_mult k;

	ulong fixed_rate;
	enum ccu_clk_flags flags;
};

#define TREE(_parent, _type, _off,				\
	     _m, _p,						\
	     _mux,						\
	     _postdiv,						\
	     _n, _k,						\
	     _fixed_rate,					\
	     _flags) {						\
	.parent = _parent,					\
	.type = _type,						\
	.off = _off,						\
	.m = _m,						\
	.p = _p,						\
	.mux = _mux,						\
	.postdiv = _postdiv,					\
	.n = _n,						\
	.k = _k,						\
	.fixed_rate = _fixed_rate,				\
	.flags = _flags,					\
}

#define MISC(_parent)						\
	TREE(_parent, CCU_CLK_TYPE_MISC, 0,			\
	     {0}, {0},						\
	     {0},						\
	     0,							\
	     {0}, {0},						\
	     0,							\
	     CCU_CLK_F_INIT_DONE)

#define FIXED(_fixed_rate)					\
	TREE(NULL, CCU_CLK_TYPE_FIXED, 0,			\
	     {0}, {0},						\
	     {0},						\
	     0,							\
	     {0}, {0},						\
	     _fixed_rate,					\
	     CCU_CLK_F_INIT_DONE)

#define NK(_parent, _off,					\
	   _nshift, _nwidth,					\
	   _kshift, _kwidth, _kmin,				\
	   _postdiv,						\
	   _flags)						\
	TREE(_parent, CCU_CLK_TYPE_NK, _off,			\
	     {0}, {0},						\
	     {0},						\
	     _postdiv,						\
	     _CCU_MULT(_nshift, _nwidth),			\
	     _CCU_MULT_MIN(_kshift, _kwidth, _kmin),		\
	     0,							\
	     CCU_CLK_F_INIT_DONE | _flags)

#define MP(_parent, _off,					\
	   _mshift, _mwidth,					\
	   _pshift, _pwidth,					\
	   _muxshift, _muxwidth,				\
	   _postdiv,						\
	   _flags)						\
	TREE(_parent, CCU_CLK_TYPE_MP, _off,			\
	     _CCU_DIV(_mshift, _mwidth),			\
	     _CCU_DIV(_pshift, _pwidth),			\
	     _CCU_MUX(_muxshift, _muxwidth),			\
	     _postdiv,						\
	     {0}, {0},						\
	     0,							\
	     CCU_CLK_F_INIT_DONE | _flags)

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
	const struct ccu_clk_tree *tree;
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
