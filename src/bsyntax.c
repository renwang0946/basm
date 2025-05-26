
/* 2025.05.24 by renwang */

#include "bctx.h"

/* Syntax tree
inst-line := inst-expr ('\n'|eof)
inst-expr *= inst-cmd inst-term,inst-term
inst-cmd := 'MOV' | 'ADD' | ...
*/

static int _parse_key(bctx* ctx);
static int _parse_dot_pseudo(bctx* ctx);
static int _parse_label(bctx* ctx, const char* lbl);
static int _end_inst_line(bctx* ctx);

/* return: -1-error; 0-eof; 1-ok */
int bctx_cmd_line(bctx* ctx)
{
	ASSERT(ctx && ctx->tk);
	blex_token* tk = ctx->tk;
	if (ctx->token_kept)
		ctx->token_kept = 0;
	else if (blexer_next(ctx->lex, ctx->tk) < 0)
		return -1;
	switch (tk->type) {
	case ETKT_KEY:
		return _parse_key(ctx);
	case ETKT_DOT:
		return _parse_dot_pseudo(ctx);
	case ETKT_LABEL:
		return _parse_label(ctx, NULL);
	case ETKT_LNEND:
		_end_inst_line(ctx);
		return 1;
	case ETKT_END:
		_end_inst_line(ctx);
		return 0;
	default:
		ASSERT(FALSE);
		break;
	}
	bctx_comm_err(ctx, NULL);
	return -1;
}

/* like:.386; .586p
 * return: 0-eof; 1-ok; -1-error
 */
static int _parse_dot_pseudo(bctx* ctx)
{
	ASSERT(ctx && ctx->tk && ctx->lex);
	blex_token* tk = ctx->tk;
	ASSERT(tk->type == ETKT_DOT);
	if (blexer_next(ctx->lex, tk) < 0)
		return -1;
	if (tk->key == NULL || tk->kend <= tk->key) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	size_t ksz = tk->kend - tk->key;
	if (ksz == 2) {
		if (!_strnicmp(tk->key, "86", 2)) {
			ctx->xmode = EI86_X86;
			return 1;
		}
		ASSERT(FALSE);
		return -1;
	} else if (ksz == 3) {
		if (!_strnicmp(tk->key, "x86", 3)) {
			ctx->xmode = EI86_X86;
			return 1;
		} else if ( !_strnicmp(tk->key, "386", 3)
		         || !_strnicmp(tk->key, "486", 3) 
		         || !_strnicmp(tk->key, "586", 3) ) {
			ctx->xmode = EI86_386;
			return 1;
		} else if (!_strnicmp(tk->key, "686", 3)) {
			ctx->xmode = EI86_686;
			return 1;
		}
		ASSERT(FALSE);
		return -1;
	}
	ASSERT(FALSE);
	bctx_comm_err(ctx, NULL);
	return -1;
}

/* return: 0-eof; 1-ok; -1-error */
static int _parse_label(bctx* ctx, const char* lbl)
{
	ASSERT(ctx);
	const char *pk, *pkend;
	if (lbl && *lbl) {
		pk = lbl;
		pkend = lbl + strlen(lbl);
	} else {
		blex_token* tk = ctx->tk;
		ASSERT(tk && tk->key && tk->kend && tk->kend > tk->key);
		pk = tk->key;
		pkend = tk->kend;
	}
	size_t ofs = ctx->stm->size + ctx->org_addr + ctx->org_ofs;
	int ret = bctx_ensure_symbol(ctx, pk, pkend, 0, ofs);
	if (ret < 0)
		return ret;
	return 1;
}

/* return: 0-eof; 1-ok; -1-error */
static int _end_inst_line(bctx* ctx)
{
	ASSERT(ctx && ctx->stm);
	bstream* stm = ctx->stm;
	if (ctx->rep_prefix) {
		ctx->rep_prefix = 0;
		ASSERT(FALSE);
		bctx_print_err(ctx, "illegal 'REP/REPZ/REPNZ'");
		return -1;
	}
	if (ctx->times_count == 1)
		return 1;
	ASSERT(stm->size >= ctx->times_start);
	if (ctx->times_count == 0) {
		bstream_set_size(stm, ctx->times_start);
	} else {
		ASSERT(ctx->times_start > 1);
		-- ctx->times_count;
		size_t bytes = stm->size - ctx->times_start;
		size_t totals = bytes * ctx->times_count;
		const uint8_t* p = stm->data + ctx->times_start;
		uint8_t* dst = stm->data + stm->size;
		bstream_set_size(stm, stm->size + totals);
		if (stm->data == NULL) {
			ASSERT(FALSE);
			bctx_print_err(ctx, "out of memory");
			ctx->times_count = 1;
			ctx->times_start = 0;
			return -1;
		}
		for (uint32_t i = 0; i < ctx->times_count; ++ i) {
			memcpy(dst, p, bytes);
			dst += bytes;
		}
		bstream_seek_end(stm);
	}
	ctx->times_count = 1;
	ctx->times_start = 0;
	return 1;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_a(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'a' || *tk->key == 'A');
	if (cmd_size <= 1)
		return 0; /*may be it is a label symbol*/
	switch (tk->key[1]) {
	case 'd':
	case 'D':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "d", 1))  /* ADD xxx, xxx */
			return bctx_inst_add(ctx, 0) ? -1 : 1;
		break;
	case 'l':
	case 'L':
		if (cmd_size == 5 && !_strnicmp(tk->key + 2, "ign", 3)) {  /* align xxx */
			bnum_val nv;
			if ( blexer_next(ctx->lex, ctx->tk) <= 0
			  || tk->type != ETKT_NUM ) {
				ASSERT(FALSE);
				bctx_comm_err(ctx, NULL);
				return -1;
			}
			if ( bsm_parse_num(tk->key, tk->kend, &nv)
			  || (nv.type != ENVT_INT && nv.type != ENVT_INT64) ) {
				ASSERT(FALSE);
				bctx_comm_err(ctx, NULL);
				return -1;
			}
			size_t v = (nv.type == ENVT_INT) ? nv.i : (uint32_t)nv.i64;
			size_t ofs = ctx->stm->size + ctx->org_addr - ctx->org_ofs;
			size_t diff = bsm_align(ofs, v) - ofs;
			for (uint32_t x = 0; x < diff; ++ x) {
				bstream_putc(stm, 0x90);
			}
			return 1;
		}
		break;
	case 'n':
	case 'N':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "d", 1))  /* AND xxx, xxx */
			return bctx_inst_add(ctx, 4) ? -1 : 1;
		break;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_b(bctx* ctx)
{
	ASSERT(ctx);
	blex_token* tk = ctx->tk;
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'b' || *tk->key == 'B');
	size_t cmd_size = tk->kend - tk->key;
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'i':
	case 'I':
		if (cmd_size == 4 && !_strnicmp(tk->key + 2, "ts", 2)) {  /* bits 16/32/64 */
			if ( blexer_next(ctx->lex, tk) <= 0
			  || tk->type != ETKT_NUM ) {
				ASSERT(FALSE);
				bctx_comm_err(ctx, NULL);
				return -1;
			}
			bnum_val nv;
			if ( bsm_parse_num(tk->key, tk->kend, &nv)
			 || nv.type != ENVT_INT ) {
				ASSERT(FALSE);
				bctx_comm_err(ctx, NULL);
				return -1;
			}
			if (nv.i == 16) {
				ctx->xmode = EI86_X86;
				return 1;
			} else if (nv.i == 32) {
				ctx->xmode = EI86_386;
				return 1;
			} else if (nv.i == 64) {
				ctx->xmode = EI86_686;
				return 1;
			}
		}
		break;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_c(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'c' || *tk->key == 'C');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'a':
	case 'A':
		if (cmd_size == 4 && !_strnicmp(tk->key + 2, "ll", 2))  /* call xxxxx */
			return bctx_inst_call(ctx) ? -1 : 1;
		break;
	case 'l':
	case 'L':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "i", 1)) {  /* cli */
			bstream_putc(stm, 0xFA);
			return 1;
		}
		break;
	case 'm':
	case 'M':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "p", 1))  /* cmp xx, xx */
			return bctx_inst_add(ctx, 7) ? -1 : 1;
		break;
	default:
		break;
	}
	return 0;
}

static int _inst_db(bctx* ctx, bptr_mode_t pm);

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_d(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'd' || *tk->key == 'D');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'b':
	case 'B':
		if (cmd_size == 2)
			return _inst_db(ctx, EPTR_BYTE) ? -1 : 1;
		break;
	case 'd':
	case 'D':
		if (cmd_size == 2)
			return _inst_db(ctx, EPTR_DWORD) ? -1 : 1;
		break;
	case 'e':
	case 'E':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "c", 1))  /* DEC */
			return bctx_inst_inc(ctx, 1) ? -1 : 1;
		break;
	case 'q':
	case 'Q':
		if (cmd_size == 2)
			return _inst_db(ctx, EPTR_QWORD) ? -1 : 1;
		break;
	case 'w':
	case 'W':
		if (cmd_size == 2)
			return _inst_db(ctx, EPTR_WORD) ? -1 : 1;
		break;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_e(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'e' || *tk->key == 'E');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'n':
	case 'N':
		if (cmd_size == 5 && !_strnicmp(tk->key + 2, "ter", 3))  /* ENTER */
			return bctx_inst_enter(ctx) ? -1 : 1;
		break;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_i(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'i' || *tk->key == 'I');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'n':
	case 'N':
		if (cmd_size == 2) {   /* IN */
			return bctx_inst_out(ctx, 1) ? -1 : 1;
		} else if (cmd_size == 3) {
			if (!_strnicmp(tk->key + 2, "t", 1))   /* INT n */
				return bctx_inst_int(ctx) ? -1 : 1;
			else if (!_strnicmp(tk->key + 2, "c", 1))  /* INC xxx */
				return bctx_inst_inc(ctx, 0) ? -1 : 1;
		}
		break;
	case 'r':
	case 'R':
		if (cmd_size == 4 && !_strnicmp(tk->key + 2, "et", 2)) {  /* IRET */
			bstream_putc(stm, 0xCF);
			return 1;
		} else if (cmd_size == 5) {
			if (!_strnicmp(tk->key + 2, "etd", 3)) {  /* IRETD */
				bstream_putc(stm, 0xCF);
				return 1;
			} else if (!_strnicmp(tk->key + 2, "etq", 3)) {  /* IRETQ */
				if (ctx->xmode != EI86_686) {
					bctx_comm_err(ctx, NULL);
					return -1;
				}
				bstream_putc(stm, 0x48);
				bstream_putc(stm, 0xCF);
				return 1;
			}
		}
		break;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_j(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'j' || *tk->key == 'L');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	if (cmd_size > 1) {
		const bjcc_cond_elem_t* jcc = _find_jcc_elem(tk->key + 1, cmd_size - 1);
		if (jcc)
			return bctx_inst_jcc(ctx, jcc) ? -1 : 1;
	}
	switch (tk->key[1]) {
	case 'm':
	case 'M':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "p", 1))  /* jmp xxxxx */
			return bctx_inst_jmp(ctx) ? -1 : 1;
		break;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_l(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'l' || *tk->key == 'L');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'e':
	case 'E':
		if ( cmd_size == 3
		  && (tk->key[2] == 'a' || tk->key[2] == 'A') ) {  /* LEA */
			return bctx_inst_lea(ctx) ? -1 : 1;
		} else if (cmd_size == 5 && !_strnicmp(tk->key + 2, "ave", 3)) {  /* LEAVE */
			bstream_putc(stm, 0xC9);
			return 1;
		}
		break;
	case 'g':
	case 'G':
		if (cmd_size == 4) {
			if (!_strnicmp(tk->key + 2, "dt", 2))  /* LGDT */
				return bctx_inst_lgdt(ctx, 0) ? -1 : 1;
		}
		break;
	case 'i':
	case 'I':
		if (cmd_size == 4) {
			if (!_strnicmp(tk->key + 2, "dt", 2))  /* LIDT */
				return bctx_inst_lgdt(ctx, 1) ? -1 : 1;
		}
		break;
	case 'o':
	case 'O':
		if (cmd_size == 4 && !_strnicmp(tk->key + 2, "op", 2))  /* LOOP */
			return bctx_inst_loop(ctx, 0) ? -1 : 1;
		break;
	case 't':
	case 'T':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "r", 1))  /* LTR */
			return bctx_inst_ltr(ctx) ? -1 : 1;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_m(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'm' || *tk->key == 'M');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'o':
	case 'O':
		if ( tk->key+3 == tk->kend
		  && (tk->key[2] == 'v' || tk->key[2] == 'V') ) {
			return bctx_inst_mov(ctx) ? -1 : 1;
		} else if (cmd_size == 5 && !_strnicmp(tk->key + 2, "vs", 2)) {
			if (tk->key[4] == 'b' || tk->key[4] == 'B')       /* MOVSB */
				return bctx_inst_movs(ctx, EPTR_BYTE) ? -1 : 1;
			else if (tk->key[4] == 'w' || tk->key[4] == 'W')  /* MOVSW */
				return bctx_inst_movs(ctx, EPTR_WORD) ? -1 : 1;
			else if (tk->key[4] == 'd' || tk->key[4] == 'D')  /* MOVSD*/
				return bctx_inst_movs(ctx, EPTR_DWORD) ? -1 : 1;
			else if (tk->key[4] == 'q' || tk->key[4] == 'Q')  /* MOVSQ */
				return bctx_inst_movs(ctx, EPTR_QWORD) ? -1 : 1;
		}
		break;
	case 'u':
	case 'U':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "l", 1))
			return bctx_inst_mul(ctx) ? -1 : 1;
		break;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_o(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'o' || *tk->key == 'P');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'r':
	case 'R':
		if (cmd_size == 2) {   /* OR */
			return bctx_inst_add(ctx, 1) ? -1 : 1;
		} else if ( tk->key+3 == tk->kend
		         && (tk->key[2] == 'g' || tk->key[2] == 'G') ) {  /* ORG */
			if ( blexer_next(ctx->lex, tk) <= 0
			  || tk->type != ETKT_NUM ) {
				ASSERT(FALSE);
				bctx_comm_err(ctx, NULL);
				return -1;
			}
			bnum_val nv;
			if ( bsm_parse_num(tk->key, tk->kend, &nv)
			  || (nv.type != ENVT_INT && nv.type != ENVT_INT64) ) {
				ASSERT(FALSE);
				bctx_comm_err(ctx, NULL);
				return -1;
			}
			ctx->org_addr = (nv.type == ENVT_INT) ? nv.i : (uint32_t)nv.i64;
			ctx->org_ofs = stm->size;
			if (bstream_empty(stm))
				ctx->code_start = ctx->org_addr;
			return 1;
		}
		break;
	case 'u':
	case 'U':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "t", 1))  /* OUT */
			return bctx_inst_out(ctx, 0) ? -1 : 1;
		break;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_p(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'p' || *tk->key == 'P');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'o':
	case 'O':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "p", 1)) {  /* POP */
			return bctx_inst_pop(ctx) ? -1 : 1;
		} else if ( (cmd_size == 4 && !_strnicmp(tk->key + 2, "pa", 2))   /* POPA */
		         || (cmd_size == 5 && !_strnicmp(tk->key + 2, "pad", 3)) ) {  /* POPAD */
			if (ctx->xmode == EI86_686) {
				bctx_comm_err(ctx, NULL);
				return -1;
			}
			bstream_putc(stm, 0x61);
			return 1;
		}
		break;
	case 'u':
	case 'U':
		if (cmd_size == 4 && !_strnicmp(tk->key + 2, "sh", 2)) {  /* PUSH */
			return bctx_inst_push(ctx) ? -1 : 1;
		} else if ( (cmd_size == 5 && !_strnicmp(tk->key + 2, "sha", 3))   /* PUSHA */
		         || (cmd_size == 6 && !_strnicmp(tk->key + 2, "shad", 4)) ) {  /* PUSHAD */
			if (ctx->xmode == EI86_686) {
				bctx_comm_err(ctx, NULL);
				return -1;
			}
			bstream_putc(stm, 0x60);
			return 1;
		}
		break;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_r(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'r' || *tk->key == 'R');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'c':
	case 'C':
		if (cmd_size == 3) {
			if (!_strnicmp(tk->key + 2, "l", 1))  /* RCL */
				return bctx_inst_sal(ctx, 2) ? -1 : 1;
			else if (!_strnicmp(tk->key + 2, "r", 1))  /* RCR */
				return bctx_inst_sal(ctx, 3) ? -1 : 1;
		}
	case 'e':
	case 'E':
		if (cmd_size == 3) {
			if (!_strnicmp(tk->key + 2, "t", 1)) {  /* RET */
				return bctx_inst_ret(ctx, 0) ? -1 : 1;
			} else if (!_strnicmp(tk->key + 2, "p", 1)) {  /* REP */
				if (ctx->rep_prefix != 0) {
					bctx_comm_err(ctx, NULL);
					return -1;
				}
				ctx->rep_prefix = 1;
				return 1;
			}
		} else if (cmd_size == 4 && !_strnicmp(tk->key + 2, "tf", 2)) {  /* RETF */
			return bctx_inst_ret(ctx, 1) ? -1 : 1;
		}
		break;
	case 'o':
	case 'O':
		if (cmd_size == 3) {
			if (!_strnicmp(tk->key + 2, "l", 1))  /* ROL */
				return bctx_inst_sal(ctx, 0) ? -1 : 1;
			if (!_strnicmp(tk->key + 2, "r", 1))  /* ROR */
				return bctx_inst_sal(ctx, 1) ? -1 : 1;
		}
		break;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_s(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 's' || *tk->key == 'S');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'a':
	case 'A':
		if (cmd_size == 3) {
			if (!_strnicmp(tk->key + 2, "l", 1))  /* SAL */
				return bctx_inst_sal(ctx, 4) ? -1 : 1;
			else if (!_strnicmp(tk->key + 2, "r", 1))  /* SAR */
				return bctx_inst_sal(ctx, 7) ? -1 : 1;
		}
		break;
	case 'h':
	case 'H':
		if (cmd_size == 3) {
			if (!_strnicmp(tk->key + 2, "l", 1))  /* SHL */
				return bctx_inst_sal(ctx, 4) ? -1 : 1;  /* NOTE: SHL same as SAL */
			else if (!_strnicmp(tk->key + 2, "r", 1))  /* SHR */
				return bctx_inst_sal(ctx, 5) ? -1 : 1;
		}
		break;
	case 't':
	case 'T':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "i", 1)) {  /* STI */
			bstream_putc(stm, 0xFB);
			return 1;
		} else if (cmd_size == 5 && !_strnicmp(tk->key + 2, "os", 2)) {
			if (tk->key[4] == 'b' || tk->key[4] == 'B')       /* STOSB */
				return bctx_inst_stos(ctx, EPTR_BYTE) ? -1 : 1;
			else if (tk->key[4] == 'w' || tk->key[4] == 'W')  /* STOSW */
				return bctx_inst_stos(ctx, EPTR_WORD) ? -1 : 1;
			else if (tk->key[4] == 'd' || tk->key[4] == 'D')  /* STOSD */
				return bctx_inst_stos(ctx, EPTR_DWORD) ? -1 : 1;
			else if (tk->key[4] == 'q' || tk->key[4] == 'Q')  /* STOSQ */
				return bctx_inst_stos(ctx, EPTR_QWORD) ? -1 : 1;
		}
		break;
	case 'u':
	case 'U':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "b", 1))  /* SUB */
			return bctx_inst_add(ctx, 5) ? -1 : 1;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_t(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 't' || *tk->key == 'T');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'i':
	case 'I':
		if (cmd_size == 5 && !_strnicmp(tk->key+2, "mes", 3)) {   /* TIMES */
			brm_opd rmo;
			rmo.opr = ERMO_IMM;
			if (bctx_parse_term(ctx, &rmo) || rmo.opr != ERMO_IMM) {
				ASSERT(FALSE);
				bctx_print_err(ctx, "'TIMES' is invalid instruction");
				return -1;
			}
			ctx->times_start = stm->size;
			ctx->times_count = (uint32_t)rmo.m.ofs;
			return 1;
		}
		break;
	default:
		break;
	}
	return 0;
}

/* return: 0-continue; 1-ok; -1-error */
static int _cmd_x(bctx* ctx)
{
	blex_token* tk = ctx->tk;
	bstream* stm = ctx->stm;
	size_t cmd_size = tk->kend - tk->key;
	ASSERT(ctx && tk && stm);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->key + 1 <= tk->kend);
	ASSERT(*tk->key == 'x' || *tk->key == 'X');
	if (cmd_size <= 1)
		return 0;  /* maybe it is a label */
	switch (tk->key[1]) {
	case 'o':
	case 'O':
		if (cmd_size == 3 && !_strnicmp(tk->key + 2, "r", 1))   /* XOR */
			return bctx_inst_add(ctx, 6) ? -1 : 1;
		break;
	default:
		ASSERT(FALSE);
		break;
	}
	return 0;
}

/* return: 0-ok; -1-error */
static int _inst_db(bctx* ctx, bptr_mode_t pm)
{
	bstream* stm = ctx->stm;
	blex_token* tk = ctx->tk;
	ASSERT(ctx && stm && tk);
	for (;;) {
		if (ctx->token_kept) {
			ctx->token_kept = 0;
		} else if (blexer_next(ctx->lex, tk) < 0) {
			ASSERT(FALSE);
			bctx_comm_err(ctx, NULL);
			return -1;
		}
		if (tk->type == ETKT_SEP) {
			continue;  /* nothing */
		} else if (tk->type == ETKT_STR) {
			if (pm != EPTR_BYTE) {
				ASSERT(FALSE);
				bctx_comm_err(ctx, "invalid instruction not in byte mode");
				return -1;
			}
			bstream_putdata(stm, tk->key, tk->kend - tk->key);
			continue;
		} else if (tk->type == ETKT_LNEND
		        || tk->type == ETKT_END
		        || tk->type == ETKT_LABEL) {
			ctx->token_kept = 1;
			break;
		}
		ctx->token_kept = 1;
		brm_opd rmo;
		if (bctx_parse_term(ctx, &rmo)) {
			ASSERT(FALSE);
			bctx_comm_err(ctx, NULL);
			return -1;
		}
		if (rmo.opr == ERMO_IMM) {
			int64_t v = rmo.m.ofs;
			switch (pm) {
			case EPTR_BYTE:
				bstream_putc(stm, (uint8_t)v);
				break;
			case EPTR_WORD:
				bstream_putw(stm, (uint16_t)v);
				break;
			case EPTR_DWORD:
				bstream_putdw(stm, (uint32_t)v);
				break;
			case EPTR_QWORD:
				bstream_putqw(stm, v);
				break;
			default:
				ASSERT(FALSE);
				bctx_comm_err(ctx, NULL);
				return -1;
			}
		}
	}
	return 0;
}

/* pseudo instruction
 * return: 0-ok; -1-error
 */
static int _pinst_equ(bctx* ctx, const char* lbl)
{
	ASSERT(ctx && lbl && *lbl);
	brm_opd rmo;
	if (bctx_parse_term(ctx, &rmo)) {
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	if (rmo.opr == ERMO_IMM) {
		int ret = bctx_ensure_symbol(ctx, lbl, NULL, 0, rmo.m.ofs);
		return ret < 0 ? -1 : 0;
	}
	bctx_comm_err(ctx, NULL);
	return -1;
}

int bctx_inst_out(bctx* ctx, int for_in/* = false */)
{
	bexpr_terms expr;
	brm_opd* rmo1 = for_in ? &expr.rmo2 : &expr.rmo1;
	brm_opd* rmo2 = for_in ? &expr.rmo1 : &expr.rmo2;
	ASSERT(ctx);
	if (bctx_parse_expr(ctx, &expr)) {
		ASSERT(FALSE);
		return -1;
	}
	if (rmo1->opr == ERMO_IMM) {
		int64_t n = rmo1->m.ofs;
		if (n < 0 || n > 255) {
			ASSERT(FALSE);
			bctx_print_err(ctx, "illegal instruction");
			return -1;
		}
		if (rmo2->opr != ERMO_REG)
			goto _INV_CMD;
		size_t rz = _reg_size(rmo2->r);
		breg_t rb = _reg_base_frsize(rz);
		if (rmo2->r != rb) {
			bctx_print_err(ctx, "illegal instruction");
			return -1;
		}
		if (rz == 8) {
			bstream_putc(ctx->stm, for_in ? 0xE4 : 0xE6);
			bstream_putc(ctx->stm, (uint8_t)n);
			return 0;
		} else if (rz == 16) {
			if (ctx->xmode >= EI86_386)
				bstream_putc(ctx->stm, 0x66);
			bstream_putc(ctx->stm, for_in ? 0xE5 : 0xE7);
			bstream_putc(ctx->stm, (uint8_t)n);
			return 0;
		} else if (rz == 32) {
			if (ctx->xmode == EI86_X86)
				bstream_putc(ctx->stm, 0x66);
			bstream_putc(ctx->stm, for_in ? 0xE5 : 0xE7);
			bstream_putc(ctx->stm, (uint8_t)n);
			return 0;
		}
	} else if ( rmo1->opr == ERMO_REG
	         && rmo1->r == ERNM_DX
	         && rmo2->opr == ERMO_REG ) {
		size_t rz = _reg_size(rmo2->r);
		breg_t rb = _reg_base_frsize(rz);
		if (rmo2->r != rb) {
			bctx_print_err(ctx, "illegal instruction");
			return -1;
		}
		if (rz == 8) {
			bstream_putc(ctx->stm, for_in ? 0xEC : 0xEE);
			return 0;
		} else if (rz == 16) {
			if (ctx->xmode >= EI86_386)
				bstream_putc(ctx->stm, 0x66);
			bstream_putc(ctx->stm, for_in ? 0xED : 0xEF);
			return 0;
		} else if (rz == 32) {
			if (ctx->xmode == EI86_X86)
				bstream_putc(ctx->stm, 0x66);
			bstream_putc(ctx->stm, for_in ? 0xED : 0xEF);
			return 0;
		}
	}
_INV_CMD:
	bctx_print_err(ctx, "illegal instruction");
	return -1;
}

/* return: 0-eof; 1-ok; -1-error */
static int _parse_key(bctx* ctx)
{
	ASSERT(ctx && ctx->tk);
	blex_token* tk = ctx->tk;
	ASSERT(tk->type == ETKT_KEY && tk->key && *tk->key);
	if (tk->key == NULL || tk->kend <= tk->key) {
		ASSERT(FALSE);
		bctx_comm_err(ctx, NULL);
		return -1;
	}
	int ret = 0;
	switch (*tk->key) {
	case 'a':
	case 'A':
		ret = _cmd_a(ctx);
		break;
	case 'b':
	case 'B':
		ret = _cmd_b(ctx);
		break;
	case 'c':
	case 'C':
		ret = _cmd_c(ctx);
		break;
	case 'd':
	case 'D':
		ret = _cmd_d(ctx);
		break;
	case 'e':
	case 'E':
		ret = _cmd_e(ctx);
		break;
	case 'i':
	case 'I':
		ret = _cmd_i(ctx);
		break;
	case 'j':
	case 'J':
		ret = _cmd_j(ctx);
		break;
	case 'l':
	case 'L':
		ret = _cmd_l(ctx);
		break;
	case 'm':
	case 'M':
		ret = _cmd_m(ctx);
		break;
	case 'o':
	case 'O':
		ret = _cmd_o(ctx);
		break;
	case 'p':
	case 'P':
		ret = _cmd_p(ctx);
		break;
	case 'r':
	case 'R':
		ret = _cmd_r(ctx);
		break;
	case 's':
	case 'S':
		ret = _cmd_s(ctx);
		break;
	case 't':
	case 'T':
		ret = _cmd_t(ctx);
		break;
	case 'x':
	case 'X':
		ret = _cmd_x(ctx);
		break;
	default:
		break;
	}
	if (ret)
		return ret < 0 ? -1 : 1;
	char tmp[LEX_TOKEN_BUFSIZE + 1];
	_lex_get_key(tk->key, tk->kend, tmp, sizeof(tmp));

	/* equ ? */
	if (ctx->token_kept)
		ctx->token_kept = 0;
	else if (blexer_next(ctx->lex, tk) < 0)
		return -1;
	if (tk->type == ETKT_KEY) {
		size_t cmd_size = tk->kend - tk->key;
		if (cmd_size == 3 && !_strnicmp(tk->key, "equ", 3)) {
			return _pinst_equ(ctx, tmp) ? -1 : 1;
		} else if (cmd_size == 2
		        && (tk->key[0] == 'd' || tk->key[0] == 'D')) {
			char c = tolower(tk->key[1]);
			if (c == 'b' || c == 'd' || c == 'q' || c == 'w') {
				int ret = _parse_label(ctx, tmp);
				if (ret <= 0)
					return ret;
				bptr_mode_t pmt = EPTR_BYTE;
				if (c == 'd')
					pmt = EPTR_DWORD;
				else if (c == 'q')
					pmt = EPTR_QWORD;
				else if (c == 'w')
					pmt = EPTR_WORD;
				return _inst_db(ctx, pmt) ? -1 : 1;
			}
		}
	}
	bctx_comm_err(ctx, NULL);
	return -1;
}

static inline int _parse_ptr_mode(bctx* ctx, brm_opd* rmo, bptr_mode_t pm)
{
	ASSERT(_has_ptr_size(pm));
	if (rmo->opr == ERMO_UNDEF) {
		rmo->opr = ERMO_MEM;
	} else if (rmo->opr != ERMO_MEM || _has_ptr_size(rmo->m.ptrs)) {
		bctx_print_err(ctx, "invalid ptr modifier");
		return -1;
	}
	rmo->m.ptrs |= pm;
	return 0;
}

static inline int _parse_reg_mode(bctx* ctx, brm_opd* rmo, brm_opd* kpr, breg_t r)
{
	ASSERT(ctx && rmo && kpr);
	if (rmo->opr != ERMO_UNDEF && rmo->opr != ERMO_MEM) {
		ASSERT(FALSE);
		bctx_print_err(ctx, "invalid register '%s'", _reg_name(r));
		return -1;
	}
	if (rmo->opr == ERMO_UNDEF) {
		rmo->opr = ERMO_REG;
		rmo->r = r;
		kpr->opr = ERMO_REG;
		kpr->r = r;
	} else {
		ASSERT(kpr->opr == rmo->opr && rmo->opr == ERMO_MEM);
		kpr->opr = ERMO_MEM;
		kpr->m.reg1 = r;
	}
	return 1;
}

static inline int _parse_seg_mode(bctx* ctx, brm_opd* rmo, brm_opd* kpr, breg_t seg)
{
	ASSERT(ctx && rmo && kpr);
	if (rmo->opr != ERMO_UNDEF && rmo->opr != ERMO_MEM) {
		ASSERT(FALSE);
		bctx_print_err(ctx, "invalid segment register '%s'", _seg_name(seg));
		return -1;
	}
	if (rmo->opr == ERMO_UNDEF) {
		rmo->opr = ERMO_SEG;
		rmo->m.seg = seg;
		kpr->opr = ERMO_SEG;
		kpr->m.seg = seg;
	} else {
		ASSERT(kpr->opr == rmo->opr && rmo->opr == ERMO_MEM);
		kpr->opr = ERMO_MEM;
		kpr->m.seg = seg;
	}
	return 1;
}

/* return: 0-ok; -1-error */
static int _combine_opds(bctx* ctx, brm_opd* rmo, brm_opd* rmo1, const brm_opd* rmo2, uint16_t op)
{
	ASSERT(ctx && rmo && rmo1 && rmo2);
	ASSERT(rmo1->opr != ERMO_UNDEF && rmo2->opr != ERMO_UNDEF && op);
	if ( rmo2->opr == ERMO_REG
	  || rmo2->opr == ERMO_SEG
	  || rmo1->opr == ERMO_REG
	  || rmo1->opr == ERMO_SEG ) {
		ASSERT(FALSE);
		bctx_print_err(ctx, "invalid instruction");
		return -1;
	}
	if (rmo2->opr == ERMO_IMM) {
		if (rmo1->opr == ERMO_IMM) {
			if (rmo2->m.sym)
				rmo1->m.sym = rmo2->m.sym;
			if (op == ETKT_ADD)
				rmo1->m.ofs += rmo2->m.ofs;
			else if (op == ETKT_SUB)
				rmo1->m.ofs -= rmo2->m.ofs;
			else if (op == ETKT_MUL)
				rmo1->m.ofs *= rmo2->m.ofs;
			else if (op == ETKT_DIV)
				rmo1->m.ofs /= rmo2->m.ofs;
			else if (op == ETKT_RAR)
				rmo1->m.ofs >>= (uint8_t)(rmo2->m.ofs);
			else {
				ASSERT(FALSE);
				bctx_print_err(ctx, "invalid instruction");
				return -1;
			}
			return 0;
		}
		bctx_print_err(ctx, "invalid instruction");
		return -1;
	}
	ASSERT(rmo2->opr == ERMO_MEM);
	if (rmo1->opr != ERMO_MEM) {
		ASSERT(FALSE);
		bctx_print_err(ctx, "invalid instruction");
		return -1;
	}
	if (op == ETKT_MUL) {
		if (rmo2->m.reg1) {
			if (rmo1->m.reg1 || rmo1->m.ofs < 0) {
				ASSERT(FALSE);
				bctx_print_err(ctx, "invalid instruction for operator '%c'", op);
				return -1;
			}
			int scale = rmo2->m.scale1 == 0 ? 1 : rmo2->m.scale1;
			scale *= (int)rmo1->m.ofs;
			if (scale > 8 || scale < 0) {
				ASSERT(FALSE);
				bctx_print_err(ctx, "invalid instruction for operator '%c'", op);
				return -1;
			}
			rmo1->m.reg1 = rmo2->m.reg1;
			rmo1->m.scale1 = (uint8_t)scale;
		} else if (rmo1->m.reg1) {
			if (rmo2->m.ofs < 0) {
				ASSERT(FALSE);
				bctx_print_err(ctx, "invalid instruction for operator '%c'", op);
				return -1;
			}
			int nScale = rmo1->m.scale1 == 0 ? 1 : rmo1->m.scale1;
			nScale *= (int)rmo2->m.ofs;
			if (nScale > 8 || nScale < 0) {
				ASSERT(FALSE);
				bctx_print_err(ctx, "invalid instruction for operator '%c'", op);
				return -1;
			}
			rmo1->m.scale1 = (uint8_t)nScale;
		} else {
			rmo1->m.ofs *= rmo2->m.ofs;
		}
	} else if (op == ETKT_SUB) {
		rmo1->m.ofs -= rmo2->m.ofs;
	} else if (op == ETKT_ADD) {
		rmo1->m.ofs += rmo2->m.ofs;
	} else if (op == ETKT_DIV) {
		rmo1->m.ofs += rmo2->m.ofs;
	} else if (op == ETKT_RAR) {
		rmo1->m.ofs >>= (uint8_t)rmo2->m.ofs;
	}

	if (rmo2->m.reg1) {
		if (op != ETKT_ADD && op != ETKT_MUL) {
			ASSERT(FALSE);
			bctx_print_err(ctx, "invalid instruction for operator '%c'", op);
			return -1;
		}
		if (rmo1->m.reg1 == ERNM_UNDEF) {
			rmo1->m.reg1 = rmo2->m.reg1;
			rmo1->m.scale1 = rmo2->m.scale1;
			rmo1->m.reg2 = rmo2->m.reg2;
			return 0;
		} else if (rmo1->m.reg2 == ERNM_UNDEF) {
			if (rmo2->m.scale1 < 1) {
				rmo1->m.reg2 = rmo2->m.reg1;
				if (rmo2->m.reg2) {
					bctx_print_err(ctx, "invalid instruction");
					return -1;
				}
				return 0;
			}
			if (rmo1->m.scale1 > 1) {
				if (rmo1->m.reg1 != rmo2->m.reg1) {
					bctx_print_err(ctx, "invalid instruction");
					return -1;
				}
				rmo1->m.scale1 += rmo2->m.scale1;
				if (rmo1->m.scale1 > 8) {
					ASSERT(FALSE);
					bctx_print_err(ctx, "invalid instruction for operator '%c'", op);
					return -1;
				}
				return 0;
			}
			rmo1->m.reg1 = rmo2->m.reg1;
			rmo1->m.scale1 = rmo2->m.scale1;
			return 0;
		}
	} else if (rmo2->m.reg2) {
		if (rmo1->m.reg2) {
			ASSERT(FALSE);
			bctx_print_err(ctx, "invalid instruction");
			return -1;
		}
		rmo1->m.reg2 = rmo2->m.reg2;
	}
	return 0;
}

static inline uint8_t _get_op_pridx(btoken_t op)
{
	switch (op) {
	case ETKT_MUL:  /* '*' */
	case ETKT_DIV:  /* '/' */
	case ETKT_RAR:
		return 1;
	case ETKT_ADD:  /* '+' */
	case ETKT_SUB:  /* '-' */
		return 5;
	case ETKT_LPARENTH:  /* '(' */
	case ETKT_LBRACKET:  /* '[' */
		return 6;
	case ETKT_RPARENTH:  /* ')' */
	case ETKT_RBRACKET:  /* ']' */
		return 7;
	case 0xFF:
		return 100;
	default:
		ASSERT(FALSE);
		break;
	}
	return 10;
}

static inline int _is_prior(btoken_t op_last, btoken_t op_curr)
{
	uint8_t l = _get_op_pridx(op_last);
	uint8_t r = _get_op_pridx(op_curr);
	if (op_last == op_curr)
		return l < 5;
	return l < r;
}

/* return: 0-ok; -1-error */
static int _try_combine_opds(bctx* ctx, brm_opd* rmo, btoken_t op)
{
	while (!_EMPTY_ARR(ctx->arr_op)) {
		btoken_t op_last = (btoken_t)bintarr_back(ctx->arr_op);
		if (!_is_prior(op_last, op))
			break;
		bintarr_pop_back(ctx->arr_op);
		switch (op_last) {
		case ETKT_ADD:  /* + */
		case ETKT_SUB:  /* - */
		case ETKT_MUL:  /* * */
		case ETKT_DIV:  /* / */
		case ETKT_RAR:  /* >> */
			{
				if (ctx->arr_opd->cnt < 2) {
					bctx_comm_err(ctx, NULL);
					return -1;
				}
				brm_opd rmo2 = *(brm_opd*)barr_back(ctx->arr_opd);
				barr_pop_back(ctx->arr_opd);
				brm_opd* rmo1 = barr_back(ctx->arr_opd);
				if (_combine_opds(ctx, rmo, rmo1, &rmo2, op_last))
					return -1;
			}
			break;
		case ETKT_LPARENTH:  /* '(' */
		case ETKT_LBRACKET:  /* '[' */
			ASSERT(!_EMPTY_ARR(ctx->arr_opd));
			return 0;
		default:
			ASSERT(FALSE);
			break;
		}
	}
	return 0;
}

static inline void _ins_kpr(barray* opds, bintarr* ops, brm_opd* kpr)
{
	ASSERT(opds && ops && kpr);
	if (!_EMPTY_ARR(ops) && bintarr_back(ops) == ETKT_SUB) {
		ASSERT(kpr->opr != ERMO_REG);
		kpr->m.ofs = -kpr->m.ofs;
		int* op_last = bintarr_back_ref(ops);
		*op_last = ETKT_ADD;
	}
	barr_push(opds, kpr);
}

/* 0-specifier;1-register; 2-command; 3-label */
static int _parse_key_expr(bctx* ctx, brm_opd* rmo, brm_opd* kpr)
{
	const blex_token* tk = ctx->tk;
	size_t kz = tk->kend - tk->key;
	const bsym_token_t* symv;
	ASSERT(ctx && tk && rmo && kpr);
	ASSERT(tk->type == ETKT_KEY && tk->key && tk->kend);
	ASSERT(tk->kend > tk->key);
	switch (kz) {
	case 2:
		{
			breg_t r = _try_reg_x86(tk->key, tk->kend);
			if (r != ERNM_UNDEF)
				return _parse_reg_mode(ctx, rmo, kpr, r);
			bseg_t seg = _try_seg_reg(tk->key, tk->kend);
			if (seg != ESNM_UNDEF)
				return _parse_seg_mode(ctx, rmo, kpr, seg);
			if (tk->key[0] == 'd' || tk->key[0] == 'D') {
				if ( tk->key[1] == 'b' || tk->key[1] == 'B'
				  || tk->key[1] == 'w' || tk->key[1] == 'W'
				  || tk->key[1] == 'd' || tk->key[1] == 'D'
				  || tk->key[1] == 'q' || tk->key[1] == 'Q' ) {
					return 2;
				}
			}
		}
		break;
	case 3:
		{
			breg_t r = _try_reg_32(tk->key, tk->kend);
			if (r != ERNM_UNDEF)
				return _parse_reg_mode(ctx, rmo, kpr, r);
			r = _try_reg_64(tk->key, tk->kend);
			if (r != ERNM_UNDEF)
				return _parse_reg_mode(ctx, rmo, kpr, r);
			if (!_strnicmp(tk->key, "ptr", 3)) {
				ASSERT(kpr->opr == ERMO_UNDEF);
				if (rmo->opr != ERMO_MEM || rmo->m.ptrs == EPTR_UNDEF) {
					ASSERT(FALSE);
					bctx_comm_err(ctx, "invalid modifier '%s'");
					return -1;
				}
				return 0;
			} else if (!_strnicmp(tk->key, "far", 3)) {
				if ((rmo->m.ptrs & EPTR_REL_MASK) != 0) {
					bctx_comm_err(ctx, "'%s' defined again");
					return -1;
				}
				rmo->m.ptrs |= EPTR_FAR;
				return 0;
			} else if (!_strnicmp(tk->key, "cr", 2)) {
				uint8_t u = tk->key[2];
				if (u < '0' || u > '8') {
					ASSERT(FALSE);
					bctx_comm_err(ctx, "invalid control register '%s'");
					return -1;
				}
				rmo->m.ofs = kpr->m.ofs = u - '0';
				rmo->opr = kpr->opr = ERMO_CR;
				return 1;
			}
		}
		break;
	case 4:
		if (!_strnicmp(tk->key, "word", 4))
			return _parse_ptr_mode(ctx, rmo, EPTR_WORD);
		else if (!_strnicmp(tk->key, "byte", 4))
			return _parse_ptr_mode(ctx, rmo, EPTR_BYTE);
		else if (!_strnicmp(tk->key, "near", 4)) {
			if ((rmo->m.ptrs & EPTR_REL_MASK) != 0) {
				bctx_comm_err(ctx, "'%s' defined again\n");
				return -1;
			}
			rmo->m.ptrs |= EPTR_NEAR;
			return 0;
		}
		break;
	case 5:
		if (!_strnicmp(tk->key, "dword", 5))
			return _parse_ptr_mode(ctx, rmo, EPTR_DWORD);
		else if (!_strnicmp(tk->key, "qword", 5))
			return _parse_ptr_mode(ctx, rmo, EPTR_QWORD);
		else if (!_strnicmp(tk->key, "fword", 5))
			return _parse_ptr_mode(ctx, rmo, EPTR_FWORD);
		else if (!_strnicmp(tk->key, "short", 5)) {
			if ((rmo->m.ptrs & EPTR_REL_MASK) != 0) {
				bctx_comm_err(ctx, "'%s' defined again\n");
				return -1;
			}
			rmo->m.ptrs |= EPTR_SHORT;
			return 0;
		}
		break;
	default:
		break;
	}
	if (rmo->opr == ERMO_REG || rmo->opr == ERMO_SEG) {
		ASSERT(FALSE);
		bctx_print_err(ctx, "invalid instruction");
		return -1;
	}
	symv = bctx_find_symbol(ctx, tk->key, tk->kend);
	if (symv) {
		ASSERT(symv->type == 0);
		if (symv->type == 0) {   /* immediate */
			if (rmo->opr == ERMO_UNDEF)
				kpr->opr = rmo->opr = ERMO_IMM;
			kpr->m.ofs = symv->val;
			return 1;
		}
		ASSERT(FALSE);
		bctx_print_err(ctx, "unsupported instruction");
		return -1;
	}
	if (ctx->pass_idx > 1) {
		ASSERT(FALSE);
		bctx_comm_err(ctx, "could not find symbol '%s'\n");
		return -1;
	}
	if (rmo->opr == ERMO_UNDEF) {
		rmo->opr = ERMO_IMM;
		kpr->opr = ERMO_IMM;
	}
	rmo->m.sym = 1;
	kpr->m.sym = 1;
	kpr->m.ofs = ctx->org_ofs ? ctx->org_ofs : 1;
	return 1;
}

static inline void _clear_rpoland(bctx* ctx)
{
	ASSERT(ctx && ctx->arr_opd && ctx->arr_op);
	barr_clear(ctx->arr_opd, 0);
	bintarr_clear(ctx->arr_op, 0);
}

static int64_t _str_imm_to_i(const char* p, const char* pend)
{
	ASSERT(p && pend && pend >= p);
	int64_t v = 0;
	size_t size = pend - p;
	if (size > 15)
		size = 15;
	for (size_t i = 0; i < size; ++i, ++p) {
		v += (*p) << (i * 8);
	}
	return v;
}

/* syntax: reg / mem | seg : [mem] | [seg:mem] | seg : [mem] + imm8
 *     mem := reg + reg + imm
 */
int bctx_parse_term(bctx* ctx, brm_opd* rmo)
{
	blex_token* tk = ctx->tk;
	ASSERT(ctx && ctx->tk);
	memset(rmo, 0, sizeof(*rmo));
	_clear_rpoland(ctx);
	for (;;) {
		if (ctx->token_kept) {
			ctx->token_kept = 0;
		} else if (blexer_next(ctx->lex, tk) < 0) {
			ASSERT(FALSE);
			bctx_print_err(ctx, "invalid instruction");
			ctx->token_kept = 1;
			goto _INV_INST;
		}
		switch (tk->type) {
		case ETKT_KEY:
			{
				int ret;
				brm_opd kpr = { rmo->opr };
				ret = _parse_key_expr(ctx, rmo, &kpr);
				if (ret < 0) {
					ASSERT(FALSE);
					goto _INV_INST;
				} else if (ret == 1) {
					ASSERT(kpr.opr);
					if (!_EMPTY_ARR(ctx->arr_op) && kpr.opr == ERMO_REG) {
						uint16_t op_last = (uint16_t)bintarr_back(ctx->arr_op);
						if (op_last != ETKT_ADD && op_last != ETKT_MUL) {
							ASSERT(FALSE);
							bctx_comm_err(ctx, NULL);
							goto _INV_INST;
						}
					}
					_ins_kpr(ctx->arr_opd, ctx->arr_op, &kpr);
				} else if (ret == 2) {
					ctx->token_kept = 1;
					goto _LOOP_END;
				}
			}
			break;
		case ETKT_NUM:
			{
				bnum_val nv;
				if ( bsm_parse_num(tk->key, tk->kend, &nv)
				  || (nv.type != ENVT_INT && nv.type != ENVT_INT64) ) {
					bctx_comm_err(ctx, "invalid number: %s");
					goto _INV_INST;
				}
				if ( rmo->opr == ERMO_MEM
				  || rmo->opr == ERMO_IMM
				  || rmo->opr == ERMO_UNDEF ) {
					if (rmo->opr == ERMO_UNDEF)
						rmo->opr = ERMO_IMM;
					brm_opd kpr = { rmo->opr };
					kpr.m.ofs = nv.type == ENVT_INT64 ? nv.i64 : nv.i;
					_ins_kpr(ctx->arr_opd, ctx->arr_op, &kpr);
				} else {
					goto _INV_INST;
				}
			}
			break;
		case ETKT_LABEL:
			{
				bseg_t seg = _try_seg_reg(tk->key, tk->kend);
				if (seg == ESNM_UNDEF) {
					ASSERT(FALSE);
					bctx_comm_err(ctx, "invalid segment register '%s'");
					goto _INV_INST;
				} else if (rmo->m.seg != ESNM_UNDEF) {
					ASSERT(FALSE);
					bctx_comm_err(ctx, "segment register '%s' was set again");
					goto _INV_INST;
				}
				if (rmo->opr == ERMO_UNDEF)
					rmo->opr = ERMO_MEM;
				else if (rmo->opr != ERMO_MEM)
					goto _INV_INST;
				rmo->m.seg = seg;
			}
			break;
		case ETKT_LBRACKET:
			if (rmo->opr != ERMO_MEM && rmo->opr != ERMO_UNDEF)
				goto _INV_INST;
			rmo->opr = ERMO_MEM;
			bintarr_push(ctx->arr_op, tk->type);
			break;
		case ETKT_RBRACKET:
			if ( rmo->opr != ERMO_MEM
			  || _try_combine_opds(ctx, rmo, tk->type) ) {
				goto _INV_INST;
			}
			break;
		case ETKT_LPARENTH:
			bintarr_push(ctx->arr_op, tk->type);
			break;
		case ETKT_RPARENTH:
			if (_try_combine_opds(ctx, rmo, tk->type))
				goto _INV_INST;
			break;
		case ETKT_ADD:
			if (_try_combine_opds(ctx, rmo, tk->type))
				goto _INV_INST;
			bintarr_push(ctx->arr_op, tk->type);
			break;
		case ETKT_SUB:
			if (_try_combine_opds(ctx, rmo, tk->type))
				goto _INV_INST;
			bintarr_push(ctx->arr_op, tk->type);
			break;
		case ETKT_MUL:
			if (_try_combine_opds(ctx, rmo, tk->type))
				goto _INV_INST;
			bintarr_push(ctx->arr_op, tk->type);
			break;
		case ETKT_RAR:
			if (_try_combine_opds(ctx, rmo, ETKT_RAR))
				goto _INV_INST;
			bintarr_push(ctx->arr_op, tk->type);
			break;
		case ETKT_DOLLAR:
		case ETKT_DDOLL:
			{
				btoken_t tt = tk->type;
				if (rmo->opr == ERMO_MEM || rmo->opr == ERMO_IMM || rmo->opr == ERMO_UNDEF) {
					brm_opd kpr = { 0 };
					if (rmo->opr == ERMO_UNDEF)
						rmo->opr = ERMO_IMM;
					kpr.opr = rmo->opr;
					if (tt == ETKT_DOLLAR)
						kpr.m.ofs = ctx->stm->size + ctx->org_addr - ctx->org_ofs;
					else
						kpr.m.ofs = ctx->code_start;
					_ins_kpr(ctx->arr_opd, ctx->arr_op, &kpr);
				} else {
					goto _INV_INST;
				}
			}
			break;
		case ETKT_SEP:
			ctx->token_kept = 1;
			goto _LOOP_END;
		case ETKT_STR:
			{
				brm_opd kpr = { 0 };
				if (rmo->opr == ERMO_UNDEF)
					rmo->opr = ERMO_IMM;
				kpr.opr = rmo->opr;
				kpr.m.ofs = _str_imm_to_i(tk->key, tk->kend);
				_ins_kpr(ctx->arr_opd, ctx->arr_op, &kpr);
			}
			break;
		case ETKT_LNEND:
		case ETKT_END:
		case ETKT_COLON:
			ctx->token_kept = 1;
			goto _LOOP_END;
		default:
			bctx_comm_err(ctx, NULL);
			goto _INV_INST;
		}
	}
_LOOP_END:
	if (_try_combine_opds(ctx, rmo, 0xFF))
		goto _INV_INST;
	if (rmo->opr == ERMO_UNDEF || ctx->arr_opd->cnt > 1) {
		ASSERT(FALSE);
		goto _INV_INST;
	}
	if (ctx->arr_opd->cnt == 1) {
		const brm_opd* kpr = (brm_opd *)barr_back(ctx->arr_opd);
		ASSERT(kpr->opr == rmo->opr);
		switch (kpr->opr) {
		case ERMO_IMM:
			rmo->m.ofs = kpr->m.ofs;
			break;
		case ERMO_MEM:
			rmo->m.reg1 = kpr->m.reg1;
			rmo->m.scale1 = kpr->m.scale1;
			rmo->m.reg2 = kpr->m.reg2;
			rmo->m.ofs = kpr->m.ofs;
			break;
		case ERMO_REG:
			ASSERT(rmo->r == kpr->r);
			break;
		case ERMO_CR:
			ASSERT(rmo->m.ofs == kpr->m.ofs);
			break;
		case ERMO_SEG:
			ASSERT(rmo->m.seg == kpr->m.seg);
			break;
		default:
			ASSERT(FALSE);
			barr_pop_back(ctx->arr_opd);
			goto _INV_INST;
		}
		barr_pop_back(ctx->arr_opd);
	}
	_clear_rpoland(ctx);
	return 0;
_INV_INST:
	bctx_print_err(ctx, "invalid instruction");
	_clear_rpoland(ctx);
	return -1;
}

int bctx_parse_expr(bctx* ctx, bexpr_terms* exp)
{
	ASSERT(ctx && ctx->tk && exp);
	memset(exp, 0, sizeof(*exp));
	if (bctx_parse_term(ctx, &exp->rmo1)) {
		ASSERT(FALSE);
		bctx_print_err(ctx, "invalid instruction");
		return -1;
	}
	blex_token* tk = ctx->tk;
	if (ctx->token_kept)
		ctx->token_kept = 0;
	else if (blexer_next(ctx->lex, tk) < 0)
		return -1;
	if (tk->type == ETKT_LNEND || tk->type == ETKT_END) {
		ctx->token_kept = 1;
		return 0;
	} else if (tk->type != ETKT_SEP) {
		ctx->token_kept = 1;
		return 0;
	}
	if (bctx_parse_term(ctx, &exp->rmo2)) {
		bctx_print_err(ctx, "invalid instruction");
		return -1;
	}
	return 0;
}

static inline int _is_x86_base_indexed_reg(breg_t r)
{
	return r == ERNM_BX || r == ERNM_BP || r == ERNM_SI || r == ERNM_DI;
}

static inline void _x86_putrm(bstream* stm, breg_t r, uint8_t cr, uint8_t crm, int32_t ofs)
{
	ASSERT(stm);
	if (ofs == 0) {
		bstream_putc(stm, cr | crm);
	} else if (ofs >= INT8_MIN && ofs <= INT8_MAX) {
		bstream_putc(stm, 0x40 | cr | crm);
		bstream_putc(stm, (uint8_t)ofs);
	} else {
		bstream_putc(stm, 0x80 | cr | crm);
		bstream_putw(stm, (uint16_t)ofs);
	}
}

/* return: 0-ok; -1-error */
static int _x86_mem_modrm(bctx* ctx, breg_t r, breg_t r1, breg_t r2, int32_t ofs)
{
	bstream* stm = ctx->stm;
	ASSERT(ctx && stm);
	if ( (r1 != ERNM_UNDEF && !_is_x86_base_indexed_reg(r1))
	  || (r2 != ERNM_UNDEF && !_is_x86_base_indexed_reg(r2)) ) {
		bctx_print_err(ctx, "invalid register is used at first operand in 16-bit mode");
		return -1;
	}
	if (r1 == ERNM_SI || r1 == ERNM_DI) {
		if (r2 == ERNM_BX || r2 == ERNM_BP) {
			breg_t t = r1;
			r1 = r2;
			r2 = t;
		} else if (r2 != ERNM_UNDEF) {
			bctx_print_err(ctx, "invalid register is used at first operand");
			return -1;
		}
	} else if (r2 != ERNM_UNDEF && r2 != ERNM_SI && r2 != ERNM_DI) {
		bctx_print_err(ctx, "invalid register is used at first operand");
		return -1;
	}
	size_t rz = _reg_size(r);
	breg_t rb = _reg_base_frsize(rz);
	uint8_t cr = (r - rb) << 3;
	if (r1 == ERNM_BX) {
		if (r2 != ERNM_UNDEF && r2 != ERNM_SI && r2 != ERNM_DI) {
			ASSERT(FALSE);
			return -1;
		}
		uint8_t crm = (r2 == ERNM_UNDEF) ? 0x07 : (r2 == ERNM_SI ? 0 : 0x01);
		_x86_putrm(stm, r, cr, crm, ofs);
		return 0;
	} else if (r1 == ERNM_BP) {
		ASSERT(r2 == ERNM_UNDEF || r2 == ERNM_SI || r2 == ERNM_DI);
		if (r2 == ERNM_SI || r2 == ERNM_DI) {
			uint8_t crm = (r2 == ERNM_SI ? 0x02 : 0x03);
			_x86_putrm(stm, r, cr, crm, ofs);
		} else if (r2 == ERNM_UNDEF) {
			if (ofs >= INT8_MIN && ofs <= INT8_MAX) {
				bstream_putc(stm, 0x40 | cr | 0x06);
				bstream_putc(stm, (uint8_t)ofs);
			} else {
				bstream_putc(stm, 0x80 | cr | 0x06);
				bstream_putw(stm, (uint16_t)ofs);
			}
		} else {
			ASSERT(FALSE);
			return -1;
		}
		return 0;
	} else if (r1 == ERNM_SI || r1 == ERNM_DI) {
		uint8_t crm = (r1 == ERNM_SI ? 0x04 : 0x05);
		_x86_putrm(stm, r, cr, crm, ofs);
		return 0;
	} else if (r1 == ERNM_UNDEF) {
		ASSERT(r2 == ERNM_UNDEF);
		if (ctx->spec_ax_fn)
			return (*ctx->spec_ax_fn)(ctx, r, ofs);
		bstream_putc(stm, cr | 0x06);
		bstream_putw(stm, (uint16_t)ofs);
		return 0;
	}
	ASSERT(FALSE);
	return 0;
}

/* calculate sib */
int _386_mem_sib(bctx* ctx, uint8_t cr, breg_t r1, uint8_t scale1, breg_t r2, int32_t ofs)
{
	bstream* stm = ctx->stm;
	int is_rip_mode = (ctx->xmode == EI86_686 && r1 == ERNM_UNDEF);	
	size_t r1z = is_rip_mode ? 64 : _reg_size(r1);
	breg_t rb = _reg_base_frsize(r1z);
	uint8_t ss = 0, zero_ofs = 0;
	ASSERT(ctx && stm && r1z >= 32);
	if (scale1 == 2)
		ss = 0x40;
	else if (scale1 == 4)
		ss = 0x80;
	else if (scale1 == 8)
		ss = 0xC0;
	uint8_t idx = is_rip_mode ? 0x20 : ((r1 - rb) << 3);
	uint8_t base = (r2 == ERNM_UNDEF) ? 0x05 : (r2 - rb);
	uint8_t sib = ss | idx | base;
	if (r1 == ERNM_ESP || r1 == ERNM_RSP) {
		if (scale1 > 1) {
			bctx_print_err(ctx, "cannot use '%s' for scaled index", _reg_name(r1));
			return -1;
		} else if (r2 == ERNM_ESP || r2 == ERNM_RSP) {
			bctx_print_err(ctx, "cannot use '%s' for scaled index", _reg_name(r2));
			return -1;
		}
		if (r2 == ERNM_UNDEF) {
			sib = 0x24;  /* only esp/rsp */
			zero_ofs = 1;
		} else {
			breg_t t = r1;
			r1 = r2;
			r2 = t;
			idx = (r1 - rb) << 3;
			base = (r2 - rb);
			sib = idx | base;
		}
	} else if (r2 == ERNM_EBP || r2 == ERNM_RBP) {
		if (ofs == 0) {
			bstream_putc(stm, 0x40 | cr | 0x04);
			bstream_putc(stm, sib);
			bstream_putc(stm, 0);
			return 0;
		}
	}

	if (ofs == 0) {
		bstream_putc(stm, cr | 0x04);  /* modR/M */
		bstream_putc(stm, sib);
		if ((r2 == ERNM_UNDEF && zero_ofs == 0))
			bstream_putdw(stm, 0);  /* disp32 */
	} else if (ofs >= INT8_MIN && ofs <= INT8_MAX) {
		bstream_putc(stm, 0x40 | cr | 0x04);  /* modR/M */
		bstream_putc(stm, sib);
		bstream_putc(stm, (uint8_t)ofs);
	} else {
		if (r2 == ERNM_UNDEF)
			bstream_putc(stm, cr | 0x04);
		else
			bstream_putc(stm, 0x80 | cr | 0x04);
		bstream_putc(stm, sib);
		bstream_putdw(stm, ofs);
	}
	return 0;
}

/* return: 0-ok; -1-error */
static int _386_mem_modrm(bctx* ctx, breg_t r, breg_t r1, uint8_t scale1, breg_t r2, int32_t ofs)
{
	bstream* stm = ctx->stm;
	size_t rz = _reg_size(r);
	uint8_t cr = (r - _reg_base_frsize(rz)) << 3;
	ASSERT(ctx && stm);
	if (r1 == ERNM_UNDEF) {
		ASSERT(r2 == ERNM_UNDEF && scale1 <= 1);
		if (ctx->spec_ax_fn) {
			int ret = (*ctx->spec_ax_fn)(ctx, r, ofs);
			if (ret < 0)
				return -1;
			else if (ret) {
				ASSERT(ret == 1);
				return 0;
			}
		}
		if (rz == 64)
			return _386_mem_sib(ctx, cr, r1, scale1, r2, ofs);
		bstream_putc(stm, cr | 0x05);
		bstream_putdw(stm, (uint32_t)ofs);
		return 0;
	}
	if (scale1 > 1 || r2 != ERNM_UNDEF || r1 == ERNM_ESP || r1 == ERNM_RSP)
		return _386_mem_sib(ctx, cr, r1, scale1, r2, ofs);
	size_t r1z = _reg_size(r1);
	ASSERT(r1z >= 32 && r2 == ERNM_UNDEF);
	uint8_t crm = r1 - (r1z == 64 ? ERNM_RAX : ERNM_EAX);
	if (ofs == 0) {
		if (r1 == ERNM_EBP || r1 == ERNM_RBP) {
			bstream_putc(stm, 0x40 | cr | crm);
			bstream_putc(stm, 0);
		} else {
			bstream_putc(stm, cr | crm);
		}
	} else if (ofs >= INT8_MIN && ofs <= INT8_MAX) {
		bstream_putc(stm, 0x40 | cr | crm);
		bstream_putc(stm, (uint8_t)ofs);
	} else {
		bstream_putc(stm, 0x80 | cr | crm);
		bstream_putdw(stm, (uint32_t)ofs);
	}
	return 0;
}

/* return: 0-ok; -1-error */
int bctx_modrm(bctx* ctx, const brm_opd* rm, const brm_opd* r, uint8_t cmd8, uint8_t cmd32)
{
	bstream* stm = ctx->stm;
	size_t rz = _reg_size(r->r);
	ASSERT(ctx && stm && rm && r);
	if (rm->opr == ERMO_REG) {
		breg_t rb;
		uint8_t cr;
		size_t rmz = _reg_size(rm->r);
		if (rmz != rz) {
			bctx_print_err(ctx, "registers size are different");
			return -1;
		}
		rb = _reg_base_frsize(rz);
		cr = ((r->r - rb) << 3) | (rm->r - rb);
		if (rz == 8) {
			bstream_putc(stm, cmd8);
			bstream_putc(stm, 0xC0 | cr);
			return 0;
		} else if (rz == 16) {
			if (ctx->xmode >= EI86_386 && !ctx->lock_prefix)
				bstream_putc(stm, 0x66);
			bstream_putc(stm, cmd32);
			bstream_putc(stm, 0xC0 | cr);
			return 0;
		} else if (rz == 32 || rz == 64) {
			if (ctx->xmode == EI86_X86 && !ctx->lock_prefix) {
				ASSERT(rz == 32);
				bstream_putc(stm, 0x66);
			} else if (rz == 64 && (cmd8 != cmd32) && !ctx->lock_prefix) {
				ASSERT(ctx->xmode == EI86_686);
				bstream_putc(stm, 0x48);
			}
			bstream_putc(stm, cmd32);
			bstream_putc(stm, 0xC0 | cr);
			return 0;
		}
		ASSERT(FALSE);
		return -1;
	}
	ASSERT(rm->opr == ERMO_MEM);
	if ( bctx_has_stab_pass(ctx)
	  && (rm->m.ofs < INT32_MIN || rm->m.ofs > UINT32_MAX
	   ||(ctx->xmode == EI86_X86
	    &&(rm->m.ofs > UINT16_MAX || rm->m.ofs < INT16_MIN))) ) {
		ASSERT(FALSE);
		bctx_print_err(ctx, "displacement '%lld' out of bounds", rm->m.ofs);
		return -1;
	}
	size_t old_pos = ctx->stm->pos;
	if (rm->m.seg != ESNM_UNDEF)
		bstream_putc(stm, _seg_prefix(rm->m.seg));
	uint8_t scale1 = 0;
	breg_t r1 = ERNM_UNDEF;
	breg_t r2 = ERNM_UNDEF;
	if (rm->m.reg1 != ERNM_UNDEF) {
		scale1 = rm->m.scale1;
		r1 = rm->m.reg1;
	}
	if (rm->m.reg2 != ERNM_UNDEF) {
		if (r1 == ERNM_UNDEF)
			r1 = rm->m.reg2;
		else
			r2 = rm->m.reg2;
	}
	ASSERT(rm->opr == ERMO_MEM);
	if (r1 != ERNM_UNDEF) {
		size_t r1z = _reg_size(r1);
		if (r1z == 8) {
			bctx_print_err(ctx, "invalid register '%s' at first operand", _reg_name(r1));
			bstream_set_size(stm, old_pos);
			return -1;
		} else if (r1z == 64 && ctx->xmode < EI86_686) {
			bctx_print_err(ctx, "register '%s' at first operand can only be used in 64-bit mode", _reg_name(r1));
			bstream_set_size(stm, old_pos);
			return -1;
		}
		if (r2 != ERNM_UNDEF && r1z != _reg_size(r2)) {
			bctx_print_err(ctx, "registers size are different at first operand");
			bstream_set_size(stm, old_pos);
			return -1;
		}
	}
	if (ctx->xmode == EI86_X86) {
		ASSERT(rz < 64);
		size_t r1z = (r1 == ERNM_UNDEF) ? 0 : _reg_size(r1);
		ASSERT(r1z < 64);
		if (rz == 32 && !ctx->lock_prefix)
			bstream_putc(stm, 0x66);
		if (r1z == 32 && !ctx->lock_prefix)
			bstream_putc(stm, 0x67);
		bstream_putc(stm, rz == 8 ? cmd8 : cmd32);
		if (r1z == 32) {
			if (_386_mem_modrm(ctx, r->r, r1, scale1, r2, (int32_t)rm->m.ofs)) {
				bstream_set_size(stm, old_pos);
				return -1;
			}
			return 0;
		}
		if (scale1 > 1) {
			bctx_print_err(ctx, "Invalid scaled number in 16-bit mode");
			return -1;
		}
		if (_x86_mem_modrm(ctx, r->r, r1, r2, (int32_t)rm->m.ofs)) {
			bstream_set_size(stm, old_pos);
			return -1;
		}
		return 0;
	}
	ASSERT(ctx->xmode == EI86_386 || ctx->xmode == EI86_686);
	size_t r1z = (r1 == ERNM_UNDEF) ? 0 : _reg_size(r1);
	if (ctx->xmode == EI86_386) {
		ASSERT(r1z < 64);
		if (rz == 16 && cmd8 != cmd32 && !ctx->lock_prefix)
			bstream_putc(stm, 0x66);
		if (r1z == 16 && !ctx->lock_prefix)
			bstream_putc(stm, 0x67);
	} else {
		if (rz == 16 && cmd8 != cmd32 && !ctx->lock_prefix)
			bstream_putc(stm, 0x66);
		if (r1z == 16) {
			bctx_print_err(ctx, "invalid register '%s'", _reg_name(r1));
			bstream_set_size(stm, old_pos);
			return -1;
		} else if (r1z == 32 && !ctx->lock_prefix) {
			bstream_putc(stm, 0x67);
		}
		if (rz == 64 && cmd8 != cmd32 && !ctx->lock_prefix)
			bstream_putc(stm, 0x48);
	}
	bstream_putc(stm, rz == 8 ? cmd8 : cmd32);
	if (r1z == 16) {
		if (scale1 > 1) {
			bctx_print_err(ctx, "invalid scaled number in 16-bit mode");
			return -1;
		} else if (_x86_mem_modrm(ctx, r->r, r1, r2, (int32_t)rm->m.ofs)) {
			bstream_set_size(stm, old_pos);
			return -1;
		}
		return 0;
	}
	if (_386_mem_modrm(ctx, r->r, r1, scale1, r2, (int32_t)rm->m.ofs)) {
		bstream_set_size(stm, old_pos);
		return -1;
	}
	return 0;
}
