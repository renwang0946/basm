
/* 2025.05.22 by renwang */

#include "blexer.h"
#include "bioext.h"

int blexer_init(blexer* lex, const char* src_file, size_t ofs)
{
	uint64_t fsize = 0;
	ASSERT(lex && bsm_file_exists(src_file));
	memset(lex, 0, sizeof(*lex));
	lex->fsrc = src_file;
	lex->fhandle = CreateFile(src_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (lex->fhandle == INVALID_HANDLE_VALUE) {
		ASSERT(FALSE);
		blexer_print_err(lex, "open ASM source file '%s' failed: %d", src_file, errno);
		return -1;
	}
	lex->fmapping = CreateFileMapping(lex->fhandle, 0, PAGE_READONLY, 0, 0, 0);
	if (lex->fmapping == NULL || lex->fmapping == INVALID_HANDLE_VALUE) {
		ASSERT(FALSE);
		blexer_print_err(lex, "mapping ASM source file '%s' failed: %s", src_file, errno);
		return -1;
	}
	lex->own_buff = 1;
	VERIFY(GetFileSizeEx(lex->fhandle, (PLARGE_INTEGER)&(fsize)));
	lex->buff = (const char*)MapViewOfFile(lex->fmapping, FILE_MAP_READ, bsm_hi_dword(ofs),
		bsm_lo_dword(ofs), (SIZE_T)fsize);
	if (lex->buff == NULL) {
		ASSERT(FALSE);
		blexer_print_err(lex, "mapping view ASM source file '%s' failed: %d", src_file, errno);
		return -1;
	}
	lex->bufe = lex->buff + fsize;
	lex->line_start = lex->bufc = lex->buff + ofs;
	return 0;
}

void blexer_drop(blexer* lex)
{
	ASSERT(lex);
	if (lex->own_buff) {
		if (lex->fhandle && lex->fhandle != INVALID_HANDLE_VALUE && lex->buff) {
			VERIFY(UnmapViewOfFile(lex->buff));
		} else {
			free((void *)lex->buff);
		}
	}
	lex->own_buff = 1;
	lex->buff = lex->bufe = NULL;
	lex->line_start = lex->bufc = NULL;
	lex->curr_row = 0;
	if (lex->fmapping && lex->fmapping != INVALID_HANDLE_VALUE) {
		CloseHandle(lex->fmapping);
		lex->fmapping = NULL;
	}
	if (lex->fhandle && lex->fhandle != INVALID_HANDLE_VALUE) {
		CloseHandle(lex->fhandle);
		lex->fhandle = NULL;
	}
	lex->fsrc = NULL;
}

static inline int _alpha_token(blexer* lex, blex_token* tk)
{
	ASSERT(tk && lex && lex->bufc && *lex->bufc);
	ASSERT(lex->bufc < lex->bufe);
	tk->type = ETKT_KEY;
	tk->key = lex->bufc;
	do {
		if ( (*lex->bufc >= 'a' && *lex->bufc <= 'z')
		  || (*lex->bufc >= 'A' && *lex->bufc <= 'Z')
		  || (*lex->bufc >= '0' && *lex->bufc <= '9')
		  || *lex->bufc == '_' || *lex->bufc == '$' ) {
			++ lex->bufc;
		} else if (*lex->bufc == ':') {
			if (tk->key == NULL) {
				ASSERT(FALSE);
				tk->type = ETKT_UNDEF;
				tk->key = NULL;
				return -1;
			}
			tk->type = ETKT_LABEL;
			tk->kend = lex->bufc ++;
			return 1;
		} else {
			break;
		}
	} while (lex->bufc < lex->bufe);
	tk->kend = lex->bufc;
	if (*tk->key == '$') {
		if (tk->kend == tk->key + 1)
			tk->type = ETKT_DOLLAR;
		else if (tk->kend == tk->key + 2 && tk->key[1] == '$')
			tk->type = ETKT_DDOLL;
	}
	return 1;
}

static int _num_token(blexer* lex, blex_token* tk)
{
	ASSERT(tk && lex && lex->bufc && *lex->bufc && lex->bufc < lex->bufe);
	int dot_flag = 0;
	tk->type = ETKT_NUM;
	tk->key = lex->bufc;
	do {
		if (*lex->bufc == '.') {
			if (dot_flag) {
				ASSERT(FALSE);
				blexer_print_err(lex, "invalid dot in column (:%d)", (int)(lex->bufc - lex->line_start));
				break;
			}
			dot_flag = 1;
		} else if (*lex->bufc >= '0' && *lex->bufc <= '9') {
			/* nothing */
		} else if (*lex->bufc >= 'a' && *lex->bufc <= 'f') {
		} else if (*lex->bufc >= 'A' && *lex->bufc <= 'F') {
		} else if (*lex->bufc == 'h' || *lex->bufc == 'b') {
			++lex->bufc;
			break;
		} else if (*lex->bufc == 'x' || *lex->bufc == 'X') {
			if (tk->key + 1 == lex->bufc && tk->key[0] == '0') {
				/* nothing */
			} else {
				break;
			}
		} else {
			break;
		}
		++ lex->bufc;
	} while (lex->bufc < lex->bufe);
	tk->kend = lex->bufc;
	return 1;
}

#define ENSURE_LOCAL_BUFF  \
	if (!use_local_buff) { \
		size_t bz = min(sizeof(tk->buff), (size_t)(lex->bufc - pc)); \
		use_local_buff = 1;  \
		memcpy(tk->buff, pc, bz); \
		pc = tk->buff + bz; \
		pe = tk->buff + sizeof(tk->buff) - 1; \
	}

static int _str_token(blexer* lex, blex_token* tk, char cSep /* = '' */)
{
	ASSERT(lex && lex->bufc && *lex->bufc && lex->bufc < lex->bufe);
	ASSERT(tk && tk->buff[0] == 0);
	int use_local_buff = 0;
	char *pc = (char*)lex->bufc;
	char* pe = (char*)lex->bufe;
	tk->type = ETKT_STR;
	tk->key = lex->bufc;
	do {
		if (*lex->bufc == cSep) {
			if (lex->bufc + 1 < lex->bufe && lex->bufc[1] == cSep) {
				ENSURE_LOCAL_BUFF;
				if (pc < pe)
					*pc ++ = cSep;
				lex->bufc += 2;
				continue;
			}
			tk->kend = use_local_buff ? pc : lex->bufc;
			++ lex->bufc;
			return 1;
		} else if (*lex->bufc == '\\' && (lex->bufc +1) < lex->bufe) {
			ENSURE_LOCAL_BUFF;
			switch (lex->bufc[1]) {
			case 'r':  /* \r */
				if (pc < pe)
					*pc ++ = '\r';
				++ lex->bufc;
				break;
			case 'n':  /* \n */
				if (pc < pe)
					*pc ++ = '\n';
				++ lex->bufc;
				break;
			case 't':  /* \t */
				if (pc < pe)
					*pc ++ = '\t';
				++ lex->bufc;
				break;
			default:
				if (pc < pe)
					*pc ++ = *lex->bufc;
				break;
			}
			++ lex->bufc;
			continue;
		} else if (*lex->bufc == '\r') {
			if (lex->bufc + 1 < lex->bufe && lex->bufc[1] == '\n') {
				if (use_local_buff && pc < pe)
					*pc ++ = *lex->bufc;
				++lex->bufc;
			}
			++ lex->curr_row;
			lex->line_start = lex->bufc + 1;
		} else if (*lex->bufc == '\n') {
			++ lex->curr_row;
			lex->line_start = lex->bufc + 1;
		}
		if (use_local_buff && pc < pe)
			*pc ++ = *lex->bufc;
		++ lex->bufc;
	} while (lex->bufc < lex->bufe);
	ASSERT(FALSE);
	blexer_token_clear(tk);
	return -1;
}

static inline void _eat_mlines_comment(blexer* lex)
{
	ASSERT(lex);
	while (lex->bufc < lex->bufe) {
		if ( lex->bufc[0] == '*'
		  && (lex->bufc + 1) < lex->bufe
		  && lex->bufc[1] == '/' ) {
			lex->bufc += 2;
			return;
		} else if (lex->bufc[0] == '\r') {
			if ((lex->bufc + 1) < lex->bufe && lex->bufc[1] == '\n')
				++lex->bufc;
			++ lex->curr_row;
			lex->line_start = lex->bufc + 1;
		}
		++ lex->bufc;
	}
}

static inline void _eat_comment(blexer* lex)
{
	ASSERT(lex && lex->bufc && lex->bufe);
	while (lex->bufc < lex->bufe) {
		if (lex->bufc[0] == '\r' || lex->bufc[0] == '\n')
			return;
		++ lex->bufc;
	}
}

/* 0:end file;-1:error;1:ok */
int blexer_next(blexer* lex, blex_token* tk)
{
	ASSERT(lex && tk);
	blexer_token_clear(tk);
	while (lex->bufc < lex->bufe) {
		if ( (*lex->bufc >= 'a' && *lex->bufc <= 'z')
		  || (*lex->bufc >= 'A' && *lex->bufc <= 'Z') ) {
			return _alpha_token(lex, tk);
		} else if (*lex->bufc >= '0' && *lex->bufc <= '9') {
			return _num_token(lex, tk);
		}
		switch (*lex->bufc) {
		case '\r':
			if ((lex->bufc + 1) < lex->bufe && lex->bufc[1] == '\n')
				++ lex->bufc;
			++ lex->curr_row;
			lex->line_start = ++ lex->bufc;
			tk->type = ETKT_LNEND;
			return 1;
		case '\n':
			++ lex->curr_row;
			lex->line_start = ++ lex->bufc;
			tk->type = ETKT_LNEND;
			return 1;
		case ' ':
		case '\t':  /* ignore white-space */
			++ lex->bufc;
			break;
		case ',':
			tk->type = ETKT_SEP;
			++ lex->bufc;
			return 1;
		case ';':  /* eat comment */
			++lex->bufc;
			_eat_comment(lex);
			break;
		case '/':
			if (lex->bufc + 1 < lex->bufe) {
				if (lex->bufc[1] == '/') {
					lex->bufc += 2;
					_eat_comment(lex);
				} else if (lex->bufc[1] == '*') {
					lex->bufc += 2;
					_eat_mlines_comment(lex);
				} else {
					tk->type = ETKT_DIV;
					++ lex->bufc;
					return 1;
				}
			} else {
				tk->type = ETKT_DIV;
				++ lex->bufc;
				return 1;
			}
			break;
		case '\'':
		case '\"':
			return _str_token(lex, tk, *lex->bufc ++);
		case '_':
		case '$':
			return _alpha_token(lex, tk);
		case '.':
			tk->type = ETKT_DOT;
			++ lex->bufc;
			return 1;
		case '[':
			tk->type = ETKT_LBRACKET;
			++ lex->bufc;
			return 1;
		case ']':
			tk->type = ETKT_RBRACKET;
			++ lex->bufc;
			return 1;
		case '(':
			tk->type = ETKT_LPARENTH;
			++ lex->bufc;
			return 1;
		case ')':
			tk->type = ETKT_RPARENTH;
			++ lex->bufc;
			return 1;
		case '+':
			tk->type = ETKT_ADD;
			++ lex->bufc;
			return 1;
		case '-':
			tk->type = ETKT_SUB;
			++ lex->bufc;
			return 1;
		case '*':
			tk->type = ETKT_MUL;
			++ lex->bufc;
			return 1;
		case ':':
			tk->type = ETKT_COLON;
			++ lex->bufc;
			return 1;
		case '>':
			if (lex->bufc + 1 < lex->bufe && lex->bufc[1] == '>') {
				lex->bufc += 2;
				tk->type = ETKT_RAR;
				return 1;
			}
			blexer_print_err(lex, "invalid character '%c'(%d:%d)", *lex->bufc, lex->curr_row + 1,
				(int)(lex->bufc - lex->line_start));
			return -1;
		default:
			blexer_print_err(lex, "invalid character '%c'(%d:%d)", *lex->bufc, lex->curr_row + 1,
				(int)(lex->bufc - lex->line_start));
			return -1;
		}
	}
	ASSERT(lex->bufc == lex->bufe);
	if (lex->bufc >= lex->bufe) {
		tk->type = ETKT_END;
		return 0;
	}
	ASSERT(FALSE);
	return -1;
}

void blexer_print_errv(blexer* lex, const char* fmt, va_list vl)
{
	ASSERT(lex);
	SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), FOREGROUND_RED);
	if (lex->fsrc && *lex->fsrc)
		fprintf(stderr, "%s:%d: ", lex->fsrc, lex->curr_row + 1);
	else
		fprintf(stderr, "%d: ",  lex->curr_row + 1);
	vfprintf(stderr, fmt, vl);
	fprintf(stderr, "\n");
	++lex->errs;
	SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN);
}

void blexer_print_err(blexer* lex, const char* fmt, ...)
{
	va_list vl;
	ASSERT(lex);
	va_start(vl, fmt);
	blexer_print_errv(lex, fmt, vl);
	va_end(vl);
}
