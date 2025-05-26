
/* 2022.05.22 by renwang */

#include "bctx.h"
#include "bioext.h"
#include "bstrext.h"
#include "bhash.h"

/*
 * initialize boasm context
 */
/* hash for symbol table */
static size_t _sym_hash(const bsym_token_t* elem)
{
	ASSERT(elem && elem->name);
	return bsm_hash_str(elem->name);
}
static int _sym_compare_elem(const bsym_token_t* elem1, const bsym_token_t* elem2)
{
	ASSERT(elem1 && elem1->name && elem2 && elem2->name);
	return strcmp(elem1->name, elem2->name);
}
static void _sym_item_free(bsym_token_t* elem)
{
	/* NOTE: could not free elem self */
	free(elem->name);
}

/* boasm context */
int bctx_init(bctx* ctx)
{
	ASSERT(ctx);
	memset(ctx, 0, sizeof(*ctx));
	ctx->xmode = EI86_386;
	ctx->tk = (blex_token*)calloc(1, sizeof(blex_token));
	if (ctx->tk == NULL)
		goto _OOM;
	ctx->stm = (bstream*)malloc(sizeof(bstream));
	if (ctx->stm == NULL)
		goto _OOM;
	VERIFY(!bstream_init(ctx->stm, 4096));
	ctx->lex = (blexer*)calloc(1, sizeof(blexer));
	if (ctx->lex == NULL)
		goto _OOM;
	ctx->sym_map = (bunsorted_map*)calloc(1, sizeof(bunsorted_map));
	if (ctx->sym_map == NULL)
		goto _OOM;
	VERIFY(!bunsorted_map_init(ctx->sym_map, 1023, sizeof(bsym_token_t),
		-1, _sym_hash, _sym_compare_elem, _sym_item_free));
	ctx->arr_opd = (barray*)malloc(sizeof(barray));
	if (ctx->arr_opd == NULL)
		goto _OOM;
	barr_init(ctx->arr_opd, sizeof(brm_opd), 6);

	ctx->arr_op = (bintarr*)malloc(sizeof(bintarr));
	if (ctx->arr_op == NULL) {
	_OOM:
		ASSERT(FALSE);
		bctx_print_err(ctx, "out of memory");
		return 100;
	}
	bintarr_init(ctx->arr_op, 6);
	return _jcc_elems_init();
}

void bctx_drop(bctx* ctx)
{
	ASSERT(ctx);
	if (ctx->stm)
		bstream_drop(ctx->stm);
	FREE_PTR(ctx->stm);
	if (ctx->sym_map) {
		bunsorted_map_drop(ctx->sym_map);
		FREE_PTR(ctx->sym_map);
	}
	if (ctx->arr_opd) {
		barr_drop(ctx->arr_opd);
		FREE_PTR(ctx->arr_opd);
	}
	if (ctx->arr_op) {
		bintarr_drop(ctx->arr_op);
		FREE_PTR(ctx->arr_op);
	}
	if (ctx->lex) {
		blexer_drop(ctx->lex);
		FREE_PTR(ctx->lex);
	}
	FREE_PTR(ctx->tk);
	FREE_PTR(ctx->src_file);
	FREE_PTR(ctx->dst_file);
}

int bctx_start(bctx* ctx)
{
	ASSERT(ctx);
	if (!bsm_file_exists(ctx->src_file)) {
		bctx_print_err(ctx, "could not find source file '%s'", ctx->src_file);
		return EXIT_FAILURE;
	}
	if (ctx->dst_file == NULL) {
		char src_dir[MAX_PATH], of_name[MAX_PATH], dst_file[MAX_PATH];
		bsm_get_dir(ctx->src_file, src_dir);
		bsm_fname_without_ext(ctx->src_file, of_name);
		bsm_sprintf(dst_file, sizeof(dst_file), "%s\\%s.out", src_dir, of_name);
		ctx->dst_file = _strdup(dst_file);
		if (ctx->dst_file == NULL) {
			bctx_print_err(ctx, "out of memory");
			return 100;
		}
	}
	if (blexer_init(ctx->lex, ctx->src_file, 0)) {
		ASSERT(FALSE);
		return EXIT_FAILURE;
	}

	/* first scan */
	ctx->pass_idx = 1;
	ctx->pass_need_more = 0;
	for (;;) {
		int ret = bctx_cmd_line(ctx);
		if (ret < 0) {
			if (ctx->lex->errs == 0)
				bctx_comm_err(ctx, NULL);
			return -1;
		}
		if (ret == 0)
			break;
	}

	/* second scan */
	while (ctx->pass_need_more) {
		if (++ ctx->pass_idx > MAX_PASS_COUNT) {
			ASSERT(FALSE);
		}
		ctx->xmode = EI86_386;
		ctx->pass_need_more = 0;
		ctx->token_kept = 0;
		ctx->rep_prefix = 0;
		ctx->lock_prefix = 0;

		ctx->org_addr = 0;
		ctx->org_ofs = 0;
		ctx->code_start = 0;
		ctx->times_start = ctx->times_count = 0;

		ctx->spec_ax_fn = NULL;
		ctx->spec_ax_ofs = 0;

		blex_token_clear(ctx->tk);
		bstream_clear_keep(ctx->stm);
		blexer_clear(ctx->lex);
		for (;;) {
			int ret = bctx_cmd_line(ctx);
			if (ret < 0)
				return -1;
			if (ret == 0)
				break;
		}
	}

	/* Save to local disk */
	if (bsm_save_stream(ctx->stm->data, ctx->stm->size, ctx->dst_file))
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

/* -1-error; 0-exists; 1-ok and updated */
int bctx_ensure_symbol(bctx* ctx, const char* sym, const char* send, uint8_t stype, int64_t val)
{
	ASSERT(ctx);
	if (sym == NULL || *sym == 0) {
		ASSERT(FALSE);
		return -1;
	}
	char sym_buff[256];
	if (send == NULL) {
		_safe_strcpy(sym_buff, sizeof(sym_buff), sym);
	} else {
		ASSERT(send > sym);
		_safe_strcpy(sym_buff, min(sizeof(sym_buff), (size_t)(send - sym + 1)), sym);
	}
	bsm_tolower(sym_buff);
	bsym_token_t stv;
	stv.type = stype;
	stv.name = sym_buff;
	stv.val = val;
	bsym_token_t* rets;
	if (bunsorted_map_insert(ctx->sym_map, &stv, &rets)) {
		ASSERT(FALSE);
		return -1;
	}
	if (rets->name == sym_buff) {
		rets->name = _strdup(sym_buff);
		if (rets->name == NULL) {
			ASSERT(FALSE);
			bctx_print_err(ctx, "out of memory");
			return -1;
		}
		ASSERT(rets->type == stype && rets->val == val);
		return 1;
	}
	ASSERT(!strcmp(rets->name, sym_buff));
	if (rets->type != stype || rets->val != val) {
		rets->type = stype;
		rets->val = val;
		ctx->pass_need_more = 1;
		return 1;
	}
	return 0;
}

const bsym_token_t* bctx_find_symbol(bctx* ctx, const char* sym, const char* send)
{
	bsym_token_t stv;
	const bsym_token_t* ret;
	ASSERT(ctx);
	if (sym == NULL || *sym == 0) {
		ASSERT(FALSE);
		return NULL;
	}
	char sym_buff[256];
	if (send == NULL) {
		_safe_strcpy(sym_buff, sizeof(sym_buff), sym);
	} else {
		ASSERT(send > sym);
		_safe_strcpy(sym_buff, min(sizeof(sym_buff), (size_t)(send - sym + 1)), sym);
	}
	bsm_tolower(sym_buff);
	stv.name = sym_buff;
	ret = bunsorted_map_find(ctx->sym_map, &stv);
	if (!ret)
		ctx->pass_need_more = 1;
	return ret;
}

/*
 * print error
 */
void bctx_print_err(bctx* ctx, const char* fmt, ...)
{
	ASSERT(ctx);
	va_list vl;
	va_start(vl, fmt);
	blexer_print_errv(ctx->lex, fmt, vl);
	va_end(vl);
}

void bctx_comm_err(bctx* ctx, const char* info/* = NULL*/)
{
	char tmp[LEX_TOKEN_BUFSIZE + 1];
	_lex_get_key(ctx->tk->key, ctx->tk->kend, tmp, sizeof(tmp));
	if (info == NULL || *info == '\0')
		blexer_print_err(ctx->lex, "illegal instruction: '%s'", tmp);
	else
		blexer_print_err(ctx->lex, info, tmp);
}
