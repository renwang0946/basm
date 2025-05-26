
/* 2025.05.23 by renwang */

#include "bctx.h"

/* return: 0-ok; -1-error */
int bctx_inst_inc(bctx* ctx, int for_dec/* = false */)
{
	brm_opd rmo;
	if (bctx_parse_term(ctx, &rmo)) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	if (rmo.opr == ERMO_REG) {
		size_t rz = _reg_size(rmo.r);
		if ( (rz == 64 && ctx->xmode != EI86_686)
		  || (rz > 16 && ctx->xmode == EI86_X86) ) {
			goto _INV;
		}
		brm_opd opd = { ERMO_REG };
		opd.r = _reg_base_frsize(rz);
		if (rz == 8 || rz == 64 || ctx->xmode == EI86_686) {
			if (for_dec)
				opd.r = (breg_t)(opd.r + 1);
			return bctx_modrm(ctx, &rmo, &opd, 0xFE, 0xFF);
		}
		if (rz == 16) {
			if (ctx->xmode == EI86_386)
				bstream_putc(ctx->stm, 0x66);
			bstream_putc(ctx->stm, (for_dec ? 0x48 : 0x40) + rmo.r - opd.r);
			return 0;
		} else if (rz == 32) {
			if (ctx->xmode == EI86_X86)
				bstream_putc(ctx->stm, 0x66);
			bstream_putc(ctx->stm, (for_dec ? 0x48 : 0x40) + rmo.r - opd.r);
			return 0;
		}
		goto _INV;
	} else if (rmo.opr == ERMO_MEM) {
		size_t rz = _try_rm_size(&rmo);
		if (rz == 0) {
			bctx_print_err(ctx, "operation size not specified");
			return -1;
		}
		brm_opd opd = { ERMO_REG };
		opd.r = (breg_t)(_reg_base_frsize(rz) + (for_dec ? 1 : 0));
		return bctx_modrm(ctx, &rmo, &opd, 0xFE, 0xFF);
	}
_INV:
	bctx_comm_err(ctx, NULL);
	return -1;
}

typedef struct _TAddSubCmd
{
	uint8_t u8AL;     /* ADD AL,imm8, +1:ADD EAX, imm32 */
	uint8_t um8r8;    /* ADD r/m8, r8  */
} _cmd_add_sub;
static const _cmd_add_sub _add_sub_cmds[] = {
	{ 0x04, 0x00 },   /* ADD AL,imm8  */
	{ 0x0C, 0x08 },   /* OR AL,imm8   */
	{ 0x14, 0x10 },   /* ADC AL,imm8  */
	{ 0x1C, 0x18 },   /* SBB AL,imm8  */
	{ 0x24, 0x20 },   /* AND AL,imm8  */
	{ 0x2C, 0x28 },   /* SUB AL,imm8  */
	{ 0x34, 0x30 },   /* XOR AL,imm8  */
	{ 0x3C, 0x38 },   /* CMP AL,imm8  */
};

/* return: 0-ok; -1-error */
static inline int _opr2_rm_cr(bctx* ctx, const brm_opd* rmo, uint8_t cr, uint8_t op8, uint8_t op32)
{
	size_t rz = _try_rm_size(rmo);
	ASSERT(ctx && rmo);
	if (rz == 0) {
		bctx_print_err(ctx, "operation size not specified");
		return -1;
	}
	brm_opd opd = { ERMO_REG };
	opd.r = (breg_t)(_reg_base_frsize(rz) + cr);
	return bctx_modrm(ctx, rmo, &opd, op8, op32);
}

/* return: 0-ok; -1-error */
static inline int _opr2_rm_cr_imm8(bctx* ctx, const brm_opd* rmo, uint8_t cr,
	int64_t imm8, uint8_t op8, uint8_t op32)
{
	if (imm8 < INT8_MIN || imm8 > INT8_MAX) {
		bctx_print_err(ctx, "operand immediate out of range");
		return -1;
	}
	if (_opr2_rm_cr(ctx, rmo, cr, 0xC0, 0xC1))
		return -1;
	bstream_putc(ctx->stm, (uint8_t)imm8);
	return 0;
}

/* sal or shl */
int bctx_inst_sal(bctx* ctx, uint8_t cr /* = 4 */)
{
	bexpr_terms expr;
	brm_opd* rmo1 = &expr.rmo1;
	brm_opd* rmo2 = &expr.rmo2;
	ASSERT(ctx);
	if (bctx_parse_expr(ctx, &expr)) {
		ASSERT(FALSE);
		return -1;
	}
	if ( (rmo1->opr != ERMO_MEM && rmo1->opr != ERMO_REG)
	  || (rmo2->opr != ERMO_IMM && rmo2->opr != ERMO_REG) ) {
		goto _INV_CMD;
	}
	if (rmo2->opr == ERMO_SEG) {
		if (rmo2->r != ERNM_CL) {
			bctx_print_err(ctx, "only register CL is allowed");
			return -1;
		}
		return _opr2_rm_cr(ctx, rmo1, cr, 0xD2, 0xD3);
	}
	ASSERT(rmo2->opr == ERMO_IMM);
	if (rmo2->m.ofs == 1)
		return _opr2_rm_cr(ctx, rmo1, cr, 0xD0, 0xD1);
	return _opr2_rm_cr_imm8(ctx, rmo1, cr, rmo2->m.ofs, 0xC0, 0xC1);
_INV_CMD:
	bctx_comm_err(ctx, NULL);
	return -1;
}

static inline int _opr2_rm_r(bctx* ctx, const brm_opd* rmo, breg_t r, uint8_t op8, uint8_t op32)
{
	size_t rz = _try_rm_size(rmo);
	ASSERT(ctx && rmo);
	if (rz == 0) {
		bctx_print_err(ctx, "operation size not specified");
		return -1;
	}
	brm_opd opd = { ERMO_REG };
	opd.r = r;
	return bctx_modrm(ctx, rmo, &opd, op8, op32);
}

static inline int _append_imm(bctx* ctx, size_t rz, int64_t imm)
{
	bstream* stm = ctx->stm;
	ASSERT(ctx && stm);
	if (rz == 8) {
		if (imm < INT8_MIN || imm > UINT8_MAX) {
			bctx_print_err(ctx, "operand immediate out of range");
			return -1;
		}
		bstream_putc(stm, (uint8_t)imm);
		return 0;
	} else if (rz == 16) {
		if (imm < INT16_MIN || imm > UINT16_MAX) {
			bctx_print_err(ctx, "operand immediate out of range");
			return -1;
		}
		bstream_putw(stm, (uint16_t)imm);
		return 0;
	} else if (rz == 32 || rz == 64) {
		if (imm < INT32_MIN || imm > UINT32_MAX) {
			bctx_print_err(ctx, "operand immediate out of range");
			return -1;
		}
		bstream_putdw(stm, (uint32_t)imm);
		return 0;
	}
	ASSERT(FALSE);
	bctx_comm_err(ctx, NULL);
	return -1;
}

/* opt: 0-add;1-or;2-adc;3-sbb;4-and;5-sub;6-xor;7-cmp
 * return: 0-ok; -1-error */
int bctx_inst_add(bctx* ctx, uint8_t opt /* = 0 */)
{
	bexpr_terms expr;
	brm_opd* rmo1 = &expr.rmo1;
	brm_opd* rmo2 = &expr.rmo2;
	bstream* stm = ctx->stm;
	ASSERT(ctx && stm);
	if (bctx_parse_expr(ctx, &expr)) {
		ASSERT(FALSE);
		return -1;
	}
	if ( (rmo1->opr != ERMO_MEM && rmo1->opr != ERMO_REG)
	  || rmo2->opr == ERMO_UNDEF ) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	ASSERT(opt >= 0 && opt < _countof(_add_sub_cmds));
	const _cmd_add_sub* asc = &_add_sub_cmds[opt];
	if ( rmo2->opr == ERMO_IMM && 
	     (rmo1->opr == ERMO_REG || rmo1->opr == ERMO_MEM) ) {
		int is_imm8 = rmo2->m.ofs >= INT8_MIN && rmo2->m.ofs <= INT8_MAX;
		if (rmo1->opr == ERMO_REG) {
			size_t rz = _reg_size(rmo1->r);
			breg_t rb = _reg_base_frsize(rz);
			if (rmo1->r == rb && !is_imm8) {
				if (rz == 8) {
					bstream_putc(stm, asc->u8AL);
					return _append_imm(ctx, rz, rmo2->m.ofs);
				} if (rz == 16) {
					if (ctx->xmode >= EI86_386)
						bstream_putc(stm, 0x66);
					bstream_putc(stm, asc->u8AL + 1);
					return _append_imm(ctx, rz, rmo2->m.ofs);
				} else if (rz == 32) {
					if (ctx->xmode == EI86_X86)
						bstream_putc(stm, 0x66);
					bstream_putc(stm, asc->u8AL + 1);
					return _append_imm(ctx, rz, rmo2->m.ofs);
				} else if (rz == 64) {
					if (ctx->xmode != EI86_686) {
						bctx_comm_err(ctx, NULL);
						return -1;
					}
					bstream_putc(stm, 0x48);
					bstream_putc(stm, asc->u8AL + 1);
					return _append_imm(ctx, rz, rmo2->m.ofs);
				}
			} else if (is_imm8) {
				if (rz == 8) {
					bstream_putc(stm, asc->u8AL);
					return _append_imm(ctx, rz, rmo2->m.ofs);
				}
				brm_opd opr = { ERMO_REG };
				opr.r = (breg_t)(_reg_base_frsize(rz) + opt);
				if (bctx_modrm(ctx, rmo1, &opr, 0x80, 0x83))
					return -1;
				return _append_imm(ctx, 8, rmo2->m.ofs);
			}
		}
		size_t rz = _plat_size(ctx->xmode);
		if (_has_ptr_size(rmo1->m.ptrs))
			rz = _ptr_mode_size(rmo1->m.ptrs);
		brm_opd opr = { ERMO_REG };
		opr.r = (breg_t)(_reg_base_frsize(rz) + opt);
		if (is_imm8) {
			ASSERT(rmo1->opr == ERMO_MEM);
			if (bctx_modrm(ctx, rmo1, &opr, 0x80, 0x83))
				return -1;
			return _append_imm(ctx, rz, rmo2->m.ofs);
		}
		if (bctx_modrm(ctx, rmo1, &opr, 0x80, 0x81))
			return -1;
		return _append_imm(ctx, rz, rmo2->m.ofs);
	}
	if (rmo2->opr == ERMO_REG) {
		if (rmo1->opr != ERMO_MEM && rmo1->opr != ERMO_REG) {
			bctx_comm_err(ctx, NULL);
			return -1;
		}
		return bctx_modrm(ctx, rmo1, rmo2, asc->um8r8, asc->um8r8 + 1);
	} else if (rmo1->opr == ERMO_REG) {
		if (rmo2->opr != ERMO_MEM)
			goto _INV_CMD;
		return bctx_modrm(ctx, rmo2, rmo1, asc->um8r8 + 2, asc->um8r8 + 3);
	}
_INV_CMD:
	bctx_comm_err(ctx, NULL);
	return -1;
}

/* return: 0-ok; -1-error */
int bctx_inst_lea(bctx* ctx)
{
	bexpr_terms expr;
	brm_opd* rmo1 = &expr.rmo1;
	brm_opd* rmo2 = &expr.rmo2;
	ASSERT(ctx);
	if (bctx_parse_expr(ctx, &expr)) {
		ASSERT(FALSE);
		return -1;
	}
	if (rmo1->opr != ERMO_REG || rmo2->opr != ERMO_MEM) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	size_t rz = _reg_size(rmo1->r);
	if (rz == 8) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	return bctx_modrm(ctx, rmo2, rmo1, 0x8E, 0x8D);
}

/* return: 0-ok; -1-error */
int bctx_inst_xchg(bctx* ctx)
{
	bexpr_terms expr;
	brm_opd* pm1 = &expr.rmo1;
	brm_opd* pm2 = &expr.rmo2;
	bstream* stm = ctx->stm;
	ASSERT(ctx && stm);
	if (bctx_parse_expr(ctx, &expr)) {
		ASSERT(FALSE);
		return -1;
	}
	if (pm1->opr != ERMO_REG && pm2->opr == ERMO_REG) {
		pm1 = &expr.rmo2;
		pm2 = &expr.rmo1;
	}
	if ( pm1->opr != ERMO_REG
	 || (pm2->opr != ERMO_REG && pm2->opr != ERMO_MEM) ) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	size_t rz = _reg_size(pm1->r);
	breg_t rb = _reg_base_frsize(rz);
	if (pm1->r == rb && pm2->opr == ERMO_REG) {
		if (rz != _reg_size(pm2->r)) {
			ASSERT(FALSE);
			bctx_comm_err(ctx, NULL);
			return -1;
		}
		if (rz == 16) {
			if (ctx->xmode >= EI86_386)
				bstream_putc(stm, 0x66);
			bstream_putc(stm, 0x90);
			bstream_putc(stm, pm2->r - rb);
			return 0;
		} else if (rz == 32) {
			if (ctx->xmode == EI86_X86)
				bstream_putc(stm, 0x66);
			bstream_putc(stm, 0x90);
			bstream_putc(stm, pm2->r - rb);
			return 0;
		} else if (rz == 64) {
			if (ctx->xmode != EI86_686) {
				bctx_comm_err(ctx, NULL);
				return -1;
			}
			bstream_putc(stm, 0x48);
			bstream_putc(stm, 0x90);
			bstream_putc(stm, pm2->r - rb);
			return 0;
		}
	}
	return bctx_modrm(ctx, pm2, pm1, 0x86, 0x87);
}

/* return: 0-ok; -1-error */
int bctx_inst_mul(bctx* ctx)
{
	brm_opd rmo;
	if ( bctx_parse_term(ctx, &rmo)
	  || (rmo.opr != ERMO_REG && rmo.opr != ERMO_MEM) ) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	size_t rz = _try_rm_size(&rmo);
	if (rz == 0) {
		bctx_print_err(ctx, "operation size not specified");
		return -1;
	}
	breg_t rb = _reg_base_frsize(rz);
	brm_opd opr = { ERMO_REG };
	opr.r = (breg_t)(rb + 4);
	return bctx_modrm(ctx, &rmo, &opr, 0xF6, 0xF7);
}
