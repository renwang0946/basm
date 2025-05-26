
/* 2025.05.22 by renwang */

#ifndef __BASM_BSTREXT_H__
#define __BASM_BSTREXT_H__

#include "bglobal.h"

#define bsm_sprintf(str, bufsize, fmt, ...)   _snprintf_s((str), (bufsize), _TRUNCATE, (fmt), __VA_ARGS__)

/* safe string operation, dst_size like sizeof(char[])
 * I dislike strcpy_s(...), when the buffer is insufficient,
 * it will fail instead of truncation
 */
char* _safe_strcpy(char* dst, size_t dst_size, const char* src);

inline const char* bsm_strchr(const char* p, const char* pend, char c)
{
	ASSERT(p && pend && p < pend);
	for (; *p && p < pend; ++p) {
		if (*p == c)
			return p;
	}
	return NULL;
}

inline const char* bsm_tolower(char* str)
{
	if (str == NULL || *str == 0)
		return str;
#ifdef _MSC_VER
	_strlwr_s(str, strlen(str) + 1);
	return str;
#else
	return strlwr(str);
#endif/*_MSC_VER*/
}

#endif/*__BASM_BSTREXT_H__*/
