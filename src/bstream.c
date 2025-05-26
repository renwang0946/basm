
/* 2025.02.22 by renwang */

#include "bstream.h"

/* initialize stream buffer */
int bstream_init(bstream* bstm, size_t init_cap)
{
	ASSERT(bstm);
	memset(bstm, 0, sizeof(*bstm));
	bstm->own_data = 1;
	if (init_cap)
		return _bstream_grow_to(bstm, init_cap);
	return 0;
}

int bstream_drop(bstream* bstm)
{
	ASSERT(bstm);
	if (bstm->own_data)
		free(bstm->data);
	memset(bstm, 0, sizeof(*bstm));
	bstm->own_data = 1;
	return 0;
}

/* grow */
int _bstream_grow_to(bstream* bstm, size_t new_cap)
{
	ASSERT(bstm && bstm->own_data && bstm->cap < new_cap);
	size_t dcap = bstm->cap;
	if (dcap < 4096)
		dcap += 4096;
	else if (dcap <= 1024 * 1024)
		dcap = (dcap << 2);
	else
		dcap = (new_cap * 3) >> 1;
	ASSERT(dcap < UINT_MAX / 2 && dcap >= new_cap);  /* huge data in memory ? */
	if (bstm->data == NULL) {
		bstm->data = (uint8_t*)malloc(dcap);
		if (bstm->data == NULL) {
			ASSERT(FALSE);
			return 100;
		}
	} else {
		uint8_t *old_data = bstm->data;
		bstm->data = (uint8_t *)realloc(bstm->data, dcap);
		if (bstm->data == NULL) {
			ASSERT(FALSE);
			free(old_data);
			bstm->cap = bstm->size = bstm->pos = 0;
			return 100;
		}
	}
	bstm->cap = dcap;
	ASSERT(bstm->data && bstm->size < dcap && dcap >= new_cap);
	return 0;
}
