
/* 2025.05.24 by renwang */

#ifndef __BASM_BARRAY_H__
#define __BASM_BARRAY_H__

#include "bglobal.h"

#define _ENSURE_ARR_CAP(arr, tp, elemsize)                        \
  if (arr->cnt == arr->cap) {                                     \
    arr->cap = barr_normal_cap(arr->cnt);                         \
    arr->elems = (tp)realloc(arr->elems, (elemsize) * arr->cap);  \
    ASSERT(arr->elems);                                           \
  }
#define _ENSURE_ARR_CAP2(arr, newsize, tp, elemsize)              \
  if (arr->cap < newsize) {                                       \
    arr->cap = barr_normal_cap(newsize);                          \
    arr->elems = (tp)realloc(arr->elems, (elemsize) * arr->cap);  \
    ASSERT(arr->elems);                                           \
  }
#define _EMPTY_ARR(arr)  ((arr)->cnt == 0)

inline size_t barr_normal_cap(size_t count)
{
	ASSERT(count < (1 << 29));  /* huge data in memory */
	return count < 8 ? 8 : ((count * 3) >> 1);
}

size_t bsm_sort_insert_idx(const void* key, const void* base, size_t elems, size_t sizeofelem,
	int (*fncompare)(const void*, const void*), BOOL* pfound);

size_t bsm_sort_insert(void* key, const void* base, size_t elems, size_t sizeofelem,
	int (*fncompare)(const void*, const void*));

/*
 * Point array
 */
typedef struct {
	void **elems;
	size_t cnt;
	size_t cap;
	void (*free_elem)(void *);  /* NOTE: you MUST maintains the element self pointer */
} bparr;

void bparr_init(bparr* arr, void (*free_elem)(void *)/* = free*/, size_t initcap/* = 0*/);
void bparr_drop(bparr* arr);

inline void *bparr_get(bparr*arr, size_t idx)
{
	ASSERT(arr);
	if (idx >= arr->cnt || !arr->elems) {
		ASSERT(FALSE);
		return NULL;
	}
	return arr->elems[idx];
}

inline void bparr_push(bparr* arr, void *p)
{
	ASSERT(arr);
	_ENSURE_ARR_CAP(arr, void **, sizeof(void *));
	arr->elems[arr->cnt ++] = p;
}

void bparr_insert(bparr *arr, size_t idx, void *p);
int bparr_erase(bparr *arr, size_t idx);

inline void bparr_clear(bparr *arr, int tozero/* = false*/)
{
	ASSERT(arr);
	if (arr->elems && tozero)
		bparr_drop(arr);
	else
		arr->cnt = 0;
}

inline void *bparr_pop(bparr *arr)
{
	ASSERT(arr);
	return arr->cnt > 0 ? arr->elems[-- arr->cnt] : NULL;
}

inline void bparr_pop_back(bparr *arr)
{
	ASSERT(arr && arr->cnt > 0);
	if (arr->cnt > 0)
		-- arr->cnt;
}

inline void *bparr_back(bparr *arr)
{
	ASSERT(arr && arr->cnt > 0);
	if (arr->cnt > 0)
		return arr->elems[arr->cnt-1];
	return NULL;
}

inline void bparr_deep_assign(const bparr *arrsrc, size_t elemsize, bparr *arrdst)
{
	size_t i;
	ASSERT(arrsrc && arrdst);
	bparr_clear(arrdst, TRUE);
	bparr_init(arrdst, arrsrc->free_elem, arrsrc->cnt);
	for (i = 0; i < arrsrc->cnt; ++ i) {
		void * ptr = malloc(elemsize);
		memcpy(ptr, arrsrc->elems[i], elemsize);
		arrdst->elems[i] = ptr;
	}
}


/*
 * Value array
 */
typedef struct  {
	void *elems;
	size_t elemsize;
	size_t cnt;
	size_t cap;
} barray;

void barr_init(barray *arr, size_t elemsize, size_t initcap/* = 0*/);
void barr_drop(barray *arr);

inline void *barr_get(barray *arr, size_t idx)
{
	ASSERT(arr);
	if (idx >= arr->cnt) {
		ASSERT(FALSE);
		return NULL;
	}
	return (char *)arr->elems + (idx * arr->elemsize);
}

inline void* barr_set(barray *arr, size_t idx, const void *elem)
{
	ASSERT(arr && elem);
	char* ptr;
	if (idx >= arr->cnt) {
		ASSERT(FALSE);
		return NULL;
	}
	ptr = (char*)arr->elems + idx * arr->elemsize;
	return memcpy(ptr, elem, arr->elemsize);
}

inline void* barr_add(barray *arr, const void *elem)
{
	char* ptr;
	ASSERT(arr && arr->elemsize > 0 && elem);
	_ENSURE_ARR_CAP(arr, void*, arr->elemsize);
	if (!arr->elems) {
		ASSERT(FALSE);
		return NULL;
	}
	ptr = (char *)arr->elems + arr->elemsize * arr->cnt;
	memcpy(ptr, elem, arr->elemsize);
	++ arr->cnt;
	return ptr;
}

#define barr_push  barr_add

inline void *barr_push_empty(barray *arr)
{
	char *p;
	ASSERT(arr && arr->elemsize > 0);
	_ENSURE_ARR_CAP(arr, void *, arr->elemsize);
	if (!arr->elems) {
		ASSERT(FALSE);
		return NULL;
	}
	p = (char *)arr->elems + arr->elemsize*arr->cnt;
	memset(p, 0, arr->elemsize);
	++ arr->cnt;
	return p;
}

void* barr_insert(barray *arr, size_t idx, const void *elem);
int barr_erase(barray *arr, size_t idx);

inline void barr_clear(barray *arr, int tozero/* = false*/)
{
	ASSERT(arr);
	if (arr->elems && tozero)
		barr_drop(arr);
	else
		arr->cnt = 0;
}

inline void barr_pop_back(barray *arr)
{
	ASSERT(arr && arr->cnt > 0);
	if (arr->cnt > 0)
		-- arr->cnt;
}

inline void *barr_back(barray *arr)
{
	ASSERT(arr && arr->cnt > 0);
	return (char *)arr->elems + (arr->cnt-1)*arr->elemsize;
}

int bsm_always_zero();

/* append: bsm_always_zero or others */
void barr_resize(barray *arr, size_t newsize, const void *append);

/*
 * Integer array
 */
typedef struct {
	int *elems;
	size_t cnt;
	size_t cap;
} bintarr;

void bintarr_init(bintarr *arr, size_t initcap/* = 0*/);
void bintarr_drop(bintarr *arr);

inline void bintarr_add(bintarr *arr, int x)
{
	ASSERT(arr);
	_ENSURE_ARR_CAP(arr, int*, sizeof(int));
	arr->elems[arr->cnt ++] = x;
}

void bintarr_insert(bintarr *arr, size_t idx, int x);
int bintarr_erase(bintarr *arr, size_t idx);

inline void bintarr_clear(bintarr *arr, int tozero/* = false*/)
{
	ASSERT(arr);
	if (arr->elems && tozero)
		bintarr_drop(arr);
	else
		arr->cnt = 0;
}

#define bintarr_push  bintarr_add

inline int bintarr_back(bintarr* arr)
{
	ASSERT(arr && arr->cnt > 0);
	return arr->elems[arr->cnt - 1];
}
inline int* bintarr_back_ref(bintarr* arr)
{
	ASSERT(arr && arr->cnt > 0);
	return &arr->elems[arr->cnt - 1];
}

inline void bintarr_pop_back(bintarr *arr)
{
	ASSERT(arr && arr->cnt > 0);
	if (arr->cnt > 0)
		-- arr->cnt;
}

#endif/*__BASM_BARRAY_H__*/
