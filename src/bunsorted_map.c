
/* 2025.05.23 by renwang */

#include "bunsorted_map.h"
#include <errno.h>
#include <stdlib.h>

/* unsorted map */

/* The reentrant version has no static variables to maintain the state.
 * Instead the interface of all functions is extended to take an argument
 * which describes the current status.
 */
typedef struct {
	size_t used;
	char elem[1];
} _unsorted_inentry;

/* For the used double hash method the table size has to be a prime. To
   correct the user given table size we need a prime test. This trivial
   algorithm is adequate because
   a)  the code is (most probably) called a few times per program run and
   b)  the number is small because the table must fit in the core  */
static inline int _usmap_isprime(size_t number)
{
	/* no even number will be passed */
	size_t div;
	for (div = 3; div <= number / div; div += 2) {
		if (number % div == 0)
			return -1;
	}
	return 0;
}

static inline size_t _usmap_inentry_size(const bunsorted_map *usmap)
{
	ASSERT(usmap);
	return sizeof(size_t) + usmap->elemsize;
}

int bunsorted_map_init(
	bunsorted_map *usmap, size_t nel, size_t elemsize, size_t grows,
	size_t (*hash)(const void *elem),
	int (*compare_elem)(const void *elem1, const void *elem2),
	void (*free_elem)(void *elem)
)
{
	/* Test for correct arguments. */
	ASSERT(hash && compare_elem && usmap && !usmap->table);
	if (!usmap) {
		errno = EINVAL;
		return -1;
    }
	if (usmap->table)  /* There is still another table active. Return with error. */
		return -1;

	/* We need a size of at least 3.  Otherwise the hash functions we
	 * use will not work.
	 */
	if (nel < 3)
		nel = 3;

	/* Change nel to the first prime number in the range [nel, UINT_MAX - 2],
	 * The '- 2' means 'nel += 2' cannot overflow. 
	 */
	for (nel |= 1; ; nel += 2) {
		if (UINT_MAX - 2 < nel) {
			errno = ENOMEM;
			return -1;
		}
		if (_usmap_isprime(nel))
			break;
    }
	usmap->size = nel;
	usmap->elemsize = elemsize;
	usmap->filled = 0;
	usmap->grows = grows;
	usmap->hash = hash;
	usmap->compare_elem = compare_elem;
	usmap->free_elem = free_elem;

	/* allocate memory and zero out */
	usmap->table = (_unsorted_inentry *)calloc(usmap->size + 1, sizeof(size_t) + usmap->elemsize);
	if (usmap->table == NULL)
		return -1;

	/* everything went alright */
	return 0;
}

/* After using the hash table it has to be destroyed. The used memory can
   be freed and the local static variable can be marked as not used.  */
void bunsorted_map_drop(bunsorted_map *usmap)
{
	/* Test for correct arguments.  */
	if (!usmap) {
		errno = EINVAL;
		return;
    }
	if (usmap->free_elem) {
		size_t i;
		size_t inent_size = _usmap_inentry_size(usmap);
		char *p = (char *)usmap->table;
		for (i = 0; i <= usmap->size; ++ i, p += inent_size) {
			_unsorted_inentry *inent = (_unsorted_inentry *)p;
			if (inent->used) {
				usmap->free_elem(inent->elem);
				inent->used = 0;
			}
		}
	}
	free(usmap->table);  /* Free used memory. */

	/* the sign for an existing table is an value != NULL in htable */
	usmap->table = NULL;
}

static int _re_unsorted_map(bunsorted_map *usmap)
{
	bunsorted_map nmap = { 0 };
	char *p;
	size_t i;
	size_t inent_size = _usmap_inentry_size(usmap);
	const void *retent;
	ASSERT(usmap && usmap->grows);
	VERIFY(bunsorted_map_init(&nmap, usmap->grows == (size_t)-1 ? (usmap->size << 1) : usmap->size+usmap->grows,
		usmap->elemsize, usmap->grows, usmap->hash, usmap->compare_elem, usmap->free_elem));

	p = (char *)usmap->table;
	for (i = 0; i <= usmap->size; ++ i, p += inent_size) {
		_unsorted_inentry *inent = (_unsorted_inentry *)p;
		if (inent->used == 0)
			continue;
		if (_bunsorted_map_search(inent->elem, 1/*TRUE*/, &retent, &nmap)) {
			ASSERT(FALSE);
			if (usmap->free_elem)
				usmap->free_elem(inent->elem);
		}
	}
	free(usmap->table);
	memcpy(usmap, &nmap, sizeof(*usmap));
	return 0;
}

/* This is the search function. It uses double hashing with open addressing.
   The argument item.key has to be a pointer to an zero terminated, most
   probably strings of chars. The function for generating a number of the
   strings is simple but fast. It can be replaced by a more complex function
   like ajw (see [Aho,Sethi,Ullman]) if the needs are shown.

   We use an trick to speed up the lookup. The table is created by hcreate
   with one more element available. This enables us to use the index zero
   special. This index will never be used because we store the first hash
   index in the field used where zero means not used. Every other value
   means used. The used field can be used as a first fast comparison for
   equality of the stored and the parameter value. This helps to prevent
   unnecessary expensive calls of strcmp.  */
int _bunsorted_map_search(const void *elem, int insert_action, const void **retelem, bunsorted_map *usmap)
{
	size_t hval;
	size_t idx;
	char *p;
	_unsorted_inentry *inent;
	size_t inent_size = _usmap_inentry_size(usmap);

	/* Compute an value for the given string. Perhaps use a better method. */
	ASSERT(elem && usmap);
	hval = usmap->hash(elem);
	if (hval == 0)
		++ hval;

	/* First hash function: simply take the module but prevent zero. */
	idx = (hval % usmap->size) + 1;
	p = (char *)usmap->table;
	inent = (_unsorted_inentry *)(p + idx * inent_size);
	if (inent->used) {
		size_t hval2;
		size_t first_idx;

		/* Further action might be required according to the action value. */
		if ( inent->used == hval
		  && !usmap->compare_elem(elem, inent->elem) ) {
			*retelem = inent->elem;
			 return 0;
		}

		/* Second hash function, as suggested in [Knuth] */
		hval2 = 1 + hval % (usmap->size - 2);
		first_idx = idx;

		do {
			/* Because SIZE is prime this guarantees to step through all
			 * available indeces.
			 */
			if (idx <= hval2)
				idx = usmap->size + idx - hval2;
			else
				idx -= hval2;

			/* If we visited all entries leave the loop unsuccessfully.  */
			if (idx == first_idx)
				break;

			/* If entry is found use it. */
			inent = (_unsorted_inentry *)(p + idx * inent_size);
			if ( inent->used == hval
			  && !usmap->compare_elem(elem, inent->elem) ) {
				*retelem = inent->elem;
				return 0;
			}
		} while (inent->used);
	}

	/* An empty bucket has been found. */
	if (insert_action) {
		if (usmap->filled == usmap->size) {
			ASSERT(FALSE);
			errno = ENOMEM;
			*retelem = NULL;
			return -1;
		}
		inent = (_unsorted_inentry *)(p + idx * inent_size);
		inent->used = hval;
		memcpy(inent->elem, elem, usmap->elemsize);

		++ usmap->filled;

		*retelem = inent->elem;

		/* If table is full and another entry should be entered return with error. */
		if (usmap->grows && usmap->filled > (usmap->size >> 1)) {
			if (_re_unsorted_map(usmap)) {
				ASSERT(FALSE);
				errno = ENOMEM;
				*retelem = NULL;
				return -1;
			}
			*retelem = bunsorted_map_find(usmap, elem);
			ASSERT(*retelem);
		}
		return 0;
	}
	errno = ESRCH;
	*retelem = NULL;
	return -1;
}

int bunsorted_map_insert(bunsorted_map *usmap, const void *elem, const void **retelem)
{
	const void *_retval;
	return _bunsorted_map_search(elem, 1/*TRUE*/, retelem ? retelem : &_retval, usmap);
}

const void *bunsorted_map_find(const bunsorted_map *usmap, const void *elem)
{
	const void* retval = NULL;
#ifdef _DEBUG
	if (_bunsorted_map_search(elem, 0/*FALSE*/, &retval, (bunsorted_map*)usmap)) {
		ASSERT(retval == NULL);
		return retval;
	}
#else
	_bunsorted_map_search(elem, 0/*FALSE*/, &retval, (bunsorted_map*)usmap);
#endif/*_DEBUG*/
	return retval;
}

const void *bunsorted_map_index(const bunsorted_map *usmap, size_t idx)
{
	_unsorted_inentry *inent;
	char *p;
	size_t inent_size = _usmap_inentry_size(usmap);
	if (!usmap || !usmap->table || idx > usmap->size) {
		ASSERT(FALSE);
		errno = EINVAL;
		return NULL;
	}
	p = (char *)usmap->table;
	inent = (_unsorted_inentry *)(p + idx * inent_size);
	if (inent->used)
		return inent->elem;
	return NULL;
}
