
/* 2025.05.22 by renwang */

#ifndef __BASM_BIOEXT_H__
#define __BASM_BIOEXT_H__

#include "bglobal.h"

int bsm_file_exists(const char* fname);
char* bsm_get_dir(const char* src, char* dst_dir);
const char* bsm_get_filename(const char* full_file);
char* bsm_fname_without_ext(const char* full_file, char* of_name);

inline int bsm_save_stream(const uint8_t* data, size_t size, const char* fname)
{
	FILE* fp = NULL;
	fopen_s(&fp, fname, "wb");
	if (fp == NULL) {
		fprintf(stderr, "open file '%s' failed: %d\n", fname, errno);
		return -1;
	}
	ASSERT(size > 0);
	VERIFY(fwrite(data, 1, size, fp) == size);
	fclose(fp);
	return 0;
}

#endif/*__BASM_BIOEXT_H__*/
