
/* 2017.05.14 by renwang */

#ifndef __BASM_BHASH_H__
#define __BASM_BHASH_H__

#include "bglobal.h"

/* Common data type hash functions */
size_t bsm_hash_str(const void *k);
size_t bsm_hash_wstr(const void *k);
size_t bsm_hash_str2(const void *k);
size_t bsm_hash_wstr2(const void *k);

static inline size_t bsm_hash_combine(size_t h1, size_t h2) { return h1 ^ (h2 << 1); }
static inline size_t bsm_hash_i(int n) { return n * 2654435761UL; }
static inline size_t bsm_hash_f(float f)
{
	/* -0.0 and 0.0 should return same hash */
	int *n;
	if (f == 0)
		return bsm_hash_i(0);
	n = (int *)&f;
	return bsm_hash_i(*n);
}
static inline size_t bsm_hash_d(double d)
{
	/* -0.0 and 0.0 should return same hash */
	int *n1, *n2;
	if (d == 0)
		return bsm_hash_i(0);
	n1 = (int *)&d;
	n2 = n1+1;
	return bsm_hash_i(*n1) ^ (bsm_hash_i(*n2) << 1);
}

static inline size_t bsm_hash_4(const char *k, size_t len)
{
	size_t ret = 2166136261UL;
	const char *e = k + len;
	while (k < e)
		ret = 16777619UL * (ret ^ (size_t)*k++);
	return ret;
}

#ifdef _WIN64
static inline size_t bsm_hash_8(const char *k, size_t len)
{
	size_t ret = 14695981039346656037ULL;
	const char *e = k + len;
	while (k < e)
		ret = 1099511628211ULL * (ret ^ (size_t)*k++);
	return ret;
}
#endif/*_WIN64*/

static inline size_t bsm_hash_data(const char *k, size_t len)
{
	size_t ret = 0;
	const char *e = k + len;
	while (k < e)
		ret = (ret * 131) + *k ++;
	return ret;
}


/* Common hash structures */

typedef struct  {
	int n;
} bsm_int;

int bsm_compare_btint(const void *a, const void *b);

typedef struct {
	char *str;
} bsm_astr;

int bsm_compare_btastr(const void *a, const void *b);

typedef struct {
	const char *str;
} bsm_const_astr;

typedef struct {
	int n;
	char *str;
} bsm_int2astr;

/* compare struct bt_int2astr, using bsm_compare_btint */
size_t bsm_hash_int2astr(void *i2astr);
void free_int2const_astr(void *i2astr);

#endif/*__BASM_BHASH_H__*/
