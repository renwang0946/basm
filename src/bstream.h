
/* 2025.05.22 by renwang
 * stream buffer
 */

#ifndef __BASM_BSTREAM_H__
#define __BASM_BSTREAM_H__

#include "bglobal.h"

typedef struct _bstream
{
	size_t pos;
	size_t size;
	size_t cap;
	uint8_t* data;
	uint8_t own_data : 1;
} bstream;

int bstream_init(bstream* stm, size_t init_cap/* = 0*/);
int bstream_drop(bstream* stm);

/* does NOT call it directly */
int _bstream_grow_to(bstream* stm, size_t new_cap);

inline void bstream_clear_keep(bstream* stm)
{
	ASSERT(stm);
	stm->pos = stm->size = 0;
}

inline int bstream_putc(bstream* stm, uint8_t c)
{
	ASSERT(stm && stm->pos <= stm->size);
	if ( stm->cap < stm->pos + 1
	  && _bstream_grow_to(stm, stm->pos + 1) ) {
		return 100;
	}
	stm->data[stm->pos++] = c;
	if (stm->pos > stm->size)
		stm->size = stm->pos;
	return 0;
}

inline int bstream_putw(bstream* stm, uint16_t w)
{
	ASSERT(stm && stm->pos <= stm->size);
	if ( stm->cap < stm->pos + sizeof(uint16_t)
	  && _bstream_grow_to(stm, stm->pos + sizeof(uint16_t)) ) {
		return 100;
	}
	*(uint16_t*)(stm->data + stm->pos) = w;
	stm->pos += sizeof(uint16_t);
	if (stm->pos > stm->size)
		stm->size = stm->pos;
	return 0;
}

inline int bstream_putdw(bstream* stm, uint32_t dw)
{
	ASSERT(stm && stm->pos <= stm->size);
	if ( stm->cap < stm->pos + sizeof(uint32_t)
	  && _bstream_grow_to(stm, stm->pos + sizeof(uint32_t)) ) {
		return 100;
	}
	*(uint32_t*)(stm->data + stm->pos) = dw;
	stm->pos += sizeof(uint32_t);
	if (stm->pos > stm->size)
		stm->size = stm->pos;
	return 0;
}

inline int bstream_putqw(bstream* stm, uint64_t qw)
{
	ASSERT(stm && stm->pos <= stm->size);
	if ( stm->cap < stm->pos + sizeof(uint64_t)
	  && _bstream_grow_to(stm, stm->pos + sizeof(uint64_t)) ) {
		return 100;
	}
	*(uint64_t*)(stm->data + stm->pos) = qw;
	stm->pos += sizeof(uint64_t);
	if (stm->pos > stm->size)
		stm->size = stm->pos;
	return 0;
}

inline int bstream_putdata(bstream* stm, const void* p, size_t size)
{
	ASSERT(stm && stm->pos <= stm->size);
	if (p == NULL || size == 0) {
		ASSERT(size == 0);
		return size == 0 ? 0 : -1;
	}
	if ( stm->cap < stm->pos + size
	  && !_bstream_grow_to(stm, stm->pos + size) ) {
		ASSERT(FALSE);
		return -1;
	}
	ASSERT(stm->data);
	memcpy(stm->data + stm->pos, p, size);
	stm->pos += size;
	if (stm->pos > stm->size)
		stm->size = stm->pos;
	return 0;
}

/* boasm stream buffer inline functions
*/

inline size_t bstream_size(const bstream* stm)
{
	ASSERT(stm);
	return stm->size;
}

inline int bstream_empty(const bstream* stm)
{
	ASSERT(stm);
	return stm->size == 0;
}

inline size_t bstream_pos(const bstream* stm)
{
	ASSERT(stm);
	return stm->pos;
}

inline const uint8_t* bstream_data(const bstream* stm)
{
	ASSERT(stm);
	return stm->data;
}

inline void bstream_set_size(bstream* stm, size_t new_size)
{
	ASSERT(stm && stm->own_data);
	if (stm->cap < new_size)
		_bstream_grow_to(stm, new_size);
	stm->size = new_size;
	if (stm->pos > stm->size)
		stm->pos = stm->size;
}

inline int bstream_seek(bstream* stm, int32_t ofs, int whence/* = SEEK_SET*/)
{
	ASSERT(stm);
	if (whence == SEEK_END) {
		ASSERT((int64_t)stm->size + ofs < INT_MAX && (int64_t)stm->size + ofs >= 0);
		stm->pos = stm->size + ofs;
	} else if (whence == SEEK_CUR) {
		ASSERT((int64_t)stm->pos + ofs < INT_MAX && (int64_t)stm->pos + ofs >= 0);
		stm->pos += ofs;
	} else {  /* whence == SEEK_SET */
		ASSERT(ofs >= 0);
		stm->pos = ofs;
	}
	ASSERT(stm->pos <= stm->size);
	return stm->pos <= stm->size;
}

#define bstream_seek_begin(stm)   bstream_seek((stm), 0, SEEK_SET)
#define bstream_seek_end(stm)     bstream_seek((stm), 0, SEEK_END)

inline int bstream_attach(bstream* stm, int own_data, uint8_t* buff, size_t buff_size)
{
	ASSERT(stm);
	bstream_drop(stm);
	stm->own_data = own_data;
	stm->cap = stm->size = buff_size;
	stm->pos = 0;
	return 0;
}

inline uint8_t* bstream_detach(bstream* stm)
{
	ASSERT(stm);
	uint8_t* p = stm->data;
	stm->data = NULL;
	VERIFY(bstream_drop(stm));
	return p;
}

#endif/*__BASM_BSTREAM_H__*/
