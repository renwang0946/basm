
/* 2025.05.23 by renwang */

#include "bhash.h"


/************************************************************************/
/* Common data type hash functions                                      */
/************************************************************************/

size_t bsm_hash_i64(const void *k)
{
#ifdef __BX_X64__
	return bsm_hash_8((char *)k, sizeof(int64_t));
#else
	return bsm_hash_4((char *)k, sizeof(int64_t));
#endif/*__bsm_X64__*/
}

size_t bsm_hash_long(const void *k)
{
#ifdef __BX_X64__
	if (sizeof(long) == 4)
		return bsm_hash_4((char *)k, 4);
	return bsm_hash_8((char *)k, sizeof(long));
#else
	return bsm_hash_4((char *)k, sizeof(long));
#endif/*__BX_X64__*/
}

size_t bsm_hash_str(const void *k)
{
	const char *p = (char *)k;
	size_t ret = 2166136261U;
	if (!p || !*p)
		return 0;
	while (*p)
		ret = 16777619UL * (ret ^ (size_t)*p ++);
	return ret;
}
size_t bsm_hash_str2(const void *k)
{
	const char *p = (char *)k;
	const char *e;
	size_t len, stride;
	size_t ret = 2166136261U;
	if (!p || !*p)
		return 0;
	len = strlen(p);
	stride = 1 + len / 10;
	for (e = p+len; p < e; p += stride)
		ret = 16777619UL * (ret ^ (size_t)*p);
	return ret;
}

size_t bsm_hash_wstr(const void *k)
{
	const wchar_t *p = (wchar_t *)k;
	size_t ret = 2166136261U;
	if (!p || !*p)
		return 0;
	while (*p)
		ret = 16777619UL * (ret ^ (size_t)*p ++);
	return ret;
}
size_t bsm_hash_wstr2(const void *k)
{
	const wchar_t *p = (wchar_t *)k;
	const wchar_t *e;
	size_t len, stride;
	size_t ret = 2166136261U;
	if (!p || !*p)
		return 0;
	len = wcslen(p);
	stride = 1 + len / 10;
	for (e = p+len; p < e; p += stride)
		ret = 16777619UL * (ret ^ (size_t)*p);
	return ret;
}

size_t bsm_hash_float(const void *k)
{
	return bsm_hash_4((char *)k, sizeof(float));
}

size_t bsm_hash_double(const void *k)
{
#ifdef __bsm_X64__
	return bsm_hash_8((char *)k, sizeof(double));
#else
	return bsm_hash_4((char *)k, sizeof(double));
#endif/*__bsm_X64__*/
}

size_t bsm_hash_char(const void *k)
{
	return bsm_hash_data((char *)k, sizeof(char));
}

size_t bsm_hash_short(const void *k)
{
	return bsm_hash_data((char *)k, sizeof(short));
}
