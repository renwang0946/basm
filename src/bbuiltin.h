
/* 2025.05.23 by renwang */

#ifndef __BASM_BBUILTIN_H__
#define __BASM_BBUILTIN_H__

#include "bglobal.h"

typedef enum
{
	ERNM_UNDEF,
	ERNM_AL,
	ERNM_CL,
	ERNM_DL,
	ERNM_BL,
	ERNM_AH,
	ERNM_CH,
	ERNM_DH,
	ERNM_BH,

	ERNM_AX,
	ERNM_CX,
	ERNM_DX,
	ERNM_BX,
	ERNM_SP,
	ERNM_BP,
	ERNM_SI,
	ERNM_DI,

	ERNM_EAX,
	ERNM_ECX,
	ERNM_EDX,
	ERNM_EBX,
	ERNM_ESP,
	ERNM_EBP,
	ERNM_ESI,
	ERNM_EDI,

	ERNM_RAX,
	ERNM_RCX,
	ERNM_RDX,
	ERNM_RBX,
	ERNM_RSP,
	ERNM_RBP,
	ERNM_RSI,
	ERNM_RDI,
} breg_t;

typedef enum
{
	ESNM_UNDEF,
	ESNM_ES,
	ESNM_CS,
	ESNM_SS,
	ESNM_DS,
	ESNM_FS,
	ESNM_GS,
} bseg_t;

typedef enum
{
	ENVT_INT,
	ENVT_INT64,
	ENVT_FLOAT,
} bnum_val_t;

typedef struct
{
	bnum_val_t type;
	union {
		int32_t i;
		int64_t i64;
		double d;
	};
} bnum_val;

typedef enum
{
	EPTR_UNDEF,
	EPTR_BYTE,
	EPTR_WORD,
	EPTR_DWORD,
	EPTR_QWORD,
	EPTR_FWORD,  /* fword, 48bit,16:32 offset */
	EPTR_OWORD,
	EPTR_NEAR  = 0x0100,
	EPTR_FAR   = 0x0200,
	EPTR_SHORT = 0x0400,
	EPTR_REL_MASK = 0xFF00,
} bptr_mode_t;

inline int _has_far_ptr(uint16_t ptrs)
{
	return (ptrs & EPTR_FAR) == EPTR_FAR;
}
inline int _has_ptr_mode(uint16_t ptrs, bptr_mode_t pm)
{
	if (pm >= EPTR_NEAR)
		return (ptrs & pm) == pm;
	return (ptrs & 0x00FF) == pm;
}
inline int _has_ptr_size(uint16_t ptrs)
{
	return (ptrs & 0x00FF) != 0;
}

typedef struct
{
	const char* jcc;
	uint8_t opt;   /* option */
	uint8_t op8;
} bjcc_cond_elem_t;

int _jcc_elems_init();
const bjcc_cond_elem_t* _find_jcc_elem(const char* jcc, size_t len);


/*
 * built in functions
 */
breg_t _try_reg_x86(const char* reg_name, const char* pend);
breg_t _try_reg_32(const char* reg_name, const char* pend);   /* 32-bits register */
breg_t _try_reg_64(const char* reg_name, const char* pend);
breg_t _try_seg_reg(const char* reg_name, const char* pend);

size_t _reg_size(breg_t r);
const char* _reg_name(breg_t r);
const char* _seg_name(breg_t eSeg);
uint8_t _seg_prefix(breg_t eSeg);

size_t _ptr_mode_size(uint16_t ptrs);

#if 0
typedef struct _bstream bstream;
int _builtin_rr8(bstream* stm, breg_t r, breg_t rm);
int _builtin_rr16(bstream* stm, breg_t r, breg_t rm);
int _builtin_rr32(bstream* stm, breg_t r, breg_t rm);
int _builtin_rr64(bstream* stm, breg_t r, breg_t rm);
#endif/*0*/

int bsm_parse_num(const char* p, const char* pend, bnum_val* val);

/*
 * inlines
 */
inline breg_t _reg_base_frsize(size_t rz)
{
	if (rz == 8)
		return ERNM_AL;
	else if (rz == 16)
		return ERNM_AX;
	else if (rz == 32)
		return ERNM_EAX;
	else if (rz == 64)
		return ERNM_RAX;
	ASSERT(FALSE);
	return ERNM_UNDEF;
}

#endif/*__BASM_BBUILTIN_H__*/
