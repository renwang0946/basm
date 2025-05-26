
/* 2025.05.22 by renwang */

#ifndef __BASM_BLEXER_H__
#define __BASM_BLEXER_H__

#include "bglobal.h"

/* lexer */

#define LEX_TOKEN_BUFSIZE   256

typedef enum 
{
	ETKT_UNDEF,
	ETKT_KEY,
	ETKT_NUM,
	ETKT_STR,
	ETKT_SEP,        /* ,  */
	ETKT_LABEL,
	ETKT_DOLLAR,     /* $  */
	ETKT_DDOLL,      /* $$ */
	ETKT_DOT,        /* .  */
	ETKT_LBRACKET,   /* [  */
	ETKT_RBRACKET,   /* ]  */
	ETKT_LPARENTH,   /* (, parentheses */
	ETKT_RPARENTH,   /* )  */
	ETKT_ADD,        /* +  */
	ETKT_SUB,        /* -  */
	ETKT_MUL,        /* *  */
	ETKT_DIV,        /* /  */
	ETKT_COLON,      /* :  */
	ETKT_RAR,        /* >> */
	ETKT_LNEND,      /* \r or \r\n */
	ETKT_END,        /* end file */
} btoken_t;

typedef struct
{
	btoken_t type;
	const char* key;
	const char* kend;
	char buff[LEX_TOKEN_BUFSIZE];
} blex_token;

inline void blex_token_clear(blex_token* tk)
{
	ASSERT(tk);
	tk->type = ETKT_UNDEF;
	tk->key = tk->kend = NULL;
}

typedef struct 
{
	uint8_t own_buff : 1;
	const char* fsrc;
	HANDLE fhandle;
	HANDLE fmapping;
	const char* buff;
	const char* bufe;  /* buffer end */

	const char* bufc;  /* current buffer */
	const char* line_start;
	uint32_t curr_row;
	uint32_t errs;
} blexer;

int blexer_init(blexer* lex, const char* src_file, size_t ofs/* = 0 */);
void blexer_drop(blexer* lex);

inline void blexer_token_clear(blex_token* tk)
{
	ASSERT(tk);
	tk->type = ETKT_UNDEF;
	tk->key = tk->kend = NULL;
}
inline void blexer_clear(blexer* lex)
{
	ASSERT(lex);
	lex->line_start = lex->bufc = lex->buff;
	lex->curr_row = 0;
}

/* 0:end file;-1:error;1:ok */
int blexer_next(blexer* lex, blex_token* tk);

/* print lexer error */
void blexer_print_err(blexer* lex, const char* fmt, ...);
void blexer_print_errv(blexer* lex, const char* fmt, va_list vl);

#endif/*__BASM_BLEXER_H__*/
