
/* 2025.05.23 by Boge */

#ifndef __BASM_BHASHMAP_H__
#define __BASM_BHASHMAP_H__

/* Hash map
 * NOTE: recommend you to use unsorted_map */

typedef struct {
	const void *key;
	const void *data;
} bt_hash_entry;

typedef struct bt_hash_inentry_s  bt_hash_inentry;

typedef struct {
	bt_hash_inentry *table;
	size_t size;
	size_t filled;
	size_t grows;
	size_t (*hash)(const void *key);
	int (*compare_key)(const void *key1, const void *key2);
	void (*free_ent)(void *key, void *data);
} bt_hash_map;

/* nel: initialing hash bucket size
 * grows: re-hash growing size, -1: double initialing size; 0: don't grow
 */
MASCMM_EXT BOOL bx_hashmap_init(
	bt_hash_map *hmap, size_t nel, size_t grows,
	size_t (*hash)(const void *key),
	int (*compare_key)(const void *key1, const void *key2),
	void (*free_ent)(void *key, void *data));

MASCMM_EXT void bx_hashmap_drop(bt_hash_map *hmap);

MASCMM_EXT BOOL bx_hashmap_insert(bt_hash_map *hmap, const bt_hash_entry *item, const bt_hash_entry **retval);
MASCMM_EXT const bt_hash_entry *bx_hashmap_find(const bt_hash_map *hmap, const void *key);

MASCMM_EXT const bt_hash_entry *bx_hashmap_index(const bt_hash_map *hmap, size_t idx);

/* Please DONOT use it directly */
BOOL bx_hashmap_search(const bt_hash_entry *item, BOOL insert_action, const bt_hash_entry **retval, bt_hash_map *hmap);

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif/*__cplusplus||c_plusplus*/

#endif/*__B_HASHMAP_H__*/
