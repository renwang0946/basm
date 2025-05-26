
/* 2025.05.23 by renwang */

#ifndef __BASM_BUNSORTED_MAP_H__
#define __BASM_BUNSORTED_MAP_H__

#include "bglobal.h"

/* unsorted map */
typedef struct {
	void *table;
	size_t size;
	size_t elemsize;
	size_t filled;
	size_t grows;
	size_t (*hash)(const void *elem);
	int (*compare_elem)(const void *elem1, const void *elem2);
	void (*free_elem)(void *elem);
} bunsorted_map;


/* nel: initialing hash bucket size
 * grows: re-hash growing size, -1: double initialing size; 0: don't grow
 */
int bunsorted_map_init(
	bunsorted_map*usmap, size_t nel, size_t elemsize, size_t grows,
	size_t (*hash)(const void *elem),
	int (*compare_elem)(const void *elem1, const void *elem2),
	void (*free_elem)(void *elem));

void bunsorted_map_drop(bunsorted_map*usmap);

/* retelem can NULL */
int bunsorted_map_insert(bunsorted_map*usmap, const void *elem, const void **retelem);
const void* bunsorted_map_find(const bunsorted_map*usmap, const void *elem);

const void* bunsorted_map_index(const bunsorted_map*usmap, size_t idx);

/* Please DONOT use it directly
 * insert_action:boolean */

int _bunsorted_map_search(const void *elem, int insert_action, const void **retelem, bunsorted_map*usmap);

#endif/*__BASM_BUNSORTED_MAP_H__*/
