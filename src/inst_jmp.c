
/* 2025.05.24 by renwang */

#include "bctx.h"

/* return: 0-ok; -1:error */
int bctx_inst_call(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	brm_opd rmo;
	ASSERT(ctx && ctx->tk);
	if (bctx_parse_term(ctx, &rmo)) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	if (ctx->token_kept)
		ctx->token_kept = 0;
	else if (blexer_next(ctx->lex, tk) <= 0)
		goto _INV;
	if (tk->type != ETKT_COLON) {
		ctx->token_kept = 1;
	} else if (rmo.opr != ERMO_IMM) {
		goto _INV;
	} else {
		/* far call with segment */
		int16_t segp = (int16_t)rmo.m.ofs;
		blex_token_clear(tk);
		if ( bctx_parse_term(ctx, &rmo)
		  || rmo.opr != ERMO_IMM ) {
			goto _INV;
		}
		if (ctx->xmode == EI86_686) {
			ASSERT(FALSE);
			bctx_print_err(ctx, "invalid segment offset in 64-bit mode");
			return -1;
		}

		if (ctx->xmode == EI86_X86) {
			int64_t n = rmo.m.ofs;
			if (bctx_has_stab_pass(ctx) && (n < INT16_MIN || n > UINT16_MAX)) {
				bctx_comm_err(ctx, "call offset out of range '%s'");
				return -1;
			}
			bstream_putc(ctx->stm, 0x9A);
			bstream_putw(ctx->stm, (uint16_t)n);
			bstream_putw(ctx->stm, segp);
			return 0;
		}
		int64_t n = rmo.m.ofs;
		if (bctx_has_stab_pass(ctx) && (n < INT32_MIN || n > UINT32_MAX)) {
			bctx_comm_err(ctx, "call offset out of range '%s'");
			return -1;
		}
		bstream_putc(ctx->stm, 0x9A);
		bstream_putdw(ctx->stm, (uint32_t)n);
		bstream_putw(ctx->stm, segp);
		return 0;
	}

	if (rmo.opr == ERMO_IMM) {
		if (ctx->xmode == EI86_X86) {
			int64_t n = bctx_rel_ofs(ctx, rmo.m.ofs, 3);
			if (bctx_has_stab_pass(ctx) && (n < INT16_MIN || n > INT16_MAX)) {
				bctx_comm_err(ctx, "'%s' out of range");
				return -1;
			}
			bstream_putc(ctx->stm, 0xE8);
			bstream_putw(ctx->stm, (uint16_t)n);
			return 0;
		}
		int64_t n = bctx_rel_ofs(ctx, rmo.m.ofs, 5);
		if (bctx_has_stab_pass(ctx) && (n < INT32_MIN || n > INT32_MAX)) {
			bctx_comm_err(ctx, "'%s' out of range");
			return -1;
		}
		bstream_putc(ctx->stm, 0xE8);
		bstream_putdw(ctx->stm, (uint32_t)n);
		return 0;
	} else if (rmo.opr == ERMO_REG) {
		brm_opd opr = { ERMO_REG };
		size_t rz = _reg_size(rmo.r);
		size_t pz = _plat_size(ctx->xmode);
		if (pz != rz) {
			bctx_print_err(ctx, "instruction not supported in %d-bit mode", pz);
			return -1;
		}
		opr.r = (breg_t)(_reg_base_frsize(rz) + 2);
		return bctx_modrm(ctx, &rmo, &opr, 0xFF, 0xFF);
	} else if (rmo.opr == ERMO_MEM) {
		brm_opd opr = { ERMO_REG };
		size_t pz = _plat_size(ctx->xmode);
		if (_has_ptr_mode(rmo.m.ptrs, EPTR_FAR)) {
			uint8_t cmd8 = 0xFF;
			if (ctx->xmode == EI86_686) {
				if (_has_ptr_mode(rmo.m.ptrs, EPTR_FWORD)) {
					bctx_print_err(ctx, "'fword' is invalid in 64-bit mode");
					return -1;
				} else if (_has_ptr_mode(rmo.m.ptrs, EPTR_QWORD)) {
					cmd8 = 0xFE;  /* different REX.W */
				}
			}
			opr.r = (breg_t)(_reg_base_frsize(pz) + 3);
			return bctx_modrm(ctx, &rmo, &opr, cmd8, 0xFF);
		}
		opr.r = (breg_t)(_reg_base_frsize(pz) + 2);
		return bctx_modrm(ctx, &rmo, &opr, 0xFF, 0xFF);
	}
_INV:
	bctx_comm_err(ctx, NULL);
	return -1;
}

int bctx_inst_ret(bctx* ctx, int for_far/* = false */)
{
	int ret;
	blex_token* tk = ctx->tk;
	ASSERT(ctx && tk);
	ret = blexer_next(ctx->lex, ctx->tk);
	ctx->token_kept = 1;
	if (ret < 0) {
		ASSERT(FALSE);
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	if (tk->type != ETKT_LNEND && tk->type != ETKT_END) {
		brm_opd rmo;
		rmo.opr = ERMO_IMM;
		if (bctx_parse_term(ctx, &rmo) || rmo.opr != ERMO_IMM) {
			ASSERT(FALSE);
			bctx_comm_err(ctx, NULL);
			return -1;
		}
		if (_has_ptr_mode(rmo.m.ptrs, EPTR_FAR))
			for_far = 1;
		if (rmo.m.ofs < 0 || rmo.m.ofs > INT16_MAX) {
			ASSERT(FALSE);
			bctx_comm_err(ctx, "invalid immediate:%s");
			return -1;
		}
		bstream_putc(ctx->stm, for_far ? 0xCA : 0xC2);
		bstream_putw(ctx->stm, (uint16_t)rmo.m.ofs);
		return 0;
	}
	bstream_putc(ctx->stm, for_far ? 0xCB : 0xC3);
	return 0;
}

int bctx_inst_int(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	ASSERT(ctx && tk);
	if (blexer_next(ctx->lex, tk) <= 0 || tk->type != ETKT_NUM) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	uint8_t u;
	bnum_val nv;
	if ( bsm_parse_num(tk->key, tk->kend, &nv)
	  || nv.type != ENVT_INT ) {
		ASSERT(FALSE);
		goto _INV;
	}
	if (nv.i < 0 || nv.i > 255) {
		ASSERT(FALSE);
		goto _INV;
	}
	u = nv.i;
	if (u == 0) {  /* INTO */
		bstream_putc(ctx->stm, 0xCE);
	} else if (u == 3) {  /* INT 3 */
		bstream_putc(ctx->stm, 0xCC);
	} else {
		bstream_putc(ctx->stm, 0xCD);
		bstream_putc(ctx->stm, u);
	}
	return 0;
_INV:
	bctx_comm_err(ctx, NULL);
	return -1;
}

int bctx_inst_jmp(bctx* ctx)
{
	int64_t n;
	blex_token* tk = ctx->tk;
	brm_opd rmo;
	ASSERT(ctx && tk);
	if (bctx_parse_term(ctx, &rmo))
		goto _INV;
	if (ctx->token_kept)
		ctx->token_kept = 0;
	else if (blexer_next(ctx->lex, tk) < 0)
		goto _INV;
	if (tk->type != ETKT_COLON) {
		ctx->token_kept = 1;
	} else if (rmo.opr != ERMO_IMM) {
		goto _INV;
	} else {
		/* far jump with segment */
		int16_t segp = (int16_t)rmo.m.ofs;
		memset(&rmo, 0, sizeof(rmo));
		if (bctx_parse_term(ctx, &rmo) || rmo.opr != ERMO_IMM)
			goto _INV;
		if (ctx->xmode == EI86_686) {
			ASSERT(FALSE);
			bctx_print_err(ctx, "invalid segment offset in 64-bit mode");
			return -1;
		} else if (ctx->xmode == EI86_X86) {
			n = rmo.m.ofs;
			if (bctx_has_stab_pass(ctx) && (n < INT16_MIN || n > INT16_MAX)) {
				bctx_comm_err(ctx, "jmp offset out of range '%s'");
				return -1;
			}
			bstream_putc(ctx->stm, 0xEA);
			bstream_putw(ctx->stm, (uint16_t)n);
			bstream_putw(ctx->stm, (uint16_t)segp);
			return 0;
		}
		n = rmo.m.ofs;
		if (bctx_has_stab_pass(ctx) && (n < INT32_MIN || n > INT32_MAX)) {
			bctx_comm_err(ctx, "jmp offset out of range '%s'");
			return -1;
		}
		bstream_putc(ctx->stm, 0xEA);
		bstream_putdw(ctx->stm, (uint32_t)n);
		bstream_putw(ctx->stm, (uint16_t)segp);
		return 0;
	}

	if (rmo.opr == ERMO_IMM) {
		if ((rmo.m.ptrs & EPTR_REL_MASK) != 0) {
			if (_has_ptr_mode(rmo.m.ptrs, EPTR_SHORT)) {
			_SHORT:
				n = bctx_rel_ofs(ctx, rmo.m.ofs, 2);
				if (bctx_has_stab_pass(ctx) && (n < INT8_MIN || n > INT8_MAX)) {
					bctx_comm_err(ctx, "jmp offset out of range '%s'");
					return -1;
				}
				bstream_putc(ctx->stm, 0xEB);
				bstream_putc(ctx->stm, (uint8_t)n);
				return 0;
			} else if (_has_ptr_mode(rmo.m.ptrs, EPTR_NEAR)) {
			_NEAR:
				if (ctx->xmode == EI86_X86) {
					n = bctx_rel_ofs(ctx, rmo.m.ofs, 3);
					if (bctx_has_stab_pass(ctx) && (n < INT16_MIN || n > INT16_MAX)) {
						bctx_comm_err(ctx, "jmp offset out of range '%s'");
						return -1;
					}
					bstream_putc(ctx->stm, 0xE9);
					bstream_putw(ctx->stm, (uint16_t)n);
					return 0;
				}
				n = bctx_rel_ofs(ctx, rmo.m.ofs, 5);
				if (bctx_has_stab_pass(ctx) && (n < INT32_MIN || n > INT32_MAX)) {
					bctx_comm_err(ctx, "jmp offset out of range '%s'");
					return -1;
				}
				bstream_putc(ctx->stm, 0xE9);
				bstream_putdw(ctx->stm, (uint32_t)n);
				return 0;
			}
		} else {
			n = bctx_rel_ofs(ctx, rmo.m.ofs, 2);
			if (n >= INT8_MIN && n <= INT8_MAX)
				goto _SHORT;
			goto _NEAR;
		}
	} else if (rmo.opr == ERMO_REG) {
		size_t rz = _reg_size(rmo.r);
		size_t pz = _plat_size(ctx->xmode);
		if (pz != rz) {
			bctx_print_err(ctx, "instruction not supported in %d-bit mode", pz);
			return -1;
		}
		brm_opd opr = { ERMO_REG };
		opr.r = (breg_t)(_reg_base_frsize(rz) + 4);
		return bctx_modrm(ctx, &rmo, &opr, 0xFF, 0xFF);
	} else if (rmo.opr == ERMO_MEM) {
		brm_opd opr = { ERMO_REG };
		size_t pz = _plat_size(ctx->xmode);
		if (_has_ptr_mode(rmo.m.ptrs, EPTR_FAR)) {
			uint8_t cmd8 = 0xFF;
			if (ctx->xmode == EI86_686) {
				if (_has_ptr_mode(rmo.m.ptrs, EPTR_FWORD)) {
					bctx_print_err(ctx, "'fword' is invalid in 64-bit mode");
					return -1;
				} else if (_has_ptr_mode(rmo.m.ptrs, EPTR_QWORD)) {
					cmd8 = 0xFE;  /* different REX.W */
				}
			}
			opr.r = (breg_t)(_reg_base_frsize(pz) + 5);
			return bctx_modrm(ctx, &rmo, &opr, cmd8, 0xFF);
		}
		opr.r = (breg_t)(_reg_base_frsize(pz) + 4);
		return bctx_modrm(ctx, &rmo, &opr, 0xFF, 0xFF);
	}
_INV:
	bctx_comm_err(ctx, NULL);
	return -1;
}

int bctx_inst_jcc(bctx* ctx, const bjcc_cond_elem_t* jcc)
{
	brm_opd rmo;
	int64_t n;
	ASSERT(ctx && jcc);
	if (bctx_parse_term(ctx, &rmo) || rmo.opr != ERMO_IMM) {
		bctx_comm_err(ctx, NULL);
		return -1;
	} else if (_has_ptr_mode(rmo.m.ptrs, EPTR_FAR)) {
		bctx_print_err(ctx, "invalid use of FAR operand specifier");
		return -1;
	} else if (_has_ptr_size(rmo.m.ptrs)) {
		bctx_print_err(ctx, "invalid operand specifier");
		return -1;
	}
	if (jcc->opt) {
		if (jcc->opt <= 3) {
			if ( (rmo.m.ofs & EPTR_REL_MASK) != 0
			  && !_has_ptr_mode(rmo.m.ptrs, EPTR_SHORT) ) {
				ASSERT(FALSE);
				bctx_print_err(ctx, "PTR mode is valid");
				return -1;
			}
			if (jcc->opt == 1) {
				/* JCXZ */
				if (ctx->xmode == EI86_686) {
					bctx_print_err(ctx, "illegal instruction in 64-bit mode");
					return -1;
				} else if (ctx->xmode == EI86_386) {
					bstream_putc(ctx->stm, 0x67);
				}
			} else if (jcc->opt == 2) {
				/* JECXZ */
				if (ctx->xmode == EI86_X86) {
					bctx_print_err(ctx, "illegal instruction in 16-bit mode");
					return -1;
				} else if (ctx->xmode == EI86_686) {
					bstream_putc(ctx->stm, 0x67);
				}
			} else if (jcc->opt == 3 && ctx->xmode != EI86_686) {
				/* JRCXZ */
				bctx_print_err(ctx, "only valid instruction in 64-bit mode");
				return -1;
			}
			if (bctx_has_stab_pass(ctx) && (rmo.m.ofs < INT8_MIN || rmo.m.ofs > INT8_MAX)) {
				ASSERT(FALSE);
				bctx_print_err(ctx, "operand offset out of range");
				return -1;
			}
			bstream_putc(ctx->stm, jcc->op8);
			bstream_putc(ctx->stm, (uint8_t)rmo.m.ofs);
			return 0;
		}
		ASSERT(FALSE);
	}
	if (_has_ptr_mode(rmo.m.ptrs, EPTR_SHORT)) {
	_SHORT:
		n = bctx_rel_ofs(ctx, rmo.m.ofs, 2);
		if (bctx_has_stab_pass(ctx) && (n < INT8_MIN || n > INT8_MAX)) {
			bctx_print_err(ctx, "operand offset out of range");
			return -1;
		}
		bstream_putc(ctx->stm, jcc->op8);
		bstream_putc(ctx->stm, (uint8_t)n);
		return 0;
	}
	n = bctx_rel_ofs(ctx, rmo.m.ofs, 2);
	if (n >= INT8_MIN && n <= INT8_MAX)
		goto _SHORT;
	if (ctx->xmode == EI86_X86) {
		n = bctx_rel_ofs(ctx, rmo.m.ofs, 4);
		if (bctx_has_stab_pass(ctx) && (n < INT16_MIN || n > INT16_MAX)) {
			bctx_print_err(ctx, "operand offset out of range");
			return -1;
		}
		bstream_putc(ctx->stm, 0x0F);
		bstream_putc(ctx->stm, jcc->op8 + 0x10);
		bstream_putw(ctx->stm, (uint16_t)n);
		return 0;
	}
	n = bctx_rel_ofs(ctx, rmo.m.ofs, 6);
	if (bctx_has_stab_pass(ctx) && (n < INT32_MIN || n > INT32_MAX)) {
		bctx_print_err(ctx, "operand offset out of range");
		return -1;
	}
	bstream_putc(ctx->stm, 0x0F);
	bstream_putc(ctx->stm, jcc->op8 + 0x10);
	bstream_putdw(ctx->stm, (uint32_t)n);
	return 0;
}

int bctx_inst_loop(bctx* ctx, int opt /* = 0 */)
{
	brm_opd rmo;
	ASSERT(ctx);
	if (bctx_parse_term(ctx, &rmo) || rmo.opr != ERMO_IMM) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	if (rmo.m.ptrs != 0 && rmo.m.ptrs != EPTR_SHORT) {
		bctx_print_err(ctx, "invalid specifier");
		return -1;
	}
	int64_t n = bctx_rel_ofs(ctx, rmo.m.ofs, 2);
	if (bctx_has_stab_pass(ctx) && (n < INT8_MIN || n > INT8_MAX)) {
		bctx_print_err(ctx, "offset out of range");
		return -1;
	}
	if (opt == 0)
		bstream_putc(ctx->stm, 0xE2);   /* loop rel8 */
	else if (opt == 1)
		bstream_putc(ctx->stm, 0xE1);   /* loope rel8 */
	else if (opt == 2)
		bstream_putc(ctx->stm, 0xE0);   /* loopne rel8 */
	else {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	bstream_putc(ctx->stm, (uint8_t)n);
	return 0;
}

int bctx_inst_push(bctx* ctx)
{
	brm_opd rmo;
	ASSERT(ctx);
	if (bctx_parse_term(ctx, &rmo)) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	if (rmo.opr == ERMO_REG) {
		size_t rz = _reg_size(rmo.r);
		if (rz == 8) {
			bctx_print_err(ctx, "invalid register '%s'", _reg_name(rmo.r));
			return -1;
		}
		if (rz == 64 && ctx->xmode != EI86_686) {
			bctx_print_err(ctx, "illegal register '%s'", _reg_name(rmo.r));
			return -1;
		}
		breg_t rb = _reg_base_frsize(rz);
		if (rz == _plat_size(ctx->xmode)) {
			bstream_putc(ctx->stm, 0x50 + rmo.r - rb);
			return 0;
		}
		brm_opd opr = { ERMO_REG };
		opr.r = (breg_t)(rb + 6);
		return bctx_modrm(ctx, &rmo, &opr, 0xFF, 0xFF);
	} else if (rmo.opr == ERMO_IMM) {
		size_t immt = 0;
		if (_has_ptr_size(rmo.m.ptrs)) {
			immt = _ptr_mode_size(rmo.m.ptrs);
		} else {
			immt = _try_n_size(rmo.m.ofs);
			if (immt > 32) {
				ASSERT(FALSE);
				immt = 32;
			}
		}
		if (immt == 8) {
			bstream_putc(ctx->stm, 0x6A);
			bstream_putc(ctx->stm, (uint8_t)rmo.m.ofs);
			return 0;
		} else if (ctx->xmode == EI86_X86) {
			if (immt != 16) {
				bctx_comm_err(ctx, NULL);
				return -1;
			}
			bstream_putc(ctx->stm, 0x68);
			bstream_putw(ctx->stm, (uint16_t)rmo.m.ofs);
			return 0;
		} else {
			bstream_putc(ctx->stm, 0x68);
			bstream_putdw(ctx->stm, (uint32_t)rmo.m.ofs);
			return 0;
		}
	} else if (rmo.opr == ERMO_MEM) {
		size_t rz = _plat_size(ctx->xmode);
		if (_has_ptr_size(rmo.m.ptrs))
			rz = _ptr_mode_size(rmo.m.ptrs);
		breg_t rb = _reg_base_frsize(rz);
		brm_opd opr = { ERMO_REG };
		opr.r = (breg_t)(rb + 6);
		return bctx_modrm(ctx, &rmo, &opr, 0xFF, 0xFF);
	} else if (rmo.opr == ERMO_SEG) {
		bseg_t seg = rmo.m.seg;
		if (seg >= ESNM_FS && ctx->xmode == EI86_X86) {
			bctx_print_err(ctx, "illegal segment register '%s'", _seg_name(seg));
			return -1;
		}
		switch (seg) {
		case ESNM_CS:
			bstream_putc(ctx->stm, 0x0e);
			break;
		case ESNM_SS:
			bstream_putc(ctx->stm, 0x16);
			break;
		case ESNM_DS:
			bstream_putc(ctx->stm, 0x1E);
			break;
		case ESNM_ES:
			bstream_putc(ctx->stm, 0x06);
			break;
		case ESNM_FS:
			bstream_putc(ctx->stm, 0x0F);
			bstream_putc(ctx->stm, 0xA0);
			break;
		case ESNM_GS:
			bstream_putc(ctx->stm, 0x0F);
			bstream_putc(ctx->stm, 0xA8);
			break;
		default:
			ASSERT(FALSE);
			bctx_comm_err(ctx, NULL);
			return -1;
		}
		return 0;
	}
	bctx_comm_err(ctx, NULL);
	return -1;
}

int bctx_inst_pop(bctx* ctx)
{
	ASSERT(ctx);
	brm_opd rmo;
	if (bctx_parse_term(ctx, &rmo)) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	if (rmo.opr == ERMO_REG) {
		size_t rz = _reg_size(rmo.r);
		if (rz == 8) {
			bctx_print_err(ctx, "invalid register '%s'", _reg_name(rmo.r));
			return -1;
		}
		if (rz == 64 && ctx->xmode != EI86_686) {
			bctx_print_err(ctx, "illegal register '%s'", _reg_name(rmo.r));
			return -1;
		}
		breg_t rb = _reg_base_frsize(rz);
		if (rz == _plat_size(ctx->xmode)) {
			bstream_putc(ctx->stm, 0x58 + rmo.r - rb);
			return 0;
		}
		brm_opd opr = { ERMO_REG };
		opr.r = rb;
		return bctx_modrm(ctx, &rmo, &opr, 0x8F, 0x8F);
	} else if (rmo.opr == ERMO_MEM) {
		size_t rz = _plat_size(ctx->xmode);
		if (_has_ptr_size(rmo.m.ptrs))
			rz = _ptr_mode_size(rmo.m.ptrs);
		breg_t rb = _reg_base_frsize(rz);
		brm_opd opr = { ERMO_REG };
		opr.r = rb;
		return bctx_modrm(ctx, &rmo, &opr, 0x8F, 0x8F);
	} else if (rmo.opr == ERMO_SEG) {
		bseg_t seg = rmo.m.seg;
		if (seg >= ESNM_FS && ctx->xmode == EI86_X86) {
			bctx_print_err(ctx, "illegal segment register '%s'", _seg_name(seg));
			return -1;
		}
		switch (seg) {
		case ESNM_SS:
			bstream_putc(ctx->stm, 0x17);
			break;
		case ESNM_DS:
			bstream_putc(ctx->stm, 0x1F);
			break;
		case ESNM_ES:
			bstream_putc(ctx->stm, 0x07);
			break;
		case ESNM_FS:
			bstream_putc(ctx->stm, 0x0F);
			bstream_putc(ctx->stm, 0xA1);
			break;
		case ESNM_GS:
			bstream_putc(ctx->stm, 0x0F);
			bstream_putc(ctx->stm, 0xA9);
			break;
		default:
			ASSERT(FALSE);
			bctx_comm_err(ctx, NULL);
			return -1;
		}
		return 0;
	}
	bctx_comm_err(ctx, NULL);
	return -1;
}
