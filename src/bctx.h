
/* 2025.05.22 by renwang */

#ifndef __BASM_BCTX_H__
#define __BASM_BCTX_H__

#include "barray.h"
#include "bstream.h"
#include "blexer.h"
#include "bbuiltin.h"
#include "bunsorted_map.h"

#define MAX_PASS_COUNT    10

typedef enum
{
	ERMO_UNDEF,
	ERMO_REG,
	ERMO_SEG,   /* cs,ds,es,... */
	ERMO_IMM,
	ERMO_MEM,
	ERMO_CR,    /* CR0-CR7 */
} brm_opr_t;

typedef struct
{
	brm_opr_t opr;
	union {
		breg_t r;
		struct {
			bseg_t seg;
			uint16_t ptrs;   /* brm_opr_t(s) */
			breg_t reg1;
			uint8_t scale1;  /* only 0,1,2,4,8 is valid for end parsed */
			breg_t reg2;
			int64_t ofs;
			uint8_t sym;     /* 1-has symbol;0-none */
		} m;
	};
} brm_opd;

typedef struct
{
	brm_opd rmo1;
	brm_opd rmo2;
} bexpr_terms;

typedef enum
{
	EI86_X86,  /* 16-bits */
	EI86_386,  /* 32-bits */
	EI86_686,  /* 64-bits */
} x86_xmode;

typedef struct
{
	char* name;
	uint8_t type;
	int64_t val;
} bsym_token_t;

/* context of boasm */
typedef struct _bctx
{
	x86_xmode xmode;
	uint8_t pass_idx;  /*current pass index, base 1*/
	uint8_t pass_need_more : 1;  /*need more pass the source code ?*/
	uint8_t token_kept : 1;
	uint8_t rep_prefix : 2;  /* 0:undefined; 1-rep; 2-repz; 3-repnz */
	uint8_t lock_prefix : 1;

	uint32_t org_addr;  /* org pseudo-instruction */
	uint32_t org_ofs;   /* when org, destination stream offset */
	uint32_t code_start;
	uint32_t times_start;  /* for times 0 db 0,0 */
	uint32_t times_count;

	/* return: 0-continue;1-ok;-1-error */
	int (*spec_ax_fn)(struct _bctx*, breg_t, int32_t);
	int64_t spec_ax_ofs;

	blex_token* tk;     /* current token */
	bstream* stm;
	blexer* lex;

	char* src_file;
	char* dst_file;
	bunsorted_map* sym_map;  /* map of bsym_token_t elements */

	barray* arr_opd;  /* array of brm_opd elements */
	bintarr* arr_op;  /* array of btoken_t elements */
} bctx;

int bctx_init(bctx* ctx);
void bctx_drop(bctx* ctx);
int bctx_start(bctx* ctx);

/* -1-error; 0-exists; 1-ok and updated */
int bctx_ensure_symbol(bctx* ctx, const char* sym, const char* send, uint8_t stype, int64_t val);
const bsym_token_t* bctx_find_symbol(bctx* ctx, const char* sym, const char* send);

/* return: -1-error; 0-eof; 1-ok */
int bctx_cmd_line(bctx* ctx);

/* return: 0-ok; -1-error */
int bctx_parse_term(bctx* ctx, brm_opd* rmo);
int bctx_parse_expr(bctx* ctx, bexpr_terms* expr);

inline int bctx_has_stab_pass(bctx* ctx)
{
	ASSERT(ctx);
	return !ctx->pass_need_more;
}
inline int64_t bctx_rel_ofs(bctx* ctx, int64_t imm, uint8_t ins_size)
{
	return imm - ctx->org_addr - ctx->stm->size + ctx->org_ofs - ins_size;
}

int _386_mem_sib(bctx* ctx, uint8_t cr, breg_t r1, uint8_t scale1, breg_t r2, int32_t ofs);

/* return: 0-ok; -1-error */
int bctx_modrm(bctx* ctx, const brm_opd* rm, const brm_opd* r, uint8_t cmd8, uint8_t cmd32);

/* Intel instruction */
int bctx_inst_mov(bctx* ctx);
int bctx_inst_movs(bctx* ctx, bptr_mode_t pm);
int bctx_inst_stos(bctx* ctx, bptr_mode_t pm);

/* opt: 0-add;1-or;2-adc;3-sbb;4-and;5-sub;6-xor;7-cmp
 * return: 0-ok; -1-error */
int bctx_inst_add(bctx* ctx, uint8_t opt /* = 0 */);

/* return: 0-ok; -1-error */
int bctx_inst_sal(bctx* ctx, uint8_t cr /* = 4 */);
int bctx_inst_lea(bctx* ctx);
int bctx_inst_xchg(bctx* ctx);
int bctx_inst_mul(bctx* ctx);

/* return: 0-ok; -1-error */
int bctx_inst_inc(bctx* ctx, int for_dec/* = false */);

/* return: 0-ok; -1-error */
int bctx_inst_call(bctx* ctx);
int bctx_inst_ret(bctx* ctx, int for_far/* = false */);
int bctx_inst_int(bctx* ctx);
int bctx_inst_jmp(bctx* ctx);
int bctx_inst_jcc(bctx* ctx, const bjcc_cond_elem_t* jcc);
int bctx_inst_loop(bctx* ctx, int opt /* = 0 */);
int bctx_inst_push(bctx* ctx);
int bctx_inst_pop(bctx* ctx);

int bctx_inst_lgdt(bctx* ctx, int for_idt/* = false */);
int bctx_inst_ltr(bctx* ctx);
int bctx_inst_enter(bctx* ctx);

/* out 92h,al; in al,92h */
int bctx_inst_out(bctx* ctx, int for_in/* = false*/);

int bctx_mov_ax_ofs(bctx* ctx, breg_t r, int32_t ofs);

void bctx_print_err(bctx* ctx, const char* fmt, ...);
void bctx_comm_err(bctx* ctx, const char* info/* = NULL*/);

/*
 * inlines
 */

inline size_t _plat_size(x86_xmode md)
{
	if (md == EI86_X86)
		return 16;
	else if (md == EI86_386)
		return 32;
	ASSERT(md == EI86_686);
	return 64;
}

inline size_t _try_n_size(int64_t n)
{
	if (n >= INT8_MIN && n <= INT8_MAX)
		return 8;
	else if (n >= INT16_MIN && n <= INT16_MAX)
		return 16;
	else if (n >= INT32_MIN && n <= INT32_MAX)
		return 32;
	return 64;
}

/* mem or register operand size */
inline size_t _try_rm_size(const brm_opd* rmo)
{
	ASSERT(rmo);
	if (rmo->opr == ERMO_REG)
		return _reg_size(rmo->r);
	if (rmo->opr == ERMO_MEM && _has_ptr_size(rmo->m.ptrs))
		return _ptr_mode_size(rmo->m.ptrs);
	return 0;
}

inline const char* _lex_get_key(const char* p, const char* pend, char* dst, size_t dstsize/*= 256*/)
{
	ASSERT(p && p < pend && dst && dstsize > 0);
	size_t ds = min(dstsize - 1, (size_t)(pend - p));
	dst[ds] = 0;
	return (char*)memcpy(dst, p, ds);
}

#endif/*__BASM_BCTX_H__*/
