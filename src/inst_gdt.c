
/* 2025.05.25 by renwang */

#include "bctx.h"

int bctx_inst_lgdt(bctx* ctx, int for_idt/* = false */)
{
	brm_opd rmo;
	ASSERT(ctx);
	if (bctx_parse_term(ctx, &rmo) || rmo.opr != ERMO_MEM) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	size_t rz = (ctx->xmode == EI86_686) ? 64 : 32;
	breg_t rb = _reg_base_frsize(rz);
	brm_opd opr = { ERMO_REG };
	opr.r = (breg_t)(rb + (for_idt ? 3 : 2));
	bstream_putc(ctx->stm, 0x0F);
	ASSERT(!ctx->lock_prefix);
	ctx->lock_prefix = 1;
	int ret = bctx_modrm(ctx, &rmo, &opr, 0x01, 0x01);
	ctx->lock_prefix = 0;
	return ret;
}

int bctx_inst_ltr(bctx* ctx)
{
	brm_opd rmo;
	ASSERT(ctx);
	if ( bctx_parse_term(ctx, &rmo)
	  || (rmo.opr != ERMO_REG && rmo.opr != ERMO_MEM) ) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	size_t rz;
	if (rmo.opr == ERMO_REG) {
		rz = _reg_size(rmo.r);
	} else {
		rz = 16;
		if (_has_ptr_size(rmo.m.ptrs))
			rz = _ptr_mode_size(rmo.m.ptrs);
	}
	if (rz != 16) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	brm_opd opr = { ERMO_REG };
	opr.r = (breg_t)(ERNM_AX + 3);
	bstream_putc(ctx->stm, 0x0F);
	ctx->lock_prefix = 1;
	int ret = bctx_modrm(ctx, &rmo, &opr, 0x00, 0x00);
	ctx->lock_prefix = 0;
	return ret;
}

int bctx_inst_enter(bctx* ctx)
{
	bexpr_terms expr;
	const brm_opd* rmo1 = &expr.rmo1;
	const brm_opd* rmo2 = &expr.rmo2;
	ASSERT(ctx);
	if (bctx_parse_expr(ctx, &expr)) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	if (rmo1->opr != ERMO_IMM || rmo2->opr != ERMO_IMM) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	bstream_putc(ctx->stm, 0xC8);
	bstream_putw(ctx->stm, (uint16_t)rmo1->m.ofs);
	bstream_putc(ctx->stm, (uint8_t)rmo2->m.ofs);
	return 0;
}
