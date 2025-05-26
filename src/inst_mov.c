
/* 2025.05.25 by renwang */

#include "bctx.h"

static inline int _reg_cr(bctx* ctx, int in_cmmreg, brm_opd* rmo, uint8_t crreg)
{
	bstream* stm = ctx->stm;
	breg_t r = rmo->r;
	size_t rz = _reg_size(r);
	breg_t rb = _reg_base_frsize(rz);
	ASSERT(ctx && stm && rmo && rmo->opr == ERMO_REG && crreg <= 8);
	if ( ctx->xmode <= EI86_386 && rz != 32
	  || (ctx->xmode == EI86_686 && rz != 64) ) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	bstream_putc(stm, 0x0F);
	bstream_putc(stm, in_cmmreg ? 0x20 : 0x22);
	uint8_t modrm = 0xC0 | (crreg << 3) | (r - rb);
	bstream_putc(stm, modrm);
	return 0;
}

static int _mov_ax_ofs(bctx* ctx, breg_t r, int32_t ofs);

/* mov rm/r,r */
static int _mov_rmr(bctx* ctx, int inv_cmd, brm_opd* rm, brm_opd* r)
{
	ASSERT(ctx && rm && r);
	if ( rm->opr == ERMO_UNDEF
	  || (r->opr != ERMO_REG && r->opr != ERMO_SEG)) {
		ASSERT(FALSE);
		return -1;
	}
	size_t rz = (r->opr == ERMO_SEG) ? 16 : _reg_size(r->r);
	if (rz == 0) {
		ASSERT(FALSE);
		return -1;
	} else if (rz == 64 && ctx->xmode < EI86_686) {
		bctx_print_err(ctx, "register '%s' is invalid at first operand", _reg_name(r->r));
		return -1;
	}

	/* mov rm/r8, r8; 88h
	 * mov rm/r16, r16; 89h
	 */
	uint8_t cmd8 = inv_cmd ? 0x8A : 0x88;
	uint8_t cmd32 = inv_cmd ? 0x8B : 0x89;
	if (r->opr == ERMO_SEG) {
		if (rz != 16 && rz != 64) {
			bctx_comm_err(ctx, NULL);
			return -1;
		}
		cmd32 = cmd8 = (inv_cmd ? 0x8C : 0x8E);
		r->opr = ERMO_REG;
		r->r = (breg_t)(r->m.seg - ESNM_ES + ERNM_AX);
		ASSERT(!ctx->lock_prefix);
		if (ctx->xmode == EI86_386)
			ctx->lock_prefix = 1;
		int ret = bctx_modrm(ctx, rm, r, cmd8, cmd32);
		ASSERT(!ret);
		ctx->lock_prefix = 0;
		return ret;
	} else if (r->opr == ERMO_REG
		&& rm->opr == ERMO_MEM && rm->m.reg1 == ERNM_UNDEF && rm->m.reg2 == ERNM_UNDEF
		&& (r->r == ERNM_AL || r->r == ERNM_AX || r->r == ERNM_EAX || r->r == ERNM_RAX)) {
		ASSERT(ctx->spec_ax_fn == NULL);
		ctx->spec_ax_fn = _mov_ax_ofs;
		ctx->spec_ax_ofs = rm->m.ofs;
		if (r->r == ERNM_RAX && (rm->m.ofs >= INT32_MIN || rm->m.ofs <= INT32_MAX)) {
			// Nothing
		} else {
			cmd8 = inv_cmd ? 0xA0 : 0xA2;
			cmd32 = inv_cmd ? 0xA1 : 0xA3;
			if (r->r == ERNM_RAX)
				rm->m.ofs = 0;
		}
		int ret = bctx_modrm(ctx, rm, r, cmd8, cmd32);
		ctx->spec_ax_fn = NULL;
		ctx->spec_ax_ofs = 0;
		return ret;
	}
	return bctx_modrm(ctx, rm, r, cmd8, cmd32);
}

/* mov r/m,imm */
static int _rm_imm(bctx* ctx, brm_opd* rm, int64_t imm)
{
	ASSERT(ctx && rm);
	if (rm->opr == ERMO_REG) {
		size_t rz = _reg_size(rm->r);
		breg_t rb = _reg_base_frsize(rz);
		uint8_t cr = rm->r - rb;
		if (rz == 8) {
			/* mov r8,imm8 */
			bstream_putc(ctx->stm, 0xB0 | cr);
			bstream_putc(ctx->stm, (uint8_t)imm);
			return 0;
		} else if (rz == 16) {
			if (ctx->xmode >= EI86_386)
				bstream_putc(ctx->stm, 0x66);
			bstream_putc(ctx->stm, 0xB8 | cr);
			bstream_putw(ctx->stm, (uint16_t)imm);
			return 0;
		} else if (rz == 32) {
			if (ctx->xmode == EI86_X86)
				bstream_putc(ctx->stm, 0x66);
			bstream_putc(ctx->stm, 0xB8 | cr);
			bstream_putdw(ctx->stm, (uint32_t)imm);
			return 0;
		}
		ASSERT(rz == 64);
		if (ctx->xmode != EI86_686) {
			bctx_print_err(ctx, "invalid instruction not in 64-bit mode");
			return -1;
		}
		bstream_putc(ctx->stm, 0x48);
		bstream_putc(ctx->stm, 0xB8 | cr);
		bstream_putqw(ctx->stm, imm);
		return 0;
	} else if (rm->opr == ERMO_MEM) {
		size_t rz = _try_rm_size(rm);
		if (rz == 0) {
			bctx_print_err(ctx, "operation size not specified");
			return -1;
		} else if (rz == 64 && ctx->xmode < EI86_686) {
			bctx_print_err(ctx, "instruction not supported in not 64-bit mode");
			return -1;
		}
		brm_opd r;
		r.opr = ERMO_REG;
		r.r = _reg_base_frsize(rz);
		if (bctx_modrm(ctx, rm, &r, 0xC6, 0xC7)) {
			ASSERT(FALSE);
			bctx_print_err(ctx, "invalid instruction");
			return -1;
		}
		if (rz == 8) {
			bstream_putc(ctx->stm, (uint8_t)imm);
		} else if (rz == 16) {
			bstream_putw(ctx->stm, (uint16_t)imm);
		} else if (rz == 32) {
			bstream_putc(ctx->stm, (uint32_t)imm);
		} else {
			ASSERT(rz == 64);
			if (bctx_has_stab_pass(ctx) && (imm < INT32_MIN || imm > UINT32_MAX)) {
				bctx_print_err(ctx, "immediate out of range");
				return -1;
			}
			bstream_putdw(ctx->stm, (uint32_t)imm);
		}
		return 0;
	}
	ASSERT(FALSE);
	bctx_print_err(ctx, "invalid instruction");
	return -1;
}

int bctx_inst_mov(bctx* ctx)
{
	bexpr_terms expr;
	brm_opd* rmo1 = &expr.rmo1;
	brm_opd* rmo2 = &expr.rmo2;
	ASSERT(ctx);
	if (bctx_parse_expr(ctx, &expr)) {
		ASSERT(FALSE);
		return -1;
	}
	if ( rmo1->opr == ERMO_UNDEF
	  || rmo2->opr == ERMO_UNDEF ) {
		goto _INV_CMD;
	}
	if (rmo1->opr == ERMO_REG) {
		if (rmo2->opr == ERMO_SEG)
			return _mov_rmr(ctx, 1, rmo1, rmo2);
		else if (rmo2->opr == ERMO_IMM)
			return _rm_imm(ctx, rmo1, rmo2->m.ofs);
		else if (rmo2->opr == ERMO_CR)
			return _reg_cr(ctx, 1, rmo1, (uint8_t)rmo2->m.ofs);
		return _mov_rmr(ctx, 1, rmo2, rmo1);
	} else if (rmo1->opr == ERMO_SEG) {
		if (rmo2->opr == ERMO_SEG)
			goto _INV_CMD;
		return _mov_rmr(ctx, 0, rmo2, rmo1);
	} else if (rmo2->opr == ERMO_REG) {
		if (rmo1->opr == ERMO_CR)
			return _reg_cr(ctx, 0, rmo2, (uint8_t)rmo1->m.ofs);
		ASSERT(rmo1->opr == ERMO_MEM);
		return _mov_rmr(ctx, 0, rmo1, rmo2);
	} else if (rmo2->opr == ERMO_SEG) {
		return _mov_rmr(ctx, 1, rmo1, rmo2);
	} else if (rmo2->opr == ERMO_IMM) {
		ASSERT(rmo1->opr == ERMO_MEM);
		return _rm_imm(ctx, rmo1, rmo2->m.ofs);
	}
_INV_CMD:
	bctx_print_err(ctx, "illegal instruction");
	return -1;
}

/* return: 0-continue;1-ok;-1-error */
static int _mov_ax_ofs(bctx* ctx, breg_t r, int32_t ofs)
{
	size_t rz = _plat_size(ctx->xmode);
	ASSERT(ctx && rz && ctx->spec_ax_fn);
	switch (rz) {
	case 16:
		if (ofs < INT16_MIN || ofs > INT16_MAX)
			return -1;
		bstream_putw(ctx->stm, (uint16_t)ofs);
		break;
	case 32:
		bstream_putdw(ctx->stm, (uint32_t)ofs);
		break;
	case 64:
		if (ctx->spec_ax_ofs >= INT32_MIN && ctx->spec_ax_ofs <= INT32_MAX) {
			uint8_t cr = (r - ERNM_RAX) << 3;
			return _386_mem_sib(ctx, cr, ERNM_UNDEF, 0, ERNM_UNDEF, (int32_t)ctx->spec_ax_ofs);
		}
		bstream_putqw(ctx->stm, ctx->spec_ax_ofs);
		break;
	default:
		ASSERT(FALSE);
		return -1;
	}
	return 1;
}

/* movsb/movsw/movsd/movsq */
int bctx_inst_movs(bctx* ctx, bptr_mode_t pm)
{
	ASSERT(ctx);
	if (ctx->rep_prefix) {
		if (ctx->rep_prefix != 1) {
			bctx_print_err(ctx, "only 'REP' is valid prefix in front of 'MOVSB'");
			return -1;
		}
		bstream_putc(ctx->stm, 0xF3);  /* rep */
		ctx->rep_prefix = 0;
	}
	switch (pm) {
	case EPTR_BYTE:
		bstream_putc(ctx->stm, 0xA4);
		return 0;
	case EPTR_WORD:
		if (ctx->xmode >= EI86_386)
			bstream_putc(ctx->stm, 0x66);
		bstream_putc(ctx->stm, 0xA5);
		return 0;
	case EPTR_DWORD:
		if (ctx->xmode == EI86_X86)
			bstream_putc(ctx->stm, 0x66);
		bstream_putc(ctx->stm, 0xA5);
		return 0;
	case EPTR_QWORD:
		if (ctx->xmode == EI86_X86) {
			bctx_comm_err(ctx, NULL);
			return -1;
		}
		bstream_putc(ctx->stm, 0x48);
		bstream_putc(ctx->stm, 0xA5);
		return 0;
	default:
		break;
	}
	ASSERT(FALSE);
	return -1;
}

/* stosb/stosw/stosd/stosq */
int bctx_inst_stos(bctx* ctx, bptr_mode_t pm)
{
	ASSERT(ctx);
	if (ctx->rep_prefix) {
		if (ctx->rep_prefix != 1) {
			bctx_print_err(ctx, "only 'REP' is valid prefix in front of 'STOSB'");
			return -1;
		}
		bstream_putc(ctx->stm, 0xF3);  /* REP */
		ctx->rep_prefix = 0;
	}
	switch (pm) {
	case EPTR_BYTE:
		bstream_putc(ctx->stm, 0xAA);
		return 0;
	case EPTR_WORD:
		if (ctx->xmode >= EI86_386)
			bstream_putc(ctx->stm, 0x66);
		bstream_putc(ctx->stm, 0xAB);
		return 0;
	case EPTR_DWORD:
		if (ctx->xmode == EI86_X86)
			bstream_putc(ctx->stm, 0x66);
		bstream_putc(ctx->stm, 0xAB);
		return 0;
	case EPTR_QWORD:
		if (ctx->xmode == EI86_X86) {
			bctx_comm_err(ctx, NULL);
			return -1;
		}
		bstream_putc(ctx->stm, 0x48);
		bstream_putc(ctx->stm, 0xAB);
		return 0;
	default:
		break;
	}
	ASSERT(FALSE);
	return 0;
}
