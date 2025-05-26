
/* 2025.05.23 by renwang */

#include "bbuiltin.h"
#include "bstrext.h"
#include "bstream.h"
#include "bunsorted_map.h"

/* Intel x86 instruction */

/*
-------------------------------------------------------------------------
instruction | Opcode | ModeR/M | SIB   | Displacement        | Immediate |
prefixes    |        |         |       |                     |           |
-------------------------------------------------------------------------
1-4 bytes    1-2 byte  1 byte   1 byte   1,2,4 bytes or none   1,2,4 bytes or none

ModeR/M
7  6 5          3  2  0
-----------------------
Mod | Reg/Opcode | R/M |
-----------------------

SIB
7    6  5    3  2   0
---------------------
Scale | Index | Base |
---------------------
*/

/* Register names */
static const char* _REGS_X86[] = {
	"AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH",
	"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI",
};
static const char* _REGS_32[] = {
	"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI",
};
static const char* _REGS_64[] = {
	"RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI",
};
static const char* _SEG_REGS[] = {
	"ES", "CS", "SS", "DS", "FS", "GS",
};

/* Segment override prefixes
 * p36 in Intel(R) 64 and IA-32 Architectures Software Developer's Manual, Volume 2 (2A & 2B)
 */
static const uint8_t _SEG_PREFIXS[] = {
	0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65,
};

static size_t _PTR_SIZES[] = {
	8, 16, 32, 64, 128
};

breg_t _try_reg_x86(const char* reg_name, const char* pend)
{
	size_t i;
	if (reg_name == NULL || *reg_name == 0 || pend - reg_name != 2)
		return ERNM_UNDEF;
	for (i = 0; i < _countof(_REGS_X86); ++i) {
		if (!_strnicmp(reg_name, _REGS_X86[i], 2))
			return (breg_t)(ERNM_AL + i);
	}
	return ERNM_UNDEF;
}

breg_t _try_reg_32(const char* reg_name, const char* pend)
{
	size_t i;
	if (reg_name == NULL || *reg_name == 0 || pend - reg_name != 3)
		return ERNM_UNDEF;
	for (i = 0; i < _countof(_REGS_32); ++i) {
		if (!_strnicmp(reg_name, _REGS_32[i], 3))
			return (breg_t)(ERNM_EAX + i);
	}
	return ERNM_UNDEF;
}

breg_t _try_reg_64(const char* reg_name, const char* pend)
{
	size_t i;
	if (reg_name == NULL || *reg_name == 0 || pend - reg_name != 3)
		return ERNM_UNDEF;
	for (i = 0; i < _countof(_REGS_64); ++i) {
		if (!_strnicmp(reg_name, _REGS_64[i], 3))
			return (breg_t)(ERNM_RAX + i);
	}
	return ERNM_UNDEF;
}

size_t _reg_size(breg_t r)
{
	if (r == ERNM_UNDEF) {
		ASSERT(FALSE);
		return 0;
	}
	if (r < ERNM_AX)
		return 8;
	else if (r < ERNM_EAX)
		return 16;
	else if (r < ERNM_RAX)
		return 32;
	return 64;
}

breg_t _try_seg_reg(const char* seg_name, const char* pend)
{
	size_t i;
	if (seg_name == NULL || *seg_name == 0 || pend - seg_name != 2)
		return ESNM_UNDEF;
	for (i = 0; i < _countof(_SEG_REGS); ++i) {
		if (!_strnicmp(seg_name, _SEG_REGS[i], 2))
			return (breg_t)(ESNM_ES + i);
	}
	return ESNM_UNDEF;
}

const char* _reg_name(breg_t r)
{
	if (r <= ERNM_UNDEF || r > ERNM_RDI) {
		ASSERT(FALSE);
		return "[unknown reg]";
	}
	if (r < ERNM_EAX)
		return _REGS_X86[r - ERNM_AL];
	else if (r < ERNM_RAX)
		return _REGS_32[r - ERNM_EAX];
	return _REGS_64[r - ERNM_RAX];
}
const char* _seg_name(bseg_t seg)
{
	if (seg <= ESNM_UNDEF || seg > ESNM_GS) {
		ASSERT(FALSE);
		return "[unknown seg]";
	}
	return _SEG_REGS[seg - 1];
}
uint8_t _seg_prefix(bseg_t seg)
{
	if (seg <= ESNM_UNDEF) {
		ASSERT(FALSE);
		return 0;
	}
	return _SEG_PREFIXS[seg - 1];
}

size_t _ptr_mode_size(uint16_t ptrs)
{
	ptrs &= 0x00FF;
	if (ptrs == EPTR_UNDEF)
		return 0;
	return _PTR_SIZES[ptrs - 1];
}

/* built in functions */

#if 0
/* like: mov r, r/m8 */
int _builtin_rr8(bstream* stm, breg_t r, breg_t rm)
{
	uint8_t opc, c;
	ASSERT(_reg_size(r) == _reg_size(rm) && _reg_size(r) == 8);
	opc = (r - ERNM_AL) << 3;
	c = 0xC0 | opc | (rm - ERNM_AL);  /* mod = 11b */
	bstream_putc(stm, c);
	return 0;
}

/* like: mov r, r / m16 */
int _builtin_rr16(bstream* stm, breg_t r, breg_t rm)
{
	/* ModR / M */
	uint8_t opc, c;
	ASSERT(_reg_size(r) == _reg_size(rm) && _reg_size(r) == 16);
	opc = (r - ERNM_AX) << 3;
	c = 0xC0 | opc | (rm - ERNM_AX);  /* mod = 11b */
	bstream_putc(stm, c);
	return 0;
}

/* like: mov r, r/m32 */
int _builtin_rr32(bstream* stm, breg_t r, breg_t rm)
{
	/* ModR / M */
	uint8_t opc, c;
	ASSERT(_reg_size(r) == _reg_size(rm) && _reg_size(r) == 32);
	opc = (r - ERNM_EAX) << 3;
	c = 0xC0 | opc | (rm - ERNM_EAX);  /* mod = 11b */
	bstream_putc(stm, c);
	return 0;
}

int _builtin_rr64(bstream* stm, breg_t r, breg_t rm)
{
	uint8_t opc, c;
	ASSERT(_reg_size(r) == _reg_size(rm) && _reg_size(r) == 64);
	opc = (r - ERNM_RAX) << 3;
	c = 0xC0 | opc | (rm - ERNM_RAX);  /* mod = 11b */
	bstream_putc(stm, c);
	return 0;
}
#endif/*0*/

static inline int _parse_hex(const char* p, const char* pend, bnum_val* nv)
{
	uint64_t v = 0;
	ASSERT(nv);
	while (p < pend) {
		uint8_t c = *(uint8_t *)p;
		if (c >= 'a' && c <= 'f')
			v = v * 16 + (c - 'a' + 0x0a);
		else if (c >= 'A' && c <= 'F')
			v = v * 16 + (c - 'A' + 0x0a);
		else if (c >= '0' && c <= '9')
			v = v * 16 + (c - '0');
		else {
			ASSERT(FALSE);
			return -1;
		}
		++p;
	}
	if (v > INT_MAX) {
		nv->type = ENVT_INT64;
		nv->i64 = v;
	} else {
		nv->type = ENVT_INT;
		nv->i = (int32_t)v;
	}
	return 0;
}

int bsm_parse_num(const char* p, const char* pend, bnum_val* nv)
{
	ASSERT(nv);
	if (p == NULL || *p == 0 || pend <= p)
		return -1;
	if (!bsm_strchr(p, pend, '.')) {
		uint64_t v = 0;
		if (pend - p > 2 && !_strnicmp(p, "0x", 2)) {
			if (pend[-1] == 'h' || pend[-1] == 'H')
				-- pend;
			if (pend - p <= 2)
				return -1;
			return _parse_hex(p + 2, pend, nv);
		}
		if (pend[-1] == 'h' || pend[-1] == 'H') {
			if (--pend <= p)
				return -1;
			return _parse_hex(p, pend, nv);
		} else if (pend[-1] == 'b' || pend[-1] == 'B') {
			ASSERT(FALSE);  /* !!!TODO */
			return -1;
		}
		while (p < pend) {
			uint8_t c = *(uint8_t*)p;
			v = v * 10 + (c - '0');
			++p;
		}
		if (v > INT_MAX) {
			nv->type = ENVT_INT64;
			nv->i64 = v;
		} else {
			nv->type = ENVT_INT;
			nv->i = (int32_t)v;
		}
		return 0;
	}
	double v = 0;
	ASSERT(FALSE);
	return -1;
}

/* jcc */
static bjcc_cond_elem_t _JCC_COND_ELEMS[] = {
	{ "a",    0, 0x77 },  /* ja, above, cf=0 and zf=0 */
	{ "ae",   0, 0x73 },  /* jae, above or equal, cf=0 */
	{ "b",    0, 0x72 },  /* jb, below, cf=1  */
	{ "be",   0, 0x76 },  /* jbe, below or equal, cf=1 or zf=1 */
	{ "c",    0, 0x72 },  /* jc, like jb */
	{ "cxz",  1, 0xE3 },  /* jcxz, cx=0, illegal in 64-bit mode */
	{ "ecxz", 2, 0xE3 },  /* jecxz, ecx=0 */
	{ "rcxz", 3, 0xE3 },  /* jrcxz, rcx=0 */
	{ "e",    0, 0x74 },  /* je, equal, zf=1 */
	{ "g",    0, 0x7F },  /* jg, greater, zf=0 and sf=of */
	{ "ge",   0, 0x7D },  /* jge, greater or equal, sf=of */
	{ "l",    0, 0x7C },  /* jl, less, sf != of */
	{ "le",   0, 0x7E },  /* jle, less or equal zf=1 or sf != of */
	{ "na",   0, 0x76 },  /* jna, like jbe, not above, below or equal, zf=1 or cf=1 */
	{ "nae",  0, 0x72 },  /* jnae, like jb, cf=1 */
	{ "nb",   0, 0x73 },  /* jnb, like jae, cf=0 */
	{ "nbe",  0, 0x77 },  /* jnbe, like ja, zf=0 and cf=0 */
	{ "nc",   0, 0x73 },  /* jnc, like jae, cf=0 */
	{ "ne",   0, 0x75 },  /* jne, not equal, zf=0 */
	{ "ng",   0, 0x7E },  /* jng, like jle, zf=1 or sf != of */
	{ "nge",  0, 0x7C },  /* jnge, like jl, sf != of */
	{ "nl",   0, 0x7D },  /* jnl, like jge, sf=of */
	{ "nle",  0, 0x7F },  /* jnle, like jg, zf=0 and sf=of */
	{ "no",   0, 0x71 },  /* jno, of=0 */
	{ "np",   0, 0x7B },  /* jnp, pf=0 */
	{ "ns",   0, 0x79 },  /* jns, sf=0 */
	{ "nz",   0, 0x75 },  /* jnz, like jne, zf=0 */
	{ "o",    0, 0x70 },  /* jo, of=1  */
	{ "p",    0, 0x7A },  /* jp, pf=1  */
	{ "pe",   0, 0x7A },  /* jpe, pf=1 */
	{ "po",   0, 0x7B },  /* jpo, pf=0 */
	{ "s",    0, 0x78 },  /* js, sf=1  */
	{ "z",    0, 0x74 },  /* jz, like je, zf=1 */
};

static int _compare_jcc_elem(const bjcc_cond_elem_t* l, const bjcc_cond_elem_t* r)
{
	ASSERT(l && r);
	return strcmp(l->jcc, r->jcc);
}

int _jcc_elems_init()
{
	qsort(_JCC_COND_ELEMS, _countof(_JCC_COND_ELEMS), sizeof(bjcc_cond_elem_t), _compare_jcc_elem);
	return 0;
}

const bjcc_cond_elem_t* _find_jcc_elem(const char* jcc, size_t len)
{
	char jname[256];
	if (jcc == NULL || *jcc == 0 || len == 0) {
		ASSERT(FALSE);
		return NULL;
	}
	ASSERT(sizeof(jname) > len);
	_safe_strcpy(jname, min(sizeof(jname), len + 1), jcc);
	bsm_tolower(jname);
	bjcc_cond_elem_t tmp = { jname };
	return bsearch(&tmp, _JCC_COND_ELEMS, _countof(_JCC_COND_ELEMS),
		sizeof(bjcc_cond_elem_t), _compare_jcc_elem);
}
