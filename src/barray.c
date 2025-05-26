
/* 2025.05.24 by renwang */

#include "barray.h"

size_t bsm_sort_insert_idx(const void *key, const void *base, size_t elems, size_t sizeofelem, 
	int (*fncompare)(const void *, const void *), BOOL *pfound)
{
	int l = 0, r, m, c;
	ASSERT(key && base && sizeofelem > 0 && fncompare);
	r = (int)(elems - 1);
	while (l <= r) {
		m = (l + r) >> 1;
		c = fncompare(key, (char *)base + m*sizeofelem);
		if (c < 0) {
			r = m - 1;
		} else if (c > 0) {
			l = m + 1;
		} else {
			if (pfound)
				*pfound = TRUE;
			return m;
		}
	}
	ASSERT(l-r == 1);
	if (pfound)
		*pfound = FALSE;
	return l;
}

size_t bsm_sort_insert(void *key, const void *base, size_t elems, size_t sizeofelem, 
	int (*fncompare)(const void *, const void *))
{
	int l = 0, r, m, c;
	char *p;
	ASSERT(key && base && sizeofelem > 0 && fncompare);
	r = (int)(elems - 1);
	while (l <= r) {
		m = (l + r) >> 1;
		c = fncompare(key, (char *)base + m*sizeofelem);
		if (c < 0) {
			r = m - 1;
		} else if (c > 0) {
			l = m + 1;
		} else {
			l = m;
			goto __CPY_ELEM;
		}
	}
	ASSERT(l-r == 1);
__CPY_ELEM:
	p = (char *)base + l*sizeofelem;
	if (l < (int)elems)
		memmove(p + sizeofelem, p, (elems-1-l)*sizeofelem);
	memcpy(p, key, sizeofelem);
	return l;
}


/*
 * Pointer array action
 */
void bparr_init(bparr *arr, void (*free_elem)(void *)/* = free*/, size_t initcap/* = 0*/)
{
	ASSERT(arr);
	arr->elems = NULL;
	arr->cnt = 0;
	arr->cap = 0;
	if (initcap > 0) {
		arr->elems = calloc(initcap, sizeof(void *));
		ASSERT(arr->elems);
	}
	arr->free_elem = free_elem;
}

void bparr_drop(bparr *arr)
{
	ASSERT(arr);
	if (arr->elems) {
		while (arr->cnt --) {
			if (arr->free_elem)
				(*arr->free_elem)(arr->elems[arr->cnt]);
		}
		free(arr->elems);
		arr->elems = NULL;
	}
	arr->cnt = 0;
	arr->cap = 0;
}

void bparr_insert(bparr *arr, size_t idx, void *p)
{
	ASSERT(arr);
	if (idx >= arr->cnt) {
		bparr_push(arr, p);
		return;
	}
	_ENSURE_ARR_CAP(arr, void **, sizeof(void *));
	memmove(arr->elems + idx + 1, arr->elems + idx, (arr->cnt - idx) * sizeof(void *));
	arr->elems[idx] = p;
	++ arr->cnt;
}

BOOL bparr_erase(bparr *arr, size_t idx)
{
	ASSERT(arr && idx < arr->cnt);
	if (arr->free_elem)
		(*arr->free_elem)(arr->elems[idx]);
	if (idx >= arr->cnt-1) {
		if (arr->cnt > 0)
			-- arr->cnt;
		return TRUE;
	}
	memmove(arr->elems + idx, arr->elems + idx + 1, (arr->cnt - idx - 1) * sizeof(void *));
	-- arr->cnt;
	return TRUE;
}


/*
 * Value array
 */
void barr_init(barray *arr, size_t elemsize, size_t initcap/* = 0*/)
{
	ASSERT(arr && elemsize > 0);
	arr->elems = NULL;
	arr->elemsize = elemsize;
	arr->cnt = 0;
	arr->cap = initcap;
	if (initcap > 0) {
		arr->elems = calloc(initcap, elemsize);
		ASSERT(arr->elems);
	}
}

void barr_drop(barray *arr)
{
	ASSERT(arr);
	free(arr->elems);
	arr->elems = NULL;
	arr->cnt = 0;
	arr->cap = 0;
}

void* barr_insert(barray *arr, size_t idx, const void *elem)
{
	char* ptr;
	ASSERT(arr);
	if (idx >= arr->cnt)
		return barr_add(arr, elem);
	_ENSURE_ARR_CAP(arr, void *, arr->elemsize);
	ptr = (char *)arr->elems + idx*arr->elemsize;
	memmove(ptr + arr->elemsize, ptr, (arr->cnt - idx) * arr->elemsize);
	memcpy(ptr, elem, arr->elemsize);
	++ arr->cnt;
	return ptr;
}

int barr_erase(barray *arr, size_t idx)
{
	char *ptrd = (char *)arr->elems + idx * arr->elemsize;
	ASSERT(arr && idx < arr->cnt);
	if (idx >= arr->cnt-1) {
		if (arr->cnt > 0)
			-- arr->cnt;
		return 0;
	}
	memmove(ptrd, ptrd + arr->elemsize, (arr->cnt - idx - 1) * arr->elemsize);
	-- arr->cnt;
	return 0;
}


/*
 * Integer array
 */
void bintarr_init(bintarr *arr, size_t initcap/* = 0*/)
{
	ASSERT(arr);
	arr->elems = NULL;
	arr->cnt = 0;
	arr->cap = initcap;
	if (initcap > 0) {
		arr->elems = calloc(initcap, sizeof(int));
		ASSERT(arr->elems);
	}
}

void bintarr_drop(bintarr *arr)
{
	ASSERT(arr);
	free(arr->elems);
	arr->elems = NULL;
	arr->cnt = 0;
	arr->cap = 0;
}

void bintarr_insert(bintarr *arr, size_t idx, int x)
{
	ASSERT(arr);
	if (idx >= arr->cnt) {
		bintarr_add(arr, x);
		return;
	}
	_ENSURE_ARR_CAP(arr, int *, sizeof(int));
	memmove(arr->elems + idx + 1, arr->elems + idx, (arr->cnt - idx) * sizeof(int));
	arr->elems[idx] = x;
	++ arr->cnt;
}

int bintarr_erase(bintarr *arr, size_t idx)
{
	ASSERT(arr && idx < arr->cnt);
	if (idx >= arr->cnt-1) {
		if (arr->cnt > 0)
			-- arr->cnt;
		return 0;
	}
	memmove(arr->elems + idx, arr->elems + idx + 1, (arr->cnt - idx - 1) * sizeof(int));
	-- arr->cnt;
	return 0;
}

int bsm_always_zero() { return 0; }

void barr_resize(barray *arr, size_t newsize, const void *append)
{
	ASSERT(arr && arr->cnt <= arr->cap);
	if (arr->cap < newsize) {
		arr->elems = realloc(arr->elems, newsize * arr->elemsize);
		ASSERT(arr->elems);
	}
	if (append) {
		uint8_t *p = (uint8_t *)arr->elems + arr->cnt * arr->elemsize;
		size_t appends = newsize - arr->cnt;
		if (append == bsm_always_zero) {
			memset(p, 0, appends*arr->elemsize);
		} else {
			size_t i;
			for (i = 0; i < appends; ++ i) {
				memcpy(p, append, arr->elemsize);
				p += arr->elemsize;
			}
		}
	}
	arr->cap = newsize;
	arr->cnt = newsize;
}
